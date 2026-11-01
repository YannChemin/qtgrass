#include "grassmoduledialog.h"
#include "grassparamwidget.h"
#include "grassxmlparser.h"
#include "../grassenv.h"
#include "../grassinterface.h"
#include "../grassmessagebar.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QFile>
#include <QFont>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>
#include <QDebug>

// ============================================================
// Construction
// ============================================================
GrassModuleDialog::GrassModuleDialog( const QString &moduleName,
                                       const QStringList &preArgs,
                                       GrassInterface *iface,
                                       QWidget *parent )
    : QDialog( parent )
    , mModuleName( moduleName )
    , mIface( iface )
{
    setWindowTitle( moduleName + " — qtgrass" );
    setMinimumSize( 600, 500 );
    resize( 750, 580 );

    // Load XML description
    QString loadErr;
    mDesc = GrassXmlParser::load( moduleName, &loadErr );
    if ( mDesc.name.isEmpty() )
    {
        // Show error inline rather than crashing
        QVBoxLayout *lay = new QVBoxLayout( this );
        lay->addWidget( new QLabel(
            QString( "Failed to load module '%1':\n%2" ).arg( moduleName, loadErr ), this ) );
        QPushButton *close = new QPushButton( "Close", this );
        connect( close, &QPushButton::clicked, this, &QDialog::reject );
        lay->addWidget( close );
        return;
    }

    // Main layout
    QVBoxLayout *root = new QVBoxLayout( this );
    root->setContentsMargins( 6, 6, 6, 6 );
    root->setSpacing( 4 );

    // Module description header
    const QString headerText = ( mDesc.label.isEmpty() ? mDesc.description : mDesc.label );
    if ( !headerText.isEmpty() )
    {
        QLabel *hdr = new QLabel( "<b>" + headerText + "</b>", this );
        hdr->setWordWrap( true );
        // Use the application palette so the description bar is readable in
        // both light and dark themes instead of hardcoding a light grey.
        const QPalette &pal = qApp->palette();
        const QColor bg  = pal.color( QPalette::AlternateBase );
        const QColor fg  = pal.color( QPalette::WindowText );
        hdr->setStyleSheet(
            QString( "padding: 6px 8px;"
                     "background-color: %1;"
                     "color: %2;"
                     "border-radius: 3px;"
                     "border: 1px solid %3;" )
            .arg( bg.name() )
            .arg( fg.name() )
            .arg( pal.color( QPalette::Mid ).name() ) );
        root->addWidget( hdr );
    }

    // Tab widget
    mTabs = new QTabWidget( this );
    buildOptionTabs( mTabs );
    buildOptionalTab( mTabs );
    buildCommandTab( mTabs );
    buildOutputTab( mTabs );
    buildManualTab(  mTabs );   // replaces the old Help button
    root->addWidget( mTabs, 1 );

    // Live command footer — always visible below tabs, shows current command
    mCommandFooter = new QLineEdit( this );
    mCommandFooter->setReadOnly( true );
    mCommandFooter->setFont( QFont( "monospace", 9 ) );
    mCommandFooter->setStyleSheet(
        "QLineEdit { background: palette(dark); color: palette(light); "
        "border: 1px solid palette(mid); border-radius: 3px; padding: 2px 6px; }" );
    mCommandFooter->setPlaceholderText( mModuleName );
    root->addWidget( mCommandFooter );

    // Progress bar (hidden until running)
    mProgress = new QProgressBar( this );
    mProgress->setRange( 0, 0 );  // indeterminate
    mProgress->setVisible( false );
    mProgress->setFixedHeight( 6 );
    root->addWidget( mProgress );

    // Button row
    QHBoxLayout *btnRow = new QHBoxLayout();
    mRunBtn    = new QPushButton( "Run", this );
    mCancelBtn = new QPushButton( "Cancel", this );
    mCopyBtn   = new QPushButton( "Copy Command", this );
    QPushButton *closeBtn = new QPushButton( "Close", this );

    mRunBtn->setDefault( true );
    mCancelBtn->setEnabled( false );

    btnRow->addWidget( mRunBtn );
    btnRow->addWidget( mCancelBtn );
    btnRow->addStretch();
    btnRow->addWidget( mCopyBtn );
    btnRow->addWidget( closeBtn );
    root->addLayout( btnRow );

    // Connections
    connect( mRunBtn,    &QPushButton::clicked, this, &GrassModuleDialog::run );
    connect( mCancelBtn, &QPushButton::clicked, this, &GrassModuleDialog::cancel );
    connect( closeBtn,   &QPushButton::clicked, this, &QDialog::accept );
    connect( mCopyBtn,   &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText( mCommandView->toPlainText() );
    } );

    // Apply pre-filled values from command line
    applyPreArgs( preArgs );

    // Initial command preview
    updateCommandPreview();
}

