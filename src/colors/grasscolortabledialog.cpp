#include "grasscolortabledialog.h"

#include "../canvas/grassmapcanvas.h"
#include "../canvas/grassrastermaplayer.h"
#include "../grassenv.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QTextStream>
#include <QVBoxLayout>

extern "C" {
#include <grass/gis.h>
#include <grass/raster.h>
}

// ============================================================
// GrassColorBar
// ============================================================
GrassColorBar::GrassColorBar( QWidget *parent )
    : QWidget( parent )
{
    setMinimumHeight( 36 );
    setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
}

void GrassColorBar::setGradient( const QVector<QPair<double, QColor>> &stops )
{
    mStops = stops;
    update();
}

void GrassColorBar::paintEvent( QPaintEvent * )
{
    QPainter p( this );
    const int W = width();
    const int H = height();

    if ( mStops.isEmpty() )
    {
        p.fillRect( rect(), QColor( 50, 50, 50 ) );
        p.setPen( QColor( "#888" ) );
        p.drawText( rect(), Qt::AlignCenter, "No preview" );
        return;
    }

    // Build a linear gradient from the stops
    QLinearGradient grad( 0, 0, W, 0 );
    for ( const auto &[pct, col] : mStops )
        grad.setColorAt( std::clamp( pct, 0.0, 1.0 ), col );

    p.fillRect( rect(), grad );

    // Border
    p.setPen( QColor( "#555" ) );
    p.drawRect( 0, 0, W-1, H-1 );
}

// ============================================================
// GrassColorTableDialog
// ============================================================

// The 57 official GRASS color table names
static const QStringList kColorTables = {
    "aspect","aspectcolr","bcyr","bgyr","blues","byg","byr",
    "celsius","corine","curvature","differences","elevation","etopo2",
    "evi","fahrenheit","forest_cover","gdd","grass","greens","grey",
    "grey.eq","grey.log","grey1.0","grey255","gyr","haxby","inferno",
    "kelvin","magma","ndvi","ndwi","nlcd","oranges","plasma",
    "population","population_dens","precipitation","precipitation_daily",
    "precipitation_monthly","rainbow","ramp","random","reds","roygbiv",
    "rstcurv","ryb","ryg","sepia","slope","soilmoisture","srtm",
    "srtm_percent","srtm_plus","terrain","viridis","water","wave"
};

