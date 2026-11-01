#include "grasscompositelayers.h"
#include "../grassenv.h"

#include <QLoggingCategory>
#include <QMap>
#include <QPainter>
#include <QPolygonF>
#include <QProcess>
#include <algorithm>
#include <cmath>
#include <csignal>
#include <csetjmp>
#include <cstring>

extern "C" {
#include <grass/gis.h>
#include <grass/raster.h>
#include <grass/vector.h>
#include <grass/vedit.h>
}

Q_LOGGING_CATEGORY(lcComposite, "qtgrass.canvas.composite")

// Suppress -Wclobbered for const char* variables that span sigsetjmp boundaries
// in crash-guard blocks.  These are false positives — the QByteArray data pointers
// are allocated before sigsetjmp is called and cannot be changed by longjmp.
#pragma GCC diagnostic ignored "-Wclobbered"

// ═══════════════════════════════════════════════════════════════════════════════
// Internal helpers
// ═══════════════════════════════════════════════════════════════════════════════

// Read a raster's geographic extent from its Cell_head.
static GrassExtent extentOf( const QString &name, const QString &mapset )
{
    const QByteArray nb = name.toLocal8Bit();
    const QByteArray mb = mapset.toLocal8Bit();
    const char *ms = mb.isEmpty() ? "" : mb.constData();
    struct Cell_head hd;
    Rast_get_cellhd( nb.constData(), ms, &hd );
    return { hd.north, hd.south, hd.east, hd.west };
}

// Read a raster's value range.
static void fpRange( const QString &name, const QString &mapset,
                     double &lo, double &hi )
{
    const QByteArray nb = name.toLocal8Bit();
    const QByteArray mb = mapset.toLocal8Bit();
    const char *ms = mb.isEmpty() ? "" : mb.constData();
    struct FPRange r;
    Rast_init_fp_range( &r );
    Rast_read_fp_range( nb.constData(), ms, &r );
    Rast_get_fp_range_min_max( &r, &lo, &hi );
    if ( lo >= hi ) { lo = 0; hi = 255; }
}

// Set up the GRASS window for viewport rendering.
// Fills savedRegion (to restore later) and vpRegion (the render window).
// dstX/Y/W/H give the rectangle within the output image that should be painted.
// Returns false when the viewport does not intersect layerExt.
struct RenderSetup {
    struct Cell_head saved, win;
    int dstX = 0, dstY = 0, dstW = 0, dstH = 0;
    int renderW = 0, renderH = 0;
    bool ok = false;
};

static RenderSetup prepareRegion( int viewW, int viewH,
                                   const GrassExtent &vp,
                                   const GrassExtent &layerExt )
{
    RenderSetup s;
    G_get_window( &s.saved );

    const double iN = std::min( vp.north, layerExt.north );
    const double iS = std::max( vp.south, layerExt.south );
    const double iE = std::min( vp.east,  layerExt.east  );
    const double iW = std::max( vp.west,  layerExt.west  );
    if ( iN <= iS || iE <= iW ) return s;

    s.dstX = int( std::round( ( iW - vp.west  ) / vp.width()  * viewW ) );
    s.dstY = int( std::round( ( vp.north - iN  ) / vp.height() * viewH ) );
    s.dstW = int( std::round( ( iE - iW        ) / vp.width()  * viewW ) );
    s.dstH = int( std::round( ( iN - iS        ) / vp.height() * viewH ) );
    if ( s.dstW <= 0 || s.dstH <= 0 ) return s;

    s.win          = s.saved;
    s.win.north    = iN;
    s.win.south    = iS;
    s.win.east     = iE;
    s.win.west     = iW;
    s.win.ns_res   = ( iN - iS ) / s.dstH;
    s.win.ew_res   = ( iE - iW ) / s.dstW;
    s.win.rows     = 0;
    s.win.cols     = 0;

    Rast_set_window( &s.win );
    s.renderH = s.win.rows;
    s.renderW = s.win.cols;
    s.ok      = ( s.renderW > 0 && s.renderH > 0 );
    return s;
}

// Normalise a DCELL value to [0, 255] given the band's range.
static inline int normByte( DCELL v, double lo, double hi )
{
    if ( hi <= lo ) return 128;
    return std::clamp( int( ( v - lo ) / ( hi - lo ) * 255.0 + 0.5 ), 0, 255 );
}

// IHS → RGB conversion (I ∈ [0,1], H ∈ [0°,360°), S ∈ [0,1]).
struct RGB3 { int r, g, b; };
static RGB3 ihsToRgb( double I, double H, double S )
{
    if ( S <= 0.0 ) {
        int v = std::clamp( int( I * 255 ), 0, 255 );
        return { v, v, v };
    }
    H = std::fmod( H, 360.0 );
    if ( H < 0 ) H += 360.0;
    const double h = H / 60.0;
    const int    sector = int( h ) % 6;
    const double f  = h - int( h );
    const double p  = I * ( 1.0 - S );
    const double q  = I * ( 1.0 - S * f );
    const double t  = I * ( 1.0 - S * ( 1.0 - f ) );
    double r, g, b;
    switch ( sector ) {
        case 0: r=I; g=t; b=p; break;
        case 1: r=q; g=I; b=p; break;
        case 2: r=p; g=I; b=t; break;
        case 3: r=p; g=q; b=I; break;
        case 4: r=t; g=p; b=I; break;
        default: r=I; g=p; b=q; break;
    }
    return {
        std::clamp( int( r * 255 ), 0, 255 ),
        std::clamp( int( g * 255 ), 0, 255 ),
        std::clamp( int( b * 255 ), 0, 255 )
    };
}

// Draw a single arrow at (cx, cy) pointing in direction angleDeg (0=North, CW).
static void drawArrow( QPainter &p, double cx, double cy,
                        double angleDeg, double size,
                        GrassRastArrowLayer::ArrowStyle style )
{
    p.save();
    p.translate( cx, cy );
    p.rotate( angleDeg );

    const double half = size / 2.0;
    const double head = size * 0.28;

    switch ( style ) {
        case GrassRastArrowLayer::ArrowStyle::Line: {
            p.drawLine( QPointF( 0, half ), QPointF( 0, -half ) );
            p.drawLine( QPointF( 0, -half ), QPointF( -head, -half + head ) );
            p.drawLine( QPointF( 0, -half ), QPointF(  head, -half + head ) );
            break;
        }
        case GrassRastArrowLayer::ArrowStyle::Filled: {
            QPolygonF arrow;
            arrow << QPointF(   0, -half )        // tip
                  << QPointF(  head, -half + head*1.2 )
                  << QPointF(  head*0.4, -half + head*1.2 )
                  << QPointF(  head*0.4, half )
                  << QPointF( -head*0.4, half )
                  << QPointF( -head*0.4, -half + head*1.2 )
                  << QPointF( -head, -half + head*1.2 );
            p.drawPolygon( arrow );
            break;
        }
        case GrassRastArrowLayer::ArrowStyle::Outline: {
            QPolygonF arrow;
            arrow << QPointF(   0, -half )
                  << QPointF(  head, -half + head*1.2 )
                  << QPointF(  head*0.4, -half + head*1.2 )
                  << QPointF(  head*0.4, half )
                  << QPointF( -head*0.4, half )
                  << QPointF( -head*0.4, -half + head*1.2 )
                  << QPointF( -head, -half + head*1.2 );
            p.setBrush( Qt::NoBrush );
            p.drawPolygon( arrow );
            break;
        }
    }
    p.restore();
}

// ═══════════════════════════════════════════════════════════════════════════════
// GrassRgbLayer
// ═══════════════════════════════════════════════════════════════════════════════

