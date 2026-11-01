#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSlider;
class GrassNvizWidget;
class GrassInterface;

// ── 3-D view dialog ───────────────────────────────────────────────────────────
class GrassNvizDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GrassNvizDialog(GrassInterface *iface, QWidget *parent = nullptr);

private:
    void loadMap();
    void exportPng();

    GrassInterface  *mIface       = nullptr;
    GrassNvizWidget *mView        = nullptr;
    QComboBox       *mElevCombo   = nullptr;
    QComboBox       *mColorCombo  = nullptr;
    QDoubleSpinBox  *mExagSpin    = nullptr;
    QCheckBox       *mWireCheck   = nullptr;
    QLabel          *mStatusLabel = nullptr;
};