GrassColorTableDialog::GrassColorTableDialog( const QString &mapName,
                                               const QString &mapset,
                                               GrassMapCanvas *canvas,
                                               QWidget *parent )
    : QDialog( parent )
    , mMapName( mapName )
    , mMapset( mapset )
    , mCanvas( canvas )
{
    const QString mapRef = mapset.isEmpty() ? mapName : mapName + "@" + mapset;
    setWindowTitle( "Color Table — " + mapRef );
    setMinimumWidth( 420 );

    QVBoxLayout *root = new QVBoxLayout( this );

    // Color table selector
    QGroupBox *selBox = new QGroupBox( "Color table", this );
    QFormLayout *form = new QFormLayout( selBox );

    mTableCombo = new QComboBox( selBox );
    mTableCombo->addItems( kColorTables );
    // Restore last-applied table for this map (persisted via QSettings)
    {
        QSettings s( "qtgrass", "qtgrass" );
        const QString saved = s.value( "colortable/" + mapRef, "elevation" ).toString();
        mTableCombo->setCurrentText( saved );
    }
    form->addRow( "Preset:", mTableCombo );

    mInvertCheck   = new QCheckBox( "Invert colors (-n)", selBox );
    mEqualizeCheck = new QCheckBox( "Histogram equalization (-e)", selBox );
    form->addRow( mInvertCheck );
    form->addRow( mEqualizeCheck );
    root->addWidget( selBox );

    // Gradient preview bar
    QGroupBox *prevBox = new QGroupBox( "Preview", this );
    QVBoxLayout *pvLay = new QVBoxLayout( prevBox );
    mBar = new GrassColorBar( prevBox );
    pvLay->addWidget( mBar );
    root->addWidget( prevBox );

    // Status
    mStatus = new QLabel( this );
    mStatus->setWordWrap( true );
    root->addWidget( mStatus );

    // Buttons
    QDialogButtonBox *btns = new QDialogButtonBox( this );
    QPushButton *applyBtn   = btns->addButton( "Apply",   QDialogButtonBox::ApplyRole );
    QPushButton *previewBtn = btns->addButton( "Preview", QDialogButtonBox::ResetRole );
    btns->addButton( QDialogButtonBox::Close );
    root->addWidget( btns );

    connect( mTableCombo, &QComboBox::currentTextChanged,
             this, &GrassColorTableDialog::onTableSelected );
    connect( applyBtn,   &QPushButton::clicked, this, &GrassColorTableDialog::applyColors );
    connect( previewBtn, &QPushButton::clicked, this, &GrassColorTableDialog::previewColors );
    connect( btns, &QDialogButtonBox::rejected, this, &QDialog::accept );

    // GRASS access model: disable Apply/Preview when the map lives in a mapset
    // other than the active one (typically PERMANENT while a user mapset is in
    // use).  The user can still inspect/compare colour tables visually.
    {
        const QString activeMapset =
            QString::fromLocal8Bit( qgetenv( "MAPSET" ) );
        const QString effectiveMapset = mapset.isEmpty() ? activeMapset : mapset;
        if ( !activeMapset.isEmpty() && effectiveMapset != activeMapset )
        {
            applyBtn->setEnabled( false );
            previewBtn->setEnabled( false );
            mStatus->setStyleSheet( "color:#c00;" );
            mStatus->setText( QString( "Read-only: '%1' lives in mapset '%2'; "
                                        "active mapset is '%3'." )
                                .arg( mapName, effectiveMapset, activeMapset ) );
        }
    }

    // Show the CURRENTLY STORED colour table from the GRASS database so the
    // user immediately sees what the raster looks like before changing it.
    showStoredColorTable();

    // Initial preview
    onTableSelected( mTableCombo->currentText() );
}

// ---------------------------------------------------------------------------
// showStoredColorTable — render the colour rules currently in the GRASS DB
// ---------------------------------------------------------------------------
void GrassColorTableDialog::showStoredColorTable()
{
    const QByteArray nameB   = mMapName.toLocal8Bit();
    const QByteArray mapsetB = mMapset.toLocal8Bit();
    const char *cName   = nameB.constData();
    const char *cMapset = mapsetB.isEmpty() ? "" : mapsetB.constData();

    struct Colors colors;
    if ( Rast_read_colors( cName, cMapset, &colors ) < 0 )
        return;   // no stored table — keep the file-based preview from loadGradient

    // Read the raster value range
    struct FPRange fpRange;
    Rast_init_fp_range( &fpRange );
    Rast_read_fp_range( cName, cMapset, &fpRange );
    DCELL dmin = 0.0, dmax = 1.0;
    Rast_get_fp_range_min_max( &fpRange, &dmin, &dmax );
    if ( dmin == dmax ) { dmax = dmin + 1.0; }

    // Sample 128 points across the range → gradient stops (0–1)
    const int nSamples = 128;
    QVector<QPair<double, QColor>> stops;
    stops.reserve( nSamples + 1 );
    for ( int i = 0; i <= nSamples; ++i )
    {
        const DCELL v = dmin + static_cast<double>(i) / nSamples * (dmax - dmin);
        int r, g, b;
        Rast_get_d_color( &v, &r, &g, &b, &colors );
        stops.append( { static_cast<double>(i) / nSamples, QColor(r, g, b) } );
    }
    Rast_free_colors( &colors );

    mBar->setGradient( stops );
    mStatus->setText( "Showing stored colour table from GRASS database." );
}

void GrassColorTableDialog::onTableSelected( const QString &name )
{
    loadGradient( name );
}

