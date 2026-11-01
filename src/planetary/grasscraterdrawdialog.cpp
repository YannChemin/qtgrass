#include "grasscraterdrawdialog.h"
#include "../grassenv.h"
#include "../grassinterface.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcCraterDraw, "qtgrass.craterdraw")

GrassCraterDrawDialog::GrassCraterDrawDialog( GrassInterface *iface,
                                                QWidget *parent )
    : QDialog( parent )
    , mIface( iface )
{
    setWindowTitle( "p.crater.draw - Auto Rim Delineation" );
    setMinimumSize( 720, 620 );
    buildUi();
    refreshMapLists();
}

GrassCraterDrawDialog::~GrassCraterDrawDialog()
{
    if ( mProcess && mProcess->state() != QProcess::NotRunning ) {
        mProcess->kill();
        mProcess->waitForFinished( 1000 );
    }
}

void GrassCraterDrawDialog::buildUi()
{
    auto *root = new QVBoxLayout( this );
    auto *tabs = new QTabWidget( this );
    root->addWidget( tabs, 1 );

    // ==================== Tab 1: Inputs ====================
    auto *inTab = new QWidget( tabs );
    auto *inLay = new QFormLayout( inTab );

    mDem = new QComboBox( inTab );
    mDem->setEditable( true );
    mDem->addItem( "" );
    inLay->addRow( "DEM raster (Strategy A):", mDem );

    mImage = new QComboBox( inTab );
    mImage->setEditable( true );
    mImage->addItem( "" );
    inLay->addRow( "Image raster (Strategy B):", mImage );

    mSunAzimuth = new QDoubleSpinBox( inTab );
    mSunAzimuth->setRange( 0.0, 360.0 );
    mSunAzimuth->setSuffix( " deg" );
    mSunAzimuth->setValue( 170.0 );
    mSunAzimuth->setSpecialValueText( "0 = north (required for image mode)" );
    inLay->addRow( "Sun azimuth (image mode):", mSunAzimuth );

    mBody = new QComboBox( inTab );
    mBody->addItems( { "moon", "mars", "mercury", "venus", "earth",
                        "ceres", "vesta", "europa", "ganymede", "callisto",
                        "io", "titan" } );
    inLay->addRow( "Planetary body (log only in v1):", mBody );

    mOutput = new QLineEdit( inTab );
    mOutput->setPlaceholderText( "detected_rims" );
    inLay->addRow( "Output vector name:", mOutput );

    auto *hint = new QLabel(
        "<i>Region must be in a metric (projected) CRS. At least one of "
        "DEM or Image must be supplied; both together activates the "
        "merged detector chain.</i>", inTab );
    hint->setWordWrap( true );
    inLay->addRow( "", hint );

    tabs->addTab( inTab, "Inputs" );

    // ==================== Tab 2: Detection ====================
    auto *detTab = new QWidget( tabs );
    auto *detLay = new QVBoxLayout( detTab );

    auto *paramBox = new QGroupBox( "Detection parameters", detTab );
    auto *paramLay = new QFormLayout( paramBox );

    mMethod = new QComboBox( paramBox );
    mMethod->addItem( "auto - pick based on supplied inputs", "auto" );
    mMethod->addItem( "dem - DEM only",                       "dem"  );
    mMethod->addItem( "image - image only",                   "image");
    mMethod->addItem( "both - merge DEM + image via NMS",     "both" );
    paramLay->addRow( "Method chain:", mMethod );

    mMinDiameter = new QDoubleSpinBox( paramBox );
    mMinDiameter->setRange( 10.0, 100000.0 );
    mMinDiameter->setValue( 100.0 );
    mMinDiameter->setSuffix( " m" );
    paramLay->addRow( "Min diameter:", mMinDiameter );

    mMaxDiameter = new QDoubleSpinBox( paramBox );
    mMaxDiameter->setRange( 100.0, 10000.0 );
    mMaxDiameter->setValue( 10000.0 );
    mMaxDiameter->setSuffix( " m (v1 cap: 10 km)" );
    paramLay->addRow( "Max diameter:", mMaxDiameter );

    mThreshold = new QDoubleSpinBox( paramBox );
    mThreshold->setRange( 0.0, 1.0 );
    mThreshold->setSingleStep( 0.05 );
    mThreshold->setValue( 0.55 );
    paramLay->addRow( "Confidence threshold:", mThreshold );

    mNmsIou = new QDoubleSpinBox( paramBox );
    mNmsIou->setRange( 0.0, 1.0 );
    mNmsIou->setSingleStep( 0.05 );
    mNmsIou->setValue( 0.30 );
    paramLay->addRow( "NMS IoU threshold:", mNmsIou );

    mScales = new QSpinBox( paramBox );
    mScales->setRange( 2, 32 );
    mScales->setValue( 8 );
    paramLay->addRow( "Log-spaced scales:", mScales );

    mAzSamples = new QSpinBox( paramBox );
    mAzSamples->setRange( 8, 64 );
    mAzSamples->setValue( 16 );
    paramLay->addRow( "Azimuth samples per ring (DEM):", mAzSamples );

    mThreads = new QSpinBox( paramBox );
    mThreads->setRange( 0, 256 );
    mThreads->setValue( 0 );
    mThreads->setSpecialValueText( "0 = library default" );
    paramLay->addRow( "OpenMP threads:", mThreads );

    mOpenCL = new QCheckBox( "Try OpenCL acceleration "
                                "(v1 stubbed - OpenMP is used)",
                                paramBox );
    paramLay->addRow( "", mOpenCL );

    detLay->addWidget( paramBox );
    detLay->addStretch();
    tabs->addTab( detTab, "Detection" );

    // ==================== Tab 3: Run log ====================
    auto *logTab = new QWidget( tabs );
    auto *logLay = new QVBoxLayout( logTab );
    mLog = new QPlainTextEdit( logTab );
    mLog->setReadOnly( true );
    mLog->setStyleSheet( "font-family: monospace; font-size: 10pt;" );
    mLog->setMaximumBlockCount( 4000 );
    mLog->setPlaceholderText( "Run output will appear here..." );
    logLay->addWidget( mLog );
    tabs->addTab( logTab, "Run log" );

    // ==================== Buttons ====================
    auto *btnBox = new QDialogButtonBox( this );
    mRunBtn    = btnBox->addButton( "&Run p.crater.draw",
                                      QDialogButtonBox::AcceptRole );
    mCancelBtn = btnBox->addButton( "Cancel run",
                                      QDialogButtonBox::DestructiveRole );
    btnBox->addButton( QDialogButtonBox::Close );
    mCancelBtn->setEnabled( false );
    root->addWidget( btnBox );

    connect( mRunBtn,    &QPushButton::clicked, this,
             &GrassCraterDrawDialog::runModule );
    connect( mCancelBtn, &QPushButton::clicked, this,
             &GrassCraterDrawDialog::cancelRun );
    connect( btnBox, &QDialogButtonBox::rejected, this, &QDialog::accept );
    connect( mMethod, &QComboBox::currentTextChanged, this,
             &GrassCraterDrawDialog::onMethodChanged );
}

