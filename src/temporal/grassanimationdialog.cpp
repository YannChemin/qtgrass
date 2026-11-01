#include "grassanimationdialog.h"
#include "../grassinterface.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressDialog>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QTemporaryFile>

// ─────────────────────────────────────────────────────────────────────────────

GrassAnimationDialog::GrassAnimationDialog(GrassInterface *iface, QWidget *parent)
    : QDialog(parent)
    , mIface(iface)
    , mDataset(new GrassTemporalDataset(this))
    , mTimer(new QTimer(this))
{
    setWindowTitle("Temporal Animation");
    resize(820, 680);
    buildUi();

    connect(mTimer, &QTimer::timeout, this, [this] {
        if (!mPlaying) return;
        const int total = mDataset->frameCount();
        if (total == 0) return;

        int next = mCurrentIdx + 1;
        if (next >= total)
        {
            if (mLoopCheck->isChecked()) next = 0;
            else { pause(); return; }
        }
        showFrame(next);
    });

    refreshDatasetList();
    updateControls(false);
}

GrassAnimationDialog::~GrassAnimationDialog()
{
    pause();
}

// ─── UI ───────────────────────────────────────────────────────────────────────

void GrassAnimationDialog::buildUi()
{
    auto *vbox = new QVBoxLayout(this);
    vbox->setSpacing(6);

    // ── Dataset selection row ──
    auto *topRow = new QHBoxLayout;
    topRow->addWidget(new QLabel("Dataset:", this));

    mTypeCombo = new QComboBox(this);
    mTypeCombo->addItem("STRDS (Raster)", "strds");
    mTypeCombo->addItem("STVDS (Vector)", "stvds");
    topRow->addWidget(mTypeCombo);

    mDatasetCombo = new QComboBox(this);
    mDatasetCombo->setMinimumWidth(220);
    mDatasetCombo->setEditable(true);
    topRow->addWidget(mDatasetCombo, 1);

    auto *refreshBtn = new QPushButton("⟳", this);
    refreshBtn->setToolTip("Refresh dataset list");
    refreshBtn->setFixedWidth(28);
    connect(refreshBtn, &QPushButton::clicked, this, [this] { refreshDatasetList(); });
    topRow->addWidget(refreshBtn);

    auto *loadBtn = new QPushButton("Load", this);
    loadBtn->setToolTip("Load selected dataset");
    connect(loadBtn, &QPushButton::clicked, this, &GrassAnimationDialog::loadDataset);
    topRow->addWidget(loadBtn);

    topRow->addWidget(new QLabel("Size:", this));
    mSizeCombo = new QComboBox(this);
    mSizeCombo->addItem("320×240",  QSize(320, 240));
    mSizeCombo->addItem("640×480",  QSize(640, 480));
    mSizeCombo->addItem("800×600",  QSize(800, 600));
    mSizeCombo->addItem("1280×720", QSize(1280, 720));
    mSizeCombo->setCurrentIndex(1);
    topRow->addWidget(mSizeCombo);

    connect(mTypeCombo, &QComboBox::currentIndexChanged,
            this, [this] { refreshDatasetList(); });

    vbox->addLayout(topRow);

    // ── Preview ──
    mPreviewLabel = new QLabel(this);
    mPreviewLabel->setAlignment(Qt::AlignCenter);
    mPreviewLabel->setMinimumSize(640, 420);
    mPreviewLabel->setStyleSheet(
        "background:#1a1a2e; border:1px solid #555; color:#aaa; font-size:14px;");
    mPreviewLabel->setText("Load a temporal dataset to begin.");
    vbox->addWidget(mPreviewLabel, 1);

    // ── Time / frame info ──
    auto *infoRow = new QHBoxLayout;
    mTimeLabel  = new QLabel("—", this);
    mFrameLabel = new QLabel("—", this);
    mFrameLabel->setAlignment(Qt::AlignRight);
    infoRow->addWidget(mTimeLabel, 1);
    infoRow->addWidget(mFrameLabel);
    vbox->addLayout(infoRow);

    // ── Timeline slider ──
    mSlider = new QSlider(Qt::Horizontal, this);
    mSlider->setRange(0, 0);
    mSlider->setTracking(true);
    connect(mSlider, &QSlider::valueChanged, this, [this](int val) {
        if (!mPlaying) showFrame(val);
    });
    vbox->addWidget(mSlider);

    // ── Playback controls ──
    auto *ctrlRow = new QHBoxLayout;

    mFirstBtn = new QPushButton("|◀", this);
    mFirstBtn->setToolTip("First frame");
    mFirstBtn->setFixedWidth(36);
    connect(mFirstBtn, &QPushButton::clicked, this, [this] { showFrame(0); });
    ctrlRow->addWidget(mFirstBtn);

    mPrevBtn  = new QPushButton("◀", this);
    mPrevBtn->setToolTip("Previous frame");
    mPrevBtn->setFixedWidth(32);
    connect(mPrevBtn, &QPushButton::clicked, this, &GrassAnimationDialog::stepBack);
    ctrlRow->addWidget(mPrevBtn);

    mPlayBtn  = new QPushButton("▶ Play", this);
    mPlayBtn->setCheckable(true);
    connect(mPlayBtn, &QPushButton::toggled, this, [this](bool on) {
        on ? play() : pause();
    });
    ctrlRow->addWidget(mPlayBtn);

    mNextBtn  = new QPushButton("▶", this);
    mNextBtn->setToolTip("Next frame");
    mNextBtn->setFixedWidth(32);
    connect(mNextBtn, &QPushButton::clicked, this, &GrassAnimationDialog::stepForward);
    ctrlRow->addWidget(mNextBtn);

    mLastBtn  = new QPushButton("▶|", this);
    mLastBtn->setToolTip("Last frame");
    mLastBtn->setFixedWidth(36);
    connect(mLastBtn, &QPushButton::clicked, this, [this] {
        if (mDataset->frameCount() > 0)
            showFrame(mDataset->frameCount() - 1);
    });
    ctrlRow->addWidget(mLastBtn);

    ctrlRow->addSpacing(12);
    ctrlRow->addWidget(new QLabel("FPS:", this));
    mFpsSpin = new QDoubleSpinBox(this);
    mFpsSpin->setRange(0.5, 30.0);
    mFpsSpin->setValue(5.0);
    mFpsSpin->setSingleStep(0.5);
    mFpsSpin->setDecimals(1);
    mFpsSpin->setFixedWidth(68);
    connect(mFpsSpin, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        mTimer->setInterval(static_cast<int>(1000.0 / v));
    });
    ctrlRow->addWidget(mFpsSpin);

    ctrlRow->addSpacing(8);
    mLoopCheck = new QCheckBox("Loop", this);
    mLoopCheck->setChecked(true);
    ctrlRow->addWidget(mLoopCheck);

    mLegendCheck = new QCheckBox("Legend", this);
    mLegendCheck->setChecked(false);
    mLegendCheck->setToolTip("Overlay d.legend on each frame");
    ctrlRow->addWidget(mLegendCheck);

    ctrlRow->addStretch(1);

    mPreRenderBtn = new QPushButton("Pre-render All", this);
    mPreRenderBtn->setToolTip(
        "Render all frames to memory for smooth playback");
    connect(mPreRenderBtn, &QPushButton::clicked,
            this, &GrassAnimationDialog::preRender);
    ctrlRow->addWidget(mPreRenderBtn);

    vbox->addLayout(ctrlRow);

    // ── Export row ──
    auto *exportRow = new QHBoxLayout;
    exportRow->addStretch(1);

    mExportPngBtn = new QPushButton("Export PNG Sequence…", this);
    connect(mExportPngBtn, &QPushButton::clicked,
            this, &GrassAnimationDialog::exportPngSequence);
    exportRow->addWidget(mExportPngBtn);

    mExportMp4Btn = new QPushButton("Export MP4…", this);
    connect(mExportMp4Btn, &QPushButton::clicked,
            this, &GrassAnimationDialog::exportMp4);
    exportRow->addWidget(mExportMp4Btn);

    auto *closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    exportRow->addWidget(closeBtn);

    vbox->addLayout(exportRow);
}

