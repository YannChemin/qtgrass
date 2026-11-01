#pragma once

#include "grassgcpcanvas.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class GrassInterface;

class GrassGeoRectDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GrassGeoRectDialog(GrassInterface *iface, QWidget *parent = nullptr);

private:
    void buildUi();
    void buildGcpTable(QVBoxLayout *vbox);

    // GCP workflow
    void startAddGcp();
    void onSrcCoordPicked(double east, double north);
    void onRefCoordPicked(double east, double north);
    void addGcpToList(double srcX, double srcY, double refX, double refY);
    void deleteSelectedGcp();
    void updateGcpTable();
    void updateResiduals();
    void refreshMarkers();

    // File I/O
    void savePoints();
    void loadPoints();
    QString pointsFilePath() const;

    // Execution
    void createGroup();
    void rectify();
    void log(const QString &msg, bool error = false);

    // UI helpers
    QStringList listVectors() const;

    // ─ widgets ────────────────────────────────────────────────────────────
    QComboBox      *mSrcCombo      = nullptr;
    QComboBox      *mRefCombo      = nullptr;
    QLineEdit      *mGroupEdit     = nullptr;
    QComboBox      *mOrderCombo    = nullptr;
    QComboBox      *mMethodCombo   = nullptr;
    QLineEdit      *mExtensionEdit = nullptr;

    GrassGcpCanvas *mSrcCanvas     = nullptr;
    GrassGcpCanvas *mRefCanvas     = nullptr;

    QTableWidget   *mGcpTable      = nullptr;
    QLabel         *mRmsLabel      = nullptr;
    QLabel         *mStatusLabel   = nullptr;

    QLabel         *mPickStatus    = nullptr;   // "Click source / reference"
    QLabel         *mLog           = nullptr;

    // ─ state ──────────────────────────────────────────────────────────────
    GrassInterface *mIface         = nullptr;
    QVector<GcpPoint> mGcps;
    int             mNextId        = 1;
    PolyTransform   mTransform;

    // Pending GCP being built
    bool            mWaitSrc = false;   // waiting for source click
    bool            mWaitRef = false;   // waiting for reference click
    double          mPendSrcX = 0, mPendSrcY = 0;

    bool            mUpdatingTable = false;
};