void GrassCraterDrawDialog::refreshMapLists()
{
    QString gisdb  = qgetenv( "GISDBASE" );
    QString loc    = qgetenv( "LOCATION_NAME" );
    QString mapset = qgetenv( "MAPSET" );
    if ( gisdb.isEmpty() || loc.isEmpty() || mapset.isEmpty() )
        return;
    QString rastDir = QString( "%1/%2/%3/cell" ).arg(gisdb, loc, mapset);
    QStringList rasts = QDir(rastDir).entryList( QDir::Files, QDir::Name );
    mDem  ->addItems( rasts );
    mImage->addItems( rasts );
}

void GrassCraterDrawDialog::onMethodChanged( const QString &method )
{
    Q_UNUSED(method);
    /* Could grey out fields here when only one input is needed - keep
     * simple in v1.                                                   */
}

QStringList GrassCraterDrawDialog::buildArgs() const
{
    QStringList args;
    QString dem    = mDem  ->currentText().trimmed();
    QString image  = mImage->currentText().trimmed();
    if ( !dem  .isEmpty() ) args << "dem="   + dem;
    if ( !image.isEmpty() ) args << "image=" + image;
    if ( !image.isEmpty() )
        args << QString( "sun_azimuth=%1" ).arg(mSunAzimuth->value());
    args << "body="           + mBody->currentText();
    args << "method="         + mMethod->currentData().toString();
    args << QString( "min_diameter=%1"   ).arg(mMinDiameter->value());
    args << QString( "max_diameter=%1"   ).arg(mMaxDiameter->value());
    args << QString( "threshold=%1"      ).arg(mThreshold->value());
    args << QString( "nms_iou=%1"        ).arg(mNmsIou->value());
    args << QString( "scales=%1"         ).arg(mScales->value());
    args << QString( "azimuth_samples=%1").arg(mAzSamples->value());
    if ( mThreads->value() > 0 )
        args << QString( "threads=%1"    ).arg(mThreads->value());
    args << "output="         + mOutput->text().trimmed();
    if ( mOpenCL->isChecked() ) args << "-c";
    args << "--overwrite";
    return args;
}

