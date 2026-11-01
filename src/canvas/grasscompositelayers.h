#pragma once

/*!
    \file grasscompositelayers.h
    \brief Native composite raster display layers rendered via the GRASS C API.

    Each class is a GrassMapLayer subclass that reads multiple raster bands
    directly from the GRASS database — no subprocess overhead — and composites
    them into a QImage in the render thread.

    \sa GrassAddLayerDialog, GrassLayerPanel
*/

#include "grassmaplayer.h"

#include <QColor>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

// ── GrassRgbLayer ─────────────────────────────────────────────────────────────

/*!
    \class GrassRgbLayer
    \brief Renders three raster maps as a true-colour (RGB) composite.

    Equivalent to the GRASS \c d.rgb module, but computed natively in Qt:
    each band is read at viewport resolution via \c Rast_get_d_row(), normalised
    to [0, 255] using the band's full-range min/max, and the three channels are
    combined into one ARGB32 image.

    \sa GrassIhsLayer
*/
class GrassRgbLayer : public GrassMapLayer
{
  public:
    /*!  \brief Configuration for a RGB composite layer. */
    struct Config {
        QString redMap,   redMapset;   /*!< Red band raster. */
        QString greenMap, greenMapset; /*!< Green band raster. */
        QString blueMap,  blueMapset;  /*!< Blue band raster. */
    };

    /*! \brief Constructs the layer from \a cfg. */
    explicit GrassRgbLayer( const Config &cfg );

    GrassExtent extent() const override;

    /*!
        \brief Render the RGB composite at viewport resolution.
        \param W   Viewport width in pixels.
        \param H   Viewport height in pixels.
        \param vp  Geographic viewport.
        \return ARGB32 image; null pixels where any band is NODATA.
    */
    QImage render( int W, int H, const GrassExtent &vp ) const override;

    const Config &config() const { return mCfg; }

    /*! \brief Reconfigure the layer (invalidates the render cache). */
    void setConfig( const Config &c ) { mCfg = c; invalidateCache(); mExtentLoaded = false; }

  private:
    Config mCfg;
    mutable GrassExtent mExtent;
    mutable bool        mExtentLoaded = false;
};

// ── GrassIhsLayer ─────────────────────────────────────────────────────────────

/*!
    \class GrassIhsLayer
    \brief Renders three raster maps as an IHS colour composite.

    Equivalent to \c d.ihs.  Intensity, Hue, and Saturation channels are read
    at viewport resolution, converted to RGB using the standard IHS→RGB
    formula, and composed into one ARGB32 image.

    Convention: I ∈ [0, 1], H ∈ [0°, 360°), S ∈ [0, 1].  Each band is
    normalised to that range from its full-range min/max.
*/
class GrassIhsLayer : public GrassMapLayer
{
  public:
    struct Config {
        QString intensityMap, intensityMapset; /*!< Intensity band. */
        QString hueMap,       hueMapset;       /*!< Hue band (normalised to [0°, 360°)). */
        QString satMap,       satMapset;       /*!< Saturation band (normalised to [0, 1]). */
    };

    explicit GrassIhsLayer( const Config &cfg );
    GrassExtent extent() const override;
    QImage render( int W, int H, const GrassExtent &vp ) const override;

    const Config &config() const { return mCfg; }
    void setConfig( const Config &c ) { mCfg = c; invalidateCache(); mExtentLoaded = false; }

  private:
    Config mCfg;
    mutable GrassExtent mExtent;
    mutable bool        mExtentLoaded = false;
};

// ── GrassShadeLayer ───────────────────────────────────────────────────────────

/*!
    \class GrassShadeLayer
    \brief Drapes a colour raster over a hillshade for 3-D relief effect.

    Equivalent to \c d.shade.  The colour raster supplies the RGB values;
    the shade raster (typically from \c r.relief or \c r.shade) is normalised
    to [0, 1] and used to modulate the colour channels:

    \code
    output.R = clamp( color.R × shade × (bright/50) )
    \endcode

    Pixels where the shade raster is NODATA fall back to the colour value.
*/
class GrassShadeLayer : public GrassMapLayer
{
  public:
    struct Config {
        QString colorMap, colorMapset; /*!< Colour raster (supplies RGB via its colour table). */
        QString shadeMap, shadeMapset; /*!< Hillshade raster (greyscale intensity). */
        int     bright = 50;           /*!< Brightness 0–100; 50 = no net change. */
    };

