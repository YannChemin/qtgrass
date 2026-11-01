#include "grassenv.h"
#include "grassapp.h"
#include "grassmessagebar.h"
#include "module/grassmoduledialog.h"
#include "startup/grassstartupdialog.h"
#include "workspace/grassworkspace.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QLoggingCategory>

// C signal handling for crash diagnosis
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <sys/resource.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>
#include <QTextStream>
#include <QTimer>

Q_LOGGING_CATEGORY(lcApp, "qtgrass.app")

// crashHandler intentionally discards the write() return value — no recovery
// is possible in an async-signal-safe context.  Suppress for this TU.
#pragma GCC diagnostic ignored "-Wunused-result"

// ── Crash signal handler ──────────────────────────────────────────────────────
// Writes a breadcrumb to stderr + log file, then re-raises with the default
// handler so that the kernel writes a core dump for gdb bt.
static void crashHandler( int sig )
{
    // async-signal-safe: write() only.  Return value discarded — in a signal
    // handler there is no meaningful error-recovery path.
    // (void) cast suppresses -Wunused-result on most compilers; the file-scope
    // pragma below handles GCC versions where the cast is insufficient.
    const char *hdr = "\n!!! CRASH: signal ";
    (void)write( STDERR_FILENO, hdr, strlen(hdr) );
    char buf[8]; int n = sig;
    int len = 0;
    do { buf[len++] = '0' + n % 10; n /= 10; } while (n);
    for (int i = len-1; i >= 0; --i) (void)write(STDERR_FILENO, &buf[i], 1);
    const char *trailer = " — see ~/qtgrass-debug.log and core dump\n";
    (void)write( STDERR_FILENO, trailer, strlen(trailer) );

    // Restore default + re-raise so the kernel writes a core dump
    ::signal( sig, SIG_DFL );
    ::raise( sig );
}

// ── Debug log file ────────────────────────────────────────────────────────────
// Written to ~/qtgrass-debug.log when the QTGRASS_DEBUG env var is set,
// or when the binary is built with CMAKE_BUILD_TYPE=Debug.
static QFile   gLogFile;
static bool    gLogEnabled = false;

static void qtMessageHandler( QtMsgType type,
                               const QMessageLogContext &ctx,
                               const QString &msg )
{
    // ── Filter known-harmless Qt-internal noise ────────────────────────────────
    // "cached device pixel ratio value was stale" is an unfixed Qt bug
    // (QTBUG) fired when a HiDPI window is first shown or moved.
    if ( msg.contains( "cached device pixel ratio value was stale" ) )
        return;

    // ── Level filtering ────────────────────────────────────────────────────────
    // In normal (non-debug) mode suppress QtDebugMsg, exactly matching the
    // behaviour of Qt's default handler in a release build.  Debug messages
    // are only shown when QTGRASS_DEBUG=1 or the binary is a Debug build.
    if ( type == QtDebugMsg && !gLogEnabled )
        return;

    // ── Format with category prefix ────────────────────────────────────────────
    // qFormatLogMessage applies the active message pattern (default:
    // "%{if-category}%{category}: %{endif}%{message}"), which restores the
    // "qtgrass.tiles: …" prefix that plain msg.toLocal8Bit() loses.
    const QString formatted = qFormatLogMessage( type, ctx, msg );
    fprintf( stderr, "%s\n", formatted.toLocal8Bit().constData() );

    // ── Write to debug log file ────────────────────────────────────────────────
    if ( gLogEnabled && gLogFile.isOpen() )
    {
        QTextStream s( &gLogFile );
        const char *level = ( type == QtDebugMsg    ) ? "DBG" :
                            ( type == QtInfoMsg      ) ? "INF" :
                            ( type == QtWarningMsg   ) ? "WRN" :
                            ( type == QtCriticalMsg  ) ? "CRT" : "FTL";
        s << QDateTime::currentDateTime().toString( "hh:mm:ss.zzz" )
          << " [" << level << "] "
          << ( ctx.file ? QFileInfo( ctx.file ).fileName() + ":" +
                          QString::number( ctx.line ) + " " : "" )
          << msg << "\n";
    }
}

