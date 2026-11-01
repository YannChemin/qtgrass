#pragma once

#include "grassdisplaycmd.h"
#include "grassmaplayer.h"

#include <QThread>
#include <QImage>
#include <QMutex>
#include <QList>

// Asynchronous GRASS display-driver renderer.
//
// Uses the GRASS Cairo display driver:
//   GRASS_RENDER_IMMEDIATE=cairo
//   GRASS_RENDER_FILE=<tmp.png>
//   GRASS_RENDER_WIDTH/HEIGHT=<w>/<h>
//   GRASS_RENDER_FILE_READ=FALSE  (first layer — clears canvas)
//   GRASS_RENDER_FILE_READ=TRUE   (subsequent layers — composite over)
//
// Runs each d.* command in sequence in a background thread.
// Emits renderComplete(QImage) when all commands finish.
// Emits renderError(QString) on failure.
//
// Thread-safety: startRender() may be called from the GUI thread;
// internally the QThread runs the subprocess chain.
class GrassDisplayDriver : public QThread
{
    Q_OBJECT

  public:
    explicit GrassDisplayDriver( QObject *parent = nullptr );
    ~GrassDisplayDriver() override;

    // Enqueue a new render request.  If a render is already in progress
    // the new request is queued and starts as soon as the current one ends.
    void startRender( const QList<GrassDisplayCmd> &cmds,
                      int width, int height );

    // Cancel the current render (best-effort).
    void cancel();

    bool isRendering() const { return isRunning(); }

  signals:
    void renderComplete( const QImage &img );
    void renderError(    const QString &msg );
    void progressStep(   int done, int total );  // for status bar

  protected:
    void run() override;

  private:
    QMutex               mMutex;
    QList<GrassDisplayCmd> mPending;   // next request waiting
    int                  mWidth  = 0;
    int                  mHeight = 0;
    bool                 mAbort  = false;

    // Current active render state (only accessed inside run())
    QList<GrassDisplayCmd> mCurrent;
    int                  mCurW = 0, mCurH = 0;

    QString              mTmpFile;   // path to the shared PNG
};