GrassRgbLayer::GrassRgbLayer( const Config &cfg )
    : GrassMapLayer( "RGB:" + cfg.redMap + "+" + cfg.greenMap + "+" + cfg.blueMap )
    , mCfg( cfg )
{}

GrassExtent GrassRgbLayer::extent() const
{
    if ( !mExtentLoaded ) {
        mExtent       = extentOf( mCfg.redMap, mCfg.redMapset );
        mExtentLoaded = true;
    }
    return mExtent;
}

QImage GrassRgbLayer::render( int viewW, int viewH,
                               const GrassExtent &vp ) const
{
    if ( cacheHit( viewW, viewH, vp ) ) return cachedImage();

    QImage img( viewW, viewH, QImage::Format_ARGB32_Premultiplied );
    img.fill( Qt::transparent );

    if ( mCfg.redMap.isEmpty() || mCfg.greenMap.isEmpty() || mCfg.blueMap.isEmpty()
         || viewW <= 0 || viewH <= 0 || !vp.isValid() )
        return img;

    const GrassExtent lext = extent();
    if ( !lext.isValid() ) { storeCache( viewW, viewH, vp, img ); return img; }

    RenderSetup s = prepareRegion( viewW, viewH, vp, lext );
    if ( !s.ok ) { storeCache( viewW, viewH, vp, img ); return img; }

    // Open bands
    double loR, hiR, loG, hiG, loB, hiB;
    fpRange( mCfg.redMap,   mCfg.redMapset,   loR, hiR );
    fpRange( mCfg.greenMap, mCfg.greenMapset, loG, hiG );
    fpRange( mCfg.blueMap,  mCfg.blueMapset,  loB, hiB );

    const QByteArray rn = mCfg.redMap.toLocal8Bit(),   rm = mCfg.redMapset.toLocal8Bit();
    const QByteArray gn = mCfg.greenMap.toLocal8Bit(), gm = mCfg.greenMapset.toLocal8Bit();
    const QByteArray bn = mCfg.blueMap.toLocal8Bit(),  bm = mCfg.blueMapset.toLocal8Bit();

    const int fdR = Rast_open_old( rn.constData(), rm.isEmpty() ? "" : rm.constData() );
    const int fdG = Rast_open_old( gn.constData(), gm.isEmpty() ? "" : gm.constData() );
    const int fdB = Rast_open_old( bn.constData(), bm.isEmpty() ? "" : bm.constData() );

    if ( fdR < 0 || fdG < 0 || fdB < 0 ) {
        if ( fdR >= 0 ) Rast_close( fdR );
        if ( fdG >= 0 ) Rast_close( fdG );
        if ( fdB >= 0 ) Rast_close( fdB );
        Rast_set_window( &s.saved );
        storeCache( viewW, viewH, vp, img );
        return img;
    }

    DCELL *rowR = Rast_allocate_d_buf();
    DCELL *rowG = Rast_allocate_d_buf();
    DCELL *rowB = Rast_allocate_d_buf();

    for ( int row = 0; row < s.renderH; ++row ) {
        const int imgRow = s.dstY + row;
        if ( imgRow < 0 || imgRow >= viewH ) continue;
        Rast_get_d_row( fdR, rowR, row );
        Rast_get_d_row( fdG, rowG, row );
        Rast_get_d_row( fdB, rowB, row );
        QRgb *line = reinterpret_cast<QRgb *>( img.scanLine( imgRow ) );
        for ( int col = 0; col < s.renderW; ++col ) {
            const int imgCol = s.dstX + col;
            if ( imgCol < 0 || imgCol >= viewW ) continue;
            if ( Rast_is_d_null_value( &rowR[col] ) ||
                 Rast_is_d_null_value( &rowG[col] ) ||
                 Rast_is_d_null_value( &rowB[col] ) ) continue;
            line[imgCol] = qRgba( normByte( rowR[col], loR, hiR ),
                                   normByte( rowG[col], loG, hiG ),
                                   normByte( rowB[col], loB, hiB ), 255 );
        }
    }

    G_free( rowR ); G_free( rowG ); G_free( rowB );
    Rast_close( fdR ); Rast_close( fdG ); Rast_close( fdB );
    Rast_set_window( &s.saved );
    storeCache( viewW, viewH, vp, img );
    return img;
}

// ═══════════════════════════════════════════════════════════════════════════════
// GrassIhsLayer
// ═══════════════════════════════════════════════════════════════════════════════

GrassIhsLayer::GrassIhsLayer( const Config &cfg )
    : GrassMapLayer( "IHS:" + cfg.intensityMap + "+" + cfg.hueMap + "+" + cfg.satMap )
    , mCfg( cfg )
{}

GrassExtent GrassIhsLayer::extent() const
{
    if ( !mExtentLoaded ) {
        mExtent       = extentOf( mCfg.intensityMap, mCfg.intensityMapset );
        mExtentLoaded = true;
    }
    return mExtent;
}

QImage GrassIhsLayer::render( int viewW, int viewH,
                               const GrassExtent &vp ) const
{
    if ( cacheHit( viewW, viewH, vp ) ) return cachedImage();

    QImage img( viewW, viewH, QImage::Format_ARGB32_Premultiplied );
    img.fill( Qt::transparent );

    if ( mCfg.intensityMap.isEmpty() || mCfg.hueMap.isEmpty() || mCfg.satMap.isEmpty()
         || viewW <= 0 || viewH <= 0 || !vp.isValid() )
        return img;

    const GrassExtent lext = extent();
    if ( !lext.isValid() ) { storeCache( viewW, viewH, vp, img ); return img; }

    RenderSetup s = prepareRegion( viewW, viewH, vp, lext );
    if ( !s.ok ) { storeCache( viewW, viewH, vp, img ); return img; }

    double loI, hiI, loH, hiH, loS, hiS;
    fpRange( mCfg.intensityMap, mCfg.intensityMapset, loI, hiI );
    fpRange( mCfg.hueMap,       mCfg.hueMapset,       loH, hiH );
    fpRange( mCfg.satMap,       mCfg.satMapset,       loS, hiS );

    const QByteArray in = mCfg.intensityMap.toLocal8Bit(), im = mCfg.intensityMapset.toLocal8Bit();
    const QByteArray hn = mCfg.hueMap.toLocal8Bit(),       hm = mCfg.hueMapset.toLocal8Bit();
    const QByteArray sn = mCfg.satMap.toLocal8Bit(),       sm = mCfg.satMapset.toLocal8Bit();

    const int fdI = Rast_open_old( in.constData(), im.isEmpty() ? "" : im.constData() );
    const int fdH = Rast_open_old( hn.constData(), hm.isEmpty() ? "" : hm.constData() );
    const int fdS = Rast_open_old( sn.constData(), sm.isEmpty() ? "" : sm.constData() );

    if ( fdI < 0 || fdH < 0 || fdS < 0 ) {
        if ( fdI >= 0 ) Rast_close( fdI );
        if ( fdH >= 0 ) Rast_close( fdH );
        if ( fdS >= 0 ) Rast_close( fdS );
        Rast_set_window( &s.saved );
        storeCache( viewW, viewH, vp, img );
        return img;
    }

    DCELL *rowI = Rast_allocate_d_buf();
    DCELL *rowH = Rast_allocate_d_buf();
    DCELL *rowS = Rast_allocate_d_buf();

    for ( int row = 0; row < s.renderH; ++row ) {
        const int imgRow = s.dstY + row;
        if ( imgRow < 0 || imgRow >= viewH ) continue;
        Rast_get_d_row( fdI, rowI, row );
        Rast_get_d_row( fdH, rowH, row );
        Rast_get_d_row( fdS, rowS, row );
        QRgb *line = reinterpret_cast<QRgb *>( img.scanLine( imgRow ) );
        for ( int col = 0; col < s.renderW; ++col ) {
            const int imgCol = s.dstX + col;
            if ( imgCol < 0 || imgCol >= viewW ) continue;
            if ( Rast_is_d_null_value( &rowI[col] ) ) continue;

            // Normalise to canonical IHS ranges
            const double I = ( hiI > loI ) ? ( rowI[col] - loI ) / ( hiI - loI ) : 0.5;
            const double H = Rast_is_d_null_value( &rowH[col] )
                             ? 0.0 : ( hiH > loH )
                               ? ( rowH[col] - loH ) / ( hiH - loH ) * 360.0 : 0.0;
            const double S = Rast_is_d_null_value( &rowS[col] )
                             ? 0.5 : ( hiS > loS )
                               ? ( rowS[col] - loS ) / ( hiS - loS ) : 0.5;

            const RGB3 rgb = ihsToRgb(
                std::clamp( I, 0.0, 1.0 ),
                std::clamp( H, 0.0, 360.0 ),
                std::clamp( S, 0.0, 1.0 ) );
            line[imgCol] = qRgba( rgb.r, rgb.g, rgb.b, 255 );
        }
    }

    G_free( rowI ); G_free( rowH ); G_free( rowS );
    Rast_close( fdI ); Rast_close( fdH ); Rast_close( fdS );
    Rast_set_window( &s.saved );
    storeCache( viewW, viewH, vp, img );
    return img;
}

