#include "grassdecorationpanel.h"
#include "grassmapcanvas.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cmath>

GrassDecorationPanel::GrassDecorationPanel( GrassMapCanvas *canvas,
                                              QWidget *parent )
    : QWidget( parent )
    , mCanvas( canvas )
{
    // Default decoration commands
    mLegendCmd   = GrassDisplayCmd::legend( "" );   // raster set via setLegendRaster()
    mBarscaleCmd = GrassDisplayCmd::barscale();
    mArrowCmd    = GrassDisplayCmd::northarrow();
    mGridCmd     = GrassDisplayCmd::grid();
    mTextCmd     = GrassDisplayCmd::text();

    // Outer layout for this widget: header + scroll area
    QVBoxLayout *outerLay = new QVBoxLayout( this );
    outerLay->setContentsMargins( 0, 0, 0, 0 );
    outerLay->setSpacing( 0 );

    QLabel *hdr = new QLabel( "<b>Decorations</b>", this );
    hdr->setStyleSheet( "color:#888; font-size:10px; padding: 2px 4px;" );
    outerLay->addWidget( hdr );

    // Scroll area holds all decoration rows so they are always reachable
    // even when the layer panel is short.
    auto *scroll = new QScrollArea( this );
    scroll->setFrameShape( QFrame::NoFrame );
    scroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    scroll->setVerticalScrollBarPolicy( Qt::ScrollBarAsNeeded );
    scroll->setWidgetResizable( true );

    // Inner container widget parented to the scroll area
    auto *inner  = new QWidget( scroll );
    auto *lay    = new QVBoxLayout( inner );
    lay->setContentsMargins( 4, 2, 4, 4 );
    lay->setSpacing( 2 );

    auto makeRow = [&]( const QString &label,
                        QCheckBox *&chk,
                        const char *slot ) -> QHBoxLayout *
    {
        QHBoxLayout *row = new QHBoxLayout();
        row->setContentsMargins( 0, 0, 0, 0 );
        chk = new QCheckBox( label, inner );
        chk->setChecked( false );
        QPushButton *btn = new QPushButton( "…", inner );
        btn->setFixedWidth( 22 );
        btn->setFixedHeight( 20 );
        row->addWidget( chk, 1 );
        row->addWidget( btn );
        connect( chk, &QCheckBox::toggled,
                 this, &GrassDecorationPanel::decorationsChanged );
        connect( btn, SIGNAL(clicked()), this, slot );
        return row;
    };

    lay->addLayout( makeRow( "Legend",       mLegendCheck,   SLOT(editLegend())    ) );
    lay->addLayout( makeRow( "Scale bar",    mBarscaleCheck, SLOT(editScalebar())  ) );
    lay->addLayout( makeRow( "North arrow",  mArrowCheck,    SLOT(editNorthArrow()) ) );
    lay->addLayout( makeRow( "Grid",         mGridCheck,     SLOT(editGrid())      ) );
    lay->addLayout( makeRow( "Text",         mTextCheck,     SLOT(editText())      ) );
    lay->addStretch( 0 );

    scroll->setWidget( inner );

    // Fix the scroll area height to fit all rows exactly (no wasted space,
    // no clipping).  Each row is ~22 px; add header + margins.
    const int rowCount  = 5;
    const int rowHeight = 22;
    const int preferred = rowCount * rowHeight + 8;  // 8 = top+bottom margins
    scroll->setFixedHeight( preferred );

    outerLay->addWidget( scroll );
}

// ---------------------------------------------------------------------------
// setLegendRaster
// ---------------------------------------------------------------------------
void GrassDecorationPanel::setLegendRaster( const QString &mapRef )
{
    // Replace the "raster=" arg
    QStringList args;
    args << "raster=" + mapRef;
    // Keep any existing positional args except raster=
    for ( const QString &a : mLegendCmd.args )
        if ( !a.startsWith( "raster=" ) ) args << a;
    mLegendCmd.args = args;
}

