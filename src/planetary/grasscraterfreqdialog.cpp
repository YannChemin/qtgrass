#include "grasscraterfreqdialog.h"
#include "../grassenv.h"
#include "../grassinterface.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextStream>
#include <QTemporaryFile>
#include <QVBoxLayout>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QLogValueAxis>
#include <QtCharts/QScatterSeries>

#include <cmath>

Q_LOGGING_CATEGORY(lcCraterFreq, "qtgrass.craterfreq")

// ===================================================================
GrassCraterFreqDialog::GrassCraterFreqDialog( GrassInterface *iface,
                                                QWidget *parent )
    : QDialog( parent )
    , mIface( iface )
{
    setWindowTitle( "p.crater.freq - Size-Frequency Dating" );
    setMinimumSize( 640, 520 );
    buildUi();
    refreshVectorList();
}

GrassCraterFreqDialog::~GrassCraterFreqDialog()
{
    if ( mProcess && mProcess->state() != QProcess::NotRunning ) {
        mProcess->kill();
        mProcess->waitForFinished( 1000 );
    }
}

// ── UI ──────────────────────────────────────────────────────────────
void GrassCraterFreqDialog::buildUi()
{
    auto *root = new QVBoxLayout( this );

    mTabs = new QTabWidget( this );
    root->addWidget( mTabs, 1 );

    // ==================== Tab 1: Inputs & parameters ==================
    auto *inputsTab = new QWidget( mTabs );
    auto *inputsLay = new QVBoxLayout( inputsTab );

    // Source toggle
    auto *srcBox = new QGroupBox( "Crater diameter source", inputsTab );
    auto *srcLay = new QVBoxLayout( srcBox );
    auto *radioRow = new QHBoxLayout;
    mUseCsv    = new QRadioButton( "&CSV file (default)",  srcBox );
    mUseVector = new QRadioButton( "&Vector map + column",  srcBox );
    mUseCsv->setChecked( true );
    radioRow->addWidget( mUseCsv );
    radioRow->addWidget( mUseVector );
    radioRow->addStretch();
    srcLay->addLayout( radioRow );

    mStack = new QStackedWidget( srcBox );
    srcLay->addWidget( mStack );

    // CSV pane
    auto *csvPane = new QWidget( mStack );
    auto *csvLay  = new QHBoxLayout( csvPane );
    csvLay->setContentsMargins( 0, 0, 0, 0 );
    csvLay->addWidget( new QLabel( "CSV path:" ) );
    mCsvPath = new QLineEdit( csvPane );
    csvLay->addWidget( mCsvPath, 1 );
    auto *csvBtn = new QPushButton( "Browse…", csvPane );
    csvLay->addWidget( csvBtn );
    mStack->addWidget( csvPane );

    // Vector pane
    auto *vecPane = new QWidget( mStack );
    auto *vecLay  = new QFormLayout( vecPane );
    vecLay->setContentsMargins( 0, 0, 0, 0 );
    mVector = new QComboBox( vecPane );
    mVector->setEditable( true );
    mColumn = new QComboBox( vecPane );
    mColumn->setEditable( true );
    mColumn->addItems( { "Df_pi", "Dat_pi", "D_eq",
                          "Dat_gault", "Dat_yield" } );
    vecLay->addRow( "Vector map:",       mVector );
    vecLay->addRow( "Diameter column:",  mColumn );
    mStack->addWidget( vecPane );

    inputsLay->addWidget( srcBox );

    // Analysis parameters
    auto *paramsBox = new QGroupBox( "Analysis parameters", inputsTab );
    auto *paramsLay = new QFormLayout( paramsBox );

    mArea = new QDoubleSpinBox( paramsBox );
    mArea->setRange( 0.001, 1.0e9 );
    mArea->setValue( 100.0 );
    mArea->setDecimals( 3 );
    mArea->setSuffix( " km^2" );
    paramsLay->addRow( "Mapped area:",  mArea );

    mBody = new QComboBox( paramsBox );
    mBody->addItems( { "moon", "mars" } );
    mBody->setCurrentText( "mars" );
    paramsLay->addRow( "Target body:",  mBody );

    mDmin = new QDoubleSpinBox( paramsBox );
    mDmin->setRange( 1e-4, 1.0e6 );
    mDmin->setValue( 0.1 );
    mDmin->setDecimals( 4 );
    mDmin->setSuffix( " km" );
    paramsLay->addRow( "Min diameter bin:", mDmin );

    mDmax = new QDoubleSpinBox( paramsBox );
    mDmax->setRange( 1e-3, 1.0e6 );
    mDmax->setValue( 100.0 );
    mDmax->setDecimals( 3 );
    mDmax->setSuffix( " km" );
    paramsLay->addRow( "Max diameter bin:", mDmax );

    mNbins = new QSpinBox( paramsBox );
    mNbins->setRange( 2, 200 );
    mNbins->setValue( 18 );
    paramsLay->addRow( "Number of bins:",  mNbins );

    mHartmann = new QCheckBox( "Use Hartmann 2005 (pending implementation)",
                                 paramsBox );
    paramsLay->addRow( "",  mHartmann );

    inputsLay->addWidget( paramsBox );

    // Output paths
    auto *outBox = new QGroupBox( "Output", inputsTab );
    auto *outLay = new QFormLayout( outBox );
    auto outCsvRow = new QHBoxLayout;
    mOutputCsv = new QLineEdit( outBox );
    auto *outBtn = new QPushButton( "Browse…", outBox );
    outCsvRow->addWidget( mOutputCsv, 1 );
    outCsvRow->addWidget( outBtn );
    outLay->addRow( "Output CSV (optional):", outCsvRow );

    auto pngRow = new QHBoxLayout;
    mOutputPng = new QLineEdit( outBox );
    auto *pngBtn = new QPushButton( "Browse…", outBox );
    pngRow->addWidget( mOutputPng, 1 );
    pngRow->addWidget( pngBtn );
    outLay->addRow( "Output PNG graph (600 dpi, A5 landscape):", pngRow );

    mShowGraph = new QCheckBox( "Show graph in &Chart tab after run "
                                  "(uses a temp CSV if Output CSV is empty)",
                                  outBox );
    mShowGraph->setChecked( true );
    outLay->addRow( "", mShowGraph );
    inputsLay->addWidget( outBox );

    inputsLay->addStretch();
    mTabs->addTab( inputsTab, "Inputs && Parameters" );

    // ==================== Tab 2: Run log ==================
    auto *logTab = new QWidget( mTabs );
    auto *logLay = new QVBoxLayout( logTab );
    mLog = new QPlainTextEdit( logTab );
    mLog->setReadOnly( true );
    mLog->setStyleSheet( "font-family: monospace; font-size: 10pt;" );
    mLog->setMaximumBlockCount( 2000 );
    mLog->setPlaceholderText( "Run output will appear here..." );
    logLay->addWidget( mLog );
    mTabs->addTab( logTab, "Run log" );

    // ==================== Tab 3: Chart ==================
    auto *chartTab = new QWidget( mTabs );
    auto *chartLay = new QVBoxLayout( chartTab );
    mChartView = new QChartView( chartTab );
    mChartView->setRenderHint( QPainter::Antialiasing );
    {
        auto *placeholder = new QChart();
        placeholder->setTitle(
            "Run p.crater.freq with 'Show graph' enabled to populate this view." );
        mChartView->setChart( placeholder );
    }
    chartLay->addWidget( mChartView, 1 );
    mTabs->addTab( chartTab, "Chart" );

    // ==================== Bottom: buttons ==================
    auto *btnBox = new QDialogButtonBox( this );
    mRunBtn    = btnBox->addButton( "&Run p.crater.freq",
                                      QDialogButtonBox::AcceptRole );
    mCancelBtn = btnBox->addButton( "Cancel run",
                                      QDialogButtonBox::DestructiveRole );
    btnBox->addButton( QDialogButtonBox::Close );
    mCancelBtn->setEnabled( false );
    root->addWidget( btnBox );

    // Wiring
    connect( mUseCsv, &QRadioButton::toggled, this, [this](bool on){
        mStack->setCurrentIndex( on ? 0 : 1 );
    });
    connect( csvBtn,    &QPushButton::clicked, this,
             &GrassCraterFreqDialog::browseCsv );
    connect( outBtn,    &QPushButton::clicked, this,
             &GrassCraterFreqDialog::browseOutput );
    connect( pngBtn,    &QPushButton::clicked, this,
             &GrassCraterFreqDialog::browsePng );
    connect( mVector, &QComboBox::currentTextChanged, this,
             &GrassCraterFreqDialog::onVectorChanged );
    connect( mRunBtn,    &QPushButton::clicked, this,
             &GrassCraterFreqDialog::runModule );
    connect( mCancelBtn, &QPushButton::clicked, this,
             &GrassCraterFreqDialog::cancelRun );
    connect( btnBox, &QDialogButtonBox::rejected, this, &QDialog::accept );
}

