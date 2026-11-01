#pragma once
#include <QDialog>

class GrassInterface;

// ── Network Analysis launcher ─────────────────────────────────────────────────
// Shows available v.net.* modules with descriptions and opens the standard
// GrassModuleDialog for the selected operation.
class GrassNetAnalysisDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GrassNetAnalysisDialog(GrassInterface *iface, QWidget *parent = nullptr);
private:
    GrassInterface *mIface = nullptr;
};
