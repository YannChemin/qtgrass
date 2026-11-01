#include "grassenv.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QLoggingCategory>

#include <proj.h>
#include <cmath>

extern "C" {
#include <setjmp.h>
#include <grass/gis.h>
#include <grass/version.h>

Q_LOGGING_CATEGORY(lcEnv, "qtgrass.env")
}

// ---------------------------------------------------------------------------
// repairProjInfo — same logic as fixupProjInfo in grasslocationwizard.cpp.
// Called from openMapset() so that pre-existing locations created without
// the fixup (e.g. MarsMercator) are transparently repaired on first open.
// ---------------------------------------------------------------------------
static void repairProjInfo( const QString &permanentDir )
{
    const QString projInfoPath = permanentDir + "/PROJ_INFO";
    const QString projSridPath = permanentDir + "/PROJ_SRID";
    if ( !QFile::exists( projInfoPath ) ) return;

    // Check whether a: and es: are already present
    QFile f( projInfoPath );
    if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) ) return;
    QStringList lines;
    bool hasA = false, hasEs = false;
    double existingR = 0.0;
    while ( !f.atEnd() )
    {
        const QString line = f.readLine().trimmed();
        if ( line.startsWith("a:")  ) hasA  = true;
        if ( line.startsWith("es:") ) hasEs = true;
        if ( line.startsWith("R:")  ) existingR = line.mid(2).trimmed().toDouble();
        lines << line;
    }
    f.close();
    if ( hasA && hasEs ) return;  // already complete

    // Determine the SRID to look up: prefer PROJ_SRID file, else fall back
    // to rebuilding from PROJ_WKT if it exists.
    QString srid;
    {
        QFile sf( projSridPath );
        if ( sf.open( QIODevice::ReadOnly | QIODevice::Text ) )
            srid = sf.readLine().trimmed();
    }

    double semiMajor = existingR;
    double es        = 0.0;

    PJ_CONTEXT *ctx = proj_context_create();
    PJ *crs = nullptr;

    if ( !srid.isEmpty() )
    {
        crs = proj_create( ctx, srid.toUtf8().constData() );
        if ( !crs )
        {
            const int colon = srid.indexOf(':');
            if ( colon > 0 )
                crs = proj_create_from_database(
                    ctx,
                    srid.left(colon).toUtf8().constData(),
                    srid.mid(colon + 1).toUtf8().constData(),
                    PJ_CATEGORY_CRS, 0, nullptr );
        }
    }

    // Fall back to WKT if SRID lookup failed
    if ( !crs )
    {
        QFile wf( permanentDir + "/PROJ_WKT" );
        if ( wf.open( QIODevice::ReadOnly | QIODevice::Text ) )
        {
            const QByteArray wkt = wf.readAll();
            crs = proj_create_from_wkt( ctx, wkt.constData(), nullptr, nullptr, nullptr );
        }
    }

    if ( crs )
    {
        PJ *ell = proj_get_ellipsoid( ctx, crs );
        if ( ell )
        {
            double a = 0.0, b = 0.0, invF = 0.0;
            int semiMinorComputed = 0;
            if ( proj_ellipsoid_get_parameters( ctx, ell, &a, &b,
                                                &semiMinorComputed, &invF ) && a > 0.0 )
            {
                semiMajor = a;
                es = ( b > 0.0 ) ? 1.0 - (b / a) * (b / a) : 0.0;
            }
            proj_destroy( ell );
        }
        proj_destroy( crs );
    }
    proj_context_destroy( ctx );

    if ( semiMajor <= 0.0 )
    {
        qCWarning(lcEnv) << "repairProjInfo: cannot determine semi-major for" << projInfoPath;
        return;
    }

    // Rewrite PROJ_INFO with a: and es: inserted after the proj: line
    QFile out( projInfoPath );
    if ( !out.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate ) )
        return;
    QTextStream ts( &out );
    bool injected = false;
    for ( const QString &line : std::as_const(lines) )
    {
        if ( line.startsWith("a:") || line.startsWith("es:") ) continue;
        ts << line << "\n";
        if ( !injected && line.startsWith("proj:") )
        {
            ts << "a: "  << QString::number( semiMajor, 'f', 0 ) << "\n";
            ts << "es: " << QString::number( es, 'g', 15 )       << "\n";
            injected = true;
        }
    }
    if ( !injected )
    {
        ts << "a: "  << QString::number( semiMajor, 'f', 0 ) << "\n";
        ts << "es: " << QString::number( es, 'g', 15 )       << "\n";
    }
    qCInfo(lcEnv) << "repairProjInfo: fixed" << projInfoPath
                  << "a=" << semiMajor << "es=" << es;
}

GrassEnv *GrassEnv::sInstance = nullptr;

GrassEnv *GrassEnv::instance()
{
    if ( !sInstance )
        sInstance = new GrassEnv();
    return sInstance;
}

