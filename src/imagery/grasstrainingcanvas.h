#pragma once

#include "../georect/grassgcpcanvas.h"
#include <QColor>
#include <QRectF>
#include <QVector>

// Extends GrassGcpCanvas with rectangle-drawing training-area mode.
// In draw mode, click+drag defines a bbox; boxDrawn(n,s,e,w) is emitted on release.
// Stored training boxes are drawn as semi-transparent colored overlays.
class GrassTrainingCanvas : public GrassGcpCanvas
{
    Q_OBJECT
public:
    explicit GrassTrainingCanvas(const QString &label, QWidget *parent = nullptr);

    void setDrawMode(bool on);
    bool drawMode() const { return mDrawMode; }

    struct TrainingBox { double n, s, e, w; QColor color; int classId; };
    void setBoxes(const QVector<TrainingBox> &boxes);

signals:
    void boxDrawn(double north, double south, double east, double west);

protected:
    void mousePressEvent  (QMouseEvent *e) override;
    void mouseMoveEvent   (QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void paintEvent       (QPaintEvent *e) override;

private:
    bool    mDrawMode   = false;
    bool    mDrawing    = false;
    QPointF mPressMap;   // map coords of press
    QPointF mCurMap;     // map coords of current mouse
    QVector<TrainingBox> mBoxes;
};
