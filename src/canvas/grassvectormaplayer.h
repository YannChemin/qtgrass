#pragma once

/*!
    \file grassvectormaplayer.h
    \brief GRASS vector map layer with OSM Carto style support.
*/

#include "grassmaplayer.h"
#include "grassosmcarto.h"

#include <QColor>
#include <QPolygonF>
#include <QVector>
#include <Qt>

/*!
    \struct GrassVectorStyle
    \brief Visual style applied when rendering a GrassVectorMapLayer.

    All style properties can be set manually or overwritten in bulk by
    applyOsmPreset() using one of the OSM Carto named presets.
*/
struct GrassVectorStyle {
    QColor lineColor   = QColor( 30, 120, 200 );     /*!< Colour for lines and polygon borders. */
    QColor fillColor   = QColor( 30, 120, 200, 60 ); /*!< Fill colour for polygon interiors. */
    QColor pointColor  = QColor( 200, 60, 30 );      /*!< Colour for point symbols. */
    float  lineWidth   = 1.2f;                        /*!< Line stroke width in pixels. */
    float  pointSize   = 6.0f;                        /*!< Point symbol diameter in pixels. */

    bool drawFill    = true;  /*!< Fill polygon interiors. */
    bool drawLines   = true;  /*!< Draw line features and polygon borders. */
    bool drawPoints  = true;  /*!< Draw point and centroid features. */

    Qt::PenStyle   penStyle   = Qt::SolidLine;       /*!< Line dash pattern. */
    Qt::BrushStyle brushStyle = Qt::SolidPattern;    /*!< Area fill pattern. */

    OsmPointShape pointShape  = OsmPointShape::Circle; /*!< Point symbol glyph. */

    QString osmPreset; /*!< Active OSM Carto preset key (empty = custom). */

    /*!
        \brief Apply an OSM Carto named preset, overwriting all style fields.
        \param pr Preset descriptor returned by osmCartoLines() / osmCartoAreas() / osmCartoPoints().
    */
    void applyOsmPreset( const OsmCartoPreset &pr )
    {
        osmPreset  = pr.key;
        lineColor  = pr.lineColor;
        lineWidth  = pr.lineWidth;
        penStyle   = pr.penStyle;
        pointShape = pr.pointShape;
        pointColor = pr.pointColor.isValid() ? pr.pointColor : pr.lineColor;
        pointSize  = pr.pointSize;
        if ( pr.hasFill )
        {
            drawFill   = true;
            fillColor  = pr.fillColor;
            brushStyle = pr.brushStyle;
            drawLines  = true;
            drawPoints = false;
        }
        else
        {
            drawFill   = false;
            drawLines  = true;
            drawPoints = true;
        }
    }
};

/*!
    \class GrassVectorMapLayer
    \brief Renders a GRASS vector map using the GRASS C Vector API.

    Geometry (lines, areas, points) is loaded once from disk via the GRASS C
    API and cached as geographic-coordinate QPolygonF collections.  render()
    then reprojects these to viewport pixels on every call, enabling fast
    re-renders after pan/zoom without re-reading the map.

    A sigsetjmp crash guard wraps Vect_open_old() to survive corrupt topology
    files (such maps should be rebuilt with \c v.build).

    \sa GrassVectorStyle, GrassMapLayer
*/
class GrassVectorMapLayer : public GrassMapLayer
{
  public:
    /*!
        \brief Constructs a vector layer.
        \param name   GRASS vector map name.
        \param mapset Mapset that owns the map (empty = current mapset).
    */
    explicit GrassVectorMapLayer( const QString &name,
                                   const QString &mapset = {} );
    ~GrassVectorMapLayer() override = default;

    /*!
        \brief Returns the cached geographic extent.

        Valid only after the first successful render().  Intentionally does
        \e not call load() here — loading a corrupt topology file from the main
        thread (e.g. inside zoomToFullExtent) would crash the process.  The
        canvas falls back to raster extents for the initial zoom.
    */
    GrassExtent extent()  const override { return mExtent; }

    /*! \brief Mapset that owns this vector map. */
    QString     mapset()  const { return mMapset; }

    /*! \brief \c true after a successful load(). */
    bool        isLoaded() const { return mLoaded; }

    /*! \brief Human-readable error from the last failed load(). */
    QString     error()    const { return mError; }

    /*! \brief Mutable reference to the rendering style. */
    GrassVectorStyle       &style()       { return mStyle; }

    /*! \brief Read-only reference to the rendering style. */
    const GrassVectorStyle &style() const { return mStyle; }

    /*!  \brief Cached line/boundary geometries (geographic coords). Available after load(). */
    const QVector<QPolygonF> &lines()  const { return mLines;  }

    /*! \brief Cached area geometries (geographic coords). Available after load(). */
    const QVector<QPolygonF> &areas()  const { return mAreas;  }

    /*! \brief Cached point/centroid geometries (geographic coords). Available after load(). */
    const QVector<QPointF>   &points() const { return mPoints; }

    /*!
        \brief Mark the layer as needing a fresh load from disk.

        Call after \c v.build to pick up repaired topology.
    */
    void reload()
    {
        mLoaded = false;
        mError.clear();
        mLines.clear();
        mAreas.clear();
        mPoints.clear();
        mExtent = {};
    }

    /*!
        \brief Render the vector layer into a pixel image.
        \param viewW Viewport width in pixels.
        \param viewH Viewport height in pixels.
        \param vp    Geographic rectangle to display.
        \return ARGB32 image of size (\a viewW × \a viewH).
    */
    QImage render( int viewW, int viewH,
                   const GrassExtent &vp ) const override;

  private:
    bool load();

    QString mMapset;
    GrassVectorStyle mStyle;

    bool    mLoaded = false;
    QString mError;
    GrassExtent mExtent;

    QVector<QPolygonF> mLines;
    QVector<QPolygonF> mAreas;
    QVector<QPointF>   mPoints;
};