// ─── Dataset loading ──────────────────────────────────────────────────────────

void GrassAnimationDialog::refreshDatasetList()
{
    const QString type = mTypeCombo->currentData().toString();
    const QStringList datasets = GrassTemporalDataset::listDatasets(type);

    const QString current = mDatasetCombo->currentText();
    mDatasetCombo->clear();
    mDatasetCombo->addItems(datasets);
    if (!current.isEmpty())
    {
        const int idx = mDatasetCombo->findText(current);
        if (idx >= 0) mDatasetCombo->setCurrentIndex(idx);
        else mDatasetCombo->setEditText(current);
    }
}

void GrassAnimationDialog::loadDataset()
{
    pause();
    const QString name = mDatasetCombo->currentText().trimmed();
    if (name.isEmpty()) return;

    // Determine type
    const GrassTemporalDataset::DataType dtype =
        (mTypeCombo->currentData().toString() == "stvds")
            ? GrassTemporalDataset::DataType::Vector
            : GrassTemporalDataset::DataType::Raster;

    mCache.clear();
    mCurrentIdx = 0;

    if (!mDataset->load(name, dtype))
    {
        mPreviewLabel->setText("Error: " + mDataset->lastError());
        updateControls(false);
        return;
    }

    const int total = mDataset->frameCount();
    mCache.resize(total);   // null QPixmaps — rendered on demand

    mSlider->setRange(0, total - 1);
    mSlider->setValue(0);

    updateControls(true);
    showFrame(0);
}

