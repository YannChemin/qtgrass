#include "grassregiondialog.h"
#include "../canvas/grassmapcanvas.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QDebug>

GrassRegionDialog::GrassRegionDialog( GrassMapCanvas *canvas,
                                       QWidget *parent )
    : QDialog( parent )
    , mCanvas( canvas )
{
    setWindowTitle( "Computational Region — g.region" );
    setMinimumWidth( 380 );

    QVBoxLayout *root = new QVBoxLayout( this );

    // ---- Extent fields ----
    QGroupBox *extBox = new QGroupBox( "Extent", this );
    QFormLayout *extForm = new QFormLayout( extBox );

    auto makeDbl = [&]( double lo, double hi ) {
        auto *s = new QDoubleSpinBox( extBox );
        s->setRange( lo, hi );
        s->setDecimals( 6 );
        s->setSingleStep( 1.0 );
        return s;
    };
    mNorth = makeDbl( -1e9, 1e9 );
    mSouth = makeDbl( -1e9, 1e9 );
    mEast  = makeDbl( -1e9, 1e9 );
    mWest  = makeDbl( -1e9, 1e9 );
    extForm->addRow( "North:",  mNorth );
    extForm->addRow( "South:",  mSouth );
    extForm->addRow( "East:",   mEast  );
    extForm->addRow( "West:",   mWest  );
    root->addWidget( extBox );

    // ---- Resolution fields ----
    QGroupBox *resBox = new QGroupBox( "Resolution", this );
    QFormLayout *resForm = new QFormLayout( resBox );

    mNSRes = new QDoubleSpinBox( resBox );
    mEWRes = new QDoubleSpinBox( resBox );
    mNSRes->setRange( 1e-10, 1e9 ); mNSRes->setDecimals( 8 );
    mEWRes->setRange( 1e-10, 1e9 ); mEWRes->setDecimals( 8 );

    mRows = new QSpinBox( resBox ); mRows->setRange( 1, 9999999 );
    mCols = new QSpinBox( resBox ); mCols->setRange( 1, 9999999 );

    resForm->addRow( "N-S resolution:", mNSRes );
    resForm->addRow( "E-W resolution:", mEWRes );
    resForm->addRow( "Rows:",           mRows );
    resForm->addRow( "Columns:",        mCols );
    root->addWidget( resBox );

    // ---- Align to map ----
    QGroupBox *alignBox = new QGroupBox( "Align to map", this );
    QVBoxLayout *alignLay = new QVBoxLayout( alignBox );

    QHBoxLayout *rRow = new QHBoxLayout();
    mRasterCombo = new QComboBox( alignBox );
    QPushButton *rBtn = new QPushButton( "Set from raster", alignBox );
    rRow->addWidget( mRasterCombo, 1 ); rRow->addWidget( rBtn );

    QHBoxLayout *vRow = new QHBoxLayout();
    mVectorCombo = new QComboBox( alignBox );
    QPushButton *vBtn = new QPushButton( "Set from vector", alignBox );
    vRow->addWidget( mVectorCombo, 1 ); vRow->addWidget( vBtn );

    alignLay->addLayout( rRow );
    alignLay->addLayout( vRow );
    root->addWidget( alignBox );

    mStatus = new QLabel( this );
    mStatus->setWordWrap( true );
    root->addWidget( mStatus );

    // ---- Buttons ----
    QDialogButtonBox *btns = new QDialogButtonBox( this );
    QPushButton *setBtn     = btns->addButton( "Set Region", QDialogButtonBox::ApplyRole );
    QPushButton *reloadBtn  = btns->addButton( "Reload",     QDialogButtonBox::ResetRole );
    btns->addButton( QDialogButtonBox::Close );
    root->addWidget( btns );

    // Connections — resolution ↔ rows/cols coupling
    connect( mNSRes, &QDoubleSpinBox::valueChanged, this, &GrassRegionDialog::nsresChanged );
    connect( mEWRes, &QDoubleSpinBox::valueChanged, this, &GrassRegionDialog::nsresChanged );
    connect( mRows,  &QSpinBox::valueChanged,       this, &GrassRegionDialog::rowsChanged  );
    connect( mCols,  &QSpinBox::valueChanged,       this, &GrassRegionDialog::rowsChanged  );
    connect( setBtn,    &QPushButton::clicked, this, &GrassRegionDialog::setRegion );
    connect( reloadBtn, &QPushButton::clicked, this, &GrassRegionDialog::loadRegion );
    connect( rBtn,      &QPushButton::clicked, this, &GrassRegionDialog::alignToRaster );
    connect( vBtn,      &QPushButton::clicked, this, &GrassRegionDialog::alignToVector );
    connect( btns, &QDialogButtonBox::rejected, this, &QDialog::accept );

    // Populate align combos
    for ( const QString &m : listMaps( "raster" ) ) mRasterCombo->addItem( m );
    for ( const QString &m : listMaps( "vector" ) ) mVectorCombo->addItem( m );

    loadRegion();
}

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
QStringList GrassRegionDialog::listMaps( const QString &type ) const
{
    const QString gb = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
    if ( gb.isEmpty() ) return {};
    QProcess p;
    p.setProcessEnvironment( QProcessEnvironment::systemEnvironment() );
    p.start( gb + "/bin/g.list", { "type=" + type, "separator=newline" } );
    p.waitForFinished( 5000 );
    return QString::fromLocal8Bit( p.readAllStandardOutput() )
           .split( '\n', Qt::SkipEmptyParts );
}

