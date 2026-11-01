#pragma once
#include <QDialog>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class GrassInterface;

// ── Map history browser ───────────────────────────────────────────────────────
// Shows the creation history of a raster or vector map (r.info -h / v.info -h)
// and allows re-running the recorded commands.
class GrassHistoryBrowserDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GrassHistoryBrowserDialog(GrassInterface *iface, QWidget *parent = nullptr);

private:
    void loadHistory();
    void reRunSelected();

    QComboBox      *mTypeCombo    = nullptr;
    QComboBox      *mMapCombo     = nullptr;
    QPlainTextEdit *mHistoryEdit  = nullptr;
    QLabel         *mStatusLabel  = nullptr;
    GrassInterface *mIface        = nullptr;
};