// ── List vectors in current mapset ──────────────────────────────────
void GrassCraterFreqDialog::refreshVectorList()
{
    QString gisdb  = qgetenv( "GISDBASE" );
    QString loc    = qgetenv( "LOCATION_NAME" );
    QString mapset = qgetenv( "MAPSET" );
    if ( gisdb.isEmpty() || loc.isEmpty() || mapset.isEmpty() )
        return;
    QString vecDir = QString( "%1/%2/%3/vector" ).arg(gisdb, loc, mapset);
    mVector->addItems( QDir(vecDir).entryList( QDir::Dirs |
                          QDir::NoDotAndDotDot, QDir::Name ) );
}

// ── When the user picks a different vector, refresh column suggestions ──
void GrassCraterFreqDialog::onVectorChanged( const QString &name )
{
    /* For now: keep the standard p.crater column-name suggestions.
     * A future enhancement would shell out to "v.info -c map=<name>" to
     * read the actual column list, but that requires a running GRASS
     * session and isn't critical for first release. */
    Q_UNUSED(name);
    refreshColumns();
}

void GrassCraterFreqDialog::refreshColumns()
{
    QString current = mColumn->currentText();
    mColumn->clear();
    mColumn->addItems( { "Df_pi", "Dat_pi", "D_eq",
                          "Dat_gault", "Dat_yield" } );
    if ( !current.isEmpty() )
        mColumn->setCurrentText( current );
}