// ---------------------------------------------------------------------------
// parseGrassNamedColor — map GRASS colour names → QColor
// ---------------------------------------------------------------------------
static QColor parseGrassNamedColor( const QString &s )
{
    // First try Qt's built-in colour names (covers "red","blue","cyan","yellow",…)
    QColor c( s.toLower() );
    if ( c.isValid() ) return c;
    // Extra GRASS aliases not in Qt
    static const QHash<QString,QColor> extra = {
        {"grey",    {128,128,128}},
        {"gray",    {128,128,128}},
        {"violet",  {238,130,238}},
        {"aqua",    {0,255,255}},
        {"brown",   {139,69,19}},
        {"orange",  {255,165,0}},
    };
    return extra.value( s.toLower(), Qt::gray );
}

// ---------------------------------------------------------------------------
// loadGrassColorFile — read $GISBASE/etc/colors/<name> and return stops [0,1]
// ---------------------------------------------------------------------------
using ColorStops = QVector<QPair<double, QColor>>;

static ColorStops loadGrassColorFile( const QString &gisBase,
                                       const QString &tableName )
{
    QFile f( gisBase + "/etc/colors/" + tableName );
    if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) )
        return {};

    struct Raw { double val; QColor color; bool isPct; };
    QVector<Raw> rows;
    QTextStream ts( &f );
    while ( !ts.atEnd() )
    {
        const QString line = ts.readLine().trimmed();
        if ( line.isEmpty() || line.startsWith('#') ) continue;

        const QStringList tok = line.split( QRegularExpression("\\s+"),
                                            Qt::SkipEmptyParts );
        if ( tok.size() < 2 ) continue;

        const bool isPct = tok[0].endsWith('%');
        const QString vs  = isPct ? tok[0].chopped(1) : tok[0];
        bool ok;
        double val = vs.toDouble( &ok );
        if ( !ok ) continue;          // skip "nv", "default", text tokens

        QColor col;
        if ( tok.size() >= 4 )        // R G B triplet
            col = { tok[1].toInt(), tok[2].toInt(), tok[3].toInt() };
        else if ( tok[1].contains(':') )   // R:G:B
        {
            const QStringList rgb = tok[1].split(':');
            if ( rgb.size() == 3 )
                col = { rgb[0].toInt(), rgb[1].toInt(), rgb[2].toInt() };
        }
        else                          // colour name
            col = parseGrassNamedColor( tok[1] );

        if ( col.isValid() )
            rows.append( { val, col, isPct } );
    }

    if ( rows.isEmpty() ) return {};

    // If all stops are percentages, normalise directly
    const bool allPct = std::all_of( rows.begin(), rows.end(),
                                     []( const Raw &r ){ return r.isPct; } );
    if ( allPct )
    {
        ColorStops out;
        for ( const Raw &r : rows )
            out.append( { r.val / 100.0, r.color } );
        return out;
    }

    // Absolute values — normalise by min/max
    double lo = rows[0].val, hi = rows[0].val;
    for ( const Raw &r : rows ) { lo = std::min(lo,r.val); hi = std::max(hi,r.val); }
    const double span = hi - lo;
    if ( span == 0.0 ) return { { 0.0, rows[0].color }, { 1.0, rows[0].color } };

    ColorStops out;
    for ( const Raw &r : rows )
        out.append( { (r.val - lo) / span, r.color } );
    return out;
}

// ---------------------------------------------------------------------------
// loadGradient: read the GRASS colour-table file and show in the preview bar
// ---------------------------------------------------------------------------
void GrassColorTableDialog::loadGradient( const QString &colorTable )
{
    const QString gisBase = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );

    // 1. Try reading the file from $GISBASE/etc/colors/
    if ( !gisBase.isEmpty() )
    {
        const ColorStops stops = loadGrassColorFile( gisBase, colorTable );
        if ( !stops.isEmpty() ) { mBar->setGradient( stops ); return; }
    }

    // 2. Fallback for computed tables that have no file (grey.eq, grey.log, …)
    static const QHash<QString, ColorStops> kFallback = {
        { "grey.eq",  { {0.0, Qt::black}, {1.0, Qt::white} } },
        { "grey.log", { {0.0, Qt::black}, {1.0, Qt::white} } },
        { "corine",   { {0.0,{230,0,77}},{0.3,{255,255,168}},{0.6,{0,100,0}},{0.9,{0,128,255}},{1.0,{255,255,255}} } },
    };
    auto it = kFallback.find( colorTable );
    if ( it != kFallback.end() ) { mBar->setGradient( it.value() ); return; }

    // 3. Last resort: black→white generic gradient
    mBar->setGradient( { {0.0, Qt::black}, {1.0, Qt::white} } );
}

