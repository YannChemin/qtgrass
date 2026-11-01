#include "grasstrainingcanvas.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

GrassTrainingCanvas::GrassTrainingCanvas(const QString &label, QWidget *parent)
    : GrassGcpCanvas(label, parent)
{}

void GrassTrainingCanvas::setDrawMode(bool on)
{
    mDrawMode = on;
    mDrawing  = false;
    setCursor(on ? Qt::CrossCursor : Qt::OpenHandCursor);
}

void GrassTrainingCanvas::setBoxes(const QVector<TrainingBox> &boxes)
{
    mBoxes = boxes;
    update();
}

void GrassTrainingCanvas::mousePressEvent(QMouseEvent *e)
{
    if (mDrawMode && e->button() == Qt::LeftButton)
    {
        double east, north;
        widgetToMap(e->position(), east, north);
        mPressMap = { east, north };
        mCurMap   = mPressMap;
        mDrawing  = true;
        e->accept();
        return;
    }
    GrassGcpCanvas::mousePressEvent(e);
}

void GrassTrainingCanvas::mouseMoveEvent(QMouseEvent *e)
{
    if (mDrawing)
    {
        double east, north;
        widgetToMap(e->position(), east, north);
        mCurMap = { east, north };
        update();
        e->accept();
        return;
    }
    GrassGcpCanvas::mouseMoveEvent(e);
}

void GrassTrainingCanvas::mouseReleaseEvent(QMouseEvent *e)
{
    if (mDrawing && e->button() == Qt::LeftButton)
    {
        mDrawing = false;
        const double n = qMax(mPressMap.y(), mCurMap.y());
        const double s = qMin(mPressMap.y(), mCurMap.y());
        const double e_ = qMax(mPressMap.x(), mCurMap.x());
        const double w  = qMin(mPressMap.x(), mCurMap.x());
        if (n > s && e_ > w)
            emit boxDrawn(n, s, e_, w);
        update();
        e->accept();
        return;
    }
    GrassGcpCanvas::mouseReleaseEvent(e);
}

void GrassTrainingCanvas::paintEvent(QPaintEvent *e)
{
    GrassGcpCanvas::paintEvent(e);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Draw stored training boxes
    for (const TrainingBox &box : mBoxes)
    {
        const QPointF tl = mapToWidget(box.w, box.n);
        const QPointF br = mapToWidget(box.e, box.s);
        const QRectF  r(tl, br);
        QColor fill = box.color;
        fill.setAlpha(50);
        p.setBrush(fill);
        p.setPen(QPen(box.color, 2));
        p.drawRect(r);
    }

    // Rubber band while dragging
    if (mDrawing)
    {
        const QPointF tl = mapToWidget(qMin(mPressMap.x(), mCurMap.x()),
                                       qMax(mPressMap.y(), mCurMap.y()));
        const QPointF br = mapToWidget(qMax(mPressMap.x(), mCurMap.x()),
                                       qMin(mPressMap.y(), mCurMap.y()));
        p.setPen(QPen(Qt::yellow, 2, Qt::DashLine));
        p.setBrush(QColor(255, 255, 0, 40));
        p.drawRect(QRectF(tl, br));
    }
}