void GrassCraterFreqDialog::browseCsv()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Choose crater CSV file", QDir::homePath(),
        "CSV files (*.csv *.txt);;All files (*)" );
    if ( !path.isEmpty() ) mCsvPath->setText( path );
}

void GrassCraterFreqDialog::browseOutput()
{
    QString path = QFileDialog::getSaveFileName(
        this, "Save SFD output", QDir::homePath(),
        "CSV files (*.csv);;All files (*)" );
    if ( !path.isEmpty() ) mOutputCsv->setText( path );
}

void GrassCraterFreqDialog::browsePng()
{
    QString path = QFileDialog::getSaveFileName(
        this, "Save chart as PNG", QDir::homePath(),
        "PNG images (*.png);;All files (*)" );
    if ( !path.isEmpty() ) mOutputPng->setText( path );
}

// ── argv ────────────────────────────────────────────────────────────
QStringList GrassCraterFreqDialog::buildArgs() const
{
    QStringList args;
    if ( mUseCsv->isChecked() ) {
        args << "input=" + mCsvPath->text().trimmed();
    } else {
        args << "vector=" + mVector->currentText().trimmed();
        args << "column=" + mColumn->currentText().trimmed();
    }
    args << QString( "area=%1"  ).arg(mArea->value());
    args << "body="  + mBody->currentText();
    args << QString( "dmin=%1"  ).arg(mDmin->value());
    args << QString( "dmax=%1"  ).arg(mDmax->value());
    args << QString( "nbins=%1" ).arg(mNbins->value());
    if ( !mOutputCsv->text().trimmed().isEmpty() )
        args << "output=" + mOutputCsv->text().trimmed();
    if ( mHartmann->isChecked() )
        args << "-t";
    return args;
}