// ═══════════════════════════════════════════════════════════════════════════════
// GrassShadeLayer
// ═══════════════════════════════════════════════════════════════════════════════

GrassShadeLayer::GrassShadeLayer( const Config &cfg )
    : GrassMapLayer( "Shade:" + cfg.colorMap + "|" + cfg.shadeMap )
    , mCfg( cfg )
{}

GrassExtent GrassShadeLayer::extent() const
{
    if ( !mExtentLoaded ) {
        mExtent       = extentOf( mCfg.colorMap, mCfg.colorMapset );
        mExtentLoaded = true;
    }
    return mExtent;
}

QImage GrassShadeLayer::render( int viewW, int viewH,
                                 const GrassExtent &vp ) const
{
    if ( cacheHit( viewW, viewH, vp ) ) return cachedImage();

    QImage img( viewW, viewH, QImage::Format_ARGB32_Premultiplied );
    img.fill( Qt::transparent );

    if ( mCfg.colorMap.isEmpty() || mCfg.shadeMap.isEmpty()
         || viewW <= 0 || viewH <= 0 || !vp.isValid() )
        return img;

    const GrassExtent lext = extent();
    if ( !lext.isValid() ) { storeCache( viewW, viewH, vp, img ); return img; }

    RenderSetup s = prepareRegion( viewW, viewH, vp, lext );
    if ( !s.ok ) { storeCache( viewW, viewH, vp, img ); return img; }

    const QByteArray cn = mCfg.colorMap.toLocal8Bit(), cm = mCfg.colorMapset.toLocal8Bit();
    const QByteArray shn = mCfg.shadeMap.toLocal8Bit(), shm = mCfg.shadeMapset.toLocal8Bit();
    const char *cms  = cm.isEmpty()  ? "" : cm.constData();
    const char *shms = shm.isEmpty() ? "" : shm.constData();

    struct Colors colColors;
    if ( Rast_read_colors( cn.constData(), cms, &colColors ) < 0 ) {
        // fallback: grey scale
        struct FPRange r; Rast_init_fp_range( &r );
        Rast_read_fp_range( cn.constData(), cms, &r );
        DCELL dmin = 0, dmax = 255;
        Rast_get_fp_range_min_max( &r, &dmin, &dmax );
        Rast_make_grey_scale_colors( &colColors, CELL( dmin ), CELL( dmax ) );
    }

    double shLo, shHi;
    fpRange( mCfg.shadeMap, mCfg.shadeMapset, shLo, shHi );
    if ( shHi <= shLo ) { shLo = 0; shHi = 255; }

    const int fdC  = Rast_open_old( cn.constData(),  cms  );
    const int fdSh = Rast_open_old( shn.constData(), shms );

    if ( fdC < 0 || fdSh < 0 ) {
        if ( fdC  >= 0 ) Rast_close( fdC  );
        if ( fdSh >= 0 ) Rast_close( fdSh );
        Rast_free_colors( &colColors );
        Rast_set_window( &s.saved );
        storeCache( viewW, viewH, vp, img );
        return img;
    }

    const double brightFactor = mCfg.bright / 50.0; // 50 → 1.0 (neutral)

    auto *red  = new unsigned char[ s.renderW ];
    auto *grn  = new unsigned char[ s.renderW ];
    auto *blu  = new unsigned char[ s.renderW ];
    auto *nul  = new unsigned char[ s.renderW ];
    DCELL *rowSh = Rast_allocate_d_buf();

    for ( int row = 0; row < s.renderH; ++row ) {
        const int imgRow = s.dstY + row;
        if ( imgRow < 0 || imgRow >= viewH ) continue;
        Rast_get_row_colors( fdC, row, &colColors, red, grn, blu, nul );
        Rast_get_d_row( fdSh, rowSh, row );
        QRgb *line = reinterpret_cast<QRgb *>( img.scanLine( imgRow ) );
        for ( int col = 0; col < s.renderW; ++col ) {
            const int imgCol = s.dstX + col;
            if ( imgCol < 0 || imgCol >= viewW ) continue;
            if ( nul[col] ) continue;

            double shade = Rast_is_d_null_value( &rowSh[col] )
                           ? 1.0
                           : ( rowSh[col] - shLo ) / ( shHi - shLo );
            shade = std::clamp( shade * brightFactor, 0.0, 1.0 );

            line[imgCol] = qRgba(
                std::clamp( int( red[col] * shade ), 0, 255 ),
                std::clamp( int( grn[col] * shade ), 0, 255 ),
                std::clamp( int( blu[col] * shade ), 0, 255 ),
                255 );
        }
    }

    delete[] red; delete[] grn; delete[] blu; delete[] nul;
    G_free( rowSh );
    Rast_close( fdC ); Rast_close( fdSh );
    Rast_free_colors( &colColors );
    Rast_set_window( &s.saved );
    storeCache( viewW, viewH, vp, img );
    return img;
}

// ═══════════════════════════════════════════════════════════════════════════════
// GrassRastNumLayer
// ═══════════════════════════════════════════════════════════════════════════════

GrassRastNumLayer::GrassRastNumLayer( const Config &cfg )
    : GrassMapLayer( "Values:" + cfg.map )
    , mCfg( cfg )
{}

GrassExtent GrassRastNumLayer::extent() const
{
    if ( !mExtentLoaded ) {
        mExtent       = extentOf( mCfg.map, mCfg.mapset );
        mExtentLoaded = true;
    }
    return mExtent;
}

