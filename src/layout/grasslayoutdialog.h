#pragma once

#include <QDialog>
#include <QGraphicsScene>

class QAction;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QGraphicsView;
class QLabel;
class QLineEdit;
class QScrollArea;
class QSpinBox;
class QTextEdit;
class GrassInterface;
class GrassMapCanvas;
class GrassLayoutItem;
class GrassLayoutMapItem;
class GrassLayoutScaleBarItem;

// ─── Scene — white page background ───────────────────────────────────────────
class GrassLayoutScene : public QGraphicsScene
{
    Q_OBJECT
public:
    explicit GrassLayoutScene(QObject *parent = nullptr);
    void setPageSize(double wMM, double hMM);

protected:
    void drawBackground(QPainter *p, const QRectF &rect) override;
    void keyPressEvent(QKeyEvent *e) override;

private:
    double mPageW = 210, mPageH = 297;
};

// ─── Main print layout dialog ─────────────────────────────────────────────────
class GrassLayoutDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GrassLayoutDialog(GrassInterface *iface, GrassMapCanvas *canvas,
                                QWidget *parent = nullptr);

    void loadFile(const QString &path);

private:
    void buildToolBar();
    void buildScene();
    void buildPropsPanel();
    void applyPageSettings();

    void addMapFrame();
    void addLegend();
    void addScaleBar();
    void addNorthArrow();
    void addText();
    void addImage();

    void onSelectionChanged();
    void populatePropsForItem(GrassLayoutItem *item);
    void clearProps();
    GrassLayoutItem *selectedItem() const;
    GrassLayoutMapItem *findFirstMapItem() const;

    void saveFile(const QString &path);
    void doExportPng();
    void doExportPdf();
    void doExportSvg();
    void renderToDevice(QPainter *painter, double scaleX, double scaleY);

    // ─ data ────────────────────────────────────────────────────────────────
    GrassInterface   *mIface  = nullptr;
    GrassMapCanvas   *mCanvas = nullptr;
    GrassLayoutScene *mScene  = nullptr;
    QGraphicsView    *mView   = nullptr;

    double mPageW   = 210;   // mm
    double mPageH   = 297;
    int    mExportDpi = 150;

    QString mFilePath;

    // Props panel
    QWidget      *mPropsWidget = nullptr;
    QScrollArea  *mPropsScroll = nullptr;
    QFormLayout  *mPropsForm   = nullptr;   // rebuilt per selection

    // Position/size spinboxes (always shown)
    QDoubleSpinBox *mXSpin = nullptr;
    QDoubleSpinBox *mYSpin = nullptr;
    QDoubleSpinBox *mWSpin = nullptr;
    QDoubleSpinBox *mHSpin = nullptr;

    // Page settings (shown when nothing selected)
    QComboBox *mPageSizeCombo   = nullptr;
    QComboBox *mOrientCombo     = nullptr;
    QSpinBox  *mDpiSpin         = nullptr;
};