    explicit GrassShadeLayer( const Config &cfg );
    GrassExtent extent() const override;
    QImage render( int W, int H, const GrassExtent &vp ) const override;

    const Config &config() const { return mCfg; }
    void setConfig( const Config &c ) { mCfg = c; invalidateCache(); mExtentLoaded = false; }

  private:
    Config mCfg;
    mutable GrassExtent mExtent;
    mutable bool        mExtentLoaded = false;
};

// ── GrassRastNumLayer ─────────────────────────────────────────────────────────

/*!
    \class GrassRastNumLayer
    \brief Renders a raster map with cell values overlaid as text.

    Equivalent to \c d.rast.num.  The base raster is drawn using its colour
    table.  When the native cell is larger than \c minCellPx pixels on screen,
    the numeric value is drawn at the cell centre using QPainter.

    This is a true native Qt render — no GRASS subprocess is involved.
*/
class GrassRastNumLayer : public GrassMapLayer
{
  public:
    struct Config {
        QString map, mapset;
        int    decimals  = 2;        /*!< Decimal places for float values. */
        int    fontSize  = 9;        /*!< Font size in points. */
        QColor textColor = Qt::black;
        QColor bgColor   = QColor( 255, 255, 255, 160 );
        bool   showBg    = true;     /*!< Draw a semi-transparent background behind text. */
        bool   showNull  = false;    /*!< Draw "null" text for NODATA cells. */
        int    minCellPx = 20;       /*!< Minimum cell width in pixels before text is drawn. */
    };

    explicit GrassRastNumLayer( const Config &cfg );
    GrassExtent extent() const override;
    QImage render( int W, int H, const GrassExtent &vp ) const override;

    const Config &config() const { return mCfg; }
    void setConfig( const Config &c ) { mCfg = c; invalidateCache(); mExtentLoaded = false; }

  private:
    Config mCfg;
    mutable GrassExtent mExtent;
    mutable bool        mExtentLoaded = false;
};

// ── GrassRastArrowLayer ───────────────────────────────────────────────────────

/*!
    \class GrassRastArrowLayer
    \brief Draws flow or aspect direction as arrows overlaid on the map.

    Equivalent to \c d.rast.arrow.  The direction raster is read at cell
    resolution; an arrow is drawn at each cell centre whose pixel size is
    at least \c minCellPx.

    GRASS aspect convention: 0 = East, increasing counter-clockwise.
    If \c dirIsGrass is \c true the angle is mapped from GRASS to screen
    coordinates accordingly.

    An optional magnitude raster scales the arrow length proportionally.
*/
class GrassRastArrowLayer : public GrassMapLayer
{
  public:
    enum class ArrowStyle { Line, Filled, Outline };

    struct Config {
        QString    dirMap,  dirMapset; /*!< Direction/aspect raster (degrees). */
        QString    magMap,  magMapset; /*!< Magnitude raster (empty = fixed size). */
        QColor     color     = QColor( 220, 40, 40 );
        int        arrowSize = 20;    /*!< Base arrow length in pixels. */
        int        minCellPx = 12;   /*!< Minimum cell pixel size before arrows are drawn. */
        ArrowStyle style     = ArrowStyle::Line;
        bool       dirIsGrass = true; /*!< Apply GRASS aspect→bearing coordinate conversion. */
    };

    explicit GrassRastArrowLayer( const Config &cfg );
    GrassExtent extent() const override;
    QImage render( int W, int H, const GrassExtent &vp ) const override;

    const Config &config() const { return mCfg; }
    void setConfig( const Config &c ) { mCfg = c; invalidateCache(); mExtentLoaded = false; }

  private:
    Config mCfg;
    mutable GrassExtent mExtent;
    mutable bool        mExtentLoaded = false;
};

// ── GrassVectThematicLayer ────────────────────────────────────────────────────

