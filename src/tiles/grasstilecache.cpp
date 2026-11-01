#include "grasstilecache.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QReadLocker>
#include <QWriteLocker>
#include <QDebug>

GrassTileCache::GrassTileCache( const QString &providerId, int maxMemoryTiles )
    : mMaxMemory( maxMemoryTiles )
{
    mCacheRoot = QStandardPaths::writableLocation( QStandardPaths::CacheLocation )
                 + "/tiles";
    setProviderId( providerId );
}

void GrassTileCache::setProviderId( const QString &id )
{
    QWriteLocker lk( &mLock );
    mProviderId = id;
    mMemory.clear();
}

QImage GrassTileCache::get( const GrassTileKey &key ) const
{
    {
        QReadLocker lk( &mLock );
        const auto it = mMemory.find( key );
        if ( it != mMemory.end() ) return it.value();
    }

    // Try disk
    QImage img = loadFromDisk( key );
    if ( !img.isNull() )
    {
        QWriteLocker lk( &mLock );
        // Evict oldest entry if at capacity (simple FIFO approximation)
        if ( mMemory.size() >= mMaxMemory )
        {
            const auto oldest = mMemory.begin();
            mMemory.erase( oldest );
        }
        mMemory.insert( key, img );
    }
    return img;
}

void GrassTileCache::put( const GrassTileKey &key, const QImage &img )
{
    if ( img.isNull() ) return;

    {
        QWriteLocker lk( &mLock );
        if ( mMemory.size() >= mMaxMemory )
            mMemory.erase( mMemory.begin() );
        mMemory.insert( key, img );
    }
    saveToDisk( key, img );
}

void GrassTileCache::clearMemory()
{
    QWriteLocker lk( &mLock );
    mMemory.clear();
}

void GrassTileCache::clearDisk()
{
    const QString dir = mCacheRoot + "/" + mProviderId;
    QDir( dir ).removeRecursively();
}

QString GrassTileCache::diskPath( const GrassTileKey &key ) const
{
    return QString( "%1/%2/%3/%4/%5.jpg" )
           .arg( mCacheRoot, mProviderId )
           .arg( key.z ).arg( key.x ).arg( key.y );
}

QImage GrassTileCache::loadFromDisk( const GrassTileKey &key ) const
{
    const QString path = diskPath( key );
    QImage img;
    img.load( path );
    return img;
}

void GrassTileCache::saveToDisk( const GrassTileKey &key, const QImage &img ) const
{
    const QString path = diskPath( key );
    QDir().mkpath( QFileInfo( path ).absolutePath() );
    img.save( path, "JPEG", 85 );
}
