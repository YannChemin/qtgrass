#pragma once

#include <QWidget>
#include <QVector>
#include <QPair>

// Lightweight QPainter-based chart widget.
// Supports two chart types set at construction time.
class GrassChartWidget : public QWidget
{
    Q_OBJECT

  public:
    enum Type { LineChart, BarChart };

    explicit GrassChartWidget( Type type, QWidget *parent = nullptr );

    // Set data: (x, y) pairs.  For bar charts x is a label, y is the count.
    void setData( const QVector<QPair<double, double>> &data,
                  const QString &xLabel = {},
                  const QString &yLabel = {} );

    // Convenience for bar charts: pass integer counts
    void setBarData( const QVector<QPair<QString, int>> &data,
                     const QString &xLabel = {},
                     const QString &yLabel = {} );

    QSize sizeHint() const override { return { 500, 300 }; }
    QSize minimumSizeHint() const override { return { 200, 150 }; }

  protected:
    void paintEvent( QPaintEvent * ) override;

  private:
    void drawLineChart( QPainter &p, const QRect &plotArea ) const;
    void drawBarChart(  QPainter &p, const QRect &plotArea ) const;

    Type    mType;
    QString mXLabel, mYLabel;
    QVector<QPair<double, double>> mData;  // (x, y) for line; (rank, count) for bar
    QVector<QString> mBarLabels;           // x-axis labels for bar chart
};