void GrassCraterDrawDialog::runModule()
{
    QString dem   = mDem  ->currentText().trimmed();
    QString image = mImage->currentText().trimmed();
    QString out   = mOutput->text().trimmed();
    if ( out.isEmpty() ) {
        mLog->appendPlainText( "ERROR: output vector name required." );
        return;
    }
    if ( dem.isEmpty() && image.isEmpty() ) {
        mLog->appendPlainText(
            "ERROR: supply at least one of DEM or Image." );
        return;
    }
    QString method = mMethod->currentData().toString();
    if ( (method == "image" || method == "both") && image.isEmpty() ) {
        mLog->appendPlainText(
            "ERROR: method=" + method + " requires an image raster." );
        return;
    }
    if ( (method == "dem"   || method == "both") && dem.isEmpty() ) {
        mLog->appendPlainText(
            "ERROR: method=" + method + " requires a DEM raster." );
        return;
    }

    QStringList args = buildArgs();
    mLog->appendPlainText( "$ p.crater.draw " + args.join(" ") );

    mProcess = new QProcess( this );
    mProcess->setProcessChannelMode( QProcess::MergedChannels );
    connect( mProcess, &QProcess::readyReadStandardOutput, this,
             &GrassCraterDrawDialog::onProcessOutput );
    connect( mProcess,
             QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
             this, &GrassCraterDrawDialog::onProcessFinished );

    mRunBtn->setEnabled( false );
    mCancelBtn->setEnabled( true );
    mProcess->start( "p.crater.draw", args );
}

void GrassCraterDrawDialog::cancelRun()
{
    if ( mProcess && mProcess->state() != QProcess::NotRunning ) {
        mProcess->kill();
        mLog->appendPlainText( "[ run cancelled ]" );
    }
}

void GrassCraterDrawDialog::onProcessOutput()
{
    if ( !mProcess ) return;
    QByteArray data = mProcess->readAllStandardOutput();
    mLog->appendPlainText( QString::fromLocal8Bit(data).trimmed() );
}

void GrassCraterDrawDialog::onProcessFinished( int exitCode,
                                                 QProcess::ExitStatus status )
{
    mLog->appendPlainText(
        QString( "[ p.crater.draw exited with code %1 (%2) ]" )
            .arg(exitCode)
            .arg( status == QProcess::NormalExit ? "normal" : "crash" ) );
    mRunBtn->setEnabled( true );
    mCancelBtn->setEnabled( false );
    if ( mProcess ) { mProcess->deleteLater(); mProcess = nullptr; }
}