QImage GrassRastNumLayer::render( int viewW, int viewH,
                                   const GrassExtent &vp ) const
{
    if ( cacheHit( viewW, viewH, vp ) ) return cachedImage();

    QImage img( viewW, viewH, QImage::Format_ARGB32_Premultiplied );
    img.fill( Qt::transparent );

    if ( mCfg.map.isEmpty() || viewW <= 0 || viewH <= 0 || !vp.isValid() )
        return img;

    const GrassExtent lext = extent();
    if ( !lext.isValid() ) { storeCache( viewW, viewH, vp, img ); return img; }

    RenderSetup s = prepareRegion( viewW, viewH, vp, lext );
    if ( !s.ok ) { storeCache( viewW, viewH, vp, img ); return img; }

    const QByteArray nb = mCfg.map.toLocal8Bit();
    const QByteArray mb = mCfg.mapset.toLocal8Bit();
    const char *ms = mb.isEmpty() ? "" : mb.constData();

    // Compute native cell pixel size — native cell at the current viewport zoom
    const double nativeCellPx = ( lext.east - lext.west )
                                / ( vp.east - vp.west ) * viewW
                                / ( s.renderW > 0 ? s.renderW : 1 );

    // ── Render base raster colours ────────────────────────────────────────────
    struct Colors colors;
    if ( Rast_read_colors( nb.constData(), ms, &colors ) < 0 )
        Rast_make_grey_scale_colors( &colors, 0, 255 );

    const int fd = Rast_open_old( nb.constData(), ms );
    if ( fd < 0 ) {
        Rast_free_colors( &colors );
        Rast_set_window( &s.saved );
        storeCache( viewW, viewH, vp, img );
        return img;
    }

    auto *red  = new unsigned char[ s.renderW ];
    auto *grn  = new unsigned char[ s.renderW ];
    auto *blu  = new unsigned char[ s.renderW ];
    auto *nul  = new unsigned char[ s.renderW ];
    DCELL *rowD = Rast_allocate_d_buf();

    // Two-pass: first draw colours, then overlay text (needs QPainter over image).
    for ( int row = 0; row < s.renderH; ++row ) {
        const int imgRow = s.dstY + row;
        if ( imgRow < 0 || imgRow >= viewH ) continue;
        Rast_get_row_colors( fd, row, &colors, red, grn, blu, nul );
        QRgb *line = reinterpret_cast<QRgb *>( img.scanLine( imgRow ) );
        for ( int col = 0; col < s.renderW; ++col ) {
            const int imgCol = s.dstX + col;
            if ( imgCol < 0 || imgCol >= viewW ) continue;
            if ( !nul[col] )
                line[imgCol] = qRgba( red[col], grn[col], blu[col], 255 );
        }
    }

    // ── Overlay text when cells are large enough ───────────────────────────────
    if ( nativeCellPx >= mCfg.minCellPx ) {
        // Seek back to row 0 by re-opening
        Rast_close( fd );
        const int fd2 = Rast_open_old( nb.constData(), ms );

        QPainter p( &img );
        QFont f( "sans", mCfg.fontSize );
        p.setFont( f );
        const QFontMetrics fm( f );

        for ( int row = 0; row < s.renderH; ++row ) {
            Rast_get_d_row( fd2, rowD, row );
            for ( int col = 0; col < s.renderW; ++col ) {
                bool isNull = Rast_is_d_null_value( &rowD[col] );
                if ( isNull && !mCfg.showNull ) continue;

                // Cell centre in image coordinates
                const double cx = s.dstX + ( col + 0.5 ) * s.dstW / (double)s.renderW;
                const double cy = s.dstY + ( row + 0.5 ) * s.dstH / (double)s.renderH;
                if ( cx < 0 || cx >= viewW || cy < 0 || cy >= viewH ) continue;

                const QString txt = isNull
                    ? "null"
                    : QString::number( rowD[col], 'f', mCfg.decimals );

                const QRect br = fm.boundingRect( txt );
                const QRectF bgR( cx - br.width()/2.0 - 2, cy - br.height()/2.0 - 1,
                                   br.width() + 4, br.height() + 2 );

                if ( mCfg.showBg ) {
                    p.fillRect( bgR, mCfg.bgColor );
                }
                p.setPen( mCfg.textColor );
                p.drawText( bgR, Qt::AlignCenter, txt );
            }
        }
        p.end();
        if ( fd2 >= 0 ) Rast_close( fd2 );
    } else {
        Rast_close( fd );
    }

    delete[] red; delete[] grn; delete[] blu; delete[] nul;
    G_free( rowD );
    Rast_free_colors( &colors );
    Rast_set_window( &s.saved );
    storeCache( viewW, viewH, vp, img );
    return img;
}

// ═══════════════════════════════════════════════════════════════════════════════
// GrassRastArrowLayer
// ═══════════════════════════════════════════════════════════════════════════════

GrassRastArrowLayer::GrassRastArrowLayer( const Config &cfg )
    : GrassMapLayer( "Arrows:" + cfg.dirMap )
    , mCfg( cfg )
{}

GrassExtent GrassRastArrowLayer::extent() const
{
    if ( !mExtentLoaded ) {
        mExtent       = extentOf( mCfg.dirMap, mCfg.dirMapset );
        mExtentLoaded = true;
    }
    return mExtent;
}

