#include "grassaddlayerdialog.h"
#include "grassmapcanvas.h"
#include "../grassenv.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

// ── helpers ───────────────────────────────────────────────────────────────────

// Populate cb with "name@mapset" strings from g.list covering all accessible mapsets.
// -m flag: output names in "name@mapset" format.
static void fillRasterCombo( QComboBox *cb )
{
    const QString gb = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
    if ( gb.isEmpty() ) return;

    QProcess p;
    p.setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );
    // Pass -m as a bare flag (not flags=m) to ensure correct name@mapset output.
    p.start( gb + "/bin/g.list",
             { "-m", "type=raster", "mapset=*", "separator=newline" } );
    p.waitForFinished( 8000 );
    const QStringList maps =
        QString::fromLocal8Bit( p.readAllStandardOutput() )
        .split( '\n', Qt::SkipEmptyParts );

    cb->addItem( "(none)", "" );
    for ( const QString &m : maps )
        cb->addItem( m.trimmed(), m.trimmed() );
}

void GrassAddLayerDialog::populateRasterCombo( QComboBox *cb ) const
{
    fillRasterCombo( cb );
}

// Split "name@mapset" → name; set mapset
QString GrassAddLayerDialog::splitMapRef( const QString &ref, QString &mapset )
{
    const int at = ref.indexOf( '@' );
    if ( at < 0 ) { mapset.clear(); return ref; }
    mapset = ref.mid( at + 1 );
    return ref.left( at );
}

// Colour-picker button helper
void GrassAddLayerDialog::pickColor( QPushButton *btn, QColor &target )
{
    QColor c = QColorDialog::getColor( target, this, "Pick colour",
                                       QColorDialog::ShowAlphaChannel );
    if ( !c.isValid() ) return;
    target = c;
    QPixmap px( 48, 16 ); px.fill( c );
    btn->setIcon( QIcon( px ) );
    btn->setText( c.name( QColor::HexArgb ) );
}

// ── page builders ─────────────────────────────────────────────────────────────

void GrassAddLayerDialog::buildRgbPage( QWidget *page )
{
    auto *form = new QFormLayout( page );
    mRgbR = new QComboBox( page ); populateRasterCombo( mRgbR );
    mRgbG = new QComboBox( page ); populateRasterCombo( mRgbG );
    mRgbB = new QComboBox( page ); populateRasterCombo( mRgbB );
    form->addRow( "Red band:",   mRgbR );
    form->addRow( "Green band:", mRgbG );
    form->addRow( "Blue band:",  mRgbB );
    auto *note = new QLabel( "Each band is auto-stretched to its full data range.", page );
    note->setWordWrap( true );
    note->setStyleSheet( "color:#888;font-size:9px;" );
    form->addRow( note );
}

void GrassAddLayerDialog::buildIhsPage( QWidget *page )
{
    auto *form = new QFormLayout( page );
    mIhsI = new QComboBox( page ); populateRasterCombo( mIhsI );
    mIhsH = new QComboBox( page ); populateRasterCombo( mIhsH );
    mIhsS = new QComboBox( page ); populateRasterCombo( mIhsS );
    form->addRow( "Intensity:",  mIhsI );
    form->addRow( "Hue:",        mIhsH );
    form->addRow( "Saturation:", mIhsS );
    auto *note = new QLabel( "Bands are normalised to I∈[0,1], H∈[0°,360°), S∈[0,1].", page );
    note->setWordWrap( true );
    note->setStyleSheet( "color:#888;font-size:9px;" );
    form->addRow( note );
}

void GrassAddLayerDialog::buildShadePage( QWidget *page )
{
    auto *form = new QFormLayout( page );
    mShColor = new QComboBox( page ); populateRasterCombo( mShColor );
    mShShade = new QComboBox( page ); populateRasterCombo( mShShade );
    mShBright = new QSpinBox( page );
    mShBright->setRange( 1, 100 ); mShBright->setValue( 50 );
    mShBright->setSuffix( " (50=neutral)" );
    form->addRow( "Colour raster:",  mShColor );
    form->addRow( "Shade raster:",   mShShade );
    form->addRow( "Brightness:",     mShBright );
    auto *note = new QLabel(
        "Shade raster is typically produced by r.relief or r.shade.", page );
    note->setWordWrap( true );
    note->setStyleSheet( "color:#888;font-size:9px;" );
    form->addRow( note );
}

