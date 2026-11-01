#include "grassscatterwidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <algorithm>
#include <limits>

GrassScatterWidget::GrassScatterWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(300, 250);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(245, 245, 250));
    setPalette(pal);
}

void GrassScatterWidget::setClasses(const QVector<IClassDef> &classes)
{
    mClasses = classes;
    update();
}

void GrassScatterWidget::setBandIndices(int bandX, int bandY)
{
    mBandX = bandX;
    mBandY = bandY;
    update();
}

void GrassScatterWidget::setBandLabels(const QStringList &labels)
{
    mBandLabels = labels;
    update();
}

GrassScatterWidget::AxisRange GrassScatterWidget::computeRange(int bandIdx) const
{
    double lo =  std::numeric_limits<double>::max();
    double hi = -std::numeric_limits<double>::max();
    for (const auto &cls : mClasses)
    {
        if (bandIdx >= cls.stats.size()) continue;
        const auto &s = cls.stats[bandIdx];
        if (s.count == 0) continue;
        lo = qMin(lo, s.mean - 3 * s.stdDev);
        hi = qMax(hi, s.mean + 3 * s.stdDev);
    }
    if (lo >= hi) { lo = 0; hi = 255; }
    const double margin = (hi - lo) * 0.05;
    return { lo - margin, hi + margin };
}

QPointF GrassScatterWidget::dataToWidget(double x, double y,
                                          const AxisRange &rx,
                                          const AxisRange &ry,
                                          const QRectF &plotArea) const
{
    const double px = plotArea.left() +
        (x - rx.lo) / (rx.hi - rx.lo) * plotArea.width();
    const double py = plotArea.bottom() -
        (y - ry.lo) / (ry.hi - ry.lo) * plotArea.height();
    return { px, py };
}

void GrassScatterWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF plotArea = QRectF(55, 15, width() - 75, height() - 50);
    p.fillRect(rect(), QColor(245, 245, 250));
    p.fillRect(plotArea, Qt::white);

    const AxisRange rx = computeRange(mBandX);
    const AxisRange ry = computeRange(mBandY);

    // Grid
    p.setPen(QPen(QColor(220, 220, 225), 0.5));
    const int nGrid = 5;
    for (int i = 0; i <= nGrid; ++i)
    {
        const double fx = plotArea.left() + i * plotArea.width()  / nGrid;
        const double fy = plotArea.top()  + i * plotArea.height() / nGrid;
        p.drawLine(QPointF(fx, plotArea.top()), QPointF(fx, plotArea.bottom()));
        p.drawLine(QPointF(plotArea.left(), fy), QPointF(plotArea.right(), fy));
    }

    // Axes
    p.setPen(QPen(Qt::black, 1));
    p.drawRect(plotArea);

    // Axis labels
    QFont lf; lf.setPointSize(7); p.setFont(lf); p.setPen(Qt::black);
    for (int i = 0; i <= nGrid; ++i)
    {
        const double vx = rx.lo + i * (rx.hi - rx.lo) / nGrid;
        const double vy = ry.lo + i * (ry.hi - ry.lo) / nGrid;
        const double px = plotArea.left() + i * plotArea.width() / nGrid;
        const double py = plotArea.bottom() - i * plotArea.height() / nGrid;

        p.drawText(QRectF(px - 20, plotArea.bottom() + 3, 40, 14),
                   Qt::AlignCenter, QString::number(vx, 'f', 0));
        p.drawText(QRectF(2, py - 7, 50, 14),
                   Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(vy, 'f', 0));
    }

    // Axis titles
    const QString xLabel = (mBandX < mBandLabels.size())
                            ? mBandLabels[mBandX] : QString("Band %1").arg(mBandX + 1);
    const QString yLabel = (mBandY < mBandLabels.size())
                            ? mBandLabels[mBandY] : QString("Band %1").arg(mBandY + 1);
    QFont tf; tf.setPointSize(8); p.setFont(tf);
    p.drawText(QRectF(plotArea.left(), plotArea.bottom() + 18, plotArea.width(), 16),
               Qt::AlignCenter, xLabel);

    // Rotated Y label
    p.save();
    p.translate(10, plotArea.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-plotArea.height() / 2, -10, plotArea.height(), 20),
               Qt::AlignCenter, yLabel);
    p.restore();

    if (mClasses.isEmpty() || rx.hi <= rx.lo || ry.hi <= ry.lo)
    {
        p.setPen(Qt::gray); p.setFont(tf);
        p.drawText(plotArea, Qt::AlignCenter,
                   mClasses.isEmpty() ? "No classes defined"
                                      : "No statistics computed yet.\nAdd training areas and compute stats.");
        return;
    }

    // Class ellipses (mean ± 2σ)
    p.setClipRect(plotArea);
    for (const auto &cls : mClasses)
    {
        if (mBandX >= cls.stats.size() || mBandY >= cls.stats.size()) continue;
        const auto &sx = cls.stats[mBandX];
        const auto &sy = cls.stats[mBandY];
        if (sx.count == 0 || sy.count == 0) continue;

        const QPointF center = dataToWidget(sx.mean, sy.mean, rx, ry, plotArea);
        const QPointF right  = dataToWidget(sx.mean + 2 * sx.stdDev, sy.mean, rx, ry, plotArea);
        const QPointF top    = dataToWidget(sx.mean, sy.mean + 2 * sy.stdDev, rx, ry, plotArea);

        const double rw = qAbs(right.x() - center.x());
        const double rh = qAbs(top.y()   - center.y());

        QColor fill = cls.color; fill.setAlpha(60);
        p.setBrush(fill);
        p.setPen(QPen(cls.color, 1.5));
        p.drawEllipse(center, rw, rh);

        // Cross at mean
        p.setPen(QPen(cls.color, 2));
        p.drawLine(center + QPointF(-5, 0), center + QPointF(5, 0));
        p.drawLine(center + QPointF(0, -5), center + QPointF(0, 5));
    }
    p.setClipping(false);

    // Legend
    const int legX = static_cast<int>(plotArea.right()) + 4;
    int legY = static_cast<int>(plotArea.top());
    QFont lef; lef.setPointSize(7); p.setFont(lef);
    for (const auto &cls : mClasses)
    {
        if (legY + 14 > height()) break;
        p.fillRect(QRect(legX, legY + 2, 10, 10), cls.color);
        p.setPen(Qt::black);
        p.drawText(QRectF(legX + 13, legY, 55, 14),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(cls.name, Qt::ElideRight, 55));
        legY += 16;
    }
}
