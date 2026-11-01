#include "grassmapexport.h"
#include "../canvas/grassmapcanvas.h"
#include "../canvas/grassmaplayer.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QPainter>
#include <QPrinter>
#include <QPushButton>
#include <QSpinBox>
#include <QSvgGenerator>
#include <QVBoxLayout>
#include <QDebug>

GrassMapExportDialog::GrassMapExportDialog( GrassMapCanvas *canvas,
                                             QWidget *parent )
    : QDialog( parent )
    , mCanvas( canvas )
{
    setWindowTitle( "Export Map" );
    setMinimumWidth( 480 );

    QVBoxLayout *root = new QVBoxLayout( this );

    // --- File path ---
    QGroupBox *fileBox = new QGroupBox( "Output file", this );
    QHBoxLayout *fRow = new QHBoxLayout( fileBox );
    mPathEdit = new QLineEdit( fileBox );
    mPathEdit->setPlaceholderText( "Choose output file…" );
    QPushButton *browseBtn = new QPushButton( "…", fileBox );
    browseBtn->setFixedWidth( 28 );
    fRow->addWidget( mPathEdit, 1 );
    fRow->addWidget( browseBtn );
    root->addWidget( fileBox );

    // --- Format and size ---
    QGroupBox *optBox = new QGroupBox( "Options", this );
    QFormLayout *form = new QFormLayout( optBox );

    mFormatCombo = new QComboBox( optBox );
    mFormatCombo->addItems( { "PNG (*.png)", "JPEG (*.jpg)",
                               "TIFF (*.tif)", "PDF (*.pdf)", "SVG (*.svg)" } );
    form->addRow( "Format:", mFormatCombo );

    mWidthSpin  = new QSpinBox( optBox );
    mHeightSpin = new QSpinBox( optBox );
    mWidthSpin->setRange( 32, 16384 );
    mHeightSpin->setRange( 32, 16384 );
    mWidthSpin->setSuffix( " px" );
    mHeightSpin->setSuffix( " px" );

    // Initialise from canvas size
    const int cw = canvas->width()  > 0 ? canvas->width()  : 800;
    const int ch = canvas->height() > 0 ? canvas->height() : 600;
    mWidthSpin->setValue( cw );
    mHeightSpin->setValue( ch );

    QHBoxLayout *sizeRow = new QHBoxLayout();
    sizeRow->addWidget( mWidthSpin );
    sizeRow->addWidget( new QLabel( "×", optBox ) );
    sizeRow->addWidget( mHeightSpin );
    sizeRow->addStretch();

    // DPI presets
    QComboBox *dpiPreset = new QComboBox( optBox );
    dpiPreset->addItems( { "Screen (current size)",
                           "150 DPI (×1.5)",
                           "300 DPI (×3)",
                           "600 DPI (×6)" } );
    connect( dpiPreset, &QComboBox::currentIndexChanged,
             this, [this, cw, ch, dpiPreset]( int idx ) {
                 double scale = 1.0;
                 if ( idx == 1 ) scale = 1.5;
                 else if ( idx == 2 ) scale = 3.0;
                 else if ( idx == 3 ) scale = 6.0;
                 mWidthSpin->setValue(  qRound( cw * scale ) );
                 mHeightSpin->setValue( qRound( ch * scale ) );
             } );

    form->addRow( "Resolution preset:", dpiPreset );
    form->addRow( "Output size:", sizeRow );
    root->addWidget( optBox );

    // --- Preview thumbnail ---
    QGroupBox *prevBox = new QGroupBox( "Preview (current canvas)", this );
    QVBoxLayout *pvLay = new QVBoxLayout( prevBox );
    mPreview = new QLabel( prevBox );
    mPreview->setAlignment( Qt::AlignCenter );
    mPreview->setFixedHeight( 120 );
    mPreview->setStyleSheet( "background:#1a1a1a;" );
    pvLay->addWidget( mPreview );
    root->addWidget( prevBox );

    // --- Status + buttons ---
    mStatus = new QLabel( this );
    mStatus->setStyleSheet( "color:#c00;" );
    root->addWidget( mStatus );

    QDialogButtonBox *btns = new QDialogButtonBox( this );
    QPushButton *exportBtn = btns->addButton( "Export", QDialogButtonBox::AcceptRole );
    btns->addButton( QDialogButtonBox::Cancel );
    connect( exportBtn, &QPushButton::clicked, this, &GrassMapExportDialog::doExport );
    connect( btns, &QDialogButtonBox::rejected, this, &QDialog::reject );
    root->addWidget( btns );

    // Connections
    connect( browseBtn,   &QPushButton::clicked,
             this, &GrassMapExportDialog::chooseFile );
    connect( mFormatCombo, &QComboBox::currentIndexChanged,
             this, &GrassMapExportDialog::updatePreview );

    updatePreview();
}