void GrassAddLayerDialog::buildNumPage( QWidget *page )
{
    auto *form = new QFormLayout( page );
    mNumMap = new QComboBox( page ); populateRasterCombo( mNumMap );

    mNumDec     = new QSpinBox( page );  mNumDec->setRange( 0, 10 );  mNumDec->setValue( 2 );
    mNumFont    = new QSpinBox( page );  mNumFont->setRange( 5, 24 ); mNumFont->setValue( 9 );
    mNumFont->setSuffix( " pt" );
    mNumMinCell = new QSpinBox( page );  mNumMinCell->setRange( 4, 200 ); mNumMinCell->setValue( 20 );
    mNumMinCell->setSuffix( " px" );
    mNumMinCell->setToolTip( "Minimum native cell width in screen pixels before text is drawn" );
    mNumShowBg   = new QCheckBox( "Draw text background",  page ); mNumShowBg->setChecked( true );
    mNumShowNull = new QCheckBox( "Label NODATA as 'null'", page );

    auto makeColorBtn = [&]( const QString &lbl, QColor &ref ) -> QPushButton * {
        auto *btn = new QPushButton( page );
        QPixmap px( 48, 16 ); px.fill( ref );
        btn->setIcon( QIcon( px ) ); btn->setText( ref.name() );
        connect( btn, &QPushButton::clicked, this,
                 [this, btn, &ref]{ pickColor( btn, ref ); } );
        form->addRow( lbl, btn );
        return btn;
    };

    form->addRow( "Raster map:",      mNumMap     );
    form->addRow( "Decimal places:",  mNumDec     );
    form->addRow( "Font size:",       mNumFont    );
    form->addRow( "Draw when cell ≥", mNumMinCell );
    form->addRow( mNumShowBg   );
    form->addRow( mNumShowNull );
    mNumTextColor = makeColorBtn( "Text colour:",       mNumTextClr );
    mNumBgColor   = makeColorBtn( "Background colour:", mNumBgClr  );
}

void GrassAddLayerDialog::buildArrowPage( QWidget *page )
{
    auto *form = new QFormLayout( page );
    mArrDir  = new QComboBox( page ); populateRasterCombo( mArrDir );
    mArrMag  = new QComboBox( page ); populateRasterCombo( mArrMag );

    mArrSize  = new QSpinBox( page ); mArrSize->setRange( 4, 120 ); mArrSize->setValue( 20 );
    mArrSize->setSuffix( " px" );
    mArrMin   = new QSpinBox( page ); mArrMin->setRange( 2, 200 );  mArrMin->setValue( 12 );
    mArrMin->setSuffix( " px" );
    mArrMin->setToolTip( "Minimum native cell width in pixels before arrows are drawn" );

    mArrStyle = new QComboBox( page );
    mArrStyle->addItem( "Line",    int( GrassRastArrowLayer::ArrowStyle::Line    ) );
    mArrStyle->addItem( "Filled",  int( GrassRastArrowLayer::ArrowStyle::Filled  ) );
    mArrStyle->addItem( "Outline", int( GrassRastArrowLayer::ArrowStyle::Outline ) );

    mArrGrass = new QCheckBox( "GRASS aspect convention (0=E, CCW)", page );
    mArrGrass->setChecked( true );

    mArrColor = new QPushButton( page );
    {
        QPixmap px( 48, 16 ); px.fill( mArrClr );
        mArrColor->setIcon( QIcon( px ) ); mArrColor->setText( mArrClr.name() );
    }
    connect( mArrColor, &QPushButton::clicked, this,
             [this]{ pickColor( mArrColor, mArrClr ); } );

    form->addRow( "Direction raster:",  mArrDir   );
    form->addRow( "Magnitude raster:",  mArrMag   );
    form->addRow( "Arrow size:",        mArrSize  );
    form->addRow( "Draw when cell ≥",   mArrMin   );
    form->addRow( "Arrow style:",       mArrStyle );
    form->addRow( "Arrow colour:",      mArrColor );
    form->addRow( mArrGrass );
}

