#pragma once

/*!
    \file grasstilecache.h
    \brief Thread-safe in-memory and on-disk tile image cache.
*/

#include "grasstilekey.h"

#include <QHash>
#include <QImage>
#include <QReadWriteLock>
#include <QString>

/*!
    \class GrassTileCache
    \brief Thread-safe in-memory and on-disk cache for XYZ tile images.

    \section cache_threading Thread safety

    \li get() — safe to call from any thread (acquires a read lock).
    \li put() — safe to call from any thread (acquires a write lock).
    \li clearMemory() — call from the main thread only.

    \section disk_layout Disk layout

    Tiles are stored at:
    \code
    ~/.cache/qtgrass/tiles/<providerId>/<z>/<x>/<y>.jpg
    \endcode

    The disk cache persists across application restarts and is shared between
    provider switches with the same \a providerId.

    \sa GrassTileFetcher, GrassTileKey
*/
class GrassTileCache
{
  public:
    /*!
        \brief Constructs a cache for a given tile provider.
        \param providerId    Provider identifier used as the disk cache subdirectory.
        \param maxMemoryTiles Maximum number of tile images to keep in memory (LRU eviction).
    */
    explicit GrassTileCache( const QString &providerId,
                              int maxMemoryTiles = 512 );

    /*!
        \brief Look up a tile.
        \param key Tile coordinates.
        \return The cached QImage, or a null QImage if the tile is not cached.
    */
    QImage get( const GrassTileKey &key ) const;

    /*!
        \brief Store a tile in memory and write it to disk.
        \param key Tile coordinates.
        \param img Tile image to cache.
    */
    void put( const GrassTileKey &key, const QImage &img );

    /*!
        \brief Discard all in-memory tiles (disk cache is untouched).
    */
    void clearMemory();

    /*!
        \brief Delete all cached tile files on disk for this provider.
    */
    void clearDisk();

    /*!
        \brief Change the active provider and update the disk cache subdirectory.
        \param id New provider identifier.
    */
    void setProviderId( const QString &id );

    /*! \brief Returns the current provider identifier. */
    QString providerId() const { return mProviderId; }

  private:
    QImage  loadFromDisk( const GrassTileKey &key ) const;
    void    saveToDisk  ( const GrassTileKey &key, const QImage &img ) const;
    QString diskPath    ( const GrassTileKey &key ) const;

    mutable QReadWriteLock              mLock;
    mutable QHash<GrassTileKey, QImage> mMemory;
    int                                 mMaxMemory;
    QString                             mProviderId;
    QString                             mCacheRoot;
};