void GrassMapExportDialog::updatePreview()
{
    if ( !mCanvas ) return;
    // Grab current canvas pixels for the thumbnail
    QPixmap px = mCanvas->grab();
    mPreview->setPixmap( px.scaled( mPreview->width(), mPreview->height(),
                                    Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
}

void GrassMapExportDialog::chooseFile()
{
    // Pick filter based on current format
    static const QStringList filters = {
        "PNG images (*.png)", "JPEG images (*.jpg *.jpeg)",
        "TIFF images (*.tif *.tiff)", "PDF documents (*.pdf)",
        "SVG vector (*.svg)"
    };
    const int idx = mFormatCombo->currentIndex();
    const QString filter = ( idx >= 0 && idx < filters.size() )
                           ? filters[idx] : filters[0];

    const QString path = QFileDialog::getSaveFileName(
        this, "Export Map", {}, filter );
    if ( !path.isEmpty() )
        mPathEdit->setText( path );
}

void GrassMapExportDialog::doExport()
{
    const QString path = mPathEdit->text().trimmed();
    if ( path.isEmpty() )
    {
        mStatus->setText( "Please choose an output file." );
        return;
    }

    const int W = mWidthSpin->value();
    const int H = mHeightSpin->value();

    const int fmt = mFormatCombo->currentIndex();
    bool ok = false;

    if ( fmt == 3 )      { exportPdf( path, W, H ); ok = true; }
    else if ( fmt == 4 ) { exportSvg( path, W, H ); ok = true; }
    else                  { exportPng( path, W, H ); ok = true; }

    if ( ok )
    {
        mStatus->setStyleSheet( "color:#1a8a1a; font-weight:bold;" );
        mStatus->setText( QString( "Saved: %1  (%2 × %3 px)" )
                          .arg( path ).arg( W ).arg( H ) );
    }
}

// ---------------------------------------------------------------------------
// PNG / JPEG / TIFF — render into QImage at target size
// ---------------------------------------------------------------------------
void GrassMapExportDialog::exportPng( const QString &path, int w, int h )
{
    const GrassExtent vp = mCanvas->viewportExtent();

    // Render all visible layers at target resolution
    QImage img( w, h, QImage::Format_RGB32 );
    img.fill( QColor( 26, 26, 26 ) );
    QPainter p( &img );

    for ( GrassMapLayer *layer : mCanvas->layers() )
    {
        if ( !layer->visible() ) continue;
        const QImage li = layer->render( w, h, vp );
        if ( !li.isNull() ) p.drawImage( 0, 0, li );
    }
    p.end();

    if ( !img.save( path ) )
        mStatus->setText( "Failed to save image." );
}

// ---------------------------------------------------------------------------
// PDF via QPrinter
// ---------------------------------------------------------------------------
void GrassMapExportDialog::exportPdf( const QString &path, int w, int h )
{
    QPrinter printer( QPrinter::HighResolution );
    printer.setOutputFormat( QPrinter::PdfFormat );
    printer.setOutputFileName( path );
    printer.setPageSize( QPageSize( QSizeF( w / 96.0, h / 96.0 ),
                                    QPageSize::Inch ) );
    printer.setFullPage( true );

    const QImage img     = mCanvas->grab().toImage()
                           .scaled( w, h, Qt::IgnoreAspectRatio,
                                    Qt::SmoothTransformation );

    QPainter p( &printer );
    p.drawImage( 0, 0, img );
    p.end();
}

// ---------------------------------------------------------------------------
// SVG via QSvgGenerator
// ---------------------------------------------------------------------------
void GrassMapExportDialog::exportSvg( const QString &path, int w, int h )
{
    QSvgGenerator gen;
    gen.setFileName( path );
    gen.setSize( { w, h } );
    gen.setViewBox( QRect( 0, 0, w, h ) );
    gen.setTitle( "GRASS GIS — qtgrass map export" );

    const GrassExtent vp = mCanvas->viewportExtent();

    QPainter p( &gen );
    p.fillRect( 0, 0, w, h, QColor( 26, 26, 26 ) );
    for ( GrassMapLayer *layer : mCanvas->layers() )
    {
        if ( !layer->visible() ) continue;
        const QImage li = layer->render( w, h, vp );
        if ( !li.isNull() ) p.drawImage( 0, 0, li );
    }
    p.end();
}