// ── Constructor ───────────────────────────────────────────────────────────────

GrassAddLayerDialog::GrassAddLayerDialog( GrassMapCanvas *canvas,
                                           LayerType type,
                                           QWidget *parent )
    : QDialog( parent )
    , mCanvas( canvas )
{
    setWindowTitle( "Add Composite Layer" );
    setMinimumWidth( 420 );

    auto *root = new QVBoxLayout( this );

    // ── Type selector ────────────────────────────────────────────────────────
    auto *typeBox = new QGroupBox( "Layer type", this );
    auto *tLay    = new QHBoxLayout( typeBox );
    mTypeCombo    = new QComboBox( typeBox );
    // Raster composites
    mTypeCombo->addItem( "🎨  RGB Composite",           int( LayerType::Rgb          ) );
    mTypeCombo->addItem( "🌈  IHS Composite",           int( LayerType::Ihs          ) );
    mTypeCombo->addItem( "🏔   Shaded Relief",           int( LayerType::Shade        ) );
    mTypeCombo->addItem( "🔢  Cell Values",              int( LayerType::RastNum      ) );
    mTypeCombo->addItem( "➡  Flow / Aspect Arrows",     int( LayerType::RastArrow    ) );
    // Vector composites
    mTypeCombo->addItem( "🗺  Thematic Choropleth",      int( LayerType::VectThematic ) );
    mTypeCombo->addItem( "📊  Feature Charts",           int( LayerType::VectChart    ) );
    mTypeCombo->addItem( "🏷  Attribute Labels",         int( LayerType::Label        ) );
    // Geometric overlays
    mTypeCombo->addItem( "🌐  Great Circle (Geodesic)",  int( LayerType::Geodesic     ) );
    mTypeCombo->addItem( "🧭  Rhumb Line (Loxodrome)",   int( LayerType::Rhumbline    ) );
    tLay->addWidget( mTypeCombo );
    root->addWidget( typeBox );

    // ── Stacked pages ────────────────────────────────────────────────────────
    mStack = new QStackedWidget( this );

    auto *pageRgb    = new QWidget(); buildRgbPage      ( pageRgb    ); mStack->addWidget( pageRgb    );
    auto *pageIhs    = new QWidget(); buildIhsPage      ( pageIhs    ); mStack->addWidget( pageIhs    );
    auto *pageShade  = new QWidget(); buildShadePage    ( pageShade  ); mStack->addWidget( pageShade  );
    auto *pageNum    = new QWidget(); buildNumPage      ( pageNum    ); mStack->addWidget( pageNum    );
    auto *pageArrow  = new QWidget(); buildArrowPage    ( pageArrow  ); mStack->addWidget( pageArrow  );
    auto *pageTheme  = new QWidget(); buildThematicPage ( pageTheme  ); mStack->addWidget( pageTheme  );
    auto *pageChart  = new QWidget(); buildChartPage    ( pageChart  ); mStack->addWidget( pageChart  );
    auto *pageLabel  = new QWidget(); buildLabelPage    ( pageLabel  ); mStack->addWidget( pageLabel  );
    auto *pageGeo    = new QWidget(); buildGeodesicPage ( pageGeo    ); mStack->addWidget( pageGeo    );
    auto *pageRhumb  = new QWidget(); buildRhumbPage    ( pageRhumb  ); mStack->addWidget( pageRhumb  );

    root->addWidget( mStack, 1 );

    // ── Buttons ──────────────────────────────────────────────────────────────
    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this );
    btns->button( QDialogButtonBox::Ok )->setText( "Add to Map" );
    connect( btns, &QDialogButtonBox::accepted, this, &QDialog::accept );
    connect( btns, &QDialogButtonBox::rejected, this, &QDialog::reject );
    root->addWidget( btns );

    // ── Wiring ───────────────────────────────────────────────────────────────
    connect( mTypeCombo, QOverload<int>::of( &QComboBox::currentIndexChanged ),
             this, &GrassAddLayerDialog::onTypeChanged );

    // Pre-select the requested type
    mTypeCombo->setCurrentIndex( int( type ) );
    mStack->setCurrentIndex( int( type ) );
}