// ---------------------------------------------------------------------------
// previewColors: apply to map but don't close — just refresh canvas
// ---------------------------------------------------------------------------
void GrassColorTableDialog::previewColors()
{
    if ( runRColors( mTableCombo->currentText() ) )
    {
        if ( mCanvas )
        {
            for ( GrassMapLayer *l : mCanvas->layers() )
            {
                auto *rl = dynamic_cast<GrassRasterMapLayer *>( l );
                if ( rl && rl->name() == mMapName )
                    rl->invalidateCache();
            }
            mCanvas->scheduleRender();
        }
        mStatus->setStyleSheet( "color:#1a8a1a;" );
        mStatus->setText( "Color table applied — canvas refreshing." );
    }
}

// ---------------------------------------------------------------------------
// applyColors: apply + close
// ---------------------------------------------------------------------------
void GrassColorTableDialog::applyColors()
{
    if ( runRColors( mTableCombo->currentText() ) )
    {
        if ( mCanvas )
        {
            // Force every matching layer to reload its color table from disk
            // and drop its pixel cache so the next render uses the new colors.
            for ( GrassMapLayer *l : mCanvas->layers() )
            {
                auto *rl = dynamic_cast<GrassRasterMapLayer *>( l );
                if ( rl && rl->name() == mMapName )
                    rl->invalidateCache();
            }
            mCanvas->scheduleRender();
        }
        mStatus->setStyleSheet( "color:#1a8a1a;" );
        mStatus->setText( "Applied — canvas will refresh." );
    }
}

bool GrassColorTableDialog::runRColors( const QString &colorTable )
{
    const QString gisBase = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
    if ( gisBase.isEmpty() ) { mStatus->setText( "GISBASE not set." ); return false; }

    // GRASS access model: a map is writeable only when it lives in the active
    // mapset.  r.colors writes to <mapset>/colr/<map>; refuse if read-only.
    const QString activeMapset =
        QString::fromLocal8Bit( qgetenv( "MAPSET" ) );
    const QString effectiveMapset = mMapset.isEmpty() ? activeMapset : mMapset;
    if ( !activeMapset.isEmpty() && effectiveMapset != activeMapset )
    {
        mStatus->setStyleSheet( "color:#c00;" );
        mStatus->setText( QString( "Read-only: '%1' lives in mapset '%2'; "
                                    "active mapset is '%3'." )
                            .arg( mMapName, effectiveMapset, activeMapset ) );
        return false;
    }

    const QString mapRef = mMapset.isEmpty()
                           ? mMapName : mMapName + "@" + mMapset;

    QStringList args = { "map=" + mapRef, "color=" + colorTable };
    if ( mInvertCheck  ->isChecked() ) args << "-n";
    if ( mEqualizeCheck->isChecked() ) args << "-e";

    // Persist chosen table so the dialog reopens on the correct selection
    QSettings( "qtgrass", "qtgrass" ).setValue( "colortable/" + mapRef, colorTable );

    QProcess proc;
    proc.setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );
    proc.start( gisBase + "/bin/r.colors", args );
    proc.waitForFinished( 10000 );

    if ( proc.exitCode() != 0 )
    {
        mStatus->setStyleSheet( "color:#c00;" );
        mStatus->setText( "r.colors failed: " +
                          proc.readAllStandardError().simplified() );
        return false;
    }
    return true;
}
