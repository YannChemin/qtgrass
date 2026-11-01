#include "grassrastermaplayer.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QLoggingCategory>
#include <algorithm>
#include <vector>


extern "C" {
#include <grass/gis.h>
#include <grass/raster.h>

Q_LOGGING_CATEGORY(lcCanvas, "qtgrass.canvas")
}

GrassRasterMapLayer::GrassRasterMapLayer( const QString &name,
                                           const QString &mapset )
    : GrassMapLayer( name )
    , mMapset( mapset )
{}

// ---------------------------------------------------------------------------
// load — reads Cell_head on first use
// ---------------------------------------------------------------------------
bool GrassRasterMapLayer::load() const
{
    if ( mLoaded ) return mError.isEmpty();
    mLoaded = true;

    const QByteArray nameBytes   = mName.toLocal8Bit();
    const QByteArray mapsetBytes = mMapset.toLocal8Bit();
    const char *mapset = mapsetBytes.isEmpty() ? "" : mapsetBytes.constData();

    QByteArray mutableName = nameBytes;
    if ( !G_find_raster( mutableName.data(), mapset ) )
    {
        mError = QString( "Raster '%1' not found in mapset '%2'" )
                 .arg( mName, mapset[0] ? mapset : "(current)" );
        qCWarning(lcCanvas) << "GrassRasterMapLayer:" << mError;
        return false;
    }

    struct Cell_head hd;
    Rast_get_cellhd( nameBytes.constData(), mapset, &hd );

    // Detect rasters whose cellhd is internally inconsistent: the proj field
    // says PROJECTION_LL (geographic) but the coordinates are in projected
    // metres (e.g. a raster raw-copied from a UTM location into a lat/lon
    // location without reprojection).  Using such extents for zoomToFullExtent
    // sets mCenter to millions of metres, which later causes
    // G_fatal_error("Illegal latitude") in Rast_set_window.
    // Use hd.proj (from the raster's own cellhd) — NOT G_projection(), which
    // is frozen at the startup location and wrong after a session switch.
    if ( hd.proj == PROJECTION_LL )
    {
        if ( std::abs( hd.north ) > 90.0 || std::abs( hd.south ) > 90.0
          || std::abs( hd.east )  > 360.0 || std::abs( hd.west ) > 360.0 )
        {
            mError = QString( "Raster '%1' has projected extent in a geographic location"
                              " (north=%2) — skipping" ).arg( mName ).arg( hd.north );
            qCWarning(lcCanvas) << "GrassRasterMapLayer:" << mError;
            return false;
        }
    }

    mExtent = { hd.north, hd.south, hd.east, hd.west };
    mRows   = hd.rows;
    mCols   = hd.cols;
    mNsRes  = hd.ns_res;
    mEwRes  = hd.ew_res;
    return true;
}

// ---------------------------------------------------------------------------
// loadOverviewInfo — reads cell_misc/<name>/qtgrass_ovr if present.
// Format: one "factor mapname" pair per line, e.g.:
//   2 bathy_L2
//   4 bathy_L4
//   8 bathy_L8
// ---------------------------------------------------------------------------
void GrassRasterMapLayer::loadOverviewInfo() const
{
    if ( mOverviewsLoaded ) return;
    mOverviewsLoaded = true;
    mOverviews.clear();

    const QString db  = QString::fromLocal8Bit( qgetenv( "GISDBASE" ) );
    const QString loc = QString::fromLocal8Bit( qgetenv( "LOCATION_NAME" ) );
    const QString ms  = mMapset.isEmpty()
                        ? QString::fromLocal8Bit( qgetenv( "MAPSET" ) )
                        : mMapset;

    if ( db.isEmpty() || loc.isEmpty() || ms.isEmpty() ) return;

    const QString markerPath = QString( "%1/%2/%3/cell_misc/%4/qtgrass_ovr" )
                               .arg( db, loc, ms, mName );
    QFile f( markerPath );
    if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) ) return;

    QTextStream ts( &f );
    while ( !ts.atEnd() )
    {
        const QString line = ts.readLine().trimmed();
        if ( line.isEmpty() || line.startsWith( '#' ) ) continue;
        const QStringList parts = line.split( ' ', Qt::SkipEmptyParts );
        if ( parts.size() < 2 ) continue;
        bool ok;
        const int factor = parts[0].toInt( &ok );
        if ( !ok || factor < 2 ) continue;
        mOverviews.append( { factor, parts[1] } );
    }

    // Sort by factor ascending so selectOverviewMap() scans coarsest-first
    std::sort( mOverviews.begin(), mOverviews.end(),
               []( const GrassRasterOverview &a, const GrassRasterOverview &b ){
                   return a.factor < b.factor; } );

    if ( !mOverviews.isEmpty() )
        qCInfo(lcCanvas) << "GrassRasterMapLayer:" << mName
                << "— loaded" << mOverviews.size() << "overview levels";
}