// ---------------------------------------------------------------------------
// activeDecorations
// ---------------------------------------------------------------------------
QList<GrassDisplayCmd> GrassDecorationPanel::activeDecorations() const
{
    QList<GrassDisplayCmd> result;
    // Render order: grid (bottom) → legend → scalebar → arrow → text (top).
    if ( mGridCheck    ->isChecked() ) result << mGridCmd;
    if ( mLegendCheck  ->isChecked() && !mLegendCmd.args.filter("raster=").isEmpty()
                                     && mLegendCmd.args.first() != "raster=" )
        result << mLegendCmd;
    if ( mBarscaleCheck->isChecked() ) result << mBarscaleCmd;
    if ( mArrowCheck   ->isChecked() ) result << mArrowCmd;
    // Text always last so it draws on top of every other decoration.
    if ( mTextCheck    ->isChecked() ) result << mTextCmd;
    return result;
}

// ---------------------------------------------------------------------------
// Helper: extract value after a prefix in an arg list
static QString argVal( const QStringList &args,
                        const QString &prefix,
                        const QString &def = {} )
{
    for ( const QString &a : args )
        if ( a.startsWith( prefix ) ) return a.mid( prefix.length() );
    return def;
}

// Helper: create a color-picker button, pre-loaded from args or default
static QPushButton *makeColorBtn( QWidget *parent, QFormLayout *form,
                                   const QString &label,
                                   QColor &colorRef )
{
    auto *btn = new QPushButton( parent );
    auto refresh = [btn, &colorRef]() {
        QPixmap px(48,16); px.fill(colorRef);
        btn->setIcon(QIcon(px)); btn->setText(colorRef.name());
    };
    refresh();
    QObject::connect( btn, &QPushButton::clicked, parent, [btn, &colorRef, refresh]() mutable {
        QColor c = QColorDialog::getColor( colorRef, btn );
        if ( c.isValid() ) { colorRef = c; refresh(); }
    });
    form->addRow( new QLabel(label, parent), btn );
    return btn;
}

// ---------------------------------------------------------------------------
QList<GrassDecorationPanel::DecoState> GrassDecorationPanel::savedState() const
{
    return {
        { "d.legend",     mLegendCmd.args,   mLegendCheck   ->isChecked() },
        { "d.barscale",   mBarscaleCmd.args,  mBarscaleCheck ->isChecked() },
        { "d.northarrow", mArrowCmd.args,     mArrowCheck    ->isChecked() },
        { "d.grid",       mGridCmd.args,      mGridCheck     ->isChecked() },
        { "d.text",       mTextCmd.args,      mTextCheck     ->isChecked() },
    };
}

void GrassDecorationPanel::restoreState( const QList<DecoState> &states )
{
    for ( const DecoState &s : states ) {
        QCheckBox     *chk = nullptr;
        GrassDisplayCmd *cmd = nullptr;
        if      ( s.prog == "d.legend"     ) { chk = mLegendCheck;   cmd = &mLegendCmd;   }
        else if ( s.prog == "d.barscale"   ) { chk = mBarscaleCheck; cmd = &mBarscaleCmd; }
        else if ( s.prog == "d.northarrow" ) { chk = mArrowCheck;    cmd = &mArrowCmd;    }
        else if ( s.prog == "d.grid"       ) { chk = mGridCheck;     cmd = &mGridCmd;     }
        else if ( s.prog == "d.text"       ) { chk = mTextCheck;     cmd = &mTextCmd;     }
        if ( !chk || !cmd ) continue;

        cmd->args = s.args;
        chk->blockSignals( true );
        chk->setChecked( s.enabled );
        chk->blockSignals( false );
    }
    emit decorationsChanged();
}

