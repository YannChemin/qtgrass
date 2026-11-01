#pragma once

/*!
    \file grasstilekey.h
    \brief Unique identifier for a slippy-map XYZ tile.
*/

#include <QHashFunctions>

/*!
    \struct GrassTileKey
    \brief Identifies a single slippy-map tile by zoom level and tile coordinates.

    Used as the key type in QHash-based tile caches.  qHash() is provided so
    GrassTileKey can be used directly in \c QHash<GrassTileKey, …> containers.

    \sa GrassTileCache, GrassTileFetcher
*/
struct GrassTileKey
{
    int z = 0;  /*!< Zoom level (0 = world, 19 = street level). */
    int x = 0;  /*!< Tile column (increases eastward). */
    int y = 0;  /*!< Tile row (increases southward, row 0 = north pole edge). */

    /*! \brief Equality comparison (required for QHash key). */
    bool operator==( const GrassTileKey &o ) const
    { return z == o.z && x == o.x && y == o.y; }
};

/*! \brief qHash overload enabling GrassTileKey as a QHash key. */
inline size_t qHash( const GrassTileKey &k, size_t seed = 0 ) noexcept
{
    return qHash( k.z, seed ) ^ qHash( k.x ) ^ qHash( k.y );
}
