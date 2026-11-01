#pragma once

/*!
    \file grassmapcanvas.h
    \brief Interactive map display widget with async rendering pipeline.
*/

#include "grassmaplayer.h"
#include "grassdisplaycmd.h"

#include <QFutureWatcher>
#include <QLabel>
#include <QList>
#include <QPixmap>
#include <QPointF>
#include <QTimer>
#include <QWidget>

class QMouseEvent;
class QWheelEvent;
class GrassRasterMapLayer;
class GrassVectorMapLayer;
class GrassDisplayDriver;

/*!
    \enum MapTool
    \brief Active interaction mode for the map canvas.
*/
enum class MapTool {
    Pan,        /*!< Left-drag to pan the view (always active for middle-drag). */
    ZoomIn,     /*!< Left-click to zoom in centred on the cursor. */
    ZoomOut,    /*!< Left-click to zoom out centred on the cursor. */
    Identify,   /*!< Left-click to query a raster cell value. */
    Profile,    /*!< Collect an elevation profile line by clicking vertices. */
    Digitize    /*!< Vector digitising — delegates clicks to GrassDigitToolbar. */
};

/*!
    \class GrassMapCanvas
    \brief Interactive map display widget with async layer compositing.

    \section render_pipeline Rendering pipeline (Direct mode)

    \code
    viewport change / layer change
      → 10 ms debounce timer (mDebounceTimer)
      → QtConcurrent::run( composite all visible layers )
      → QFutureWatcher::finished → swap mPixmap → update()
    \endcode

    During a pan drag the last good \c mPixmap is shifted in paintEvent so the
    UI stays live.  A new render is scheduled when the drag ends.

    \section coord_system Coordinate system

    The viewport is described by a GrassExtent (geographic rectangle in the
    native CRS of the GRASS location).  \c mCenter holds the geographic centre
    point and \c mPixelsPerUnit holds the scale factor (pixels per map unit).

    \sa GrassMapLayer, GrassLayerPanel, GrassDisplayDriver
*/
class GrassMapCanvas : public QWidget
{
    Q_OBJECT

  public:
    /*! \brief Constructs an empty canvas. Layers are added via addRasterLayer() / addVectorLayer(). */
    explicit GrassMapCanvas( QWidget *parent = nullptr );
    ~GrassMapCanvas() override;

    // ── Layer management ────────────────────────────────────────────────────

    /*!
        \brief Add a raster layer to the top of the layer stack.
        \param name   GRASS raster map name.
        \param mapset Mapset that owns the map (empty = current mapset).
        \return Pointer to the new layer (owned by the canvas).
    */
    GrassRasterMapLayer *addRasterLayer( const QString &name,
                                          const QString &mapset = {} );

    /*!
        \brief Add a vector layer to the top of the layer stack.
        \param name   GRASS vector map name.
        \param mapset Mapset that owns the map (empty = current mapset).
        \return Pointer to the new layer (owned by the canvas).
    */
    GrassVectorMapLayer *addVectorLayer( const QString &name,
                                          const QString &mapset = {} );

    /*! \brief Remove all layers and schedule a re-render. */
    void clearLayers();

    /*!
        \brief Show or hide a layer by name.
        \param name    Layer name to look up.
        \param visible \c true to show, \c false to hide.
    */
    void setLayerVisible( const QString &name, bool visible );

    /*!
        \brief Read-only access to the layer stack (bottom = index 0, top = last).
    */
    const QList<GrassMapLayer *> &layers() const { return mLayers; }

    /*!
        \brief Insert a layer at a specific stack position.

        The canvas takes ownership of \a layer.
        Position 0 is the bottom-most (first rendered) layer.

        \param index Stack index (0 = bottom).
        \param layer Layer to insert.
    */
    void insertLayerAt( int index, GrassMapLayer *layer );

    /*!
        \brief Replace the layer stack order without changing ownership.

        \a orderedLayers must contain exactly the same pointers as \c layers()
        in a new order.  Ignored if sizes differ.  Does not schedule a render —
        call scheduleRender() after if needed.
    */
    void setLayerOrder( const QList<GrassMapLayer *> &orderedLayers );