GrassEnv::GrassEnv( QObject *parent )
    : QObject( parent )
{}

bool GrassEnv::init()
{
    mValid = false;
    mError.clear();

    // Determine GISBASE: env var takes priority, then the compile-time default
    // baked in at build time (QTGRASS_DEFAULT_GISBASE = GRASS_PREFIX).
    const char *gb = getenv( "GISBASE" );
    if ( !gb || !*gb )
    {
#ifdef QTGRASS_DEFAULT_GISBASE
        const char *defGisbase = QTGRASS_DEFAULT_GISBASE;
        if ( defGisbase && *defGisbase && QDir( QString::fromUtf8(defGisbase) ).exists() )
        {
            qputenv( "GISBASE", defGisbase );
            gb = getenv( "GISBASE" );
            qCDebug(lcEnv) << "GISBASE not set — using build-time default:" << defGisbase;
        }
#endif
        if ( !gb || !*gb )
        {
            mError = "GISBASE is not set. "
                     "Run qtgrass from inside a GRASS session, or set GISBASE manually.";
            return false;
        }
    }
    mGisBase = QString::fromUtf8( gb );

    // Verify the installed header version matches the runtime library.
    // G_no_gisinit() will call G_fatal_error() if they differ, which by
    // default calls exit(). We don't try to catch that here; it's the same
    // behaviour QGIS uses.
    G_set_gisrc_mode( G_GISRC_MODE_MEMORY );
    G_no_gisinit();

    // --- Discover the active GRASS session ---
    //
    // Priority 1: $GISRC file  (set by "grass --exec" and interactive GRASS
    //             shells). GRASS stores GISDBASE/LOCATION_NAME/MAPSET there,
    //             NOT as individual env vars.  Format: "KEY: VALUE\n".
    if ( const char *gisrcPath = getenv( "GISRC" ) )
    {
        QFile f( QString::fromLocal8Bit( gisrcPath ) );
        if ( f.open( QIODevice::ReadOnly | QIODevice::Text ) )
        {
            QTextStream in( &f );
            while ( !in.atEnd() )
            {
                const QString line = in.readLine();
                const int colon = line.indexOf( ':' );
                if ( colon < 0 ) continue;
                const QString key = line.left( colon ).trimmed();
                const QString val = line.mid( colon + 1 ).trimmed();
                if      ( key == "GISDBASE" )      mGisDatabase = val;
                else if ( key == "LOCATION_NAME" ) mLocation    = val;
                else if ( key == "MAPSET" )        mMapset      = val;
            }
        }
    }

    // Priority 2: individual env vars (set by the --qtui hook in parser.c,
    //             or when the caller exports them explicitly). These override
    //             the GISRC file values when both are present.
    if ( const char *v = getenv( "GISDBASE" ) )      if ( *v ) mGisDatabase = QString::fromUtf8( v );
    if ( const char *v = getenv( "LOCATION_NAME" ) ) if ( *v ) mLocation    = QString::fromUtf8( v );
    if ( const char *v = getenv( "MAPSET" ) )        if ( *v ) mMapset      = QString::fromUtf8( v );

    // Push the discovered session into GRASS's in-memory state so any GRASS
    // C API call (G_mapset, G_get_window, Rast_open_old …) works immediately.
    // Without this, G_mapset() fires G_fatal_error("MAPSET is not set").
    if ( !mGisDatabase.isEmpty() && !mLocation.isEmpty() && !mMapset.isEmpty() )
    {
        G_setenv_nogisrc( "GISDBASE",      mGisDatabase.toUtf8().constData() );
        G_setenv_nogisrc( "LOCATION_NAME", mLocation.toUtf8().constData() );
        G_setenv_nogisrc( "MAPSET",        mMapset.toUtf8().constData() );

        // Mirror into the real OS environment so child processes (g.list
        // in GrassDataCatalog, module dialogs, etc.) inherit the session
        // even when there is no $GISRC file (e.g. desktop or .gxw launch).
        qputenv( "GISDBASE",      mGisDatabase.toUtf8() );
        qputenv( "LOCATION_NAME", mLocation.toUtf8() );
        qputenv( "MAPSET",        mMapset.toUtf8() );
    }

    mValid = true;
    qCInfo(lcEnv) << "GrassEnv: GISBASE=" << mGisBase
            << "GISDBASE=" << mGisDatabase
            << "LOCATION=" << mLocation
            << "MAPSET=" << mMapset;
    return true;
}