// ─── Rendering ───────────────────────────────────────────────────────────────

// Retrieve render size from the size combo
static QPair<int,int> comboSize(QComboBox *combo) {
    const QSize s = combo->currentData().toSize();
    return { s.width(), s.height() };
}

QPixmap GrassAnimationDialog::renderFrame(const TemporalFrame &frame)
{
    auto [w, h] = comboSize(mSizeCombo);

    // Temporary PNG file
    QTemporaryFile tmp;
    tmp.setFileTemplate(QDir::tempPath() + "/qtgrass_anim_XXXXXX.png");
    tmp.setAutoRemove(false);
    if (!tmp.open()) return {};
    const QString tmpPath = tmp.fileName();
    tmp.close();

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("GRASS_RENDER_IMMEDIATE",       "cairo");
    env.insert("GRASS_RENDER_FILE",            tmpPath);
    env.insert("GRASS_RENDER_FILE_READ",       "FALSE");
    env.insert("GRASS_RENDER_WIDTH",           QString::number(w));
    env.insert("GRASS_RENDER_HEIGHT",          QString::number(h));
    env.insert("GRASS_RENDER_BACKGROUNDCOLOR", "white");

    // d.rast renders the map
    QProcess drast;
    drast.setProcessEnvironment(env);
    drast.start("d.rast", {"map=" + frame.mapName});
    if (!drast.waitForFinished(15000) || drast.exitCode() != 0)
    {
        QFile::remove(tmpPath);
        return {};
    }

    // Optionally overlay legend
    if (mLegendCheck->isChecked())
    {
        env.insert("GRASS_RENDER_FILE_READ", "TRUE");
        QProcess dlegend;
        dlegend.setProcessEnvironment(env);
        dlegend.start("d.legend",
                      {"raster=" + frame.mapName,
                       "at=5,50,2,5"});   // bottom-left corner
        dlegend.waitForFinished(8000);
    }

    QPixmap pix(tmpPath);
    QFile::remove(tmpPath);
    return pix;
}