    /*!
        \brief Schedule a re-render (debounced — safe to call repeatedly).

        Multiple calls within the debounce window (10 ms) collapse into one
        render.  Always safe to call from the main thread.
    */
    void scheduleRender();

    // ── Viewport control ─────────────────────────────────────────────────────

    /*! \brief Zoom to fit the union of all layer extents. */
    void zoomToFullExtent();

    /*!
        \brief Zoom to a specific geographic rectangle.
        \param ext Target geographic extent.
    */
    void zoomToExtent( const GrassExtent &ext );

    /*! \brief Zoom in by 50 % centred on the current view centre. */
    void zoomIn();

    /*! \brief Zoom out by 50 % centred on the current view centre. */
    void zoomOut();

    /*! \brief Returns the currently visible geographic rectangle. */
    GrassExtent viewportExtent() const;

    /*! \brief Current scale in pixels per map unit. */
    double pixelsPerUnit() const { return mPixelsPerUnit; }

    // ── Tool mode ────────────────────────────────────────────────────────────

    /*!
        \brief Set the active interaction tool.
        \param t New tool mode.
    */
    void setTool( MapTool t );

    /*! \brief Returns the currently active tool. */
    MapTool tool() const { return mTool; }

    /*!
        \brief Set the active layer by name (used by the layer tree selection).

        Identify queries prefer this layer when it is a visible raster.
        Pass an empty string to clear and fall back to the topmost visible raster.
    */
    void setActiveLayerName( const QString &name ) { mActiveLayerName = name; }

    // ── Region overlay ───────────────────────────────────────────────────────

    /*!
        \brief Show or hide the GRASS computational region rectangle.
        \param on \c true to show the overlay.
    */
    void setShowRegion( bool on ) { mShowRegion = on; update(); }

    /*! \brief \c true when the region overlay is enabled. */
    bool showRegion() const { return mShowRegion; }

    /*!
        \brief Refresh the cached GRASS computational region used by the overlay.

        Call after openMapset() so the overlay reflects the new session.
        Safe to call from the main thread; does not block on a render.
    */
    void refreshRegionCache();

    /*!
        \brief Reset the viewport to match the current GRASS region.

        Call after refreshRegionCache() when switching sessions so the first
        render uses valid coordinates.
    */
    void resetViewportToRegion();

    // ── Render mode ──────────────────────────────────────────────────────────

    /*!
        \enum RenderMode
        \brief Selects between the built-in Qt renderer and the GRASS display driver.
    */
    enum class RenderMode {
        Direct,       /*!< Built-in: each layer renders via its own C API call. */
        DisplayDriver /*!< GRASS d.* display driver pipeline (legacy/3-D). */
    };

    /*!
        \brief Switch between Direct and DisplayDriver rendering.
        \param m New render mode.
    */
    void setRenderMode( RenderMode m );

    /*! \brief Returns the current render mode. */
    RenderMode renderMode() const { return mRenderMode; }

    /*!
        \brief Set the list of decoration commands appended after layer commands.

        Used by GrassDecorationPanel to inject \c d.legend, \c d.barscale,
        \c d.northarrow, and \c d.grid calls.

        \param cmds Ordered list of enabled decoration commands.
    */
    void setDecorations( const QList<GrassDisplayCmd> &cmds );

    /*!
        \brief Update the digitise rubber-band polyline.

        Called by GrassDigitToolbar to provide the in-progress line/area vertices.
        \param geoPts Vertices in geographic coordinates.
    */
    void setDigitPoints( const QVector<QPointF> &geoPts );

    /*!
        \brief Build the display command list from the current layer stack.
        \return Ordered list of \c d.* commands ready for GrassDisplayDriver.
    */
    QList<GrassDisplayCmd> buildDisplayCmds() const;

  signals:
    /*!
        \brief Emitted on every mouse-move to update the status-bar coordinates.
        \param east  Geographic easting under the cursor.
        \param north Geographic northing under the cursor.
    */
    void coordsChanged( double east, double north );

    /*! \brief Emitted when an async render begins. */
    void renderingStarted();