// ---------------------------------------------------------------------------
// Settings dialogs — pre-populated from current cmd args (persistent memory)
// ---------------------------------------------------------------------------
void GrassDecorationPanel::editLegend()
{
    QDialog dlg( this );
    dlg.setWindowTitle( "Legend settings" );
    QFormLayout *form = new QFormLayout( &dlg );

    QLineEdit *atEdit = new QLineEdit(
        argVal( mLegendCmd.args, "at=", "5,50,2,9" ), &dlg );
    atEdit->setToolTip( "at=bottom%,top%,left%,right% (0–100)" );
    form->addRow( "Position (at):", atEdit );

    QLineEdit *titleEdit = new QLineEdit(
        argVal( mLegendCmd.args, "title=", "" ), &dlg );
    titleEdit->setPlaceholderText( "(optional)" );
    form->addRow( "Title:", titleEdit );

    // ── Font ────────────────────────────────────────────────────────────
    auto *fontBox = new QComboBox( &dlg );
    fontBox->setEditable( true );
    fontBox->addItems( { "sans-serif", "serif", "monospace",
                         "DejaVu Sans", "DejaVu Serif", "DejaVu Sans Mono",
                         "Arial", "Helvetica", "Times New Roman",
                         "Courier New", "Ubuntu", "Liberation Sans",
                         "romans", "italic", "greeks", "cyrillic" } );
    fontBox->setCurrentText( argVal( mLegendCmd.args, "fontname=", "sans-serif" ) );
    fontBox->setToolTip( "Font family name — common system fonts and GRASS font names.\n"
                         "You can also type any installed font name." );
    form->addRow( "Font family:", fontBox );

    auto *sizeBox = new QSpinBox( &dlg );
    sizeBox->setRange( 5, 48 );
    sizeBox->setValue( argVal( mLegendCmd.args, "fontsize=", "8" ).toInt() );
    sizeBox->setSuffix( " pt" );
    form->addRow( "Font size:", sizeBox );

    QCheckBox *boldChk = new QCheckBox( "Bold", &dlg );
    boldChk->setChecked( mLegendCmd.args.contains("bold") );
    QCheckBox *italicChk = new QCheckBox( "Italic", &dlg );
    italicChk->setChecked( mLegendCmd.args.contains("italic") );
    auto *fontStyleRow = new QHBoxLayout();
    fontStyleRow->addWidget( boldChk );
    fontStyleRow->addWidget( italicChk );
    form->addRow( "Font style:", fontStyleRow );

    // ── Font colour ──────────────────────────────────────────────────────
    QColor fontColor( argVal( mLegendCmd.args, "fontcolor=", "#ffffff" ) );
    if ( !fontColor.isValid() ) fontColor = Qt::white;
    auto *colorBtn = new QPushButton( &dlg );
    auto refreshColorBtn = [&]() {
        QPixmap px(48,16); px.fill(fontColor);
        colorBtn->setIcon(QIcon(px)); colorBtn->setText(fontColor.name());
    };
    refreshColorBtn();
    connect( colorBtn, &QPushButton::clicked, &dlg, [&]() {
        QColor c = QColorDialog::getColor( fontColor, &dlg );
        if ( c.isValid() ) { fontColor = c; refreshColorBtn(); }
    });
    form->addRow( "Font color:", colorBtn );

    QDialogButtonBox *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg );
    connect( btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
    connect( btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );
    form->addRow( btns );

    if ( dlg.exec() != QDialog::Accepted ) return;

    QStringList args;
    for ( const QString &a : mLegendCmd.args )
        if ( a.startsWith("raster=") ) { args << a; break; }
    args << "at=" + atEdit->text().trimmed();
    if ( !titleEdit->text().isEmpty() )
        args << "title=" + titleEdit->text().trimmed();
    args << "fontname=" + fontBox->currentText().trimmed();
    args << "fontsize=" + QString::number( sizeBox->value() );
    args << "fontcolor=" + fontColor.name();
    if ( boldChk->isChecked() )   args << "bold";
    if ( italicChk->isChecked() ) args << "italic";
    mLegendCmd.args = args;
    emit decorationsChanged();
}

