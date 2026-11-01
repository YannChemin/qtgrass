#include "grasstilefetcher.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcTiles, "qtgrass.tiles")

GrassTileFetcher::GrassTileFetcher( const QString &urlTemplate,
                                      GrassTileCache *cache,
                                      QObject *parent )
    : QObject( parent )
    , mNam( new QNetworkAccessManager( this ) )
    , mCache( cache )
    , mUrlTemplate( urlTemplate )
{}

void GrassTileFetcher::setUrlTemplate( const QString &urlTemplate )
{
    for ( QNetworkReply *r : std::as_const( mPending ) )
        r->abort();
    mPending.clear();
    mCache->clearMemory();

    mUrlTemplate   = urlTemplate;
    mErrorCount    = 0;
    mLastErrorHost = {};
}

QString GrassTileFetcher::buildUrl( const GrassTileKey &key ) const
{
    QString url = mUrlTemplate;
    url.replace( "{z}", QString::number( key.z ) );
    url.replace( "{x}", QString::number( key.x ) );
    url.replace( "{y}", QString::number( key.y ) );
    return url;
}

void GrassTileFetcher::fetchTile( int z, int x, int y )
{
    const GrassTileKey key{ z, x, y };

    if ( mPending.contains( key ) )         return;
    if ( !mCache->get( key ).isNull() )     return;
    if ( mPending.size() >= mMaxParallel )  return;

    const QString url = buildUrl( key );
    if ( url.isEmpty() ) return;

    QNetworkRequest req{ QUrl( url ) };
    req.setRawHeader( "User-Agent",
        "Mozilla/5.0 qtgrass/0.1 (compatible; tile client)" );

    QNetworkReply *reply = mNam->get( req );
    reply->setProperty( "tile_z", z );
    reply->setProperty( "tile_x", x );
    reply->setProperty( "tile_y", y );

    connect( reply, &QNetworkReply::finished,
             this, &GrassTileFetcher::onReplyFinished );

    mPending.insert( key, reply );
}

void GrassTileFetcher::onReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>( sender() );
    if ( !reply ) return;

    const GrassTileKey key{
        reply->property( "tile_z" ).toInt(),
        reply->property( "tile_x" ).toInt(),
        reply->property( "tile_y" ).toInt()
    };
    mPending.remove( key );

    if ( reply->error() == QNetworkReply::NoError )
    {
        QImage img;
        if ( img.loadFromData( reply->readAll() ) && !img.isNull() )
        {
            mErrorCount = 0;   // reset error streak on any success
            mCache->put( key, img );
            emit tileFetched( key );
        }
        // else: server returned 200 but empty/invalid body (e.g. Google Maps
        // blocking without API key) — treat the same as an error
        else
        {
            ++mErrorCount;
            logTileError( reply->url().host(),
                          "server returned empty or invalid image data" );
        }
    }
    else
    {
        ++mErrorCount;
        logTileError( reply->url().host(), reply->errorString() );
    }

    reply->deleteLater();
}

void GrassTileFetcher::logTileError( const QString &host,
                                      const QString &reason )
{
    // First failure for this host → warn the user once
    if ( mLastErrorHost != host )
    {
        mLastErrorHost = host;
        mErrorCount    = 1;
        qCWarning(lcTiles) << "Tile fetch failed from" << host
                            << "—" << reason
                            << "(further errors from this host will be suppressed)";
        return;
    }

    // Subsequent failures: log only at debug level so they don't spam stderr
    if ( mErrorCount == kErrorSummaryThreshold )
    {
        qCDebug(lcTiles) << "Tile fetch: +" << mErrorCount
                          << "consecutive errors from" << host
                          << "(is an API key or different provider needed?)";
    }
    else
    {
        qCDebug(lcTiles) << "Tile fetch error from" << host << "—" << reason;
    }
}
