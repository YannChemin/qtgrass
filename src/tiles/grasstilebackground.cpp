#include "grasstilebackground.h"
#include "grasstilecoords.h"

#include <QMetaObject>
#include <QPainter>
#include <QFont>
#include <QPolygonF>
#include <QTransform>
#include <algorithm>
#include <cmath>

#include <proj.h>

extern "C" {
#include <grass/gis.h>
#include <grass/gprojects.h>
#include <cpl_error.h>
}

GrassTileBackground::GrassTileBackground( GrassTileCache   *cache,
                                            GrassTileFetcher *fetcher,
                                            bool              isGeographic,
                                            const QString    &attribution )
    : GrassMapLayer( "__tile_background__" )
    , mCache( cache )
    , mFetcher( fetcher )
    , mIsGeographic( isGeographic )
    , mAttribution( attribution )
{
    if ( !isGeographic )
    {
        // Store the WKT of the current GRASS location so the render thread
        // can build a thread-local PROJ transform without calling GRASS API.
        // GPJ_grass_to_wkt (via GPJ_grass_to_osr) has a heap-corruption bug
        // for non-Earth spherical CRS (+R_C notation, Mars/Moon), so we only
        // call it when the semi-major axis looks Earth-like (a > 6,000,000 m).
        struct Key_Value *pinfo = G_get_projinfo();
        struct Key_Value *uinfo = G_get_projunits();
        if ( pinfo && uinfo )
        {
            const char *aStr = G_find_key_value( "a", pinfo );
            const double a   = ( aStr && *aStr ) ? atof( aStr ) : 6378137.0;
            if ( a > 6000000.0 )   // Earth-like; skip for Moon (1,737 km) and Mars (3,396 km)
            {
                CPLPushErrorHandler( CPLQuietErrorHandler );
                char *wkt = GPJ_grass_to_wkt( pinfo, uinfo, 0, 0 );
                CPLPopErrorHandler();
                if ( wkt )
                {
                    mProjFromWkt = QByteArray( wkt );
                    G_free( wkt );
                }
            }
        }
        if ( pinfo ) G_free_key_value( pinfo );
        if ( uinfo ) G_free_key_value( uinfo );
    }
}

GrassExtent GrassTileBackground::extent() const
{
    // Covers the entire world in geographic CRS.
    return { 85.051, -85.051, 180.0, -180.0 };
}

// ── Helper: draw a grid of tiles onto img ─────────────────────────────────
// Shared by the geographic and projected rendering paths.
// The difference is how tile lat/lon bounds are mapped to img pixels.

static void queueFetch( GrassTileFetcher *fetcher, int z, int tx, int ty )
{
    QMetaObject::invokeMethod( fetcher, "fetchTile",
        Qt::QueuedConnection,
        Q_ARG(int,z), Q_ARG(int,tx), Q_ARG(int,ty) );
}

// ── Geographic (lat/lon) rendering — same coordinate system as tiles ───────
static void renderGeo( QPainter &p, GrassTileCache *cache, GrassTileFetcher *fetcher,
                        const GrassExtent &vp, int viewW, int viewH )
{
    const double north = std::min( vp.north, TileCoords::kMaxLat );
    const double south = std::max( vp.south, TileCoords::kMinLat );
    const double east  = std::min( vp.east,  180.0 );
    const double west  = std::max( vp.west, -180.0 );
    if ( north <= south || east <= west ) return;

    const int z = TileCoords::bestZoom( east-west, viewW, 0, 19 );
    const int maxTile = (1<<z) - 1;
    const int tx0 = std::max( 0, TileCoords::lonToTileX( west,  z ) );
    const int tx1 = std::min( maxTile, TileCoords::lonToTileX( east,  z ) );
    const int ty0 = std::max( 0, TileCoords::latToTileY( north, z ) );
    const int ty1 = std::min( maxTile, TileCoords::latToTileY( south, z ) );

    for ( int ty = ty0; ty <= ty1; ++ty )
        for ( int tx = tx0; tx <= tx1; ++tx )
        {
            const GrassTileKey key{ z, tx, ty };
            const QImage tile = cache->get( key );
            if ( tile.isNull() ) { queueFetch(fetcher,z,tx,ty); continue; }

            const double tW = TileCoords::tileXToLon( tx,   z );
            const double tE = TileCoords::tileXToLon( tx+1, z );
            const double tN = TileCoords::tileYToLat( ty,   z );
            const double tS = TileCoords::tileYToLat( ty+1, z );

            const int px = static_cast<int>(std::round((tW-vp.west)/vp.width()*viewW));
            const int py = static_cast<int>(std::round((vp.north-tN)/vp.height()*viewH));
            const int pw = static_cast<int>(std::round((tE-tW)/vp.width()*viewW));
            const int ph = static_cast<int>(std::round((tN-tS)/vp.height()*viewH));
            if ( pw > 0 && ph > 0 )
                p.drawImage( QRect(px,py,pw,ph), tile );
        }
}

