#pragma once

/*!
    \file grassmaplayer.h
    \brief Abstract layer base class and GrassExtent geographic bounding box.
*/

#include <QImage>
#include <QString>
#include <algorithm>

/*!
    \struct GrassExtent
    \brief Geographic bounding box in map units (same CRS as the GRASS location).

    All coordinates are in the native units of the GRASS location — degrees for
    geographic (lat/lon) CRS, metres (or other linear units) for projected CRS.
*/
struct GrassExtent {
    double north = 0;   /*!< Northern boundary. */
    double south = 0;   /*!< Southern boundary. */
    double east  = 0;   /*!< Eastern boundary.  */
    double west  = 0;   /*!< Western boundary.  */

    /*! \brief Returns \c true when the bounding box is geometrically valid. */
    bool isValid() const { return north > south && east > west; }

    /*! \brief Width in map units (east − west). */
    double width()  const { return east  - west;  }

    /*! \brief Height in map units (north − south). */
    double height() const { return north - south; }
};

/*!
    \class GrassMapLayer
    \brief Abstract base class for anything the map canvas can render.

    Concrete subclasses implement extent() and render() to produce a QImage for
    a given viewport rectangle and pixel size.

    \section cache Per-layer image cache

    render() should call cacheHit() on entry and return cachedImage() on a hit.
    On a miss it should compute a new image, call storeCache(), and return the
    image.  The cache is invalidated explicitly via invalidateCache() when the
    layer data changes on disk, and implicitly on every viewport change (a cache
    miss via cacheHit()).

    \sa GrassRasterMapLayer, GrassVectorMapLayer, GrassTileBackground
*/
class GrassMapLayer
{
  public:
    /*! \brief Constructs a layer with the given map \a name. */
    explicit GrassMapLayer( const QString &name ) : mName( name ) {}
    virtual ~GrassMapLayer() = default;

    /*! \brief GRASS map name (without mapset suffix). */
    QString name()    const { return mName;    }

    /*! \brief \c true if the layer participates in composite rendering. */
    bool    visible() const { return mVisible; }

    /*! \brief Show or hide this layer in the canvas composite. */
    void    setVisible( bool v ) { mVisible = v; }

    /*! \brief Opacity in [0, 1]; applied by the canvas compositor. */
    double  opacity()           const { return mOpacity; }

    /*!
        \brief Set the layer opacity.
        \param o Value clamped to [0, 1].
    */
    void    setOpacity( double o )    { mOpacity = std::clamp( o, 0.0, 1.0 ); }

    /*!
        \brief Returns the geographic extent covered by this layer.
        May trigger a lazy load; implementations must be const.
    */
    virtual GrassExtent extent() const = 0;

    /*!
        \brief Render the layer into a pixel image.

        \param viewW Viewport width in pixels.
        \param viewH Viewport height in pixels.
        \param vp    Geographic rectangle to display.
        \return A \c QImage of size (\a viewW × \a viewH), or a null QImage on
                error.  Implementations should call cacheHit() / storeCache().
    */
    virtual QImage render( int viewW, int viewH,
                           const GrassExtent &vp ) const = 0;

    /*!
        \brief Drop the cached pixel image.

        Call after any operation that rewrites the map on disk (e.g. running a
        GRASS module) so the next render() recomputes from fresh data.

        Thread-safe only when called from the same thread as render().
    */
    void invalidateCache() { mCache = {}; mCacheW = 0; mCacheH = 0; }

  protected:
    /*!
        \brief Returns \c true when the cached image matches the requested viewport.
        \param W  Requested pixel width.
        \param H  Requested pixel height.
        \param vp Requested geographic viewport.
    */
    bool cacheHit( int W, int H, const GrassExtent &vp ) const
    {
        return !mCache.isNull()  &&
               mCacheW == W      &&
               mCacheH == H      &&
               mCacheVp.north == vp.north &&
               mCacheVp.south == vp.south &&
               mCacheVp.east  == vp.east  &&
               mCacheVp.west  == vp.west;
    }

    /*! \brief Returns the cached image (valid only after a cacheHit()). */
    const QImage &cachedImage() const { return mCache; }

    /*!
        \brief Store \a img as the cached image for the given viewport.
        \param W   Pixel width.
        \param H   Pixel height.
        \param vp  Geographic viewport.
        \param img Rendered image to cache.
    */
    void storeCache( int W, int H, const GrassExtent &vp,
                     const QImage &img ) const
    {
        mCache   = img;
        mCacheW  = W;
        mCacheH  = H;
        mCacheVp = vp;
    }

    QString mName;
    bool    mVisible = true;
    double  mOpacity = 1.0;

  private:
    mutable QImage      mCache;
    mutable GrassExtent mCacheVp;
    mutable int         mCacheW = 0;
    mutable int         mCacheH = 0;
};
