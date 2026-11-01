#include "grassdisplaydriver.h"

#include <QDir>
#include <QFile>
#include <QMutexLocker>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcDriver, "qtgrass.canvas.driver")

GrassDisplayDriver::GrassDisplayDriver( QObject *parent )
    : QThread( parent )
{
    // Shared temp file for all renders (overwritten each time)
    mTmpFile = QStandardPaths::writableLocation( QStandardPaths::TempLocation )
               + "/qtgrass_display.png";
}

GrassDisplayDriver::~GrassDisplayDriver()
{
    cancel();
    wait( 5000 );
    QFile::remove( mTmpFile );
}

// ---------------------------------------------------------------------------
// startRender — store request; (re)start thread if idle
// ---------------------------------------------------------------------------
void GrassDisplayDriver::startRender( const QList<GrassDisplayCmd> &cmds,
                                       int width, int height )
{
    {
        QMutexLocker lk( &mMutex );
        mPending = cmds;
        mWidth   = width;
        mHeight  = height;
        mAbort   = false;
    }

    if ( !isRunning() )
        start( QThread::LowPriority );
    // If already running, it will pick up mPending after finishing the current frame
}

void GrassDisplayDriver::cancel()
{
    QMutexLocker lk( &mMutex );
    mAbort = true;
    mPending.clear();
}

// ---------------------------------------------------------------------------
// run — background rendering loop
// ---------------------------------------------------------------------------
void GrassDisplayDriver::run()
{
    // Loop: pick up the latest pending request, render it; repeat until nothing left
    for (;;)
    {
        {
            QMutexLocker lk( &mMutex );
            if ( mPending.isEmpty() || mAbort ) return;
            mCurrent = mPending;
            mCurW    = mWidth;
            mCurH    = mHeight;
            mPending.clear();
        }

        if ( mCurW <= 0 || mCurH <= 0 || mCurrent.isEmpty() ) continue;

        const QString gb = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
        if ( gb.isEmpty() )
        {
            emit renderError( "GISBASE not set" );
            return;
        }

        // Build base environment: inherit current + GRASS display vars
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert( "GRASS_RENDER_IMMEDIATE",  "cairo" );
        env.insert( "GRASS_RENDER_FILE",       mTmpFile );
        env.insert( "GRASS_RENDER_WIDTH",      QString::number( mCurW ) );
        env.insert( "GRASS_RENDER_HEIGHT",     QString::number( mCurH ) );
        env.insert( "GRASS_RENDER_FILE_COMPRESSION", "0" );

        // Extend PATH so d.* scripts can find each other
        const QString oldPath = env.value( "PATH" );
        env.insert( "PATH", gb + "/bin:" + gb + "/scripts:" + oldPath );

        // Filter enabled commands
        QList<GrassDisplayCmd> active;
        for ( const GrassDisplayCmd &c : mCurrent )
            if ( c.enabled ) active.append( c );

        if ( active.isEmpty() ) continue;

        bool firstLayer = true;
        int done = 0;
        emit progressStep( 0, active.size() );

        for ( const GrassDisplayCmd &cmd : active )
        {
            {
                QMutexLocker lk( &mMutex );
                if ( mAbort || !mPending.isEmpty() ) goto next_request;
            }

            {
                env.insert( "GRASS_RENDER_FILE_READ",
                            firstLayer ? "FALSE" : "TRUE" );
                firstLayer = false;

                const QString prog = cmd.prog.contains( '/' )
                                     ? cmd.prog
                                     : gb + "/bin/" + cmd.prog;

                QProcess proc;
                proc.setProcessEnvironment( env );
                proc.setProcessChannelMode( QProcess::MergedChannels );
                proc.start( prog, cmd.args );

                if ( !proc.waitForFinished( 30000 ) )
                {
                    qCWarning(lcDriver) << "GrassDisplayDriver:" << cmd.prog << "timed out";
                    proc.kill();
                }
                else if ( proc.exitCode() != 0 )
                {
                    qCWarning(lcDriver) << "GrassDisplayDriver:" << cmd.prog
                               << "exit" << proc.exitCode()
                               << QString::fromLocal8Bit( proc.readAll() );
                }

                emit progressStep( ++done, active.size() );
            }
        }

        {
            // Load result into QImage
            QImage img( mTmpFile );
            if ( img.isNull() )
                emit renderError( "Display driver produced no image" );
            else
                emit renderComplete( img );
        }

        next_request:;
    }
}