void GrassDecorationPanel::editScalebar()
{
    QDialog dlg( this );
    dlg.setWindowTitle( "Scale bar settings" );
    QFormLayout *form = new QFormLayout( &dlg );

    QComboBox *style = new QComboBox( &dlg );
    style->addItems( { "simple","full_checker","part_checker","mixed_checker",
                       "tail_checker","up_ticks","down_ticks","no_ticks",
                       "major_ticks","major_minor_ticks" } );
    style->setCurrentText( argVal( mBarscaleCmd.args, "style=", "simple" ) );
    form->addRow( "Style:", style );

    QLineEdit *at = new QLineEdit(
        argVal( mBarscaleCmd.args, "at=", "80,2" ), &dlg );
    at->setToolTip( "Position: bottom%, left% (0–100)" );
    form->addRow( "Position (at):", at );

    QComboBox *units = new QComboBox( &dlg );
    units->addItems( { "auto","meters","kilometers","feet","miles","nautmiles" } );
    units->setCurrentText( argVal( mBarscaleCmd.args, "units=", "auto" ) );
    form->addRow( "Units:", units );

    // ── Colours ─────────────────────────────────────────────────────────────
    QColor fgColor( argVal( mBarscaleCmd.args, "fgcolor=", "#ffffff" ) );
    if ( !fgColor.isValid() ) fgColor = Qt::white;
    QColor bgColor( argVal( mBarscaleCmd.args, "bgcolor=", "#000000" ) );
    if ( !bgColor.isValid() ) bgColor = Qt::black;
    makeColorBtn( &dlg, form, "Foreground color:", fgColor );
    makeColorBtn( &dlg, form, "Background color:", bgColor );

    QDialogButtonBox *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg );
    connect( btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
    connect( btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );
    form->addRow( btns );

    if ( dlg.exec() != QDialog::Accepted ) return;
    mBarscaleCmd.args = { "style="   + style->currentText(),
                          "at="      + at->text().trimmed(),
                          "units="   + units->currentText(),
                          "fgcolor=" + fgColor.name(),
                          "bgcolor=" + bgColor.name() };
    emit decorationsChanged();
}

void GrassDecorationPanel::editNorthArrow()
{
    QDialog dlg( this );
    dlg.setWindowTitle( "North arrow settings" );
    QFormLayout *form = new QFormLayout( &dlg );

    QComboBox *style = new QComboBox( &dlg );
    style->addItems( { "default (arrow + N)",
                       "pointed (solid arrowhead)",
                       "compass (N/S/E/W)",
                       "line (arrow line only)" } );
    style->setCurrentText( argVal( mArrowCmd.args, "style=", "default (arrow + N)" ) );
    form->addRow( "Style:", style );

    QLineEdit *at = new QLineEdit(
        argVal( mArrowCmd.args, "at=", "90,5" ), &dlg );
    at->setToolTip( "Position: bottom%, left% (0–100)" );
    form->addRow( "Position (at):", at );

    QDoubleSpinBox *size = new QDoubleSpinBox( &dlg );
    size->setRange( 1, 30 );
    size->setValue( argVal( mArrowCmd.args, "size=", "8" ).toDouble() );
    size->setSuffix( "%" );
    form->addRow( "Size (% of display):", size );

    // ── Colours ─────────────────────────────────────────────────────────────
    QColor fgColor( argVal( mArrowCmd.args, "fgcolor=", "#ffffff" ) );
    if ( !fgColor.isValid() ) fgColor = Qt::white;
    QColor bgColor( argVal( mArrowCmd.args, "bgcolor=", "#000000" ) );
    if ( !bgColor.isValid() ) bgColor = Qt::black;
    makeColorBtn( &dlg, form, "Foreground color:", fgColor );
    makeColorBtn( &dlg, form, "Background color:", bgColor );

    QDialogButtonBox *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg );
    connect( btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
    connect( btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );
    form->addRow( btns );

    if ( dlg.exec() != QDialog::Accepted ) return;
    mArrowCmd.args = { "style="   + style->currentText(),
                       "at="      + at->text().trimmed(),
                       "size="    + QString::number( size->value() ),
                       "fgcolor=" + fgColor.name(),
                       "bgcolor=" + bgColor.name() };
    emit decorationsChanged();
}

// ---------------------------------------------------------------------------
// niceGridSpacing — largest "nice" value <= range/maxTicks rounded up so that
// there are at most maxTicks grid lines across the given range.
// "Nice" means 1, 2 or 5 times a power of ten.
static double niceGridSpacing( double range, int maxTicks = 10 )
{
    if ( range <= 0.0 || maxTicks <= 0 ) return 1.0;
    const double raw  = range / maxTicks;
    const double exp  = std::floor( std::log10( raw ) );
    const double base = std::pow( 10.0, exp );
    for ( double mult : { 1.0, 2.0, 5.0, 10.0 } )
        if ( base * mult >= raw ) return base * mult;
    return base * 10.0;
}

