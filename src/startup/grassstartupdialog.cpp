#include "grassstartupdialog.h"
#include "grassmapsetbrowser.h"
#include "grasslocationwizard.h"
#include "../grassenv.h"

extern "C" {
#include <grass/version.h>
}

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextStream>

// ---------------------------------------------------------------------------
// Scan GRASS databases for .gxw workspace files.
// When activeLocation is provided, only that location is scanned.
// Returns a list of absolute paths.
// ---------------------------------------------------------------------------
static QStringList findWorkspaceFiles( const QStringList &databases,
                                       const QString &activeDb = {},
                                       const QString &activeLocation = {} )
{
    QStringList found;
    QStringList visited;

    for ( const QString &db : databases )
    {
        const QString absDb = QDir( db ).absolutePath();
        if ( absDb.isEmpty() || visited.contains( absDb ) ) continue;
        visited << absDb;

        const QDir dbDir( absDb );
        if ( !dbDir.exists() ) continue;

        // If filtering to active location, only scan that one
        QStringList locations;
        if ( !activeLocation.isEmpty() && absDb == QDir( activeDb ).absolutePath() )
            locations << activeLocation;
        else
            locations = dbDir.entryList( QDir::Dirs | QDir::NoDotAndDotDot );

        for ( const QString &loc : std::as_const( locations ) )
        {
            const QDir locDir( absDb + "/" + loc );
            if ( !QFileInfo( locDir.filePath( "PERMANENT/WIND" ) ).exists() )
                continue;  // not a GRASS location

            for ( const QFileInfo &fi : locDir.entryInfoList( { "*.gxw" }, QDir::Files ) )
                found << fi.absoluteFilePath();
        }
    }
    found.sort( Qt::CaseInsensitive );
    return found;
}

// ---------------------------------------------------------------------------
// Create a minimal empty .gxw for a given session.  Used to materialise the
// default auto-save workspace when none exists for the active location.
// ---------------------------------------------------------------------------
static bool createDefaultWorkspace( const QString &path,
                                    const QString &db,
                                    const QString &loc,
                                    const QString &ms )
{
    QFile f( path );
    if ( !f.open( QIODevice::WriteOnly | QIODevice::Text ) )
        return false;
    QTextStream out( &f );
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<!DOCTYPE gxw SYSTEM \"grass-gxw.dtd\">\n"
        << "<gxw>\n"
        << "    <session>\n"
        << "        <database>" << db  << "</database>\n"
        << "        <location>" << loc << "</location>\n"
        << "        <mapset>"   << ms  << "</mapset>\n"
        << "    </session>\n"
        << "    <display name=\"Map Display 1\" render=\"1\" mode=\"0\" "
        << "showCompExtent=\"1\" alignExtent=\"1\" constrainRes=\"0\" "
        << "showStatusbar=\"1\" showToolbars=\"1\" isDocked=\"1\" "
        << "extent=\"0,0,0,0,0,1\" viewMode=\"2d\">\n"
        << "    </display>\n"
        << "</gxw>\n";
    return true;
}