void GrassAddLayerDialog::onTypeChanged( int idx )
{
    mStack->setCurrentIndex( idx );
}

// ── createLayer ───────────────────────────────────────────────────────────────

GrassMapLayer *GrassAddLayerDialog::createLayer() const
{
    const auto type = LayerType( mTypeCombo->currentData().toInt() );

    auto pick = []( QComboBox *cb ) -> QPair<QString,QString> {
        const QString ref = cb->currentData().toString();
        QString ms; QString n = GrassAddLayerDialog::splitMapRef( ref, ms );
        return { n, ms };
    };

    switch ( type ) {

        case LayerType::Rgb: {
            const auto [rn, rm] = pick( mRgbR );
            const auto [gn, gm] = pick( mRgbG );
            const auto [bn, bm] = pick( mRgbB );
            if ( rn.isEmpty() || gn.isEmpty() || bn.isEmpty() ) return nullptr;
            GrassRgbLayer::Config cfg;
            cfg.redMap = rn; cfg.redMapset = rm;
            cfg.greenMap = gn; cfg.greenMapset = gm;
            cfg.blueMap = bn; cfg.blueMapset = bm;
            return new GrassRgbLayer( cfg );
        }

        case LayerType::Ihs: {
            const auto [in, im] = pick( mIhsI );
            const auto [hn, hm] = pick( mIhsH );
            const auto [sn, sm] = pick( mIhsS );
            if ( in.isEmpty() || hn.isEmpty() || sn.isEmpty() ) return nullptr;
            GrassIhsLayer::Config cfg;
            cfg.intensityMap = in; cfg.intensityMapset = im;
            cfg.hueMap = hn; cfg.hueMapset = hm;
            cfg.satMap = sn; cfg.satMapset = sm;
            return new GrassIhsLayer( cfg );
        }

        case LayerType::Shade: {
            const auto [cn, cm] = pick( mShColor );
            const auto [sn, sm] = pick( mShShade );
            if ( cn.isEmpty() || sn.isEmpty() ) return nullptr;
            GrassShadeLayer::Config cfg;
            cfg.colorMap = cn; cfg.colorMapset = cm;
            cfg.shadeMap = sn; cfg.shadeMapset = sm;
            cfg.bright   = mShBright->value();
            return new GrassShadeLayer( cfg );
        }

        case LayerType::RastNum: {
            const auto [mn, mm] = pick( mNumMap );
            if ( mn.isEmpty() ) return nullptr;
            GrassRastNumLayer::Config cfg;
            cfg.map = mn; cfg.mapset = mm;
            cfg.decimals  = mNumDec->value();
            cfg.fontSize  = mNumFont->value();
            cfg.minCellPx = mNumMinCell->value();
            cfg.showBg    = mNumShowBg->isChecked();
            cfg.showNull  = mNumShowNull->isChecked();
            cfg.textColor = mNumTextClr;
            cfg.bgColor   = mNumBgClr;
            return new GrassRastNumLayer( cfg );
        }

        case LayerType::RastArrow: {
            const auto [dn, dm] = pick( mArrDir );
            if ( dn.isEmpty() ) return nullptr;
            const auto [mn, mm] = pick( mArrMag );
            GrassRastArrowLayer::Config cfg;
            cfg.dirMap    = dn; cfg.dirMapset = dm;
            cfg.magMap    = mn; cfg.magMapset = mm;
            cfg.arrowSize = mArrSize->value();
            cfg.minCellPx = mArrMin->value();
            cfg.style     = GrassRastArrowLayer::ArrowStyle( mArrStyle->currentData().toInt() );
            cfg.dirIsGrass= mArrGrass->isChecked();
            cfg.color     = mArrClr;
            return new GrassRastArrowLayer( cfg );
        }

        case LayerType::VectThematic: {
            const auto [mn, mm] = pick( mThMap );
            if ( mn.isEmpty() || mThCol->text().trimmed().isEmpty() ) return nullptr;
            GrassVectThematicLayer::Config cfg;
            cfg.map    = mn; cfg.mapset = mm;
            cfg.column   = mThCol->text().trimmed();
            cfg.nClasses = mThN->value();
            cfg.method   = GrassVectThematicLayer::ClassMethod( mThMethod->currentData().toInt() );
            cfg.ramp     = GrassVectThematicLayer::ColorRamp( mThRamp->currentData().toInt() );
            cfg.reverse  = mThRev->isChecked();
            return new GrassVectThematicLayer( cfg );
        }

        case LayerType::VectChart: {
            const auto [mn, mm] = pick( mChMap );
            if ( mn.isEmpty() || mChCols->text().trimmed().isEmpty() ) return nullptr;
            GrassVectChartLayer::Config cfg;
            cfg.map     = mn; cfg.mapset = mm;
            cfg.columns = mChCols->text().trimmed().split( QRegularExpression( "[,;\\s]+" ), Qt::SkipEmptyParts );
            cfg.chartType = GrassVectChartLayer::ChartType( mChType->currentData().toInt() );
            cfg.maxSize   = mChSize->value();
            cfg.scale     = mChScale->isChecked();
            return new GrassVectChartLayer( cfg );
        }

        case LayerType::Label: {
            const auto [mn, mm] = pick( mLbMap );
            if ( mn.isEmpty() || mLbCol->text().trimmed().isEmpty() ) return nullptr;
            GrassLabelLayer::Config cfg;
            cfg.map     = mn; cfg.mapset = mm;
            cfg.labelColumn = mLbCol->text().trimmed();
            cfg.fontSize    = mLbFont->value();
            cfg.haloWidth   = mLbHalo->value();
            cfg.bold        = mLbBold->isChecked();
            cfg.italic      = mLbItalic->isChecked();
            cfg.textColor   = mLbTextClr;
            return new GrassLabelLayer( cfg );
        }

        case LayerType::Geodesic: {
            GrassGeodesicLayer::Config cfg;
            cfg.lon1 = mGeoLon1->value(); cfg.lat1 = mGeoLat1->value();
            cfg.lon2 = mGeoLon2->value(); cfg.lat2 = mGeoLat2->value();
            cfg.color = mGeoClr;
            return new GrassGeodesicLayer( cfg );
        }

        case LayerType::Rhumbline: {
            GrassRhumblineLayer::Config cfg;
            cfg.lon1 = mRhmLon1->value(); cfg.lat1 = mRhmLat1->value();
            cfg.lon2 = mRhmLon2->value(); cfg.lat2 = mRhmLat2->value();
            cfg.color = mRhmClr;
            return new GrassRhumblineLayer( cfg );
        }
    }
    return nullptr;
}