// ── Run ────────────────────────────────────────────────────────────
void GrassCraterFreqDialog::runModule()
{
    /* Validate */
    if ( mUseCsv->isChecked() && mCsvPath->text().trimmed().isEmpty() ) {
        mLog->appendPlainText( "ERROR: choose a CSV path first." );
        return;
    }
    if ( mUseVector->isChecked() &&
         ( mVector->currentText().trimmed().isEmpty() ||
           mColumn->currentText().trimmed().isEmpty() ) )
    {
        mLog->appendPlainText(
            "ERROR: vector mode requires both a vector map name and a "
            "diameter column name." );
        return;
    }

    /* If the user wants a chart but did not set an output CSV path,
     * stage one in $TMPDIR so we can parse the SFD table after the run. */
    mLastCsvPath.clear();
    QStringList args = buildArgs();
    bool want_chart = mShowGraph->isChecked() ||
                       !mOutputPng->text().trimmed().isEmpty();
    bool has_user_csv = !mOutputCsv->text().trimmed().isEmpty();
    if ( want_chart && !has_user_csv ) {
        QTemporaryFile tmpFile( QDir::tempPath() +
                                  "/p_crater_freq_XXXXXX.csv" );
        tmpFile.setAutoRemove( false );
        if ( !tmpFile.open() ) {
            mLog->appendPlainText(
                "WARNING: cannot create temp CSV for graph; graph disabled." );
        } else {
            mLastCsvPath = tmpFile.fileName();
            tmpFile.close();
            args << "output=" + mLastCsvPath;
        }
    } else if ( has_user_csv ) {
        mLastCsvPath = mOutputCsv->text().trimmed();
    }

    mLog->appendPlainText( "$ p.crater.freq " + args.join(" ") );

    mProcess = new QProcess( this );
    mProcess->setProcessChannelMode( QProcess::MergedChannels );
    connect( mProcess, &QProcess::readyReadStandardOutput, this,
             &GrassCraterFreqDialog::onProcessOutput );
    connect( mProcess,
             QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
             this, &GrassCraterFreqDialog::onProcessFinished );

    mRunBtn->setEnabled( false );
    mCancelBtn->setEnabled( true );
    mProcess->start( "p.crater.freq", args );
}

void GrassCraterFreqDialog::cancelRun()
{
    if ( mProcess && mProcess->state() != QProcess::NotRunning ) {
        mProcess->kill();
        mLog->appendPlainText( "[ run cancelled by user ]" );
    }
}

void GrassCraterFreqDialog::onProcessOutput()
{
    if ( !mProcess ) return;
    QByteArray data = mProcess->readAllStandardOutput();
    mLog->appendPlainText( QString::fromLocal8Bit(data).trimmed() );
}

void GrassCraterFreqDialog::onProcessFinished( int exitCode,
                                                 QProcess::ExitStatus status )
{
    mLog->appendPlainText(
        QString( "[ p.crater.freq exited with code %1 (%2) ]" )
            .arg(exitCode)
            .arg( status == QProcess::NormalExit ? "normal" : "crash" ) );
    mRunBtn->setEnabled( true );
    mCancelBtn->setEnabled( false );
    if ( mProcess ) { mProcess->deleteLater(); mProcess = nullptr; }

    /* On clean exit, build/refresh the chart if requested. */
    if ( exitCode == 0 && status == QProcess::NormalExit &&
         !mLastCsvPath.isEmpty() ) {
        redrawChartFromCsv();
    }
}

// ===================================================================
//  CSV parser + chart builder
// ===================================================================