// ---------------------------------------------------------------------------
GrassStartupDialog::GrassStartupDialog( GrassEnv *env, QWidget *parent )
    : QDialog( parent )
    , mEnv( env )
{
    setWindowTitle( "qtgrass — Start" );
    setMinimumSize( 520, 440 );

    QVBoxLayout *lay = new QVBoxLayout( this );
    lay->setSpacing( 12 );

    // Header
    QLabel *title = new QLabel(
        QString( "<h2>GRASS GIS — qtgrass</h2>"
                 "<small style='color:#888;'>GRASS %1 &nbsp;·&nbsp; qtgrass %2</small>" )
            .arg( GRASS_VERSION_STRING, QTGRASS_VERSION ),
        this );
    title->setAlignment( Qt::AlignCenter );
    lay->addWidget( title );

    // Detect whether we were launched from inside a GRASS session
    const bool hasSession = !env->gisDatabase().isEmpty()
                         && !env->location().isEmpty()
                         && !env->mapset().isEmpty();

    if ( hasSession )
    {
        QLabel *sessLbl = new QLabel(
            QString( "<small>Session: <b>%1 / %2 / %3</b></small>" )
                .arg( env->gisDatabase(), env->location(), env->mapset() ),
            this );
        sessLbl->setAlignment( Qt::AlignCenter );
        lay->addWidget( sessLbl );
    }

    // ── Workspace list ────────────────────────────────────────────────────
    // When a GRASS session is active, only list .gxw files from that location.
    // When no session, scan ~/grassdata + any stored database.
    QStringList databases;
    if ( hasSession )
    {
        databases << env->gisDatabase();
    }
    else
    {
        databases << QDir::homePath() + "/grassdata";
        const QString savedDb = QSettings().value( "session/gisdbase" ).toString();
        if ( !savedDb.isEmpty() && !databases.contains( savedDb ) )
            databases << savedDb;
    }

    // Auto-create default.gxw / default_<mapset>.gxw when missing
    QString highlightPath;
    if ( hasSession )
    {
        const QString locDir = env->gisDatabase() + "/" + env->location();
        const QString defFile = ( env->mapset() == "PERMANENT" )
                                ? "default.gxw"
                                : "default_" + env->mapset() + ".gxw";
        const QString defPath = locDir + "/" + defFile;

        // Also check the plain default.gxw as a fallback highlight candidate
        const QString plainDefault = locDir + "/default.gxw";

        if ( !QFile::exists( defPath ) )
            createDefaultWorkspace( defPath, env->gisDatabase(),
                                    env->location(), env->mapset() );

        // Prefer mapset-specific file; fall back to default.gxw
        if ( QFile::exists( defPath ) )
            highlightPath = defPath;
        else if ( QFile::exists( plainDefault ) )
            highlightPath = plainDefault;
    }

    const QStringList gxwFiles = findWorkspaceFiles(
        databases,
        hasSession ? env->gisDatabase() : QString(),
        hasSession ? env->location()    : QString() );

    if ( !gxwFiles.isEmpty() )
    {
        const QString grpTitle = hasSession
            ? QString( "Workspaces in %1" ).arg( env->location() )
            : "Recent workspaces";
        auto *grp    = new QGroupBox( grpTitle, this );
        auto *grpLay = new QVBoxLayout( grp );

        mWorkspaceTable = new QTableWidget( 0, 2, grp );
        mWorkspaceTable->setHorizontalHeaderLabels( { "Location", "Workspace file" } );
        mWorkspaceTable->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
        mWorkspaceTable->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::Stretch );
        mWorkspaceTable->verticalHeader()->setVisible( false );
        mWorkspaceTable->setSelectionBehavior( QAbstractItemView::SelectRows );
        mWorkspaceTable->setSelectionMode( QAbstractItemView::SingleSelection );
        mWorkspaceTable->setEditTriggers( QAbstractItemView::NoEditTriggers );
        mWorkspaceTable->setAlternatingRowColors( true );
        mWorkspaceTable->setShowGrid( false );

        int highlightRow = -1;
        for ( const QString &path : gxwFiles )
        {
            const QFileInfo fi( path );
            const QString loc  = fi.absoluteDir().dirName();
            const int row = mWorkspaceTable->rowCount();
            mWorkspaceTable->insertRow( row );

            auto *locItem  = new QTableWidgetItem( loc );
            auto *fileItem = new QTableWidgetItem( fi.fileName() );
            locItem->setData(  Qt::UserRole, path );
            fileItem->setData( Qt::UserRole, path );
            locItem->setToolTip( path );
            fileItem->setToolTip( path );
            mWorkspaceTable->setItem( row, 0, locItem  );
            mWorkspaceTable->setItem( row, 1, fileItem );

            if ( !highlightPath.isEmpty() && path == highlightPath )
                highlightRow = row;
        }
        mWorkspaceTable->resizeRowsToContents();

        if ( highlightRow >= 0 )
        {
            mWorkspaceTable->selectRow( highlightRow );
            mWorkspaceTable->scrollTo( mWorkspaceTable->model()->index( highlightRow, 0 ) );
        }

        grpLay->addWidget( mWorkspaceTable );

        auto *openWsBtn = new QPushButton( "Open selected workspace", grp );
        grpLay->addWidget( openWsBtn );

        connect( openWsBtn, &QPushButton::clicked,
                 this, &GrassStartupDialog::openSelectedWorkspace );
        connect( mWorkspaceTable, &QTableWidget::cellDoubleClicked,
                 this, [this]( int, int ) { openSelectedWorkspace(); } );

        lay->addWidget( grp, 1 );
    }
    else
    {
        const QString noWsText = hasSession
            ? QString( "No workspace files (.gxw) found in %1/%2." )
                  .arg( env->gisDatabase(), env->location() )
            : "No workspace files (.gxw) found in ~/grassdata.";
        QLabel *noWs = new QLabel( noWsText, this );
        noWs->setAlignment( Qt::AlignCenter );
        noWs->setStyleSheet( "color:#888;" );
        lay->addWidget( noWs );
    }

    // ── Action buttons ────────────────────────────────────────────────────
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing( 12 );

    QPushButton *openBtn = new QPushButton( "Open Existing Mapset…", this );
    openBtn->setMinimumHeight( 40 );
    openBtn->setDefault( gxwFiles.isEmpty() );

    QPushButton *newBtn = new QPushButton( "Create New Location…", this );
    newBtn->setMinimumHeight( 40 );

    btnRow->addWidget( openBtn );
    btnRow->addWidget( newBtn );
    lay->addLayout( btnRow );

    // ── Skip row: two buttons side by side ───────────────────────────────
    QHBoxLayout *skipRow = new QHBoxLayout();
    skipRow->addStretch();

    if ( hasSession )
    {
        // "Loading with Mapset (no Workspace)" — session already valid, just accept
        QPushButton *withMsBtn = new QPushButton(
            QString( "Loading with Mapset (%1) — no Workspace" ).arg( env->mapset() ),
            this );
        withMsBtn->setMinimumHeight( 32 );
        withMsBtn->setToolTip(
            "Continue with the current GRASS session without loading a workspace file.\n"
            "An auto-save workspace (default.gxw / default_<mapset>.gxw) will be used\n"
            "if auto-save is enabled in File → Preferences → Auto-save Workspace." );
        connect( withMsBtn, &QPushButton::clicked, this, &QDialog::accept );
        skipRow->addWidget( withMsBtn );
    }

    QPushButton *skipBtn = new QPushButton( "Skip (no mapset)", this );
    skipBtn->setMinimumHeight( 32 );
    skipRow->addWidget( skipBtn );
    lay->addLayout( skipRow );

    connect( openBtn, &QPushButton::clicked, this, &GrassStartupDialog::openExisting );
    connect( newBtn,  &QPushButton::clicked, this, &GrassStartupDialog::createNew );
    connect( skipBtn, &QPushButton::clicked, this, &QDialog::reject );
}