QImage GrassRastArrowLayer::render( int viewW, int viewH,
                                     const GrassExtent &vp ) const
{
    if ( cacheHit( viewW, viewH, vp ) ) return cachedImage();

    QImage img( viewW, viewH, QImage::Format_ARGB32_Premultiplied );
    img.fill( Qt::transparent );

    if ( mCfg.dirMap.isEmpty() || viewW <= 0 || viewH <= 0 || !vp.isValid() )
        return img;

    const GrassExtent lext = extent();
    if ( !lext.isValid() ) { storeCache( viewW, viewH, vp, img ); return img; }

    RenderSetup s = prepareRegion( viewW, viewH, vp, lext );
    if ( !s.ok ) { storeCache( viewW, viewH, vp, img ); return img; }

    // Native cell pixel size (how wide is one original raster cell on screen)
    const double nativeCellPx = std::max( 1.0,
        ( lext.east - lext.west ) / ( vp.east - vp.west ) * viewW
        / std::max( 1, s.renderW ) );

    if ( nativeCellPx < mCfg.minCellPx ) {
        // Too zoomed out — draw nothing (transparent overlay)
        Rast_set_window( &s.saved );
        storeCache( viewW, viewH, vp, img );
        return img;
    }

    const QByteArray dn  = mCfg.dirMap.toLocal8Bit();
    const QByteArray dm  = mCfg.dirMapset.toLocal8Bit();
    const char *dms = dm.isEmpty() ? "" : dm.constData();

    const bool hasMag = !mCfg.magMap.isEmpty();
    const QByteArray mn  = mCfg.magMap.toLocal8Bit();
    const QByteArray mm  = mCfg.magMapset.toLocal8Bit();
    const char *mms = mm.isEmpty() ? "" : mm.constData();

    double magLo = 0, magHi = 1;
    if ( hasMag ) fpRange( mCfg.magMap, mCfg.magMapset, magLo, magHi );

    const int fdDir = Rast_open_old( dn.constData(), dms );
    const int fdMag = hasMag ? Rast_open_old( mn.constData(), mms ) : -1;

    if ( fdDir < 0 ) {
        if ( fdMag >= 0 ) Rast_close( fdMag );
        Rast_set_window( &s.saved );
        storeCache( viewW, viewH, vp, img );
        return img;
    }

    DCELL *rowDir = Rast_allocate_d_buf();
    DCELL *rowMag = hasMag ? Rast_allocate_d_buf() : nullptr;

    QPainter p( &img );
    p.setRenderHint( QPainter::Antialiasing );
    const QColor c = mCfg.color;
    p.setPen( QPen( c, 1.5 ) );
    if ( mCfg.style == ArrowStyle::Filled ) p.setBrush( c );
    else p.setBrush( Qt::NoBrush );

    const double cellW = s.dstW / (double)std::max( 1, s.renderW );
    const double cellH = s.dstH / (double)std::max( 1, s.renderH );

    for ( int row = 0; row < s.renderH; ++row ) {
        Rast_get_d_row( fdDir, rowDir, row );
        if ( hasMag ) Rast_get_d_row( fdMag, rowMag, row );

        for ( int col = 0; col < s.renderW; ++col ) {
            if ( Rast_is_d_null_value( &rowDir[col] ) ) continue;

            double angleDeg = rowDir[col];
            // GRASS aspect: 0=E, 90=N, 180=W, 270=S (CCW from East)
            // Screen rotation: 0=North, CW — convert
            if ( mCfg.dirIsGrass ) angleDeg = 90.0 - angleDeg;
            // angleDeg now in meteorological convention (0=N, CW)

            double arrowPx = mCfg.arrowSize;
            if ( hasMag && rowMag && !Rast_is_d_null_value( &rowMag[col] ) ) {
                const double normMag = ( magHi > magLo )
                    ? ( rowMag[col] - magLo ) / ( magHi - magLo ) : 0.5;
                arrowPx = std::clamp( normMag, 0.1, 1.0 ) * mCfg.arrowSize;
            }
            arrowPx = std::min( arrowPx, std::min( cellW, cellH ) * 0.85 );

            const double cx = s.dstX + ( col + 0.5 ) * cellW;
            const double cy = s.dstY + ( row + 0.5 ) * cellH;
            if ( cx < 0 || cx >= viewW || cy < 0 || cy >= viewH ) continue;

            drawArrow( p, cx, cy, angleDeg, arrowPx, mCfg.style );
        }
    }
    p.end();

    G_free( rowDir );
    if ( rowMag ) G_free( rowMag );
    Rast_close( fdDir );
    if ( fdMag >= 0 ) Rast_close( fdMag );
    Rast_set_window( &s.saved );
    storeCache( viewW, viewH, vp, img );
    return img;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Shared vector-attribute loader
// Runs v.db.select and returns cat→value(s) map.
// ═══════════════════════════════════════════════════════════════════════════════
#include <QProcessEnvironment>

static QMap<int, QStringList> runVdbSelect( const QString &map,
                                             const QString &mapset,
                                             const QStringList &columns,
                                             int layer = 1 )
{
    QMap<int, QStringList> result;
    const QString gb = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
    if ( gb.isEmpty() ) return result;

    const QString mapRef = mapset.isEmpty() ? map : map + "@" + mapset;
    QStringList args = {
        "map=" + mapRef,
        "columns=cat," + columns.join( "," ),
        "layer=" + QString::number( layer ),
        "separator=pipe",
        "flags=c"   // -c: no column header
    };

    QProcess p;
    p.setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );
    p.start( gb + "/bin/v.db.select", args );
    p.waitForFinished( 15000 );

    for ( const QString &line :
          QString::fromLocal8Bit( p.readAllStandardOutput() )
              .split( '\n', Qt::SkipEmptyParts ) )
    {
        const QStringList parts = line.split( '|' );
        if ( parts.isEmpty() ) continue;
        bool ok;
        int cat = parts[0].trimmed().toInt( &ok );
        if ( !ok ) continue;
        QStringList vals;
        for ( int i = 1; i < parts.size(); ++i )
            vals << parts[i].trimmed();
        result[cat] = vals;
    }
    return result;
}