/*!
    \class GrassVectThematicLayer
    \brief Renders vector features coloured by a numeric attribute (choropleth).

    Equivalent to \c d.vect.thematic.  Attribute values are read from the
    vector's linked table, classified into N equal-interval or quantile breaks,
    and each feature is filled with the corresponding colour from a diverging or
    sequential ramp.

    The rendering uses the GRASS C vector API for geometry and spawns a single
    \c v.db.select subprocess call to fetch the attribute column.

    \sa GrassVectChartLayer
*/
class GrassVectThematicLayer : public GrassMapLayer
{
  public:
    enum class ClassMethod { EqualInterval, Quantile, StdDev };
    enum class ColorRamp   { YlOrRd, Blues, Greens, RdYlGn, Viridis, Plasma };

    struct Config {
        QString     map, mapset;
        QString     column;           /*!< Numeric attribute column to classify. */
        int         nClasses  = 5;    /*!< Number of classes. */
        ClassMethod method    = ClassMethod::EqualInterval;
        ColorRamp   ramp      = ColorRamp::YlOrRd;
        bool        reverse   = false;
        QColor      nullColor = Qt::lightGray;
        float       lineWidth = 0.6f;
        int         layer     = 1;    /*!< GRASS attribute layer. */
    };

    explicit GrassVectThematicLayer( const Config &cfg );
    GrassExtent extent() const override;
    QImage render( int W, int H, const GrassExtent &vp ) const override;

    const Config &config() const { return mCfg; }
    void setConfig( const Config &c ) { mCfg = c; invalidateCache(); mExtentLoaded = false; mAttrLoaded = false; }

  private:
    void loadAttributes() const;
    QColor classColor( int cls, int nCls ) const;

    Config mCfg;
    mutable GrassExtent            mExtent;
    mutable bool                   mExtentLoaded = false;
    mutable bool                   mAttrLoaded   = false;
    mutable QMap<int, double>      mCatValues;   // cat → attribute value
    mutable QVector<double>        mBreaks;       // class break values
};

// ── GrassVectChartLayer ───────────────────────────────────────────────────────

/*!
    \class GrassVectChartLayer
    \brief Draws proportional pie or bar charts at vector feature centroids.

    Equivalent to \c d.vect.chart.  One or more numeric attribute columns are
    read for each feature and visualised as a pie or bar chart centred at the
    feature's centroid (or point location).  Charts scale with the sum of the
    selected columns.

    \sa GrassVectThematicLayer
*/
class GrassVectChartLayer : public GrassMapLayer
{
  public:
    enum class ChartType { Pie, Bar };

    struct Config {
        QString          map, mapset;
        QStringList      columns;        /*!< Numeric columns (one per chart segment/bar). */
        QList<QColor>    colors;         /*!< Per-column colours (auto-assigned when empty). */
        ChartType        chartType = ChartType::Pie;
        int              maxSize   = 40; /*!< Maximum chart diameter / bar height in pixels. */
        bool             scale     = true; /*!< Scale chart size by feature value sum. */
        int              layer     = 1;
    };

    explicit GrassVectChartLayer( const Config &cfg );
    GrassExtent extent() const override;
    QImage render( int W, int H, const GrassExtent &vp ) const override;

    const Config &config() const { return mCfg; }
    void setConfig( const Config &c ) { mCfg = c; invalidateCache(); mExtentLoaded = false; mAttrLoaded = false; }

  private:
    void loadAttributes() const;
    QColor columnColor( int idx ) const;
    void drawPie( QPainter &p, QPointF centre, double size,
                  const QVector<double> &vals ) const;
    void drawBar( QPainter &p, QPointF centre, double size,
                  const QVector<double> &vals ) const;

    Config mCfg;
    mutable GrassExtent                      mExtent;
    mutable bool                             mExtentLoaded = false;
    mutable bool                             mAttrLoaded   = false;
    mutable QMap<int, QVector<double>>       mCatValues;  // cat → column values
    mutable double                           mMaxSum = 1.0;
};

// ── GrassLabelLayer ───────────────────────────────────────────────────────────