// QSettings keys
static const char *kGisdb    = "session/gisdbase";
static const char *kLocation = "session/location";
static const char *kMapset   = "session/mapset";

int main( int argc, char *argv[] )
{
    // ── Early logging setup ───────────────────────────────────────────────
    // Activate when QTGRASS_DEBUG is set, or when compiled as Debug build.
#ifdef QT_DEBUG
    gLogEnabled = true;
#else
    gLogEnabled = ( qgetenv( "QTGRASS_DEBUG" ) == "1" );
#endif
    // Register crash handlers before QApplication (catches Qt init crashes)
    ::signal( SIGSEGV, crashHandler );
    ::signal( SIGABRT, crashHandler );
    ::signal( SIGBUS,  crashHandler );

    // Lift the core-dump size limit so gdb can analyse crashes
    {
        struct rlimit rl { RLIM_INFINITY, RLIM_INFINITY };
        setrlimit( RLIMIT_CORE, &rl );
    }

    // Always install our message handler so known-harmless Qt-internal messages
    // (e.g. "cached device pixel ratio stale") are filtered from stderr,
    // regardless of whether debug logging is active.
    qInstallMessageHandler( qtMessageHandler );

    if ( gLogEnabled )
    {
        const QString logPath = QDir::homePath() + "/qtgrass-debug.log";
        gLogFile.setFileName( logPath );
        gLogFile.open( QIODevice::WriteOnly | QIODevice::Text |
                       QIODevice::Append );
        qCInfo(lcApp) << "=== qtgrass debug session" << QDateTime::currentDateTime().toString() << "===";
        qCInfo(lcApp) << "argv[0]:" << ( argc > 0 ? argv[0] : "(none)" );
        qCInfo(lcApp) << "DISPLAY :" << qgetenv( "DISPLAY" );
        qCInfo(lcApp) << "GISBASE :" << qgetenv( "GISBASE" );
        qCInfo(lcApp) << "GISRC   :" << qgetenv( "GISRC" );
        qCInfo(lcApp) << "GISDBASE:" << qgetenv( "GISDBASE" );
        qCInfo(lcApp) << "LOCATION:" << qgetenv( "LOCATION_NAME" );
        qCInfo(lcApp) << "MAPSET  :" << qgetenv( "MAPSET" );
    }

    QApplication app( argc, argv );
    app.setApplicationName( "qtgrass" );
    app.setApplicationVersion( QTGRASS_VERSION );
    app.setOrganizationName( "qtgrass" );

    if ( gLogEnabled ) qCInfo(lcApp) << "QApplication created";

    // Prevent Qt from auto-quitting when the startup dialog closes before the
    // main window is shown.  GrassApp::closeEvent() calls QApplication::quit()
    // explicitly so the X-button still works correctly.
    app.setQuitOnLastWindowClosed( false );

    QCommandLineParser cli;
    cli.setApplicationDescription( "Qt6 GUI for GRASS GIS" );
    cli.addHelpOption();
    cli.addVersionOption();

    QCommandLineOption moduleOpt( "module",
        "Open Qt6 dialog for <module>", "module" );
    cli.addOption( moduleOpt );

    QCommandLineOption qtuiCmdOpt( "qtui-cmd",
        "Full reconstructed GRASS command (set by GRASS parser)", "cmd" );
    cli.addOption( qtuiCmdOpt );

    QCommandLineOption grassDirOpt( "grassdir",
        "Override GISBASE (path to GRASS installation)", "path" );
    cli.addOption( grassDirOpt );

    QCommandLineOption workspaceOpt( "workspace",
        "Open a .gxw workspace file directly (skips startup dialog)", "file" );
    cli.addOption( workspaceOpt );

    cli.addPositionalArgument( "workspace",
        "GRASS workspace file (.gxw) to open on startup", "[file.gxw]" );

    cli.process( app );

    if ( cli.isSet( grassDirOpt ) )
        qputenv( "GISBASE", cli.value( grassDirOpt ).toUtf8() );

    // Detect a .gxw file passed as --workspace or as bare positional arg
    // (the latter is what a file manager sends on double-click).
    QString workspacePath;
    if ( cli.isSet( workspaceOpt ) )
        workspacePath = cli.value( workspaceOpt );
    else
    {
        const QStringList pos = cli.positionalArguments();
        if ( !pos.isEmpty() && pos[0].endsWith( ".gxw", Qt::CaseInsensitive ) )
            workspacePath = pos[0];
    }

    QString moduleName;
    QStringList preArgs;

    if ( cli.isSet( qtuiCmdOpt ) )
    {
        QStringList parts = cli.value( qtuiCmdOpt ).trimmed()
                            .split( ' ', Qt::SkipEmptyParts );
        if ( !parts.isEmpty() )
        {
            moduleName = QFileInfo( parts.takeFirst() ).fileName();
            for ( const QString &p : std::as_const( parts ) )
                if ( p != "--qtui" && p != "--ui" )
                    preArgs << p;
        }
    }
    else if ( cli.isSet( moduleOpt ) )
    {
        moduleName = cli.value( moduleOpt );
        preArgs    = cli.positionalArguments();
    }

    const bool moduleMode = !moduleName.isEmpty();

    if ( gLogEnabled ) qCInfo(lcApp) << "Initialising GrassEnv…";
    GrassEnv *env = GrassEnv::instance();
    if ( !env->init() )
    {
        qCWarning(lcApp) << "GrassEnv::init() failed:" << env->lastError();
        if ( moduleMode )
        {
            QMessageBox::critical( nullptr, "qtgrass — GRASS init failed", env->lastError() );
            return 1;
        }
    }
    if ( gLogEnabled ) qCInfo(lcApp) << "GrassEnv init OK — location:" << env->location();
    if ( gLogEnabled ) qCInfo(lcApp) << "moduleMode=" << moduleMode
                               << " moduleName=" << moduleName
                               << " workspacePath=" << workspacePath;

    if ( moduleMode )
    {
        if ( gLogEnabled ) qCInfo(lcApp) << "Entering module mode:" << moduleName;
        GrassModuleDialog dlg( moduleName, preArgs );
        dlg.exec();
        if ( gLogEnabled ) qCInfo(lcApp) << "Module dialog closed — exiting";
        return 0;
    }

    // --- Full GUI mode — always start fresh (amnesic) ---
    // qtgrass never restores a previous session automatically.  The startup
    // wizard always runs so the user explicitly chooses a project each time.
    //
    // The only exception is an explicit .gxw file supplied as a command-line
    // argument or via file-association double-click: that is a deliberate user
    // action and bypasses the wizard.
    if ( gLogEnabled ) qCInfo(lcApp) << "Entering full-GUI mode";
    QSettings settings;
    QString workspaceToLoad;
    QString gxwSessionWarning;

    if ( !workspacePath.isEmpty() )
    {
        // Explicit .gxw argument — open it directly, no wizard needed.
        workspaceToLoad = workspacePath;
        if ( gLogEnabled ) qCInfo(lcApp) << ".gxw argument:" << workspaceToLoad;

        // Pre-initialise the GRASS session from the file's location in the db
        // so the canvas, catalog and child processes all see a valid session.
        GrassWorkspace ws;
        ws.load( workspacePath );

        QString inferDb, inferLoc, inferMs;
        {
            QDir wsDir = QFileInfo( workspacePath ).absoluteDir();
            for ( int depth = 0; depth < 3 && !wsDir.isRoot(); ++depth )
            {
                if ( QFileInfo( wsDir.filePath( "PERMANENT/WIND" ) ).exists() )
                {
                    inferLoc = wsDir.dirName();
                    wsDir.cdUp();
                    inferDb  = wsDir.absolutePath();
                    inferMs  = ws.mapset().isEmpty() ? "PERMANENT" : ws.mapset();
                    break;
                }
                wsDir.cdUp();
            }
        }

        const QString db  = inferDb.isEmpty()  ? ws.database() : inferDb;
        const QString loc = inferLoc.isEmpty() ? ws.location() : inferLoc;
        const QString ms  = inferMs.isEmpty()  ? ws.mapset()   : inferMs;

        if ( !inferLoc.isEmpty() && !ws.location().isEmpty()
             && ws.location() != inferLoc )
        {
            gxwSessionWarning = QString(
                "Workspace session mismatch: file is inside project '%1' "
                "but its &lt;session&gt; block refers to '%2'. "
                "Using the path-inferred project." )
                .arg( inferLoc, ws.location() );
            qCWarning(lcApp).noquote() << gxwSessionWarning;
        }

        if ( !db.isEmpty() && !loc.isEmpty() && !ms.isEmpty() )
        {
            // Safe to call openMapset() here: env->location() is empty because
            // we have deliberately not restored any previous session above.
            if ( env->openMapset( db, loc, ms ) )
                qCInfo(lcApp) << "Session from .gxw:" << db << loc << ms;
            else
                qCWarning(lcApp) << "openMapset() from .gxw failed:" << env->lastError();
        }
    }
    // (No Priority 2 / Priority 3 — session from GISRC or QSettings is
    //  intentionally ignored so qtgrass always starts fresh.)

    // Create the main window.
    if ( gLogEnabled ) qCInfo(lcApp) << "Creating GrassApp window…";
    GrassApp window( env );

    // Always show the startup wizard unless a .gxw was explicitly supplied.
    if ( workspaceToLoad.isEmpty() )
    {
        GrassStartupDialog startup( env, &window );
        if ( startup.exec() == QDialog::Accepted )
            workspaceToLoad = startup.selectedWorkspace();
        else if ( env->location().isEmpty() )
            qCWarning(lcApp) << "No mapset selected; launching without active session.";
        // else: user clicked Skip but a GRASS session was already active via GISRC —
        // that session remains valid; no warning needed.
    }

    if ( gLogEnabled ) qCInfo(lcApp) << "Calling window.show()…";
    window.show();
    if ( gLogEnabled ) qCInfo(lcApp) << "window.show() returned — entering app.exec()";

    // Load workspace layers after the window is shown (canvas is now sized).
    if ( !workspaceToLoad.isEmpty() )
        QTimer::singleShot( 300, &window, [&window, workspaceToLoad] {
            window.loadWorkspace( workspaceToLoad );
        } );

    // Show session mismatch warning if the .gxw <session> block was stale.
    if ( !gxwSessionWarning.isEmpty() )
        window.messageBar()->pushMessage(
            "Workspace session mismatch", gxwSessionWarning,
            GrassMessageBar::Warning, 0 );   // persistent until dismissed

    if ( env->isValid() && !env->location().isEmpty() )
    {
        window.messageBar()->pushMessage(
            "Ready",
            QString( "Mapset %1 / %2 opened." ).arg( env->location(), env->mapset() ),
            GrassMessageBar::Info, 4 );
    }
    else
    {
        window.messageBar()->pushMessage(
            "No mapset",
            "Use File → Open Mapset to connect to a GRASS project.",
            GrassMessageBar::Warning, 0 );
    }

    if ( gLogEnabled ) qCInfo(lcApp) << "Starting app.exec()";
    const int ret = app.exec();
    if ( gLogEnabled ) qCInfo(lcApp) << "app.exec() returned:" << ret;

    // Remember only the database path so the startup wizard can scan for
    // workspace files next time.  Location and mapset are NOT saved — qtgrass
    // is amnesic of which project was active and always shows the wizard.
    if ( !env->gisDatabase().isEmpty() )
        settings.setValue( kGisdb, env->gisDatabase() );
    settings.remove( kLocation );
    settings.remove( kMapset );

    return ret;
}