// Read area centroid and line/point positions from an open vector map
// Returns cat → geographic (east, north)
static QMap<int, QPointF> vectorCentroids( const QString &map,
                                             const QString &mapset )
{
    QMap<int, QPointF> result;
    const QByteArray nb = map.toLocal8Bit();
    const QByteArray mb = mapset.toLocal8Bit();
    const char *ms = mb.isEmpty() ? "" : mb.constData();

    struct Map_info vmap;
    if ( Vect_open_old( &vmap, nb.constData(), ms ) < 0 )
        return result;

    struct line_pnts *pnts = Vect_new_line_struct();
    struct line_cats *cats = Vect_new_cats_struct();
    const int nlines = Vect_get_num_lines( &vmap );

    for ( int i = 1; i <= nlines; ++i )
    {
        const int type = Vect_read_line( &vmap, pnts, cats, i );
        if ( type < 0 || pnts->n_points == 0 ) continue;

        // Centroid / point: use its coordinates directly
        if ( type & ( GV_POINT | GV_CENTROID ) )
        {
            const double cx = pnts->x[0];
            const double cy = pnts->y[0];
            if ( cats->n_cats > 0 )
                result[cats->cat[0]] = { cx, cy };
        }
    }

    // Also compute area centroids
    const int nareas = Vect_get_num_areas( &vmap );
    for ( int a = 1; a <= nareas; ++a )
    {
        // area centroid: mean of boundary points (fallback if no centroid line)
        if ( Vect_get_area_centroid( &vmap, a ) > 0 ) continue; // already have centroid line
        if ( Vect_get_area_points( &vmap, a, pnts ) < 0 ) continue;
        double cx = 0, cy = 0;
        for ( int k = 0; k < pnts->n_points; ++k ) { cx += pnts->x[k]; cy += pnts->y[k]; }
        if ( pnts->n_points > 0 ) {
            cx /= pnts->n_points; cy /= pnts->n_points;
            struct line_cats *ac = Vect_new_cats_struct();
            Vect_get_area_cats( &vmap, a, ac );
            if ( ac->n_cats > 0 )
                result[ac->cat[0]] = { cx, cy };
            Vect_destroy_cats_struct( ac );
        }
    }

    Vect_destroy_line_struct( pnts );
    Vect_destroy_cats_struct( cats );
    Vect_close( &vmap );
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// GrassVectThematicLayer
// ═══════════════════════════════════════════════════════════════════════════════


// sigsetjmp guard reused from GrassVectorMapLayer to survive corrupt topo
static sigjmp_buf          sThemJmpBuf;
static volatile sig_atomic_t sThemJmpActive = 0;
static void themCrashGuard( int ) {
    if ( sThemJmpActive ) { sThemJmpActive = 0; siglongjmp( sThemJmpBuf, 1 ); }
    struct sigaction sa {}; sa.sa_handler = SIG_DFL; sigaction( SIGSEGV, &sa, nullptr ); raise( SIGSEGV );
}

GrassVectThematicLayer::GrassVectThematicLayer( const Config &cfg )
    : GrassMapLayer( "Thematic:" + cfg.map )
    , mCfg( cfg )
{}

GrassExtent GrassVectThematicLayer::extent() const
{
    if ( !mExtentLoaded ) {
        const QByteArray nb = mCfg.map.toLocal8Bit();
        const QByteArray mb = mCfg.mapset.toLocal8Bit();
        struct Map_info vmap;
        if ( Vect_open_old( &vmap, nb.constData(), mb.isEmpty() ? "" : mb.constData() ) >= 0 ) {
            struct bound_box bbox;
            Vect_get_map_box( &vmap, &bbox );
            mExtent = { bbox.N, bbox.S, bbox.E, bbox.W };
            Vect_close( &vmap );
        }
        mExtentLoaded = true;
    }
    return mExtent;
}

void GrassVectThematicLayer::loadAttributes() const
{
    if ( mAttrLoaded ) return;
    mAttrLoaded = true;
    mCatValues.clear();
    mBreaks.clear();

    const auto raw = runVdbSelect( mCfg.map, mCfg.mapset, { mCfg.column }, mCfg.layer );
    QVector<double> vals;
    for ( auto it = raw.begin(); it != raw.end(); ++it ) {
        if ( it.value().isEmpty() ) continue;
        bool ok; double v = it.value()[0].toDouble( &ok );
        if ( ok ) { mCatValues[it.key()] = v; vals << v; }
    }
    if ( vals.isEmpty() ) return;

    std::sort( vals.begin(), vals.end() );
    const int n = mCfg.nClasses;

    if ( mCfg.method == ClassMethod::Quantile ) {
        for ( int i = 0; i <= n; ++i ) {
            const int idx = int( double(i) / n * (vals.size()-1) );
            mBreaks << vals[std::clamp(idx, 0, int(vals.size()-1))];
        }
    } else { // EqualInterval (and StdDev falls back here)
        const double lo = vals.first(), hi = vals.last();
        for ( int i = 0; i <= n; ++i )
            mBreaks << lo + double(i) / n * ( hi - lo );
    }
}

QColor GrassVectThematicLayer::classColor( int cls, int nCls ) const
{
    const double t = nCls > 1 ? double(cls) / (nCls-1) : 0.5;
    const double v = mCfg.reverse ? 1.0 - t : t;
    switch ( mCfg.ramp ) {
        case ColorRamp::Blues:   return QColor::fromHsv( 210, int(80+v*175), int(220-v*80) );
        case ColorRamp::Greens:  return QColor::fromHsv( 120, int(60+v*180), int(200-v*70) );
        case ColorRamp::RdYlGn: {
            if ( v < 0.5 ) return QColor::fromHsv( int(v*2*60), 200, 220 );
            return QColor::fromHsv( int((v-0.5)*2*120+60), 200, 200 );
        }
        case ColorRamp::Viridis: return QColor::fromHsv( int((1-v)*270), int(v*220+35), int(200-v*50) );
        case ColorRamp::Plasma:  return QColor::fromHsv( int((1-v)*300), 240, int(180+v*75) );
        default: // YlOrRd
            if ( v < 0.5 ) return QColor::fromHsv( int(60-v*60), 200+int(v*55), 240 );
            return QColor::fromHsv( int(30-(v-0.5)*30), 240, int(200-v*60) );
    }
}

QImage GrassVectThematicLayer::render( int viewW, int viewH,
                                        const GrassExtent &vp ) const
{
    if ( cacheHit( viewW, viewH, vp ) ) return cachedImage();

    QImage img( viewW, viewH, QImage::Format_ARGB32_Premultiplied );
    img.fill( Qt::transparent );

    if ( mCfg.map.isEmpty() || mCfg.column.isEmpty() || viewW<=0 || viewH<=0 || !vp.isValid() ) {
        storeCache(viewW, viewH, vp, img); return img;
    }

    const GrassExtent lext = extent();
    if ( !lext.isValid() ) { storeCache(viewW,viewH,vp,img); return img; }

    loadAttributes();
    if ( mBreaks.isEmpty() ) { storeCache(viewW,viewH,vp,img); return img; }

    // Open vector with crash guard
    const QByteArray nb = mCfg.map.toLocal8Bit();
    const QByteArray mb = mCfg.mapset.toLocal8Bit();
    const char *ms = mb.isEmpty() ? "" : mb.constData();

    struct Map_info vmap;
    {
        struct sigaction newSa{}, oldSa{};
        newSa.sa_handler = themCrashGuard; sigemptyset(&newSa.sa_mask);
        sigaction(SIGSEGV, &newSa, &oldSa); sThemJmpActive = 1;
        volatile bool ok = false;
        if ( sigsetjmp(sThemJmpBuf, 1) == 0 )
            ok = ( Vect_open_old(&vmap, nb.constData(), ms) >= 0 );
        sThemJmpActive = 0; sigaction(SIGSEGV, &oldSa, nullptr);
        if (!ok) { storeCache(viewW,viewH,vp,img); return img; }
    }

    auto geo2px = [&](double e, double n) -> QPointF {
        return { (e - vp.west) / vp.width() * viewW,
                 (vp.north - n) / vp.height() * viewH };
    };

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);

    struct line_pnts *pnts = Vect_new_line_struct();
    struct line_cats *cats = Vect_new_cats_struct();
    const int nCls = mCfg.nClasses;

    auto colorFor = [&](int cat) -> QColor {
        if (!mCatValues.contains(cat)) return mCfg.nullColor;
        const double v = mCatValues[cat];
        // find class
        int cls = 0;
        for (int k = 0; k < mBreaks.size()-1; ++k)
            if (v >= mBreaks[k]) cls = k;
        return classColor(cls, nCls);
    };

    // Draw areas
    const int nareas = Vect_get_num_areas(&vmap);
    for (int a = 1; a <= nareas; ++a) {
        int centLineId = Vect_get_area_centroid(&vmap, a);
        int cat = -1;
        if (centLineId > 0) {
            Vect_read_line(&vmap, nullptr, cats, centLineId);
            if (cats->n_cats > 0) cat = cats->cat[0];
        }
        if (Vect_get_area_points(&vmap, a, pnts) < 0) continue;
        QPolygonF poly;
        for (int k=0; k<pnts->n_points; ++k)
            poly << geo2px(pnts->x[k], pnts->y[k]);
        p.setBrush(colorFor(cat));
        p.setPen(QPen(colorFor(cat).darker(140), mCfg.lineWidth));
        p.drawPolygon(poly);
    }

    // Draw lines and points
    const int nlines = Vect_get_num_lines(&vmap);
    for (int i=1; i<=nlines; ++i) {
        const int type = Vect_read_line(&vmap, pnts, cats, i);
        if (type<0 || !(type & (GV_LINE|GV_POINT))) continue;
        int cat = cats->n_cats > 0 ? cats->cat[0] : -1;
        const QColor c = colorFor(cat);
        if (type & GV_LINE) {
            QPolygonF poly;
            for (int k=0; k<pnts->n_points; ++k)
                poly << geo2px(pnts->x[k], pnts->y[k]);
            p.setPen(QPen(c, mCfg.lineWidth));
            p.setBrush(Qt::NoBrush);
            p.drawPolyline(poly);
        } else {
            p.setBrush(c); p.setPen(Qt::NoPen);
            p.drawEllipse(geo2px(pnts->x[0], pnts->y[0]), 4.0, 4.0);
        }
    }

    Vect_destroy_line_struct(pnts); Vect_destroy_cats_struct(cats);
    Vect_close(&vmap);
    p.end();
    storeCache(viewW, viewH, vp, img);
    return img;
}

// ═══════════════════════════════════════════════════════════════════════════════
// GrassVectChartLayer
// ═══════════════════════════════════════════════════════════════════════════════

GrassVectChartLayer::GrassVectChartLayer( const Config &cfg )
    : GrassMapLayer( "Chart:" + cfg.map )
    , mCfg( cfg )
{}

GrassExtent GrassVectChartLayer::extent() const
{
    if (!mExtentLoaded) {
        const QByteArray nb = mCfg.map.toLocal8Bit();
        const QByteArray mb = mCfg.mapset.toLocal8Bit();
        struct Map_info vmap;
        if (Vect_open_old(&vmap, nb.constData(), mb.isEmpty()?"":mb.constData()) >= 0) {
            struct bound_box bbox; Vect_get_map_box(&vmap, &bbox);
            mExtent = {bbox.N, bbox.S, bbox.E, bbox.W};
            Vect_close(&vmap);
        }
        mExtentLoaded = true;
    }
    return mExtent;
}