bool GrassEnv::openMapset( const QString &gisDatabase,
                            const QString &location,
                            const QString &mapset )
{
    // Validate paths exist
    QDir dir( gisDatabase + "/" + location + "/" + mapset );
    if ( !dir.exists() )
    {
        mError = QString( "Mapset not found: %1" ).arg( dir.absolutePath() );
        return false;
    }

    // Transparently repair PROJ_INFO if a:/es: are missing (spherical IAU CRS
    // created before this fix).  Safe to call every time — returns immediately
    // when the file is already complete.
    repairProjInfo( gisDatabase + "/" + location + "/PERMANENT" );

    // Update in-memory GRASS env vars (used by the GRASS C API in this process)
    G_setenv_nogisrc( "GISDBASE",      gisDatabase.toUtf8().constData() );
    G_setenv_nogisrc( "LOCATION_NAME", location.toUtf8().constData() );
    G_setenv_nogisrc( "MAPSET",        mapset.toUtf8().constData() );

    // Mirror session into the real OS environment so that child processes
    // (g.list in the data catalog, module dialogs, etc.) inherit the session.
    // G_setenv_nogisrc() only updates GRASS's in-memory GISRC — it does NOT
    // affect the process environment that QProcess children read.
    qputenv( "GISDBASE",      gisDatabase.toUtf8() );
    qputenv( "LOCATION_NAME", location.toUtf8() );
    qputenv( "MAPSET",        mapset.toUtf8() );

    // Re-read the new mapset's WIND file and activate it as the computational
    // region.  G_get_window() caches the window in G__.window; G_unset_window()
    // clears the cache so the next G_get_window() reads from the new location's
    // WIND file rather than returning the stale previous session's region.
    // Without this, G_projection() / savedRegion.proj stay at the old location's
    // projection type, causing Rast_set_window() to validate UTM coords as lat/lon.
    //
    // Use FATAL_RETURN so that unusual CRS (e.g. Mercator on Mars) don't call
    // exit() — G_fatal_error returns instead of terminating the process.
    {
        // Arm G_fatal_longjmp so that unusual CRS (e.g. Mercator on Mars) that
        // trigger G_fatal_error() inside G_get_window/G_set_window longjmp here
        // instead of calling exit(), keeping the application alive.
        struct Cell_head window = {};
        if ( setjmp( *G_fatal_longjmp(1) ) == 0 )
        {
            G_unset_window();
            G_get_window( &window );
            G_set_window( &window );
        }
        else
        {
            qCWarning(lcEnv) << "G_get/set_window fatal for" << location << mapset
                             << "— region not updated, session still set";
        }
        G_fatal_longjmp( 0 );  // disarm
    }

    mGisDatabase = gisDatabase;
    mLocation    = location;
    mMapset      = mapset;

    emit mapsetChanged( mGisDatabase, mLocation, mMapset );
    return true;
}

QProcessEnvironment GrassEnv::buildSubprocessEnv() const
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // Write a temporary GISRC file so child processes can call G_gisinit().
    // qtgrass uses G_GISRC_MODE_MEMORY (no GISRC file); subprocesses need the file.
    if ( !mGisDatabase.isEmpty() && !mLocation.isEmpty() && !mMapset.isEmpty() )
    {
        const QString gisrcPath = QDir::tempPath()
            + QString( "/qtgrass_%1.gisrc" ).arg( QCoreApplication::applicationPid() );
        QFile f( gisrcPath );
        if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) )
        {
            QTextStream ts( &f );
            ts << "GISDBASE: "      << mGisDatabase << "\n";
            ts << "LOCATION_NAME: " << mLocation    << "\n";
            ts << "MAPSET: "        << mMapset       << "\n";
            ts << "GUI: text\n";
            f.close();
            env.insert( "GISRC", gisrcPath );
        }
    }

    // Ensure GISBASE and library paths are propagated.
    if ( !mGisBase.isEmpty() )
    {
        env.insert( "GISBASE", mGisBase );

        const QString oldPath = env.value( "PATH" );
        env.insert( "PATH", mGisBase + "/bin:" + mGisBase + "/scripts:" + oldPath );

        const QString oldLd = env.value( "LD_LIBRARY_PATH" );
        env.insert( "LD_LIBRARY_PATH",
                    mGisBase + "/lib" + ( oldLd.isEmpty() ? "" : ":" + oldLd ) );

        // GRASS Python packages (grass.script, grass.pygrass, …) live in
        // $GISBASE/etc/python.  Without this, Python-based modules such as
        // i.ai, r.learn.*, v.class.ml etc. fail with
        //   ModuleNotFoundError: No module named 'grass'
        const QString grassPy  = mGisBase + "/etc/python";
        const QString oldPyPath = env.value( "PYTHONPATH" );
        env.insert( "PYTHONPATH",
                    grassPy + ( oldPyPath.isEmpty() ? "" : ":" + oldPyPath ) );
    }

    // Mirror the session vars so any code reading env vars also works.
    env.insert( "GISDBASE",      mGisDatabase );
    env.insert( "LOCATION_NAME", mLocation    );
    env.insert( "MAPSET",        mMapset       );

    // Python for script modules
    if ( env.value( "GRASS_PYTHON" ).isEmpty() )
    {
        const QString py = QStandardPaths::findExecutable( "python3" );
        if ( !py.isEmpty() )
            env.insert( "GRASS_PYTHON", py );
    }

    return env;
}
