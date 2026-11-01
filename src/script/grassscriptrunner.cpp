#include "../grassenv.h"
#include "grassscriptrunner.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QFont>
#include <QDebug>

GrassScriptRunner::GrassScriptRunner( QWidget *parent )
    : QDialog( parent )
{
    setWindowTitle( "GRASS Script Runner" );
    setMinimumSize( 680, 520 );

    QVBoxLayout *root = new QVBoxLayout( this );

    // Hint label
    QLabel *hint = new QLabel(
        "Enter GRASS commands, one per line. Lines starting with # are comments.", this );
    hint->setStyleSheet( "color:#888; font-style:italic;" );
    root->addWidget( hint );

    // Editor + log splitter
    QSplitter *split = new QSplitter( Qt::Vertical, this );

    mEditor = new QPlainTextEdit( split );
    mEditor->setFont( QFont( "Monospace", 10 ) );
    mEditor->setStyleSheet( "background:#1e1e1e; color:#d4d4d4;" );
    mEditor->setPlaceholderText(
        "# Example:\n"
        "r.slope.aspect elevation=dem slope=slope aspect=aspect\n"
        "r.colors map=slope color=slope\n"
        "r.colors map=aspect color=aspectcolr\n" );

    mLog = new QPlainTextEdit( split );
    mLog->setReadOnly( true );
    mLog->setFont( QFont( "Monospace", 9 ) );
    mLog->setStyleSheet( "background:#0d1117; color:#8b949e;" );
    mLog->setMaximumBlockCount( 5000 );

    split->setStretchFactor( 0, 2 );
    split->setStretchFactor( 1, 1 );
    root->addWidget( split, 1 );

    // Status + progress
    mStatus = new QLabel( " Ready. ", this );
    root->addWidget( mStatus );

    // Buttons
    QDialogButtonBox *btns = new QDialogButtonBox( this );
    QPushButton *runBtn  = btns->addButton( "Run All",  QDialogButtonBox::ApplyRole );
    QPushButton *stopBtn = btns->addButton( "Stop",     QDialogButtonBox::ResetRole );
    btns->addButton( QDialogButtonBox::Close );
    stopBtn->setEnabled( false );
    root->addWidget( btns );

    connect( runBtn,  &QPushButton::clicked, this, &GrassScriptRunner::run );
    connect( stopBtn, &QPushButton::clicked, this, &GrassScriptRunner::stop );
    connect( btns, &QDialogButtonBox::rejected, this, [this] {
        stop(); accept();
    } );

    // Enable/disable stop button with process state
    connect( this, &GrassScriptRunner::accepted, stopBtn, [stopBtn]{ stopBtn->setEnabled(false); } );
    // (run sets it enabled, finished sets it disabled — done via mStatus updates)
    Q_UNUSED( stopBtn );
}

void GrassScriptRunner::setCommands( const QStringList &cmds )
{
    mEditor->setPlainText( cmds.join( '\n' ) );
}

// ---------------------------------------------------------------------------
// run — build queue and start
// ---------------------------------------------------------------------------
void GrassScriptRunner::run()
{
    if ( mProcess && mProcess->state() != QProcess::NotRunning ) return;

    mQueue.clear();
    mLog->clear();

    for ( const QString &line : mEditor->toPlainText().split( '\n', Qt::SkipEmptyParts ) )
    {
        const QString cmd = line.trimmed();
        if ( cmd.isEmpty() || cmd.startsWith( '#' ) ) continue;
        mQueue.append( cmd );
    }

    if ( mQueue.isEmpty() ) { mStatus->setText( "Nothing to run." ); return; }

    mTotal = mQueue.size();
    mDone  = 0;
    runNext();
}

bool GrassScriptRunner::runNext()
{
    if ( mQueue.isEmpty() )
    {
        mStatus->setText( QString( "✓ Finished — %1 command(s) run." ).arg( mTotal ) );
        return false;
    }

    const QString cmd = mQueue.takeFirst();
    ++mDone;
    mStatus->setText( QString( "Running %1/%2: %3" ).arg( mDone ).arg( mTotal ).arg( cmd ) );
    mLog->appendPlainText( QString( "\n$ %1" ).arg( cmd ) );

    const QString gb = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );

    // Split command into argv (naive: split on spaces — fine for GRASS key=val style)
    const QStringList parts = cmd.split( ' ', Qt::SkipEmptyParts );
    if ( parts.isEmpty() ) { runNext(); return true; }

    const QString prog = parts.first().contains( '/' )
                         ? parts.first()
                         : ( gb + "/bin/" + parts.first() );
    const QStringList args = parts.mid( 1 );

    QProcessEnvironment env = GrassEnv::instance()->buildSubprocessEnv();
    const QString oldPath = env.value( "PATH" );
    env.insert( "PATH", gb + "/bin:" + gb + "/scripts:" + oldPath );

    mProcess = new QProcess( this );
    mProcess->setProcessEnvironment( env );
    mProcess->setProcessChannelMode( QProcess::MergedChannels );

    connect( mProcess, &QProcess::readyReadStandardOutput,
             this, &GrassScriptRunner::onProcessOutput );
    connect( mProcess, qOverload<int, QProcess::ExitStatus>( &QProcess::finished ),
             this, &GrassScriptRunner::onProcessFinished );

    mProcess->start( prog, args );
    return true;
}

void GrassScriptRunner::stop()
{
    mQueue.clear();
    if ( mProcess && mProcess->state() != QProcess::NotRunning )
    {
        mProcess->terminate();
        if ( !mProcess->waitForFinished( 2000 ) )
            mProcess->kill();
        mLog->appendPlainText( "\n[Stopped by user]" );
        mStatus->setText( "Stopped." );
    }
}

void GrassScriptRunner::onProcessOutput()
{
    const QByteArray data = mProcess->readAllStandardOutput();
    mLog->appendPlainText( QString::fromLocal8Bit( data ) );
    mLog->ensureCursorVisible();
}

void GrassScriptRunner::onProcessFinished( int exitCode, int )
{
    const QByteArray remaining = mProcess->readAllStandardOutput();
    if ( !remaining.isEmpty() )
        mLog->appendPlainText( QString::fromLocal8Bit( remaining ) );

    if ( exitCode != 0 )
    {
        mLog->appendPlainText( QString( "[exit %1]" ).arg( exitCode ) );
        mQueue.clear();
        mStatus->setText( QString( "✗ Command failed (exit %1). Stopped." ).arg( exitCode ) );
        return;
    }

    mProcess->deleteLater();
    mProcess = nullptr;
    runNext();
}