// ── Page builders for new layer types ────────────────────────────────────────

void GrassAddLayerDialog::buildThematicPage( QWidget *page )
{
    auto *form = new QFormLayout( page );
    mThMap    = new QComboBox( page ); populateVectorCombo( mThMap );
    mThCol    = new QLineEdit( page ); mThCol->setPlaceholderText( "attribute column name" );
    mThN      = new QSpinBox( page );  mThN->setRange(2,20); mThN->setValue(5);
    mThMethod = new QComboBox( page );
    mThMethod->addItem( "Equal interval", int( GrassVectThematicLayer::ClassMethod::EqualInterval ) );
    mThMethod->addItem( "Quantile",       int( GrassVectThematicLayer::ClassMethod::Quantile       ) );
    mThMethod->addItem( "Std deviation",  int( GrassVectThematicLayer::ClassMethod::StdDev         ) );
    mThRamp   = new QComboBox( page );
    mThRamp->addItem( "Yellow→Red (YlOrRd)", int( GrassVectThematicLayer::ColorRamp::YlOrRd  ) );
    mThRamp->addItem( "Blues",               int( GrassVectThematicLayer::ColorRamp::Blues   ) );
    mThRamp->addItem( "Greens",              int( GrassVectThematicLayer::ColorRamp::Greens  ) );
    mThRamp->addItem( "Red↔Green (RdYlGn)", int( GrassVectThematicLayer::ColorRamp::RdYlGn  ) );
    mThRamp->addItem( "Viridis",             int( GrassVectThematicLayer::ColorRamp::Viridis ) );
    mThRamp->addItem( "Plasma",              int( GrassVectThematicLayer::ColorRamp::Plasma  ) );
    mThRev    = new QCheckBox( "Reverse colour ramp", page );
    form->addRow( "Vector map:",    mThMap    );
    form->addRow( "Column:",        mThCol    );
    form->addRow( "Classes:",       mThN      );
    form->addRow( "Method:",        mThMethod );
    form->addRow( "Colour ramp:",   mThRamp   );
    form->addRow( mThRev );
}