GrassModuleDialog::~GrassModuleDialog()
{
    if ( mProcess && mProcess->state() != QProcess::NotRunning )
    {
        mProcess->kill();
        mProcess->waitForFinished( 2000 );
    }
}

// ============================================================
// Tab builders
// ============================================================
void GrassModuleDialog::buildOptionTabs( QTabWidget *tabs )
{
    // One tab per guisection; params-only in each section.
    // Flags are collected separately into the "Optional" tab.
    QStringList sections = mDesc.sections();

    // Always have an "Options" catch-all for params without a section
    bool hasUnsectioned = false;
    for ( const auto &p : mDesc.params )
        if ( p.guiSection.isEmpty() ) { hasUnsectioned = true; break; }

    // Build section → scrollable form mapping (params only)
    auto makeScrollForm = [this]( const QString &section ) -> QWidget *
    {
        QScrollArea *scroll = new QScrollArea();
        scroll->setWidgetResizable( true );
        scroll->setFrameShape( QFrame::NoFrame );

        QWidget *container = new QWidget();
        QVBoxLayout *lay = new QVBoxLayout( container );
        lay->setSpacing( 4 );
        lay->setContentsMargins( 8, 8, 8, 8 );

        for ( const auto &pd : mDesc.params )
        {
            if ( pd.guiSection != section ) continue;
            GrassParamWidget *w = GrassParamWidget::create( pd, container );
            mParamWidgets.append( w );
            lay->addWidget( w );
            connect( w, &GrassParamWidget::valueChanged,
                     this, &GrassModuleDialog::updateCommandPreview );
        }

        lay->addStretch();
        scroll->setWidget( container );
        return scroll;
    };

    if ( hasUnsectioned || sections.isEmpty() )
        tabs->addTab( makeScrollForm( "" ), "Options" );

    for ( const QString &sec : sections )
    {
        if ( sec.isEmpty() ) continue;
        tabs->addTab( makeScrollForm( sec ), sec );
    }
}

void GrassModuleDialog::buildOptionalTab( QTabWidget *tabs )
{
    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable( true );
    scroll->setFrameShape( QFrame::NoFrame );

    QWidget *container = new QWidget();
    QVBoxLayout *lay = new QVBoxLayout( container );
    lay->setSpacing( 6 );
    lay->setContentsMargins( 8, 8, 8, 8 );

    // Standard GRASS flags (overwrite, verbose, quiet) — always added
    // These are parsed as "hidden" in the XML so we create them manually
    // and look for a matching parsed flag to re-use its description.
    struct StdFlag { const char *name; const char *label; const char *desc; };
    const StdFlag stdFlags[] = {
        { "overwrite", "Allow output files to overwrite existing files",
          "Allow existing files to be overwritten" },
        { "verbose",   "Verbose module output",  "Verbose module output" },
        { "quiet",     "Quiet module output",    "Quiet module output" },
    };

    QLabel *stdHdr = new QLabel( "<b>Standard flags</b>", container );
    lay->addWidget( stdHdr );

    for ( const auto &sf : stdFlags )
    {
        // Find in parsed flags list (they are marked hidden but parsed)
        GrassFlagDesc fd;
        fd.name        = sf.name;
        fd.label       = sf.label;
        fd.description = sf.desc;
        fd.hidden      = false;
        for ( const auto &pf : mDesc.flags )
            if ( pf.name == sf.name ) { fd = pf; fd.hidden = false; break; }

        GrassFlagWidget *fw = new GrassFlagWidget( fd, container );
        mFlagWidgets.append( fw );
        lay->addWidget( fw );
        connect( fw, &GrassFlagWidget::valueChanged,
                 this, &GrassModuleDialog::updateCommandPreview );
    }

    // Module-specific flags (non-hidden, any guisection)
    bool addedModuleHdr = false;
    for ( const auto &fd : mDesc.flags )
    {
        if ( fd.hidden ) continue;
        if ( !addedModuleHdr )
        {
            lay->addSpacing( 8 );
            QLabel *fhdr = new QLabel( "<b>Module flags</b>", container );
            lay->addWidget( fhdr );
            addedModuleHdr = true;
        }
        GrassFlagWidget *fw = new GrassFlagWidget( fd, container );
        mFlagWidgets.append( fw );
        lay->addWidget( fw );
        connect( fw, &GrassFlagWidget::valueChanged,
                 this, &GrassModuleDialog::updateCommandPreview );
    }

    if ( !addedModuleHdr )
    {
        QLabel *none = new QLabel( "<i>No module-specific flags.</i>", container );
        none->setStyleSheet( QString( "color: %1;" )
            .arg( qApp->palette().color( QPalette::PlaceholderText ).name() ) );
        lay->addWidget( none );
    }

    lay->addStretch();
    scroll->setWidget( container );
    tabs->addTab( scroll, "Optional" );
}