    /*! \brief Emitted when an async render completes and the pixmap is swapped. */
    void renderingDone();

    /*!
        \brief Emitted in Identify mode after a left-click on a raster layer.
        \param layerName Name of the raster layer that was queried.
        \param east      Click easting.
        \param north     Click northing.
        \param value     Cell value string, or \c "null" / \c "error".
    */
    void identifyResult( const QString &layerName, double east, double north,
                         const QString &value );

    /*!
        \brief Emitted in Profile mode when the profile line is complete.

        The line ends on a right-click or double-click.

        \param rasterName Name of the topmost raster layer to profile.
        \param mapset     Mapset of \a rasterName.
        \param coords     Profile line vertices in geographic coordinates.
    */
    void profileLineReady( const QString &rasterName, const QString &mapset,
                           const QVector<QPointF> &coords );

    /*!
        \brief Emitted in Digitize mode on a left-click.
        \param east  Click easting in map units.
        \param north Click northing in map units.
    */
    void digitizeClick(  double east, double north );

    /*! \brief Emitted in Digitize mode on a right-click to end the current feature. */
    void digitizeRightClick();

    /*!
        \brief Emitted when the snap indicator position changes.

        Snap indicator is drawn as a crosshair when the cursor is near a vertex.
        \param east    Snap target easting (NaN when no snap).
        \param north   Snap target northing (NaN when no snap).
        \param snapped \c true when a snap target was found.
    */
    void snapIndicatorChanged( double east, double north, bool snapped );

  protected:
    void paintEvent( QPaintEvent * )        override;
    void resizeEvent( QResizeEvent * )      override;
    void mousePressEvent( QMouseEvent * )   override;
    void mouseMoveEvent( QMouseEvent * )    override;
    void mouseReleaseEvent( QMouseEvent * ) override;
    void wheelEvent( QWheelEvent * )        override;

  private slots:
    void onDriverRenderComplete( const QImage &img );
    void onRenderFinished();

  private:
    void startAsyncRender();
    void triggerDisplayDriverRender();
    void drawRegionOverlay( QPainter &p );
    void drawBusyOverlay(   QPainter &p );

    QPointF pixelToMap( const QPointF &px  ) const;
    QPointF mapToPixel( const QPointF &map ) const;

    void zoomToward( const QPointF &mousePos, double factor );
    void zoomToLayerIfNeeded( const GrassExtent &layerExt );

    QString identifyRaster( GrassRasterMapLayer *layer,
                             double east, double north ) const;

    QList<GrassMapLayer *> mLayers;
    QString mActiveLayerName;

    QPointF mCenter;
    double  mPixelsPerUnit = 1.0;

    // Track what mPixmap was rendered at so paintEvent can shift it during pan.
    QPointF mRenderCenter;
    double  mRenderPpu     = 1.0;

    MapTool mTool       = MapTool::Pan;
    bool    mShowRegion = true;

    bool    mPanning = false;
    QPointF mLastMouse;
    QPointF mCenterAtPanStart;

    QVector<QPointF> mProfilePts;
    QVector<QPointF> mProfilePxPts;

    QVector<QPointF> mDigitGeo;
    QPointF          mDigitCursorGeo;
    bool             mDigitHasSnap = false;

    QPixmap mPixmap;

    QTimer                  *mDebounceTimer = nullptr;
    QFutureWatcher<QImage>  *mFutureWatcher = nullptr;
    bool                     mRenderPending = false;

    struct RegionCache {
        double north = 0, south = 0, east = 0, west = 0;
        bool valid = false;
    } mRegion;

    RenderMode          mRenderMode  = RenderMode::Direct;
    GrassDisplayDriver *mDriver      = nullptr;
    QList<GrassDisplayCmd> mDecorations;
    bool                mDriverBusy  = false;
    QString             mBusyMsg     = "Rendering…";

    // Identify popup — small overlay label shown near the click point
    QLabel *mIdentifyPopup  = nullptr;
    QTimer *mIdentifyTimer  = nullptr;
    void showIdentifyPopup( const QPoint &pixelPos, const QString &text );
};