void GrassVectChartLayer::loadAttributes() const
{
    if (mAttrLoaded) return;
    mAttrLoaded = true;
    mCatValues.clear(); mMaxSum = 1.0;

    const auto raw = runVdbSelect(mCfg.map, mCfg.mapset, mCfg.columns, mCfg.layer);
    double maxSum = 0;
    for (auto it = raw.begin(); it != raw.end(); ++it) {
        QVector<double> vals;
        double sum = 0;
        for (const QString &s : it.value()) {
            bool ok; double v = s.toDouble(&ok);
            vals << (ok ? v : 0.0);
            if (ok && v > 0) sum += v;
        }
        mCatValues[it.key()] = vals;
        maxSum = std::max(maxSum, sum);
    }
    mMaxSum = std::max(maxSum, 1.0);
}

QColor GrassVectChartLayer::columnColor( int idx ) const
{
    if (idx < mCfg.colors.size()) return mCfg.colors[idx];
    // auto-cycle through a categorical palette
    static const QList<QColor> pal = {
        {228,26,28},{55,126,184},{77,175,74},{152,78,163},
        {255,127,0},{166,86,40},{247,129,191},{153,153,153}
    };
    return pal[idx % pal.size()];
}

void GrassVectChartLayer::drawPie( QPainter &p, QPointF c,
                                    double size,
                                    const QVector<double> &vals ) const
{
    double sum = 0; for (double v : vals) if (v>0) sum += v;
    if (sum <= 0) return;
    const double r = size / 2.0;
    const QRectF rect(c.x()-r, c.y()-r, size, size);
    int startAngle = 90*16; // QPainter: 0=3 o'clock, angles in 1/16 deg; start at top
    for (int i=0; i<vals.size(); ++i) {
        if (vals[i] <= 0) continue;
        const int span = int(vals[i] / sum * 360 * 16);
        p.setBrush(columnColor(i));
        p.drawPie(rect, startAngle, -span); // CCW
        startAngle -= span;
    }
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Qt::black, 0.5));
    p.drawEllipse(rect);
}

void GrassVectChartLayer::drawBar( QPainter &p, QPointF c,
                                    double size,
                                    const QVector<double> &vals ) const
{
    const int n = vals.size();
    if (n == 0) return;
    const double bw = size / n;
    double maxV = *std::max_element(vals.begin(), vals.end());
    if (maxV <= 0) return;
    for (int i = 0; i < n; ++i) {
        const double h = std::max(0.0, vals[i] / maxV * size);
        const double x = c.x() - size/2.0 + i * bw;
        const double y = c.y() - h;
        p.setBrush(columnColor(i));
        p.setPen(QPen(columnColor(i).darker(140), 0.5));
        p.drawRect(QRectF(x, y, bw*0.9, h));
    }
}

QImage GrassVectChartLayer::render( int viewW, int viewH,
                                     const GrassExtent &vp ) const
{
    if (cacheHit(viewW, viewH, vp)) return cachedImage();

    QImage img(viewW, viewH, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    if (mCfg.map.isEmpty() || mCfg.columns.isEmpty() || viewW<=0 || viewH<=0 || !vp.isValid()) {
        storeCache(viewW,viewH,vp,img); return img;
    }

    loadAttributes();

    const auto centroids = vectorCentroids(mCfg.map, mCfg.mapset);
    if (centroids.isEmpty()) { storeCache(viewW,viewH,vp,img); return img; }

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);

    auto geo2px = [&](double e, double n) -> QPointF {
        return { (e - vp.west) / vp.width() * viewW,
                 (vp.north - n) / vp.height() * viewH };
    };

    for (auto it = centroids.begin(); it != centroids.end(); ++it) {
        if (!mCatValues.contains(it.key())) continue;
        const QVector<double> &vals = mCatValues[it.key()];
        const QPointF px = geo2px(it.value().x(), it.value().y());
        if (px.x() < -50 || px.x() > viewW+50 || px.y() < -50 || px.y() > viewH+50) continue;

        double sz = mCfg.maxSize;
        if (mCfg.scale) {
            double sum = 0; for (double v : vals) if (v>0) sum += v;
            sz = std::clamp(std::sqrt(sum / mMaxSum) * mCfg.maxSize,
                            4.0, double(mCfg.maxSize));
        }

        if (mCfg.chartType == ChartType::Pie)
            drawPie(p, px, sz, vals);
        else
            drawBar(p, px, sz, vals);
    }

    p.end();
    storeCache(viewW, viewH, vp, img);
    return img;
}

// ═══════════════════════════════════════════════════════════════════════════════
// GrassLabelLayer
// ═══════════════════════════════════════════════════════════════════════════════

GrassLabelLayer::GrassLabelLayer( const Config &cfg )
    : GrassMapLayer( "Labels:" + cfg.map )
    , mCfg( cfg )
{}

GrassExtent GrassLabelLayer::extent() const
{
    if (!mExtentLoaded) {
        const QByteArray nb = mCfg.map.toLocal8Bit();
        const QByteArray mb = mCfg.mapset.toLocal8Bit();
        struct Map_info vmap;
        if (Vect_open_old(&vmap, nb.constData(), mb.isEmpty()?"":mb.constData()) >= 0) {
            struct bound_box bbox; Vect_get_map_box(&vmap, &bbox);
            mExtent = {bbox.N, bbox.S, bbox.E, bbox.W};
            Vect_close(&vmap);
        }
        mExtentLoaded = true;
    }
    return mExtent;
}

void GrassLabelLayer::loadAttributes() const
{
    if (mAttrLoaded) return;
    mAttrLoaded = true;
    mLabels.clear();
    mCentroids.clear();

    const auto raw = runVdbSelect(mCfg.map, mCfg.mapset, {mCfg.labelColumn}, mCfg.layer);
    for (auto it = raw.begin(); it != raw.end(); ++it)
        if (!it.value().isEmpty())
            mLabels[it.key()] = it.value()[0];

    mCentroids = vectorCentroids(mCfg.map, mCfg.mapset);
}

