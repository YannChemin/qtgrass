#pragma once
#include <QDialog>
#include <QVector>
#include <QPair>

class QComboBox;
class QLabel;
class QLineEdit;

// Lightweight time-series chart (no dependency on profile module)
class TplotChart : public QWidget
{
    Q_OBJECT
public:
    explicit TplotChart(QWidget *parent = nullptr);
    // data: (time_index, value); labels: x-axis date strings
    void setData(const QVector<QPair<double,double>> &data,
                 const QStringList &xLabels = {});

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVector<QPair<double,double>> mData;
    QStringList mXLabels;
};

// ── Temporal time-series plot dialog ─────────────────────────────────────────
// Queries t.rast.list to get the map sequence, then runs r.what at the
// user-specified point for each map and plots the resulting time series.
class GrassTplotDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GrassTplotDialog(QWidget *parent = nullptr);

private:
    void loadStrds();
    void runTplot();

    QComboBox  *mStrdCombo    = nullptr;
    QLineEdit  *mEastEdit     = nullptr;
    QLineEdit  *mNorthEdit    = nullptr;
    TplotChart *mChart        = nullptr;
    QLabel     *mStatusLabel  = nullptr;
};
