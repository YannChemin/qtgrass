#pragma once

#include "grassscatterwidget.h"
#include <QDialog>
#include <QVector>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSplitter;
class QTableWidget;
class GrassTrainingCanvas;
class GrassInterface;

// ── Supervised classification dialog (i.class replacement) ───────────────────
class GrassIclassDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GrassIclassDialog(GrassInterface *iface, QWidget *parent = nullptr);

private:
    void buildUi();
    void loadGroup();

    void addClass();
    void deleteClass();
    void editClassColor();
    void selectClass(int row);

    void startDrawing();    // enter training-box drawing mode
    void onBoxDrawn(double n, double s, double e, double w);
    void clearClassAreas();
    void updateBoxOverlay();

    void computeStats();
    BandStats statsForBand(const IClassDef::BBox &bbox, const QString &band);

    void runClassify();
    bool buildTrainingMap(const QString &mapName);
    void log(const QString &msg, bool error = false);

    // ─ widgets ────────────────────────────────────────────────────────────
    QComboBox         *mGroupCombo      = nullptr;
    QComboBox         *mDisplayBand     = nullptr;
    GrassTrainingCanvas *mCanvas        = nullptr;
    GrassScatterWidget  *mScatter       = nullptr;
    QComboBox         *mScatterBandX    = nullptr;
    QComboBox         *mScatterBandY    = nullptr;
    QTableWidget      *mClassTable      = nullptr;
    QPushButton       *mDrawBtn         = nullptr;
    QLabel            *mStatusLabel     = nullptr;
    QLabel            *mLogLabel        = nullptr;
    QComboBox         *mMethodCombo     = nullptr;
    QLineEdit         *mOutputEdit      = nullptr;

    // ─ state ──────────────────────────────────────────────────────────────
    GrassInterface    *mIface           = nullptr;
    QVector<IClassDef> mClasses;
    QStringList        mBands;          // band names in group
    int                mSelectedClass   = -1;
    int                mNextClassId     = 1;
    bool               mDrawMode        = false;
};