void GrassDecorationPanel::editGrid()
{
    QDialog dlg( this );
    dlg.setWindowTitle( "Grid settings" );
    QFormLayout *form = new QFormLayout( &dlg );

    const QString storedSize = argVal( mGridCmd.args, "size=", "auto" );
    const bool    wasAuto    = ( storedSize == "auto" );

    // ── Auto / manual spacing toggle ─────────────────────────────────────
    auto *autoChk = new QCheckBox( "Auto (≤10 lines per axis, updates on zoom)", &dlg );
    autoChk->setChecked( wasAuto );
    form->addRow( "Spacing:", autoChk );

    auto *spacing = new QDoubleSpinBox( &dlg );
    spacing->setRange( 1e-6, 1e12 );
    spacing->setDecimals( 3 );
    spacing->setToolTip( "Fixed grid spacing in map units" );
    spacing->setEnabled( !wasAuto );

    // Pre-fill spinbox: use stored value if manual, else compute from viewport
    if ( wasAuto ) {
        double hint = 1000.0;
        if ( mCanvas ) {
            const GrassExtent ext = mCanvas->viewportExtent();
            if ( ext.isValid() )
                hint = niceGridSpacing( std::max( ext.width(), ext.height() ) );
        }
        spacing->setValue( hint );
    } else {
        spacing->setValue( storedSize.toDouble() );
    }
    form->addRow( "Fixed spacing:", spacing );

    connect( autoChk, &QCheckBox::toggled, spacing,
             [spacing]( bool on ) { spacing->setEnabled( !on ); } );

    QLineEdit *color = new QLineEdit(
        argVal( mGridCmd.args, "color=", "grey" ), &dlg );
    color->setToolTip( "GRASS colour name or R:G:B" );
    form->addRow( "Color:", color );

    QCheckBox *labelsChk = new QCheckBox( "Show coordinate labels", &dlg );
    labelsChk->setChecked( !mGridCmd.args.contains("-n") );
    form->addRow( labelsChk );

    QCheckBox *borderChk = new QCheckBox( "Draw border", &dlg );
    borderChk->setChecked( !mGridCmd.args.contains("-b") );
    form->addRow( borderChk );

    QDialogButtonBox *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg );
    connect( btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
    connect( btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );
    form->addRow( btns );

    if ( dlg.exec() != QDialog::Accepted ) return;
    mGridCmd.args.clear();
    const QString sizeVal = autoChk->isChecked()
                            ? "auto"
                            : QString::number( spacing->value(), 'g', 10 );
    mGridCmd.args << "size="  + sizeVal
                  << "color=" + color->text().trimmed();
    if ( !labelsChk->isChecked() ) mGridCmd.args << "-n";
    if ( !borderChk->isChecked()  ) mGridCmd.args << "-b";
    emit decorationsChanged();
}