bool GrassCraterFreqDialog::parseOutputCsv(
    const QString &path,
    QString *body, double *ageGa,
    double *slope, double *intercept,
    QVector<double> *D,
    QVector<int>    *nInBin,
    QVector<double> *Nobs,
    QVector<double> *Nnpf,
    QVector<double> *Npl,
    QString *errorMsg )
{
    QFile f( path );
    if ( !f.open(QIODevice::ReadOnly | QIODevice::Text) ) {
        *errorMsg = QString( "Cannot open %1 for reading." ).arg(path);
        return false;
    }
    QTextStream in( &f );
    while ( !in.atEnd() ) {
        QString line = in.readLine().trimmed();
        if ( line.isEmpty() ) continue;

        if ( line.startsWith('#') ) {
            /* Parse header metadata.  Example lines:
             *   # body=mars  area=100.0 km^2  N=15  age=2.451 Ga
             *   # Power-law fit log10(N)=-1.18+-0.63*log10(D) rmse...  */
            QRegularExpression re_body  ("body=(\\S+)");
            QRegularExpression re_age   ("age=([-+0-9.eE]+)");
            QRegularExpression re_inter ("log10\\(N\\)=([-+0-9.eE]+)");
            QRegularExpression re_slope ("([-+][0-9.eE]+)\\*log10\\(D\\)");
            QRegularExpressionMatch m;
            if ( (m = re_body .match(line)).hasMatch() ) *body      = m.captured(1);
            if ( (m = re_age  .match(line)).hasMatch() ) *ageGa     = m.captured(1).toDouble();
            if ( (m = re_inter.match(line)).hasMatch() ) *intercept = m.captured(1).toDouble();
            if ( (m = re_slope.match(line)).hasMatch() ) *slope     = m.captured(1).toDouble();
            continue;
        }

        /* Data row: D_km  n_in_bin  N_cum_obs  N_npf_age  N_powerlaw_fit */
        QStringList parts = line.split( QRegularExpression("\\s+"),
                                          Qt::SkipEmptyParts );
        if ( parts.size() < 5 ) continue;
        bool ok1, ok2, ok3, ok4, ok5;
        double d_v   = parts[0].toDouble(&ok1);
        int    nbin  = parts[1].toInt   (&ok2);
        double Nob_v = parts[2].toDouble(&ok3);
        double Nnp_v = parts[3].toDouble(&ok4);
        double Npl_v = parts[4].toDouble(&ok5);
        if ( !(ok1 && ok2 && ok3 && ok4 && ok5) ) continue;
        D     ->append( d_v );
        nInBin->append( nbin );
        Nobs  ->append( Nob_v );
        Nnpf  ->append( Nnp_v );
        Npl   ->append( Npl_v );
    }
    return !D->isEmpty();
}

void GrassCraterFreqDialog::buildChart(
    const QString &body, double ageGa,
    const QVector<double> &D,
    const QVector<double> &Nobs,
    const QVector<double> &Nnpf,
    const QVector<double> &Npl )
{
    auto *chart = new QChart();
    chart->setTitle( QString( "<h3>%1 - age = %2 Ga</h3>" )
                          .arg( body.isEmpty() ? "(unknown body)" : body )
                          .arg( ageGa, 0, 'f', 3 ) );

    /* Observed scatter (only points with N_obs > 0). */
    auto *obs = new QScatterSeries();
    obs->setName( "Observed N(>=D)" );
    obs->setMarkerSize( 9.0 );
    /* NPF curve (only positive values). */
    auto *npf = new QLineSeries();
    npf->setName( "NPF (Neukum 1983 / Ivanov 2001)" );
    /* Power-law fit. */
    auto *pl  = new QLineSeries();
    pl->setName( "Power-law fit" );
    QPen plPen( QColor(0x40, 0x40, 0x40) );
    plPen.setStyle( Qt::DashLine );
    plPen.setWidthF( 1.5 );
    pl->setPen( plPen );

    double Dmin = 1e30, Dmax = -1e30, Ymin = 1e30, Ymax = -1e30;
    auto trackBounds = [&](double x, double y) {
        if ( x <= 0.0 || y <= 0.0 ) return;
        if ( x < Dmin ) Dmin = x;
        if ( x > Dmax ) Dmax = x;
        if ( y < Ymin ) Ymin = y;
        if ( y > Ymax ) Ymax = y;
    };

    for ( int i = 0; i < D.size(); i++ ) {
        if ( Nobs[i] > 0.0 ) { obs->append(D[i], Nobs[i]);
                                trackBounds(D[i], Nobs[i]); }
        if ( Nnpf[i] > 0.0 ) { npf->append(D[i], Nnpf[i]);
                                trackBounds(D[i], Nnpf[i]); }
        if ( Npl[i]  > 0.0 ) { pl ->append(D[i], Npl[i]);
                                trackBounds(D[i], Npl[i]); }
    }

    chart->addSeries( npf );
    chart->addSeries( pl  );
    chart->addSeries( obs );

    /* Log axes - round bounds to neat decades. */
    auto *ax = new QLogValueAxis();
    ax->setTitleText( "Crater diameter D [km]" );
    ax->setBase( 10.0 );
    ax->setLabelFormat( "%g" );
    if ( Dmin < Dmax ) {
        ax->setRange( pow(10.0, floor(log10(Dmin))),
                      pow(10.0, ceil (log10(Dmax))) );
    }

    auto *ay = new QLogValueAxis();
    ay->setTitleText( "Cumulative N(>=D) [km^-2]" );
    ay->setBase( 10.0 );
    ay->setLabelFormat( "%g" );
    if ( Ymin < Ymax ) {
        ay->setRange( pow(10.0, floor(log10(Ymin))),
                      pow(10.0, ceil (log10(Ymax))) );
    }

    chart->addAxis( ax, Qt::AlignBottom );
    chart->addAxis( ay, Qt::AlignLeft   );
    npf->attachAxis( ax ); npf->attachAxis( ay );
    pl ->attachAxis( ax ); pl ->attachAxis( ay );
    obs->attachAxis( ax ); obs->attachAxis( ay );

    chart->legend()->setAlignment( Qt::AlignBottom );

    /* Swap into the view (old chart deleted). */
    QChart *old = mChartView->chart();
    mChartView->setChart( chart );
    if ( old ) old->deleteLater();
}

