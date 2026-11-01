#pragma once

#include "grasstemporaldataset.h"
#include <QDialog>
#include <QVector>
#include <QPixmap>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSlider;
class QTimer;
class GrassInterface;

class GrassAnimationDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GrassAnimationDialog(GrassInterface *iface, QWidget *parent = nullptr);
    ~GrassAnimationDialog() override;

private:
    void buildUi();
    void refreshDatasetList();
    void loadDataset();
    void preRender();                          // renders all frames sequentially
    QPixmap renderFrame(const TemporalFrame &frame);   // render one frame → QPixmap
    void showFrame(int idx);
    void play();
    void pause();
    void stepForward();
    void stepBack();
    void exportPngSequence();
    void exportMp4();
    void updateControls(bool hasFrames);

    // ─── widgets ───────────────────────────────────────────────────────────
    QComboBox      *mDatasetCombo  = nullptr;
    QComboBox      *mTypeCombo     = nullptr;
    QComboBox      *mSizeCombo     = nullptr;
    QLabel         *mPreviewLabel  = nullptr;
    QSlider        *mSlider        = nullptr;
    QLabel         *mTimeLabel     = nullptr;
    QLabel         *mFrameLabel    = nullptr;
    QPushButton    *mFirstBtn      = nullptr;
    QPushButton    *mPrevBtn       = nullptr;
    QPushButton    *mPlayBtn       = nullptr;
    QPushButton    *mNextBtn       = nullptr;
    QPushButton    *mLastBtn       = nullptr;
    QDoubleSpinBox *mFpsSpin       = nullptr;
    QCheckBox      *mLoopCheck     = nullptr;
    QCheckBox      *mLegendCheck   = nullptr;
    QPushButton    *mPreRenderBtn  = nullptr;
    QPushButton    *mExportPngBtn  = nullptr;
    QPushButton    *mExportMp4Btn  = nullptr;

    // ─── data ──────────────────────────────────────────────────────────────
    GrassInterface       *mIface        = nullptr;
    GrassTemporalDataset *mDataset      = nullptr;
    QVector<QPixmap>      mCache;       // indexed by frame index
    int                   mCurrentIdx  = 0;
    bool                  mPlaying     = false;
    QTimer               *mTimer       = nullptr;
    int                   mRenderW     = 640;
    int                   mRenderH     = 480;
};
