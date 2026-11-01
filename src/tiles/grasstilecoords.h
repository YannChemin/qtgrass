#pragma once

/*!
    \file grasstilecoords.h
    \brief Pure tile coordinate math for the Web Mercator / EPSG:3857 XYZ scheme.

    All functions are stateless (no Qt, no GRASS dependency) and safe to call
    from any thread.
*/

#include <algorithm>
#include <cmath>

/*!
    \namespace TileCoords
    \brief Utility functions for converting between geographic and XYZ tile coordinates.
*/
namespace TileCoords {

/*! \brief Maximum latitude representable in Web Mercator (Mercator projects ±85.051129°). */
static constexpr double kMaxLat =  85.051129;
/*! \brief Minimum latitude representable in Web Mercator. */
static constexpr double kMinLat = -85.051129;

/*!
    \brief Convert a longitude to a tile column at a given zoom level.
    \param lon Longitude in degrees [−180, 180].
    \param z   Zoom level.
    \return Tile column index.
*/
inline int lonToTileX( double lon, int z )
{
    return static_cast<int>(
        std::floor( ( lon + 180.0 ) / 360.0 * ( 1 << z ) ) );
}

/*!
    \brief Convert a latitude to a tile row at a given zoom level.

    Row 0 is at the north pole edge; rows increase southward.

    \param lat Latitude in degrees (clamped to [kMinLat, kMaxLat]).
    \param z   Zoom level.
    \return Tile row index.
*/
inline int latToTileY( double lat, int z )
{
    const double latRad = lat * M_PI / 180.0;
    return static_cast<int>(
        std::floor( ( 1.0 - std::log( std::tan( latRad ) +
                      1.0 / std::cos( latRad ) ) / M_PI ) / 2.0 * ( 1 << z ) ) );
}

/*!
    \brief Return the longitude of the left (west) edge of a tile column.
    \param x Tile column index.
    \param z Zoom level.
    \return Western longitude in degrees.
*/
inline double tileXToLon( int x, int z )
{
    return x / static_cast<double>( 1 << z ) * 360.0 - 180.0;
}

/*!
    \brief Return the latitude of the top (north) edge of a tile row.
    \param y Tile row index.
    \param z Zoom level.
    \return Northern latitude in degrees.
*/
inline double tileYToLat( int y, int z )
{
    const double n = M_PI - 2.0 * M_PI * y / static_cast<double>( 1 << z );
    return 180.0 / M_PI * std::atan( 0.5 * ( std::exp( n ) - std::exp( -n ) ) );
}

/*!
    \brief Choose the zoom level at which tiles are approximately 256 pixels wide.

    \param viewWidthDeg Viewport east–west extent in degrees.
    \param viewWidthPx  Viewport width in pixels.
    \param zMin         Minimum allowed zoom level (default 0).
    \param zMax         Maximum allowed zoom level (default 19).
    \return Zoom level clamped to [\a zMin, \a zMax].
*/
inline int bestZoom( double viewWidthDeg, int viewWidthPx,
                     int zMin = 0, int zMax = 19 )
{
    if ( viewWidthDeg <= 0.0 || viewWidthPx <= 0 ) return zMin;
    const double z = std::log2( 360.0 / viewWidthDeg * viewWidthPx / 256.0 );
    return std::clamp( static_cast<int>( std::floor( z ) ), zMin, zMax );
}

} // namespace TileCoords