QImage GrassLabelLayer::render( int viewW, int viewH, const GrassExtent &vp ) const
{
    if (cacheHit(viewW, viewH, vp)) return cachedImage();

    QImage img(viewW, viewH, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    if (mCfg.map.isEmpty() || mCfg.labelColumn.isEmpty() || viewW<=0 || viewH<=0 || !vp.isValid()) {
        storeCache(viewW,viewH,vp,img); return img;
    }

    loadAttributes();
    if (mLabels.isEmpty() || mCentroids.isEmpty()) { storeCache(viewW,viewH,vp,img); return img; }

    auto geo2px = [&](double e, double n) -> QPointF {
        return { (e - vp.west) / vp.width() * viewW,
                 (vp.north - n) / vp.height() * viewH };
    };

    QPainter p(&img);
    p.setRenderHint(QPainter::TextAntialiasing);
    QFont font("sans", mCfg.fontSize);
    font.setBold(mCfg.bold); font.setItalic(mCfg.italic);
    p.setFont(font);
    const QFontMetrics fm(font);

    for (auto it = mCentroids.begin(); it != mCentroids.end(); ++it) {
        if (!mLabels.contains(it.key())) continue;
        const QString &txt = mLabels[it.key()];
        QPointF px = geo2px(it.value().x(), it.value().y());
        px.rx() += mCfg.xOffset;
        px.ry() += mCfg.yOffset;
        if (px.x() < -200 || px.x() > viewW+200 || px.y() < -20 || px.y() > viewH+20) continue;

        const QRect br = fm.boundingRect(txt);
        const double tx = px.x() - br.width()/2.0;
        const double ty = px.y() + br.height()/2.0 - fm.descent();

        // halo
        if (mCfg.haloWidth > 0) {
            p.setPen(QPen(mCfg.haloColor, mCfg.haloWidth * 2, Qt::SolidLine,
                           Qt::RoundCap, Qt::RoundJoin));
            for (int dx=-1; dx<=1; ++dx)
            for (int dy=-1; dy<=1; ++dy)
                if (dx||dy)
                    p.drawText(QPointF(tx+dx*mCfg.haloWidth, ty+dy*mCfg.haloWidth), txt);
        }
        p.setPen(mCfg.textColor);
        p.drawText(QPointF(tx, ty), txt);
    }

    p.end();
    storeCache(viewW, viewH, vp, img);
    return img;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Great circle and rhumb line math
// ═══════════════════════════════════════════════════════════════════════════════
static constexpr double PI = M_PI;
static inline double toRad(double deg) { return deg * PI / 180.0; }
static inline double toDeg(double rad) { return rad * 180.0 / PI; }

// Great circle waypoints between (lon1,lat1) and (lon2,lat2) in degrees
static QVector<QPointF> greatCirclePoints(double lon1, double lat1,
                                           double lon2, double lat2, int n)
{
    const double φ1 = toRad(lat1), λ1 = toRad(lon1);
    const double φ2 = toRad(lat2), λ2 = toRad(lon2);
    const double d = 2*std::asin(std::sqrt(
        std::pow(std::sin((φ2-φ1)/2), 2) +
        std::cos(φ1)*std::cos(φ2)*std::pow(std::sin((λ2-λ1)/2), 2)));

    QVector<QPointF> pts;
    pts.reserve(n+1);
    if (d < 1e-10) { pts << QPointF(lon1,lat1) << QPointF(lon2,lat2); return pts; }

    for (int i = 0; i <= n; ++i) {
        const double t = double(i) / n;
        const double A = std::sin((1-t)*d) / std::sin(d);
        const double B = std::sin(  t *d) / std::sin(d);
        const double x = A*std::cos(φ1)*std::cos(λ1) + B*std::cos(φ2)*std::cos(λ2);
        const double y = A*std::cos(φ1)*std::sin(λ1) + B*std::cos(φ2)*std::sin(λ2);
        const double z = A*std::sin(φ1)               + B*std::sin(φ2);
        const double lat = toDeg(std::atan2(z, std::sqrt(x*x+y*y)));
        const double lon = toDeg(std::atan2(y, x));
        pts << QPointF(lon, lat);
    }
    return pts;
}

// Rhumb line waypoints
static QVector<QPointF> rhumbPoints(double lon1, double lat1,
                                     double lon2, double lat2, int n)
{
    QVector<QPointF> pts;
    pts.reserve(n+1);
    for (int i = 0; i <= n; ++i) {
        const double t = double(i) / n;
        // simple linear interpolation in lat/lon (rhumb = constant bearing,
        // which is a straight line on Mercator but we draw on the actual CRS)
        pts << QPointF(lon1 + t*(lon2-lon1), lat1 + t*(lat2-lat1));
    }
    return pts;
}

// ── GrassGeodesicLayer ────────────────────────────────────────────────────────

GrassGeodesicLayer::GrassGeodesicLayer( const Config &cfg )
    : GrassMapLayer( QString("Geodesic(%1,%2→%3,%4)")
                     .arg(cfg.lon1).arg(cfg.lat1).arg(cfg.lon2).arg(cfg.lat2) )
    , mCfg( cfg )
{}

GrassExtent GrassGeodesicLayer::extent() const
{
    const double n = std::max(mCfg.lat1, mCfg.lat2);
    const double s = std::min(mCfg.lat1, mCfg.lat2);
    const double e = std::max(mCfg.lon1, mCfg.lon2);
    const double w = std::min(mCfg.lon1, mCfg.lon2);
    return { n+1, s-1, e+1, w-1 };
}

QVector<QPointF> GrassGeodesicLayer::waypoints() const
{
    return greatCirclePoints(mCfg.lon1, mCfg.lat1,
                              mCfg.lon2, mCfg.lat2, mCfg.nSegments);
}

QImage GrassGeodesicLayer::render( int viewW, int viewH, const GrassExtent &vp ) const
{
    if (cacheHit(viewW, viewH, vp)) return cachedImage();

    QImage img(viewW, viewH, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    if (viewW<=0 || viewH<=0 || !vp.isValid()) { storeCache(viewW,viewH,vp,img); return img; }

    auto geo2px = [&](double e, double n) -> QPointF {
        return { (e - vp.west) / vp.width() * viewW,
                 (vp.north - n) / vp.height() * viewH };
    };

    const auto wpts = waypoints();
    if (wpts.size() < 2) { storeCache(viewW,viewH,vp,img); return img; }

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(mCfg.color, mCfg.lineWidth, mCfg.penStyle, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    QPolygonF poly;
    for (const QPointF &gpt : wpts)
        poly << geo2px(gpt.x(), gpt.y());
    p.drawPolyline(poly);

    // draw endpoints
    p.setBrush(mCfg.color);
    p.setPen(QPen(Qt::white, 1));
    p.drawEllipse(geo2px(mCfg.lon1, mCfg.lat1), 4.0, 4.0);
    p.drawEllipse(geo2px(mCfg.lon2, mCfg.lat2), 4.0, 4.0);

    p.end();
    storeCache(viewW, viewH, vp, img);
    return img;
}

// ── GrassRhumblineLayer ───────────────────────────────────────────────────────

GrassRhumblineLayer::GrassRhumblineLayer( const Config &cfg )
    : GrassMapLayer( QString("Rhumb(%1,%2→%3,%4)")
                     .arg(cfg.lon1).arg(cfg.lat1).arg(cfg.lon2).arg(cfg.lat2) )
    , mCfg( cfg )
{}

GrassExtent GrassRhumblineLayer::extent() const
{
    return { std::max(mCfg.lat1,mCfg.lat2)+1, std::min(mCfg.lat1,mCfg.lat2)-1,
             std::max(mCfg.lon1,mCfg.lon2)+1, std::min(mCfg.lon1,mCfg.lon2)-1 };
}

QVector<QPointF> GrassRhumblineLayer::waypoints() const
{
    return rhumbPoints(mCfg.lon1, mCfg.lat1, mCfg.lon2, mCfg.lat2, mCfg.nSegments);
}

QImage GrassRhumblineLayer::render( int viewW, int viewH, const GrassExtent &vp ) const
{
    if (cacheHit(viewW, viewH, vp)) return cachedImage();

    QImage img(viewW, viewH, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    if (viewW<=0 || viewH<=0 || !vp.isValid()) { storeCache(viewW,viewH,vp,img); return img; }

    auto geo2px = [&](double e, double n) -> QPointF {
        return { (e - vp.west) / vp.width() * viewW,
                 (vp.north - n) / vp.height() * viewH };
    };

    const auto wpts = waypoints();
    if (wpts.size() < 2) { storeCache(viewW,viewH,vp,img); return img; }

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(mCfg.color, mCfg.lineWidth, mCfg.penStyle, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    QPolygonF poly;
    for (const QPointF &gpt : wpts)
        poly << geo2px(gpt.x(), gpt.y());
    p.drawPolyline(poly);

    p.setBrush(mCfg.color);
    p.setPen(QPen(Qt::white, 1));
    p.drawEllipse(geo2px(mCfg.lon1, mCfg.lat1), 4.0, 4.0);
    p.drawEllipse(geo2px(mCfg.lon2, mCfg.lat2), 4.0, 4.0);

    p.end();
    storeCache(viewW, viewH, vp, img);
    return img;
}