void GrassAddLayerDialog::buildChartPage( QWidget *page )
{
    auto *form = new QFormLayout( page );
    mChMap   = new QComboBox( page ); populateVectorCombo( mChMap );
    mChCols  = new QLineEdit( page );
    mChCols->setPlaceholderText( "col1, col2, col3 (comma-separated numeric columns)" );
    mChType  = new QComboBox( page );
    mChType->addItem( "Pie",  int( GrassVectChartLayer::ChartType::Pie ) );
    mChType->addItem( "Bar",  int( GrassVectChartLayer::ChartType::Bar ) );
    mChSize  = new QSpinBox( page );  mChSize->setRange(8,200);  mChSize->setValue(40);
    mChSize->setSuffix(" px");
    mChScale = new QCheckBox( "Scale chart size by total value", page );
    mChScale->setChecked( true );
    form->addRow( "Vector map:",    mChMap   );
    form->addRow( "Data columns:",  mChCols  );
    form->addRow( "Chart type:",    mChType  );
    form->addRow( "Max size:",      mChSize  );
    form->addRow( mChScale );
}

void GrassAddLayerDialog::buildLabelPage( QWidget *page )
{
    auto *form = new QFormLayout( page );
    mLbMap    = new QComboBox( page ); populateVectorCombo( mLbMap );
    mLbCol    = new QLineEdit( page ); mLbCol->setPlaceholderText( "attribute column" );
    mLbFont   = new QSpinBox( page );  mLbFont->setRange(5,36);  mLbFont->setValue(10);
    mLbFont->setSuffix(" pt");
    mLbHalo   = new QSpinBox( page );  mLbHalo->setRange(0,8);   mLbHalo->setValue(2);
    mLbHalo->setToolTip("0 = no halo");
    mLbBold   = new QCheckBox( "Bold",   page );
    mLbItalic = new QCheckBox( "Italic", page );
    mLbTextColor = new QPushButton( page );
    {
        QPixmap px(48,16); px.fill(mLbTextClr);
        mLbTextColor->setIcon(QIcon(px)); mLbTextColor->setText(mLbTextClr.name());
    }
    connect(mLbTextColor, &QPushButton::clicked, this,
            [this]{ pickColor(mLbTextColor, mLbTextClr); });
    form->addRow( "Vector map:",   mLbMap    );
    form->addRow( "Label column:", mLbCol    );
    form->addRow( "Font size:",    mLbFont   );
    form->addRow( "Halo width:",   mLbHalo   );
    form->addRow( "Text colour:",  mLbTextColor );
    auto *chkRow = new QHBoxLayout();
    chkRow->addWidget(mLbBold); chkRow->addWidget(mLbItalic); chkRow->addStretch();
    form->addRow( chkRow );
}

static QDoubleSpinBox *makeCoordSpin( QWidget *parent, double lo, double hi, double val ) {
    auto *s = new QDoubleSpinBox(parent);
    s->setRange(lo, hi); s->setDecimals(6); s->setValue(val); s->setSingleStep(1.0);
    return s;
}