void GrassStartupDialog::openSelectedWorkspace()
{
    if ( !mWorkspaceTable ) return;
    const int row = mWorkspaceTable->currentRow();
    if ( row < 0 ) return;
    const QTableWidgetItem *item = mWorkspaceTable->item( row, 0 );
    if ( !item ) return;
    const QString path = item->data( Qt::UserRole ).toString();
    if ( path.isEmpty() ) return;
    mSelectedWorkspace = path;
    accept();
}

void GrassStartupDialog::openExisting()
{
    GrassMapsetBrowser dlg( this );
    dlg.setInitial( mEnv->gisDatabase(), mEnv->location(), mEnv->mapset() );

    if ( dlg.exec() != QDialog::Accepted ) return;

    if ( !dlg.selectedWorkspace().isEmpty() )
    {
        mSelectedWorkspace = dlg.selectedWorkspace();
        accept();
        return;
    }

    // If the user confirmed the session that is already active (e.g. pre-filled
    // from a .gxw argument), accept without calling openMapset() again.
    // A second G_setenv_nogisrc() call for identical keys corrupts GRASS's
    // internal heap and causes a segfault later in G_get_set_window().
    if ( dlg.gisDatabase() == mEnv->gisDatabase() &&
         dlg.location()    == mEnv->location()    &&
         dlg.mapset()      == mEnv->mapset()       &&
         mEnv->isValid() )
    {
        accept();
        return;
    }

    if ( !mEnv->openMapset( dlg.gisDatabase(), dlg.location(), dlg.mapset() ) )
    {
        QMessageBox::critical( this, "Error", mEnv->lastError() );
        return;
    }
    accept();
}

void GrassStartupDialog::createNew()
{
    GrassLocationWizard wiz( mEnv, this );
    if ( wiz.exec() != QDialog::Accepted ) return;

    QString err;
    if ( !wiz.createLocation( &err ) )
    {
        QMessageBox::critical( this, "Location creation failed", err );
        return;
    }
    accept();
}