// ---------------------------------------------------------------------------
// selectOverviewMap — GDAL-style level selection.
// Use the finest pre-built overview whose resolution is coarser than
// or equal to the required output resolution.
// Returns the native map name when no overview qualifies.
// ---------------------------------------------------------------------------
QString GrassRasterMapLayer::selectOverviewMap( double reqEwRes ) const
{
    if ( mOverviews.isEmpty() || mEwRes <= 0 ) return mName;

    // Decimation factor the viewport is requesting
    const int decimation = static_cast<int>( reqEwRes / mEwRes );
    if ( decimation < 2 ) return mName;

    // Largest overview factor that does not exceed the decimation
    // (avoids upsampling artefacts)
    QString best = mName;
    for ( const GrassRasterOverview &ovr : mOverviews )
    {
        if ( ovr.factor <= decimation )
            best = ovr.mapName;  // keep the coarsest qualifying level
        else
            break;               // sorted ascending — stop when we overshoot
    }
    return best;
}

// ---------------------------------------------------------------------------
// render — Phase B: GRASS viewport-region resampling + overview selection.
//
// Complexity: O(outW × outH) regardless of native raster size.
// With pre-built overview levels the number of native cells GRASS reads
// from disk is proportional to the overview map (e.g. L8 = 64× smaller).
// ---------------------------------------------------------------------------
QImage GrassRasterMapLayer::render( int viewW, int viewH,
                                     const GrassExtent &vp ) const
{
    if ( cacheHit( viewW, viewH, vp ) )
        return cachedImage();

    if ( !load() || viewW <= 0 || viewH <= 0 || !vp.isValid() )
        return {};

    // Lazy-load overview info
    loadOverviewInfo();

    // Intersection of viewport with raster extent
    const double intN = std::min( vp.north, mExtent.north );
    const double intS = std::max( vp.south, mExtent.south );
    const double intE = std::min( vp.east,  mExtent.east  );
    const double intW = std::max( vp.west,  mExtent.west  );

    QImage img( viewW, viewH, QImage::Format_ARGB32_Premultiplied );
    img.fill( Qt::transparent );

    if ( intN <= intS || intE <= intW )
    {
        storeCache( viewW, viewH, vp, img );
        return img;
    }

    // Destination rectangle within the output image
    const int dstX = static_cast<int>( std::round( ( intW - vp.west  ) / vp.width()  * viewW ) );
    const int dstY = static_cast<int>( std::round( ( vp.north - intN  ) / vp.height() * viewH ) );
    const int dstW = static_cast<int>( std::round( ( intE - intW      ) / vp.width()  * viewW ) );
    const int dstH = static_cast<int>( std::round( ( intN - intS      ) / vp.height() * viewH ) );

    if ( dstW <= 0 || dstH <= 0 )
    {
        storeCache( viewW, viewH, vp, img );
        return img;
    }

    // Select the best-fit overview map (may be the native map)
    const double reqEwRes   = ( intE - intW ) / dstW;
    const QString renderMap = selectOverviewMap( reqEwRes );

    const QByteArray nameBytes   = renderMap.toLocal8Bit();
    const QByteArray mapsetBytes = mMapset.toLocal8Bit();
    const char *mapset = mapsetBytes.isEmpty() ? "" : mapsetBytes.constData();

    // ── Rendering — Phase B ────────────────────────────────────────────────
    // Set GRASS region = viewport intersection at output resolution.
    // GRASS resamples the raster to (dstW × dstH) → O(outW × outH) reads.
    //
    // Near-global rasters (native EW extent ≥ 360°, e.g. bathy or the
    // grid_float_All_2023_* series with east=180.003°) require a special
    // safeE clamp.  GRASS internally computes:
    //   native_cols = round( region_extent / native_ew_res )
    // If region_extent is 360.0 exactly, floating-point gives
    //   round(360.0 / ew_res) = file_cols ± 1 (rounding direction varies).
    // An off-by-one produces a mismatched row buffer → heap corruption → SIGABRT.
    //
    // Fix: clamp safeE so that region_extent = 360° - one_native_cell.
    //   native_cols = round((360 - ew_res) / ew_res) = round(file_cols - 1)
    //               = file_cols - 1  (safe: reads one fewer column, misses
    //                                  only the pixel at longitude > 180°).
    // ── Phase B: viewport-resolution resampling ────────────────────────────
    // GRASS resamples the raster to exactly (renderH × renderW) output cells.
    //
    // safeE clamping for near-global rasters (native EW extent ≥ 360°):
    //   GRASS internally computes  native_cols = round(region_extent / ew_res).
    //   Setting region_extent = 360° causes floating-point rounding to give
    //   native_cols = file_cols ± 1.  A +1 overcount → buffer overflow → SIGABRT.
    //   Fix: cap region_extent at (360° - one_native_cell) so the rounded value
    //   is always file_cols - 1 (safe undercount: we lose one pixel at lon > 180°).
    struct Cell_head savedRegion;
    G_get_window( &savedRegion );

    // safeE clamping — ONLY for geographic (lat/lon) CRS.
    // In PROJECTION_LL, extents are in degrees (0–360).  Setting region_extent
    // = 360° makes GRASS compute native_cols = file_cols ± 1 due to floating-
    // point rounding; a +1 overcount → buffer overflow → SIGABRT.
    // Fix: cap to 360° - one_native_cell.
    //
    // For projected CRS (UTM, State Plane, …) extents are in metres.
    // 15 000 m > 359 → the old guard fired incorrectly, clamping the EW
    // extent to 350 m and producing a badly stretched stripe pattern.
    const double nativeEwExtent = mExtent.east - mExtent.west;
    double safeE = intE;
    if ( savedRegion.proj == PROJECTION_LL )
    {
        if ( nativeEwExtent > 359.0 && mEwRes > 0 )
        {
            const double maxSafeExt = 360.0 - mEwRes;
            if ( intE - intW > maxSafeExt )
                safeE = intW + maxSafeExt;
        }
        else if ( intE - intW > 360.0 )
        {
            safeE = intW + 360.0;
        }
    }

    // ── Build the render window ───────────────────────────────────────────
    // Set the window to viewport resolution — GRASS handles resampling via
    // nearest-neighbour internally (same as wxPython d.rast).  One GRASS cell
    // per output pixel.  Simple, correct, and matches reference rendering.
    const double vpNsRes = ( intN - intS ) / dstH;
    const double vpEwRes = ( safeE - intW ) / dstW;

    struct Cell_head vpRegion = savedRegion;
    vpRegion.north  = intN;
    vpRegion.south  = intS;
    vpRegion.east   = safeE;
    vpRegion.west   = intW;
    vpRegion.ns_res = vpNsRes;
    vpRegion.ew_res = vpEwRes;
    vpRegion.rows   = 0;
    vpRegion.cols   = 0;

    // Safety clamp for geographic locations.
    if ( savedRegion.proj == PROJECTION_LL )
    {
        vpRegion.north = std::min( vpRegion.north,  90.0 );
        vpRegion.south = std::max( vpRegion.south, -90.0 );
        if ( vpRegion.north <= vpRegion.south || vpRegion.east <= vpRegion.west )
        {
            Rast_set_window( &savedRegion );
            storeCache( viewW, viewH, vp, img );
            return img;
        }
        vpRegion.ns_res = ( vpRegion.north - vpRegion.south ) / dstH;
    }

    Rast_set_window( &vpRegion );
    const int renderH = vpRegion.rows;   // output rows  (≈ dstH)
    const int renderW = vpRegion.cols;   // output cols  (≈ dstW)

    struct Colors colors;
    if ( Rast_read_colors( nameBytes.constData(), mapset, &colors ) < 0 )
    {
        // No color table — build a grey scale from the raster's actual data range.
        RASTER_MAP_TYPE rtype = Rast_map_type( nameBytes.constData(), mapset );
        CELL cmin = 0, cmax = 255;
        if ( rtype == CELL_TYPE )
        {
            struct Range range;
            Rast_init_range( &range );
            if ( Rast_read_range( nameBytes.constData(), mapset, &range ) >= 0 )
                Rast_get_range_min_max( &range, &cmin, &cmax );
        }
        else
        {
            struct FPRange fprange;
            Rast_init_fp_range( &fprange );
            if ( Rast_read_fp_range( nameBytes.constData(), mapset, &fprange ) >= 0 )
            {
                DCELL dmin, dmax;
                Rast_get_fp_range_min_max( &fprange, &dmin, &dmax );
                cmin = static_cast<CELL>( dmin );
                cmax = static_cast<CELL>( dmax );
            }
        }
        if ( cmax <= cmin ) cmax = cmin + 255;
        Rast_make_grey_scale_colors( &colors, cmin, cmax );
    }

    const int fd = Rast_open_old( nameBytes.constData(), mapset );
    if ( fd < 0 )
    {
        mError = QString( "Cannot open raster '%1'" ).arg( renderMap );
        Rast_free_colors( &colors );
        Rast_set_window( &savedRegion );
        storeCache( viewW, viewH, vp, img );
        return img;
    }

    auto *red   = new unsigned char[ renderW ];
    auto *green = new unsigned char[ renderW ];
    auto *blue  = new unsigned char[ renderW ];
    auto *nul   = new unsigned char[ renderW ];

    // One GRASS row per output row — GRASS handles resampling internally.
    for ( int row = 0; row < renderH; ++row )
    {
        const int imgRow = dstY + row;
        if ( imgRow < 0 || imgRow >= viewH ) continue;
        Rast_get_row_colors( fd, row, &colors, red, green, blue, nul );
        QRgb *scanline = reinterpret_cast<QRgb *>( img.scanLine( imgRow ) );
        for ( int col = 0; col < renderW; ++col )
        {
            const int imgCol = dstX + col;
            if ( imgCol < 0 || imgCol >= viewW ) continue;
            if ( !nul[col] )
                scanline[imgCol] = qRgba( red[col], green[col], blue[col], 255 );
        }
    }

    Rast_close( fd );
    Rast_free_colors( &colors );
    delete[] red; delete[] green; delete[] blue; delete[] nul;
    Rast_set_window( &savedRegion );

    storeCache( viewW, viewH, vp, img );
    return img;
}