// ---------------------------------------------------------------------------
void GrassDecorationPanel::editText()
{
    QDialog dlg( this );
    dlg.setWindowTitle( "Text decoration settings" );
    QFormLayout *form = new QFormLayout( &dlg );

    // ── Text content ──────────────────────────────────────────────────────
    auto *textEdit = new QLineEdit( argVal( mTextCmd.args, "text=", "Map Title" ), &dlg );
    textEdit->setMinimumWidth( 240 );
    form->addRow( "Text:", textEdit );

    // ── Position (x%, y% from lower-left) ───────────────────────────────
    const QString atVal = argVal( mTextCmd.args, "at=", "50,97" );
    const QStringList atParts = atVal.split( ',' );
    auto *xSpin = new QDoubleSpinBox( &dlg );
    xSpin->setRange( 0, 100 ); xSpin->setDecimals( 1 ); xSpin->setSuffix( " %" );
    xSpin->setValue( atParts.value(0).toDouble() );
    xSpin->setToolTip( "X position — 0 = left edge, 100 = right edge" );
    auto *ySpin = new QDoubleSpinBox( &dlg );
    ySpin->setRange( 0, 100 ); ySpin->setDecimals( 1 ); ySpin->setSuffix( " %" );
    ySpin->setValue( atParts.value(1, "97").toDouble() );
    ySpin->setToolTip( "Y position — 0 = bottom, 100 = top" );
    auto *posRow = new QHBoxLayout;
    posRow->addWidget( new QLabel( "X:", &dlg ) );
    posRow->addWidget( xSpin );
    posRow->addWidget( new QLabel( "Y:", &dlg ) );
    posRow->addWidget( ySpin );
    form->addRow( "Position:", posRow );

    // ── Alignment ────────────────────────────────────────────────────────
    auto *alignBox = new QComboBox( &dlg );
    alignBox->addItems( { "ll", "lc", "lr", "cl", "cc", "cr", "ul", "uc", "ur" } );
    alignBox->setCurrentText( argVal( mTextCmd.args, "align=", "uc" ) );
    alignBox->setToolTip( "First letter = vertical anchor (l=lower, c=center, u=upper)\n"
                          "Second letter = horizontal anchor (l=left, c=center, r=right)" );
    form->addRow( "Alignment:", alignBox );

    // ── Font ─────────────────────────────────────────────────────────────
    auto *fontBox = new QFontComboBox( &dlg );
    fontBox->setCurrentFont( QFont( argVal( mTextCmd.args, "font=", "sans-serif" ) ) );
    form->addRow( "Font family:", fontBox );

    auto *sizeSpin = new QDoubleSpinBox( &dlg );
    sizeSpin->setRange( 0.5, 50.0 ); sizeSpin->setDecimals( 1 );
    sizeSpin->setSuffix( " % frame ht." );
    sizeSpin->setValue( argVal( mTextCmd.args, "size=", "5" ).toDouble() );
    sizeSpin->setToolTip( "Text height as percentage of display frame height" );
    form->addRow( "Size:", sizeSpin );

    auto *boldChk = new QCheckBox( "Bold", &dlg );
    boldChk->setChecked( mTextCmd.args.contains("-b") );
    form->addRow( "", boldChk );

    auto *rotSpin = new QDoubleSpinBox( &dlg );
    rotSpin->setRange( -360, 360 ); rotSpin->setDecimals( 1 );
    rotSpin->setSuffix( "°" );
    rotSpin->setValue( argVal( mTextCmd.args, "rotation=", "0" ).toDouble() );
    form->addRow( "Rotation (CCW):", rotSpin );

    // ── Colors ───────────────────────────────────────────────────────────
    QColor fgColor( argVal( mTextCmd.args, "color=", "#000000" ) );
    if ( !fgColor.isValid() ) fgColor = Qt::black;
    makeColorBtn( &dlg, form, "Text color:", fgColor );

    // Background — stored as "none" or a color; map to a QColor for the picker
    const QString bgArg = argVal( mTextCmd.args, "bgcolor=", "none" );
    bool hasBg = ( bgArg != "none" );
    QColor bgColor( hasBg ? bgArg : "#ffffff" );
    if ( !bgColor.isValid() ) bgColor = Qt::white;
    auto *bgChk = new QCheckBox( "Show background", &dlg );
    bgChk->setChecked( hasBg );
    auto *bgBtn = new QPushButton( &dlg );
    {   // initialise bg button appearance
        QPixmap px(48,16); px.fill(bgColor);
        bgBtn->setIcon(QIcon(px)); bgBtn->setText(bgColor.name());
    }
    bgBtn->setEnabled( hasBg );
    connect( bgChk, &QCheckBox::toggled, bgBtn, &QPushButton::setEnabled );
    connect( bgBtn, &QPushButton::clicked, &dlg, [&bgBtn, &bgColor]() mutable {
        QColor c = QColorDialog::getColor( bgColor, bgBtn );
        if ( c.isValid() ) {
            bgColor = c;
            QPixmap px(48,16); px.fill(c);
            bgBtn->setIcon(QIcon(px)); bgBtn->setText(c.name());
        }
    });
    auto *bgRow = new QHBoxLayout;
    bgRow->addWidget( bgChk );
    bgRow->addWidget( bgBtn );
    form->addRow( "Background:", bgRow );

    QDialogButtonBox *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg );
    connect( btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
    connect( btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );
    form->addRow( btns );

    if ( dlg.exec() != QDialog::Accepted ) return;

    const QString atStr = QString::number( xSpin->value(), 'f', 1 ) + ","
                        + QString::number( ySpin->value(), 'f', 1 );
    const QString bgStr = ( bgChk->isChecked() ? bgColor.name() : "none" );
    const QString font  = fontBox->currentFont().family();

    mTextCmd = GrassDisplayCmd::text(
        textEdit->text(),
        atStr,
        alignBox->currentText(),
        sizeSpin->value(),
        fgColor.name(),
        bgStr,
        font,
        boldChk->isChecked(),
        rotSpin->value() );
    emit decorationsChanged();
}
