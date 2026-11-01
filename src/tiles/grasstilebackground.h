#pragma once

/*!
    \file grasstilebackground.h
    \brief GrassMapLayer subclass rendering slippy-map XYZ tiles.
*/

#include "../canvas/grassmaplayer.h"
#include "grasstilecache.h"
#include "grasstilefetcher.h"

#include <QByteArray>
#include <QString>

/*!
    \class GrassTileBackground
    \brief Renders slippy-map XYZ tiles as the bottom-most canvas layer.

    Provides satellite, street-map, and terrain backgrounds sourced from any
    XYZ tile provider (Google, Bing, OpenStreetMap, …).

    \section threading Thread safety

    Tile fetching is async — GrassTileFetcher runs on the main thread and
    delivers tiles to GrassTileCache (thread-safe).  render() reads the cache
    from the render thread and queues missing fetches via
    \c QMetaObject::invokeMethod (Qt::QueuedConnection).

    \section proj Projected CRS support

    For geographic CRS locations the tiles are pasted directly (Web Mercator
    tiles match lat/lon closely at large scales).

    For projected CRS locations each tile's four corners are reprojected from
    EPSG:4326 to the location CRS via a thread-local PROJ 9 transform, and the
    tile image is warped onto the viewport using \c QTransform::quadToQuad().

    \sa GrassTileCache, GrassTileFetcher, GrassMapCanvas
*/
class GrassTileBackground : public GrassMapLayer
{
  public:
    /*!
        \brief Constructs a tile background layer.

        \param cache        Thread-safe tile image cache (non-owning pointer).
        \param fetcher      Tile HTTP fetcher, must live on the main thread (non-owning pointer).
        \param isGeographic \c true when the GRASS location uses a lat/lon CRS.
        \param attribution  Attribution text to display in the lower-right corner.
    */
    explicit GrassTileBackground( GrassTileCache   *cache,
                                   GrassTileFetcher *fetcher,
                                   bool              isGeographic,
                                   const QString    &attribution = {} );

    /*!
        \brief Returns the full world extent for geographic CRS, or a large
               bbox in map units for projected CRS.
    */
    GrassExtent extent() const override;

    /*!
        \brief Render visible tiles into a pixel image.

        Missing tiles are fetched asynchronously; already-cached tiles are
        painted immediately.

        \param viewW Viewport width in pixels.
        \param viewH Viewport height in pixels.
        \param vp    Geographic rectangle to fill.
        \return ARGB32 image of size (\a viewW × \a viewH).
    */
    QImage render( int viewW, int viewH,
                   const GrassExtent &vp ) const override;

    /*!
        \brief Set the provider attribution text.
        \param text Attribution string (HTML allowed).
    */
    void    setAttribution( const QString &text ) { mAttribution = text; }

    /*! \brief Returns the raw attribution text. */
    QString attribution() const { return mAttribution; }

    /*!
        \brief Enable or disable the attribution overlay.
        \param on \c true to show the attribution text.
    */
    void setShowAttribution( bool on ) { mShowAttribution = on; }

    /*! \brief \c true when the attribution overlay is visible. */
    bool showAttribution() const { return mShowAttribution; }

    /*!
        \brief Returns the text to overlay, or an empty string when hidden.

        Attribution is suppressed for projected CRS sessions where tile coverage
        is unreliable.
    */
    QString attributionText() const
    { return (mShowAttribution && mIsGeographic) ? mAttribution : QString{}; }

  private:
    GrassTileCache   *mCache;
    GrassTileFetcher *mFetcher;
    bool              mIsGeographic;
    QString           mAttribution;
    bool              mShowAttribution = true;

    QByteArray        mProjFromWkt;
};
