#pragma once

#include <QDialog>

class GrassChartWidget;
class QLabel;
class QSpinBox;

// Shows an r.stats histogram for a raster map.
class GrassHistogramDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit GrassHistogramDialog( const QString &rasterMap,
                                    const QString &mapset = {},
                                    QWidget *parent = nullptr );

  private slots:
    void refresh();

  private:
    void loadData( int nSteps );

    QString          mMapRef;
    GrassChartWidget *mChart  = nullptr;
    QLabel           *mStatus = nullptr;
    QSpinBox         *mSteps  = nullptr;
};