// ── Projected CRS rendering — reproject tiles to the viewport's CRS ────────
// Uses PROJ 6+ per-thread context for thread safety.
// Each tile is warped using a perspective quad-to-quad transform.
static void renderProjected( QPainter &p, GrassTileCache *cache, GrassTileFetcher *fetcher,
                              const GrassExtent &vp, int viewW, int viewH,
                              const QByteArray &fromWkt )
{
    if ( fromWkt.isEmpty() ) return;

    // ── Set up thread-local PROJ transform: current CRS → WGS84 ───────────
    PJ_CONTEXT *ctx = proj_context_create();
    // PROJ can use WKT, EPSG codes, or PROJ strings as source definition
    PJ *toWgs = proj_create_crs_to_crs( ctx,
        fromWkt.constData(), "EPSG:4326", nullptr );
    if ( !toWgs ) { proj_context_destroy(ctx); return; }
    // Normalize so lon is always X, lat is always Y
    PJ *norm = proj_normalize_for_visualization( ctx, toWgs );
    proj_destroy( toWgs );
    if ( !norm ) { proj_context_destroy(ctx); return; }

    // Project projected (e,n) → WGS84 (lon,lat)
    auto toLatLon = [&]( double e, double n, double &lon, double &lat )
    {
        PJ_COORD c = proj_coord( e, n, 0, 0 );
        PJ_COORD r = proj_trans( norm, PJ_FWD, c );
        lon = r.xy.x;  lat = r.xy.y;
    };
    // Project WGS84 (lon,lat) → projected (e,n)
    auto toProj = [&]( double lon, double lat, double &e, double &n )
    {
        PJ_COORD c = proj_coord( lon, lat, 0, 0 );
        PJ_COORD r = proj_trans( norm, PJ_INV, c );
        e = r.xy.x;  n = r.xy.y;
    };

    // ── Get viewport bounding box in WGS84 ────────────────────────────────
    double lon_nw, lat_nw, lon_ne, lat_ne, lon_sw, lat_sw, lon_se, lat_se;
    toLatLon( vp.west, vp.north, lon_nw, lat_nw );
    toLatLon( vp.east, vp.north, lon_ne, lat_ne );
    toLatLon( vp.west, vp.south, lon_sw, lat_sw );
    toLatLon( vp.east, vp.south, lon_se, lat_se );

    const double lat_min = std::min({lat_nw,lat_ne,lat_sw,lat_se});
    const double lat_max = std::max({lat_nw,lat_ne,lat_sw,lat_se});
    const double lon_min = std::min({lon_nw,lon_ne,lon_sw,lon_se});
    const double lon_max = std::max({lon_nw,lon_ne,lon_sw,lon_se});

    if ( lat_max < TileCoords::kMinLat || lat_min > TileCoords::kMaxLat )
    {
        proj_destroy(norm); proj_context_destroy(ctx);
        return;
    }

    // ── Select zoom and tile range ─────────────────────────────────────────
    const int z = TileCoords::bestZoom( lon_max - lon_min, viewW, 0, 19 );
    const int maxTile = (1<<z) - 1;
    const int tx0 = std::max( 0, TileCoords::lonToTileX(
        std::max(lon_min, -180.0), z ) );
    const int tx1 = std::min( maxTile, TileCoords::lonToTileX(
        std::min(lon_max,  180.0), z ) );
    const int ty0 = std::max( 0, TileCoords::latToTileY(
        std::min(lat_max, TileCoords::kMaxLat), z ) );
    const int ty1 = std::min( maxTile, TileCoords::latToTileY(
        std::max(lat_min, TileCoords::kMinLat), z ) );

    // Helper: map projected (e,n) → viewport pixel
    auto toVpPx = [&]( double e, double n ) -> QPointF {
        return { (e - vp.west)  / vp.width()  * viewW,
                 (vp.north - n) / vp.height() * viewH };
    };

    p.setRenderHint( QPainter::SmoothPixmapTransform );

    for ( int ty = ty0; ty <= ty1; ++ty )
        for ( int tx = tx0; tx <= tx1; ++tx )
        {
            const GrassTileKey key{ z, tx, ty };
            const QImage tile = cache->get( key );
            if ( tile.isNull() ) { queueFetch(fetcher,z,tx,ty); continue; }

            // Tile corners in WGS84
            const double tLon0 = TileCoords::tileXToLon( tx,   z );
            const double tLon1 = TileCoords::tileXToLon( tx+1, z );
            const double tLat0 = TileCoords::tileYToLat( ty,   z );   // north
            const double tLat1 = TileCoords::tileYToLat( ty+1, z );   // south

            // Tile corners → projected
            double e00,n00, e10,n10, e01,n01, e11,n11;
            toProj( tLon0, tLat0, e00, n00 );  // NW
            toProj( tLon1, tLat0, e10, n10 );  // NE
            toProj( tLon0, tLat1, e01, n01 );  // SW
            toProj( tLon1, tLat1, e11, n11 );  // SE

            // Projected → viewport pixels
            const QPointF pNW = toVpPx( e00, n00 );
            const QPointF pNE = toVpPx( e10, n10 );
            const QPointF pSW = toVpPx( e01, n01 );
            const QPointF pSE = toVpPx( e11, n11 );

            const int tw = tile.width();
            const int th = tile.height();

            // Warp tile quad → viewport quad using a perspective transform
            QPolygonF src, dst;
            src << QPointF(0,0) << QPointF(tw,0) << QPointF(tw,th) << QPointF(0,th);
            dst << pNW << pNE << pSE << pSW;

            QTransform xf;
            if ( QTransform::quadToQuad( src, dst, xf ) )
            {
                p.setTransform( xf );
                p.drawImage( QPointF(0,0), tile );
                p.resetTransform();
            }
        }

    proj_destroy( norm );
    proj_context_destroy( ctx );
}

// ── Public render entry point ──────────────────────────────────────────────
QImage GrassTileBackground::render( int viewW, int viewH,
                                     const GrassExtent &vp ) const
{
    if ( cacheHit( viewW, viewH, vp ) )
        return cachedImage();

    QImage img( viewW, viewH, QImage::Format_RGB32 );
    img.fill( QColor( 30, 30, 30 ) );   // dark ocean fallback

    if ( viewW <= 0 || viewH <= 0 || !vp.isValid() )
    {
        storeCache( viewW, viewH, vp, img );
        return img;
    }

    QPainter p( &img );

    if ( mIsGeographic )
        renderGeo( p, mCache, mFetcher, vp, viewW, viewH );
    else if ( !mProjFromWkt.isEmpty() )
        renderProjected( p, mCache, mFetcher, vp, viewW, viewH, mProjFromWkt );
    // else: no projection info → stays dark grey

    p.end();
    storeCache( viewW, viewH, vp, img );
    return img;
}
