#pragma once

#include <QWidget>
#include <QColor>
#include <QString>
#include <QVector>

// Per-class statistics for one band
struct BandStats {
    double mean  = 0;
    double stdDev = 0;
    int    count  = 0;
};

// One imagery class with per-band stats
struct IClassDef {
    int     id      = 0;
    QString name;
    QColor  color   = Qt::blue;

    // Training bboxes [n, s, e, w] in map coordinates
    struct BBox { double n, s, e, w; };
    QVector<BBox> areas;

    // Computed stats indexed by band index
    QVector<BandStats> stats;

    bool hasStats() const {
        for (const auto &s : stats)
            if (s.count > 0) return true;
        return false;
    }
};

// ── 2-D scatter plot widget ───────────────────────────────────────────────────
// Shows class separability as ellipses (mean ± 2σ) for two selected bands.
class GrassScatterWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GrassScatterWidget(QWidget *parent = nullptr);

    void setClasses(const QVector<IClassDef> &classes);
    void setBandIndices(int bandX, int bandY);
    void setBandLabels(const QStringList &labels);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    struct AxisRange { double lo, hi; };
    AxisRange computeRange(int bandIdx) const;
    QPointF dataToWidget(double x, double y,
                         const AxisRange &rx, const AxisRange &ry,
                         const QRectF &plotArea) const;

    QVector<IClassDef> mClasses;
    QStringList        mBandLabels;
    int                mBandX = 0;
    int                mBandY = 1;
};
