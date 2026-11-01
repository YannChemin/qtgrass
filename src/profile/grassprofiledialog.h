#pragma once

#include <QDialog>
#include <QVector>
#include <QPair>

class GrassChartWidget;
class QLabel;
class QComboBox;

// Shows an r.profile elevation chart for a pair of coordinates.
// Launched from the canvas Profile tool: when the user draws a line,
// the dialog opens and the chart is populated.
class GrassProfileDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit GrassProfileDialog( QWidget *parent = nullptr );

    // Run r.profile with the given raster + coordinate list and display result.
    // coords: list of (east, north) waypoints along the profile line.
    void runProfile( const QString &rasterMap,
                     const QString &mapset,
                     const QVector<QPointF> &coords );

  private:
    void parseAndDisplay( const QString &output );

    GrassChartWidget *mChart     = nullptr;
    QLabel           *mStatus    = nullptr;
    QLabel           *mInfoLabel = nullptr;
};