void GrassCraterFreqDialog::savePngA5Landscape( const QString &path )
{
    /* A5 landscape at 600 dpi:
     *   210 mm * 600 / 25.4 = 4961 px
     *   148 mm * 600 / 25.4 = 3496 px                                */
    constexpr int W = 4961;
    constexpr int H = 3496;
    QImage img( W, H, QImage::Format_ARGB32 );
    img.fill( Qt::white );

    QChart *chart = mChartView->chart();
    if ( !chart ) return;

    QPainter p( &img );
    p.setRenderHint( QPainter::Antialiasing );

    /* Render the chart into the full image area; let QGraphicsScene
     * handle the layout. */
    QSizeF prevSize = chart->size();
    chart->resize( QSizeF( W, H ) );
    chart->scene()->render( &p,
                              QRectF(0, 0, W, H),
                              QRectF(0, 0, W, H) );
    chart->resize( prevSize );
    p.end();

    if ( img.save( path, "PNG" ) ) {
        mLog->appendPlainText(
            QString( "Chart saved to %1 (%2 x %3 px @ 600 dpi)" )
                .arg(path).arg(W).arg(H) );
    } else {
        mLog->appendPlainText(
            QString( "ERROR: failed to save chart PNG to %1" ).arg(path) );
    }
}

void GrassCraterFreqDialog::redrawChartFromCsv()
{
    if ( mLastCsvPath.isEmpty() ) return;
    QString body, err;
    double ageGa = 0.0, slope = 0.0, intercept = 0.0;
    QVector<double> D, Nobs, Nnpf, Npl;
    QVector<int>    nInBin;

    if ( !parseOutputCsv( mLastCsvPath, &body, &ageGa, &slope, &intercept,
                            &D, &nInBin, &Nobs, &Nnpf, &Npl, &err ) ) {
        mLog->appendPlainText(
            QString( "ERROR: parsing output CSV failed: %1" ).arg(err) );
        return;
    }

    buildChart( body, ageGa, D, Nobs, Nnpf, Npl );
    /* Switch to the Chart tab so the user sees the result. */
    if ( mTabs ) mTabs->setCurrentIndex( mTabs->count() - 1 );

    /* If user asked for a PNG, save it. */
    QString png = mOutputPng->text().trimmed();
    if ( !png.isEmpty() ) {
        savePngA5Landscape( png );
    }
}