void GrassAnimationDialog::showFrame(int idx)
{
    const int total = mDataset->frameCount();
    if (idx < 0 || idx >= total) return;

    mCurrentIdx = idx;
    mSlider->setValue(idx);

    // Fetch from cache or render on demand
    if (mCache[idx].isNull())
        mCache[idx] = renderFrame(mDataset->frames()[idx]);

    const QPixmap &pix = mCache[idx];
    if (pix.isNull())
    {
        mPreviewLabel->setText(
            QString("Render failed for frame %1").arg(idx + 1));
    }
    else
    {
        mPreviewLabel->setPixmap(
            pix.scaled(mPreviewLabel->size(),
                       Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    // Update time / frame info
    const TemporalFrame &f = mDataset->frames()[idx];
    const QString timeStr = f.startTime.isEmpty()
                            ? QString("Frame %1").arg(idx + 1)
                            : f.startTime +
                              (f.endTime.isEmpty() ? QString() :
                               " → " + f.endTime);
    mTimeLabel->setText(timeStr);
    mFrameLabel->setText(QString("Frame %1 / %2").arg(idx + 1).arg(total));
}

// ─── Pre-render ───────────────────────────────────────────────────────────────

void GrassAnimationDialog::preRender()
{
    const int total = mDataset->frameCount();
    if (total == 0) return;

    pause();
    QProgressDialog progress("Pre-rendering frames…", "Cancel", 0, total, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    for (int i = 0; i < total; ++i)
    {
        progress.setValue(i);
        QApplication::processEvents();
        if (progress.wasCanceled()) break;

        if (mCache[i].isNull())
            mCache[i] = renderFrame(mDataset->frames()[i]);
    }
    progress.setValue(total);

    // Refresh current display
    if (!mCache[mCurrentIdx].isNull())
        mPreviewLabel->setPixmap(
            mCache[mCurrentIdx].scaled(mPreviewLabel->size(),
                                       Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation));
}

// ─── Playback ────────────────────────────────────────────────────────────────

void GrassAnimationDialog::play()
{
    if (mDataset->frameCount() == 0) return;
    mPlaying = true;
    mPlayBtn->setText("⏸ Pause");
    mTimer->setInterval(static_cast<int>(1000.0 / mFpsSpin->value()));
    mTimer->start();
}

void GrassAnimationDialog::pause()
{
    mPlaying = false;
    mTimer->stop();
    mPlayBtn->setChecked(false);
    mPlayBtn->setText("▶ Play");
}

void GrassAnimationDialog::stepForward()
{
    const int total = mDataset->frameCount();
    if (total == 0) return;
    showFrame((mCurrentIdx + 1) % total);
}

void GrassAnimationDialog::stepBack()
{
    const int total = mDataset->frameCount();
    if (total == 0) return;
    showFrame((mCurrentIdx - 1 + total) % total);
}

// ─── Export ──────────────────────────────────────────────────────────────────

void GrassAnimationDialog::exportPngSequence()
{
    if (mDataset->frameCount() == 0) return;
    pause();

    const QString dir = QFileDialog::getExistingDirectory(
        this, "Export PNG Sequence — Choose Output Directory");
    if (dir.isEmpty()) return;

    bool ok;
    const QString prefix = QInputDialog::getText(
        this, "Filename Prefix", "Frame filename prefix:", QLineEdit::Normal,
        "frame_", &ok);
    if (!ok) return;

    const int total = mDataset->frameCount();
    QProgressDialog progress("Exporting PNG frames…", "Cancel", 0, total, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    int saved = 0;
    for (int i = 0; i < total; ++i)
    {
        progress.setValue(i);
        QApplication::processEvents();
        if (progress.wasCanceled()) break;

        if (mCache[i].isNull())
            mCache[i] = renderFrame(mDataset->frames()[i]);

        if (!mCache[i].isNull())
        {
            const QString path = QString("%1/%2%3.png")
                                 .arg(dir, prefix)
                                 .arg(i, 4, 10, QChar('0'));
            mCache[i].save(path, "PNG");
            ++saved;
        }
    }
    progress.setValue(total);

    QMessageBox::information(this, "Export complete",
        QString("Saved %1 of %2 frames to:\n%3").arg(saved).arg(total).arg(dir));
}

void GrassAnimationDialog::exportMp4()
{
    if (mDataset->frameCount() == 0) return;
    pause();

    // Check ffmpeg availability
    QProcess check;
    check.start("ffmpeg", {"-version"});
    if (!check.waitForFinished(3000) || check.exitCode() != 0)
    {
        QMessageBox::warning(this, "ffmpeg not found",
            "ffmpeg is required for MP4 export.\n"
            "Install it with:\n  sudo apt install ffmpeg");
        return;
    }

    const QString outPath = QFileDialog::getSaveFileName(
        this, "Export MP4", {}, "MPEG-4 Video (*.mp4)");
    if (outPath.isEmpty()) return;

    // Pre-render all frames to a temp directory
    const QString tmpDir =
        QDir::tempPath() + "/qtgrass_anim_" +
        QString::number(QCoreApplication::applicationPid());
    QDir().mkpath(tmpDir);

    const int total = mDataset->frameCount();
    QProgressDialog progress("Rendering frames for MP4…", "Cancel", 0, total, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    bool cancelled = false;
    for (int i = 0; i < total; ++i)
    {
        progress.setValue(i);
        QApplication::processEvents();
        if (progress.wasCanceled()) { cancelled = true; break; }

        if (mCache[i].isNull())
            mCache[i] = renderFrame(mDataset->frames()[i]);

        if (!mCache[i].isNull())
        {
            const QString framePath =
                QString("%1/frame_%04d.png").arg(tmpDir).replace("%04d",
                    QString("%1").arg(i, 4, 10, QChar('0')));
            mCache[i].save(framePath, "PNG");
        }
    }
    progress.setValue(total);

    if (cancelled)
    {
        QDir(tmpDir).removeRecursively();
        return;
    }

    // Run ffmpeg
    const QStringList ffArgs = {
        "-y",
        "-framerate", QString::number(static_cast<int>(mFpsSpin->value())),
        "-i",         tmpDir + "/frame_%04d.png",
        "-c:v",       "libx264",
        "-pix_fmt",   "yuv420p",
        "-preset",    "medium",
        "-crf",       "23",
        outPath
    };

    QProgressDialog ffProg("Encoding MP4…", QString(), 0, 0, this);
    ffProg.setWindowModality(Qt::WindowModal);
    ffProg.show();
    QApplication::processEvents();

    QProcess ff;
    ff.setProcessChannelMode(QProcess::MergedChannels);
    ff.start("ffmpeg", ffArgs);
    ff.waitForFinished(-1);

    ffProg.close();
    QDir(tmpDir).removeRecursively();

    if (ff.exitCode() != 0)
    {
        const QString err = QString::fromLocal8Bit(ff.readAll()).right(600);
        QMessageBox::warning(this, "MP4 export failed",
                             "ffmpeg exited with error:\n\n" + err);
    }
    else
    {
        QMessageBox::information(this, "MP4 export complete",
            QString("Saved: %1\nFrames: %2 @ %3 FPS")
            .arg(outPath).arg(total).arg(mFpsSpin->value(), 0, 'f', 1));
    }
}

// ─── Controls ────────────────────────────────────────────────────────────────

void GrassAnimationDialog::updateControls(bool hasFrames)
{
    mSlider->setEnabled(hasFrames);
    mFirstBtn->setEnabled(hasFrames);
    mPrevBtn->setEnabled(hasFrames);
    mPlayBtn->setEnabled(hasFrames);
    mNextBtn->setEnabled(hasFrames);
    mLastBtn->setEnabled(hasFrames);
    mPreRenderBtn->setEnabled(hasFrames);
    mExportPngBtn->setEnabled(hasFrames);
    mExportMp4Btn->setEnabled(hasFrames);
}
