#pragma once

#include <QWidget>
#include <QPixmap>
#include <QPointF>
#include <QVector>

// A single GCP point (one matched pair of source ↔ reference coordinates).
struct GcpPoint {
    int     id      = 0;
    double  srcX    = 0, srcY    = 0;  // source (unrectified) coords
    double  refX    = 0, refY    = 0;  // reference (target) coords
    bool    enabled = true;

    // Filled in by PolyTransform::computeResiduals()
    double  resX    = 0, resY    = 0;  // residuals in ref units
    double  rms     = 0;
};

// Polynomial coordinate transformation (order 1, 2, or 3) via least squares.
class PolyTransform
{
public:
    bool fit(const QVector<GcpPoint> &gcps, int order);
    bool isValid() const { return mValid; }

    void transform(double x, double y, double &e, double &n) const;
    void computeResiduals(QVector<GcpPoint> &gcps) const;
    double totalRms(const QVector<GcpPoint> &gcps) const;

    int minPoints() const { return (mOrder + 1) * (mOrder + 2) / 2; }

private:
    static QVector<double> basis(double x, double y, int order);
    static bool gaussElim(QVector<QVector<double>> A, QVector<double> &b);

    bool           mValid = false;
    int            mOrder = 1;
    QVector<double> mCoeffE, mCoeffN;
};

// ── Simplified map canvas for GCP picking ────────────────────────────────────
// Renders a GRASS raster via d.rast at the current computational region.
// Pan/zoom is implemented as Qt-level pixel transformation on the cached pixmap.
// In AddGcp mode a left-click emits coordClicked(east, north).
class GrassGcpCanvas : public QWidget
{
    Q_OBJECT
public:
    explicit GrassGcpCanvas(const QString &label, QWidget *parent = nullptr);

    void setMap(const QString &mapName);
    void renderMap();   // (re)renders the current map into the cache

    // GCP markers: call whenever the GCP list changes
    void setGcps(const QVector<GcpPoint> &gcps, bool showSrc);

    void setPickMode(bool on) { mPickMode = on; updateCursor(); }
    bool pickMode() const    { return mPickMode; }

    // Fit the cached pixmap into the widget
    void zoomFit();

    QString mapName() const { return mMapName; }

signals:
    void coordClicked(double east, double north);

protected:
    void paintEvent     (QPaintEvent *)    override;
    void resizeEvent    (QResizeEvent *)   override;
    void mousePressEvent(QMouseEvent *)    override;
    void mouseMoveEvent (QMouseEvent *)    override;
    void mouseReleaseEvent(QMouseEvent *)  override;
    void wheelEvent     (QWheelEvent *)    override;

protected:
    void    updateCursor();
    QPointF mapToWidget(double east, double north) const;
    void    widgetToMap(QPointF px, double &east, double &north) const;

    QString mLabel;
    QString mMapName;
    QPixmap mPixmap;

    // Geographic extent of the cached pixmap (from g.region -g at render time)
    double mExtW = 0, mExtE = 1, mExtN = 1, mExtS = 0;

    // Pan/zoom state
    QPointF mOffset;   // top-left corner of the pixmap in widget pixels
    double  mScale = 1.0;  // pixmap pixels per screen pixel

    bool    mPanning = false;
    QPointF mLastMouse;

    bool    mPickMode = false;

    // GCP markers to draw
    struct Marker { double east, north; int id; bool active; };
    QVector<Marker> mMarkers;
    bool            mShowSrc = true;
};