void GrassModuleDialog::buildCommandTab( QTabWidget *tabs )
{
    mCommandView = new QPlainTextEdit();
    mCommandView->setReadOnly( true );
    mCommandView->setFont( QFont( "monospace" ) );
    mCommandView->setStyleSheet( "background: #1e1e1e; color: #d4d4d4;" );
    tabs->addTab( mCommandView, "Command" );
}

void GrassModuleDialog::buildOutputTab( QTabWidget *tabs )
{
    mOutputView = new QPlainTextEdit();
    mOutputView->setReadOnly( true );
    mOutputView->setFont( QFont( "monospace" ) );
    mOutputView->setStyleSheet( "background: #1e1e1e; color: #cccccc;" );
    tabs->addTab( mOutputView, "Output" );
}

void GrassModuleDialog::buildManualTab( QTabWidget *tabs )
{
    QWidget *page = new QWidget();
    QVBoxLayout *lay = new QVBoxLayout( page );
    lay->setContentsMargins( 0, 4, 0, 0 );
    lay->setSpacing( 4 );

    // Search bar
    QLineEdit *search = new QLineEdit( page );
    search->setPlaceholderText( "Find in page…" );
    search->setClearButtonEnabled( true );
    lay->addWidget( search );

    // HTML viewer
    QTextBrowser *browser = new QTextBrowser( page );
    browser->setOpenExternalLinks( false );
    browser->setOpenLinks( true );
    const QString gb = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
    browser->setSearchPaths( { gb + "/docs/html/" } );

    const QString htmlPath = gb + "/docs/html/" + mModuleName + ".html";
    if ( QFile::exists( htmlPath ) )
    {
        browser->setSource( QUrl::fromLocalFile( htmlPath ) );
    }
    else
    {
        browser->setHtml( QString(
            "<html><body style='font-family:sans-serif;padding:20px'>"
            "<h2>%1</h2>"
            "<p style='color:#888'>Help page not found at:<br>"
            "<code>%2</code></p>"
            "<p>Run <code>%1 --help</code> in a terminal for usage.</p>"
            "</body></html>" ).arg( mModuleName, htmlPath ) );
    }
    lay->addWidget( browser, 1 );

    connect( search, &QLineEdit::textChanged, browser,
             [browser]( const QString &t ) { browser->find( t ); } );

    tabs->addTab( page, "Manual" );
}

// ============================================================
// Pre-fill from command line key=val arguments
// ============================================================
void GrassModuleDialog::applyPreArgs( const QStringList &preArgs )
{
    for ( const QString &arg : preArgs )
    {
        const int eq = arg.indexOf( '=' );
        if ( eq < 0 ) continue;
        const QString key = arg.left( eq ).trimmed();
        const QString val = arg.mid( eq + 1 ).trimmed();

        for ( GrassParamWidget *w : mParamWidgets )
        {
            if ( w->key() != key ) continue;
            w->setValue( val );
            break;
        }
    }
}

// ============================================================
// Command preview
// ============================================================
void GrassModuleDialog::updateCommandPreview()
{
    QStringList parts;
    parts << mModuleName;
    parts << collectArgs();

    const QString cmd = parts.join( " " );
    mCommandView->setPlainText( cmd );
    if ( mCommandFooter )
        mCommandFooter->setText( cmd );
}

QStringList GrassModuleDialog::collectArgs() const
{
    QStringList args;
    for ( const GrassParamWidget *w : mParamWidgets )
        args << w->commandArgs();
    for ( const GrassFlagWidget *fw : mFlagWidgets )
    {
        const QString a = fw->commandArg();
        if ( !a.isEmpty() ) args << a;
    }
    return args;
}

// ============================================================
// Validation
// ============================================================
bool GrassModuleDialog::validateAll()
{
    bool ok = true;
    for ( GrassParamWidget *w : mParamWidgets )
    {
        const bool valid = w->isValid();
        w->markInvalid( !valid );
        if ( !valid ) ok = false;
    }
    return ok;
}