void GrassAddLayerDialog::buildGeodesicPage( QWidget *page )
{
    auto *form = new QFormLayout( page );
    mGeoLon1 = makeCoordSpin(page, -180, 180, 0);  mGeoLat1 = makeCoordSpin(page, -90, 90, 0);
    mGeoLon2 = makeCoordSpin(page, -180, 180, 10); mGeoLat2 = makeCoordSpin(page, -90, 90, 10);
    mGeoColor = new QPushButton(page);
    { QPixmap px(48,16); px.fill(mGeoClr); mGeoColor->setIcon(QIcon(px)); mGeoColor->setText(mGeoClr.name()); }
    connect(mGeoColor, &QPushButton::clicked, this, [this]{ pickColor(mGeoColor, mGeoClr); });
    auto *p1 = new QHBoxLayout(); p1->addWidget(new QLabel("Lon:")); p1->addWidget(mGeoLon1);
    p1->addWidget(new QLabel("Lat:")); p1->addWidget(mGeoLat1);
    auto *p2 = new QHBoxLayout(); p2->addWidget(new QLabel("Lon:")); p2->addWidget(mGeoLon2);
    p2->addWidget(new QLabel("Lat:")); p2->addWidget(mGeoLat2);
    form->addRow( "Start point:", p1 );
    form->addRow( "End point:",   p2 );
    form->addRow( "Line colour:", mGeoColor );
    auto *note = new QLabel("Great circle = shortest path on the sphere (minimum distance).", page);
    note->setWordWrap(true); note->setStyleSheet("color:#888;font-size:9px;");
    form->addRow(note);
}

void GrassAddLayerDialog::buildRhumbPage( QWidget *page )
{
    auto *form = new QFormLayout( page );
    mRhmLon1 = makeCoordSpin(page,-180,180,0);  mRhmLat1 = makeCoordSpin(page,-90,90,0);
    mRhmLon2 = makeCoordSpin(page,-180,180,10); mRhmLat2 = makeCoordSpin(page,-90,90,10);
    mRhmColor = new QPushButton(page);
    { QPixmap px(48,16); px.fill(mRhmClr); mRhmColor->setIcon(QIcon(px)); mRhmColor->setText(mRhmClr.name()); }
    connect(mRhmColor, &QPushButton::clicked, this, [this]{ pickColor(mRhmColor, mRhmClr); });
    auto *p1 = new QHBoxLayout(); p1->addWidget(new QLabel("Lon:")); p1->addWidget(mRhmLon1);
    p1->addWidget(new QLabel("Lat:")); p1->addWidget(mRhmLat1);
    auto *p2 = new QHBoxLayout(); p2->addWidget(new QLabel("Lon:")); p2->addWidget(mRhmLon2);
    p2->addWidget(new QLabel("Lat:")); p2->addWidget(mRhmLat2);
    form->addRow( "Start point:", p1 );
    form->addRow( "End point:",   p2 );
    form->addRow( "Line colour:", mRhmColor );
    auto *note = new QLabel("Rhumb line = constant compass bearing (straight on Mercator).", page);
    note->setWordWrap(true); note->setStyleSheet("color:#888;font-size:9px;");
    form->addRow(note);
}

QString GrassAddLayerDialog::layerDisplayName() const
{
    return {};  // derived from the map names in the layer constructor
}

// ── populateVectorCombo (vectors) ─────────────────────────────────────────────

void GrassAddLayerDialog::populateVectorCombo( QComboBox *cb ) const
{
    const QString gb = QString::fromLocal8Bit( qgetenv("GISBASE") );
    if (gb.isEmpty()) return;
    QProcess p;
    p.setProcessEnvironment(GrassEnv::instance()->buildSubprocessEnv());
    p.start(gb+"/bin/g.list", {"-m","type=vector","mapset=*","separator=newline"});
    p.waitForFinished(8000);
    const QStringList maps = QString::fromLocal8Bit(p.readAllStandardOutput())
                             .split('\n', Qt::SkipEmptyParts);
    cb->addItem("(none)", "");
    for (const QString &m : maps) cb->addItem(m.trimmed(), m.trimmed());
}