void GrassRegionDialog::runGRegion( const QStringList &args )
{
    const QString gb = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
    if ( gb.isEmpty() ) return;
    QProcess p;
    p.setProcessEnvironment( QProcessEnvironment::systemEnvironment() );
    p.start( gb + "/bin/g.region", args );
    p.waitForFinished( 10000 );
    const QString err = QString::fromLocal8Bit( p.readAllStandardError() );
    if ( p.exitCode() != 0 )
    {
        mStatus->setStyleSheet( "color:#c00;" );
        mStatus->setText( "g.region failed: " + err.simplified() );
    }
    else
    {
        mStatus->setStyleSheet( "color:#1a8a1a;" );
        mStatus->setText( "Region updated." );
        if ( mCanvas ) mCanvas->update(); // redraws region overlay
    }
}

// ---------------------------------------------------------------------------
// loadRegion — parse g.region -p output
// ---------------------------------------------------------------------------
void GrassRegionDialog::loadRegion()
{
    const QString gb = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
    if ( gb.isEmpty() ) return;

    QProcess p;
    p.setProcessEnvironment( QProcessEnvironment::systemEnvironment() );
    p.start( gb + "/bin/g.region", { "-p" } );
    p.waitForFinished( 5000 );

    const QString out = QString::fromLocal8Bit( p.readAllStandardOutput() );
    QMap<QString, double> vals;
    for ( const QString &line : out.split( '\n', Qt::SkipEmptyParts ) )
    {
        const int col = line.indexOf( ':' );
        if ( col < 0 ) continue;
        const QString key = line.left( col ).trimmed();
        bool ok;
        const double val = line.mid( col + 1 ).trimmed().toDouble( &ok );
        if ( ok ) vals[key] = val;
    }

    mUpdating = true;
    if ( vals.contains("north") ) mNorth->setValue( vals["north"] );
    if ( vals.contains("south") ) mSouth->setValue( vals["south"] );
    if ( vals.contains("east")  ) mEast->setValue(  vals["east"]  );
    if ( vals.contains("west")  ) mWest->setValue(  vals["west"]  );
    if ( vals.contains("nsres") ) mNSRes->setValue(  vals["nsres"] );
    if ( vals.contains("ewres") ) mEWRes->setValue(  vals["ewres"] );
    if ( vals.contains("rows")  ) mRows->setValue( static_cast<int>( vals["rows"] ) );
    if ( vals.contains("cols")  ) mCols->setValue( static_cast<int>( vals["cols"] ) );
    mUpdating = false;

    mStatus->setStyleSheet( "color:#555; font-style:italic;" );
    mStatus->setText( "Current computational region loaded." );
}

// ---------------------------------------------------------------------------
// nsresChanged — update rows/cols from N-S and E-W res
// ---------------------------------------------------------------------------
void GrassRegionDialog::nsresChanged()
{
    if ( mUpdating ) return;
    const double ns = mNSRes->value();
    const double ew = mEWRes->value();
    if ( ns <= 0 || ew <= 0 ) return;
    const double height = mNorth->value() - mSouth->value();
    const double width  = mEast->value()  - mWest->value();
    mUpdating = true;
    mRows->setValue( std::max( 1, static_cast<int>( height / ns ) ) );
    mCols->setValue( std::max( 1, static_cast<int>( width  / ew ) ) );
    mUpdating = false;
}

void GrassRegionDialog::rowsChanged()
{
    if ( mUpdating ) return;
    const double height = mNorth->value() - mSouth->value();
    const double width  = mEast->value()  - mWest->value();
    const int rows = mRows->value();
    const int cols = mCols->value();
    if ( rows <= 0 || cols <= 0 ) return;
    mUpdating = true;
    mNSRes->setValue( height / rows );
    mEWRes->setValue( width  / cols );
    mUpdating = false;
}

// ---------------------------------------------------------------------------
// setRegion — apply via g.region
// ---------------------------------------------------------------------------
void GrassRegionDialog::setRegion()
{
    runGRegion( {
        QString( "n=%1" ).arg( mNorth->value(), 0, 'f', 6 ),
        QString( "s=%1" ).arg( mSouth->value(), 0, 'f', 6 ),
        QString( "e=%1" ).arg( mEast ->value(), 0, 'f', 6 ),
        QString( "w=%1" ).arg( mWest ->value(), 0, 'f', 6 ),
        QString( "rows=%1" ).arg( mRows->value() ),
        QString( "cols=%1" ).arg( mCols->value() ),
    } );
}

void GrassRegionDialog::alignToRaster()
{
    const QString name = mRasterCombo->currentText().trimmed();
    if ( name.isEmpty() ) return;
    runGRegion( { "raster=" + name } );
    loadRegion();
}

void GrassRegionDialog::alignToVector()
{
    const QString name = mVectorCombo->currentText().trimmed();
    if ( name.isEmpty() ) return;
    runGRegion( { "vector=" + name } );
    loadRegion();
}