// ============================================================
// Run / Cancel
// ============================================================
QString GrassModuleDialog::moduleBin() const
{
    return GrassXmlParser::findModuleBin( mModuleName );
}

QProcessEnvironment GrassModuleDialog::buildEnv() const
{
    // Use GrassEnv to build a complete environment including a real GISRC file.
    // Child processes need GISRC because they call G_gisinit() which reads from
    // the file (our in-memory G_GISRC_MODE_MEMORY session is not inherited).
    return GrassEnv::instance()->buildSubprocessEnv();
}

void GrassModuleDialog::run()
{
    if ( !validateAll() )
    {
        QMessageBox::warning( this, mModuleName,
                              "Please fill in all required parameters (shown in red)." );
        return;
    }

    const QString bin = moduleBin();
    if ( bin.isEmpty() )
    {
        QMessageBox::critical( this, mModuleName,
                               "Module binary not found. Check GISBASE." );
        return;
    }

    // Switch to Output tab
    mTabs->setCurrentWidget( mOutputView->parentWidget() );
    // Actually find the tab index
    for ( int i = 0; i < mTabs->count(); ++i )
        if ( mTabs->tabText( i ) == "Output" )
            mTabs->setCurrentIndex( i );

    mOutputView->clear();
    mOutputView->appendPlainText( "$ " + mCommandView->toPlainText() + "\n" );

    mProcess = new QProcess( this );
    mProcess->setProcessEnvironment( buildEnv() );
    mProcess->setProcessChannelMode( QProcess::MergedChannels );

    connect( mProcess, &QProcess::started,
             this, &GrassModuleDialog::onProcessStarted );
    connect( mProcess, &QProcess::readyReadStandardOutput,
             this, &GrassModuleDialog::onProcessOutput );
    connect( mProcess, qOverload<int, QProcess::ExitStatus>( &QProcess::finished ),
             this, &GrassModuleDialog::onProcessFinished );

    mProcess->start( bin, collectArgs() );
}

void GrassModuleDialog::cancel()
{
    if ( mProcess && mProcess->state() != QProcess::NotRunning )
    {
        mProcess->terminate();
        if ( !mProcess->waitForFinished( 3000 ) )
            mProcess->kill();
    }
}

void GrassModuleDialog::onProcessStarted()
{
    mRunBtn->setEnabled( false );
    mCancelBtn->setEnabled( true );
    mProgress->setVisible( true );
}

void GrassModuleDialog::onProcessOutput()
{
    const QByteArray data = mProcess->readAllStandardOutput();
    // Colour GRASS progress lines (GRASS outputs "XX%" on stderr)
    const QString text = QString::fromLocal8Bit( data );
    mOutputView->moveCursor( QTextCursor::End );
    mOutputView->insertPlainText( text );
    mOutputView->ensureCursorVisible();
}

void GrassModuleDialog::onProcessFinished( int exitCode, int exitStatus )
{
    mProgress->setVisible( false );
    mRunBtn->setEnabled( true );
    mCancelBtn->setEnabled( false );

    const bool success = ( exitStatus == QProcess::NormalExit && exitCode == 0 );
    mOutputView->appendPlainText(
        success ? "\n✓ Finished successfully."
                : QString( "\n✗ Finished with exit code %1." ).arg( exitCode ) );

    // Notify interface to add output layers (Part 4 will wire this up)
    if ( success && mIface )
    {
        // Iterate over "new" (output) raster/vector params that were set
        for ( const GrassParamWidget *w : mParamWidgets )
        {
            if ( !w->desc().hasGisprompt ) continue;
            const auto &gp = w->desc().gisprompt;
            if ( gp.age != "new" ) continue;
            const QStringList vals = w->values();
            if ( vals.isEmpty() ) continue;

            const QString mapName = vals.first();
            const QString loc  = QString::fromLocal8Bit( qgetenv( "LOCATION_NAME" ) );
            const QString ms   = QString::fromLocal8Bit( qgetenv( "MAPSET" ) );
            const QString gisdb= QString::fromLocal8Bit( qgetenv( "GISDBASE" ) );

            if ( gp.prompt == "raster" )
            {
                const QString uri = gisdb + "/" + loc + "/" + ms + "/cell/" + mapName;
                mIface->addRasterLayer( uri, mapName, "grassraster" );
            }
            else if ( gp.prompt == "vector" )
            {
                const QString uri = gisdb + "/" + loc + "/" + ms + "/vector/" + mapName;
                mIface->addVectorLayer( uri, mapName, "grass" );
            }
        }
    }

    mProcess->deleteLater();
    mProcess = nullptr;
}
