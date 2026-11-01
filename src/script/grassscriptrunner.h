#pragma once

#include <QDialog>
#include <QStringList>

class QPlainTextEdit;
class QLabel;
class QProcess;

// Batch script runner: paste GRASS commands one per line, run them
// sequentially with streaming output to the log pane.
// Lines starting with '#' are treated as comments and skipped.
class GrassScriptRunner : public QDialog
{
    Q_OBJECT

  public:
    explicit GrassScriptRunner( QWidget *parent = nullptr );

    // Pre-fill with a list of commands (e.g. session history)
    void setCommands( const QStringList &cmds );

  private slots:
    void run();
    void stop();
    void onProcessOutput();
    void onProcessFinished( int exitCode, int status );

  private:
    bool runNext();
    QString buildEnvPath() const;

    QPlainTextEdit *mEditor  = nullptr;
    QPlainTextEdit *mLog     = nullptr;
    QLabel         *mStatus  = nullptr;

    QProcess    *mProcess    = nullptr;
    QStringList  mQueue;    // remaining commands to run
    int          mTotal  = 0;
    int          mDone   = 0;
};
