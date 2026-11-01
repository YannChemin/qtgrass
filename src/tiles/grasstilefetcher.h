#pragma once

/*!
    \file grasstilefetcher.h
    \brief Async HTTP fetcher for XYZ map tiles.
*/

#include "grasstilecache.h"
#include "grasstilekey.h"

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

/*!
    \class GrassTileFetcher
    \brief Fetches XYZ tiles over HTTP and stores them in a GrassTileCache.

    \section fetch_threading Thread requirements

    \b Must live on the main (GUI) thread — owns a \c QNetworkAccessManager.
    Call fetchTile() via Qt::QueuedConnection from render threads.

    \section deduplication Request deduplication

    Calling fetchTile() for a tile that is already in-flight or already cached
    is a no-op.  At most \c maxParallel (default 6) requests are in flight
    simultaneously; additional requests are queued.

    \section signals Signals

    Connect tileFetched() to \c GrassMapCanvas::scheduleRender() so the canvas
    repaints as each tile arrives.

    \sa GrassTileCache, GrassTileBackground
*/
class GrassTileFetcher : public QObject
{
    Q_OBJECT

  public:
    /*!
        \brief Constructs the fetcher for a given URL template.
        \param urlTemplate URL template with \c {x}, \c {y}, \c {z} placeholders,
               e.g. \c "https://tile.openstreetmap.org/{z}/{x}/{y}.png".
        \param cache       Tile cache to check before fetching and to store into on arrival.
        \param parent      Parent QObject (must be on the main thread).
    */
    explicit GrassTileFetcher( const QString &urlTemplate,
                                GrassTileCache *cache,
                                QObject *parent = nullptr );

    /*!
        \brief Set the maximum number of simultaneous HTTP requests.
        \param n Parallel request limit (default 6).
    */
    void setMaxParallel( int n ) { mMaxParallel = n; }

    /*!
        \brief Switch to a different tile provider.

        Cancels all in-flight requests and clears the memory cache.
        \param urlTemplate New URL template.
    */
    void setUrlTemplate( const QString &urlTemplate );

    /*! \brief Returns the current URL template. */
    QString urlTemplate() const { return mUrlTemplate; }

  public slots:
    /*!
        \brief Request a tile, fetching it from the network if not cached.

        Safe to call via \c Qt::QueuedConnection from the render thread.
        Duplicate requests (same tile already in-flight or cached) are silently ignored.

        \param z Zoom level.
        \param x Tile column.
        \param y Tile row.
    */
    void fetchTile( int z, int x, int y );

  signals:
    /*!
        \brief Emitted when a tile has been downloaded and stored in the cache.
        \param key Coordinates of the tile that arrived.
    */
    void tileFetched( GrassTileKey key );

  private slots:
    void onReplyFinished();

  private:
    QString buildUrl( const GrassTileKey &key ) const;

    // Log the first error from a host as a Warning; subsequent ones at Debug
    // to avoid spamming stderr when a provider blocks requests (e.g. Google
    // Maps without an API key returning empty responses).
    void logTileError( const QString &host, const QString &reason );

    static constexpr int kErrorSummaryThreshold = 10;

    QNetworkAccessManager                *mNam;
    GrassTileCache                       *mCache;
    QString                               mUrlTemplate;
    QHash<GrassTileKey, QNetworkReply *>  mPending;
    int                                   mMaxParallel  = 6;
    int                                   mErrorCount   = 0;
    QString                               mLastErrorHost;
};