/*!
    \class GrassLabelLayer
    \brief Draws attribute text labels at vector feature locations.

    Equivalent to \c d.label.  For each visible feature, the value of
    \c labelColumn is drawn at the feature centroid (or point) using the
    configured font and colour.  An optional halo improves readability on
    complex backgrounds.

    \sa GrassVectThematicLayer
*/
class GrassLabelLayer : public GrassMapLayer
{
  public:
    struct Config {
        QString  map, mapset;
        QString  labelColumn;          /*!< Attribute column whose value is displayed. */
        int      fontSize   = 10;
        QColor   textColor  = Qt::black;
        QColor   haloColor  = Qt::white;
        int      haloWidth  = 2;       /*!< 0 = no halo. */
        bool     bold       = false;
        bool     italic     = false;
        int      xOffset    = 0;       /*!< Horizontal pixel offset from the feature centre. */
        int      yOffset    = 0;       /*!< Vertical pixel offset (positive = down). */
        int      layer      = 1;
    };

    explicit GrassLabelLayer( const Config &cfg );
    GrassExtent extent() const override;
    QImage render( int W, int H, const GrassExtent &vp ) const override;

    const Config &config() const { return mCfg; }
    void setConfig( const Config &c ) { mCfg = c; invalidateCache(); mExtentLoaded = false; mAttrLoaded = false; }

  private:
    void loadAttributes() const;

    Config mCfg;
    mutable GrassExtent       mExtent;
    mutable bool              mExtentLoaded = false;
    mutable bool              mAttrLoaded   = false;
    mutable QMap<int,QString> mLabels;          // cat → label string
    mutable QMap<int,QPointF> mCentroids;       // cat → geographic centroid
};

// ── GrassGeodesicLayer ────────────────────────────────────────────────────────

/*!
    \class GrassGeodesicLayer
    \brief Draws a great circle arc between two geographic points.

    Equivalent to \c d.geodesic.  The shortest path on the sphere (great circle)
    between the two WGS84 coordinates is approximated with \c nSegments line
    segments and drawn on the canvas in any CRS.

    The arc is computed analytically without any external library dependency.
*/
class GrassGeodesicLayer : public GrassMapLayer
{
  public:
    struct Config {
        double  lon1 = 0, lat1 = 0;  /*!< Start point (decimal degrees, WGS84). */
        double  lon2 = 0, lat2 = 0;  /*!< End point. */
        QColor  color     = QColor( 220, 80, 20 );
        float   lineWidth = 1.5f;
        Qt::PenStyle penStyle = Qt::SolidLine;
        int     nSegments = 180;     /*!< Number of polyline segments approximating the arc. */
    };

    explicit GrassGeodesicLayer( const Config &cfg );
    GrassExtent extent() const override;
    QImage render( int W, int H, const GrassExtent &vp ) const override;

    const Config &config() const { return mCfg; }
    void setConfig( const Config &c ) { mCfg = c; invalidateCache(); }

  private:
    // Returns the great circle waypoints as (lon, lat) pairs in degrees.
    QVector<QPointF> waypoints() const;

    Config mCfg;
};

// ── GrassRhumblineLayer ───────────────────────────────────────────────────────

/*!
    \class GrassRhumblineLayer
    \brief Draws a rhumb line (loxodrome) between two geographic points.

    Equivalent to \c d.rhumbline.  A rhumb line is a path that crosses all
    meridians at the same angle — it appears as a straight line on a Mercator
    projection but as a spiral on others.  The line is approximated with
    \c nSegments polyline segments.
*/
class GrassRhumblineLayer : public GrassMapLayer
{
  public:
    struct Config {
        double  lon1 = 0, lat1 = 0;  /*!< Start point (decimal degrees). */
        double  lon2 = 0, lat2 = 0;  /*!< End point. */
        QColor  color     = QColor( 20, 120, 220 );
        float   lineWidth = 1.5f;
        Qt::PenStyle penStyle = Qt::DashLine;
        int     nSegments = 90;
    };

    explicit GrassRhumblineLayer( const Config &cfg );
    GrassExtent extent() const override;
    QImage render( int W, int H, const GrassExtent &vp ) const override;

    const Config &config() const { return mCfg; }
    void setConfig( const Config &c ) { mCfg = c; invalidateCache(); }

  private:
    QVector<QPointF> waypoints() const;

    Config mCfg;
};
