#include "grassiclassdialog.h"
#include "grasstrainingcanvas.h"
#include "../grassinterface.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QThread>
#include <QToolBar>
#include <QVBoxLayout>

static const QList<QColor> kDefaultColors = {
    { 220,  50,  50 }, {  50, 180,  50 }, {  50, 100, 220 },
    { 220, 180,  30 }, { 180,  50, 180 }, {  30, 200, 200 },
    { 220, 140,  50 }, { 100,  50,  50 }, {  50,  50, 100 }
};

GrassIclassDialog::GrassIclassDialog(GrassInterface *iface, QWidget *parent)
    : QDialog(parent), mIface(iface)
{
    setWindowTitle("Supervised Classification (iclass)");
    resize(1100, 740);
    buildUi();
}

void GrassIclassDialog::buildUi()
{
    auto *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(4);

    // ── Group selector ──
    auto *topRow = new QHBoxLayout;
    topRow->addWidget(new QLabel("Imagery group:", this));
    mGroupCombo = new QComboBox(this);
    mGroupCombo->setEditable(true);
    mGroupCombo->setMinimumWidth(160);
    {
        QProcess p; p.start("i.group", {"-l"});
        if (p.waitForFinished(8000))
            for (const QString &l : QString::fromLocal8Bit(p.readAllStandardOutput())
                                    .split('\n', Qt::SkipEmptyParts))
                if (!l.startsWith('-') && l.contains('@'))
                    mGroupCombo->addItem(l.trimmed());
    }
    topRow->addWidget(mGroupCombo, 1);

    topRow->addWidget(new QLabel("Display band:", this));
    mDisplayBand = new QComboBox(this);
    mDisplayBand->setMinimumWidth(130);
    topRow->addWidget(mDisplayBand);

    auto *loadBtn = new QPushButton("Load", this);
    connect(loadBtn, &QPushButton::clicked, this, &GrassIclassDialog::loadGroup);
    topRow->addWidget(loadBtn);

    topRow->addSpacing(12);
    topRow->addWidget(new QLabel("Method:", this));
    mMethodCombo = new QComboBox(this);
    mMethodCombo->addItems({"maxlik", "smap"});
    topRow->addWidget(mMethodCombo);

    topRow->addWidget(new QLabel("Output:", this));
    mOutputEdit = new QLineEdit("classified", this);
    mOutputEdit->setFixedWidth(120);
    topRow->addWidget(mOutputEdit);

    vbox->addLayout(topRow);

    // ── Main split: canvas | scatter ──
    auto *mainSplit = new QSplitter(Qt::Horizontal, this);

    mCanvas = new GrassTrainingCanvas("Training Canvas", this);
    connect(mCanvas, &GrassTrainingCanvas::boxDrawn,
            this, &GrassIclassDialog::onBoxDrawn);
    mainSplit->addWidget(mCanvas);

    // Scatter panel
    auto *scatterPanel = new QWidget(this);
    auto *sv = new QVBoxLayout(scatterPanel);
    sv->setContentsMargins(0, 0, 0, 0);
    sv->addWidget(new QLabel("Scatter Plot (class separability)", scatterPanel));

    auto *scBandRow = new QHBoxLayout;
    scBandRow->addWidget(new QLabel("X:", scatterPanel));
    mScatterBandX = new QComboBox(scatterPanel);
    mScatterBandX->setFixedWidth(120);
    connect(mScatterBandX, &QComboBox::currentIndexChanged, this,
            [this](int i) { mScatter->setBandIndices(i, mScatterBandY->currentIndex()); });
    scBandRow->addWidget(mScatterBandX);
    scBandRow->addWidget(new QLabel("Y:", scatterPanel));
    mScatterBandY = new QComboBox(scatterPanel);
    mScatterBandY->setFixedWidth(120);
    connect(mScatterBandY, &QComboBox::currentIndexChanged, this,
            [this](int i) { mScatter->setBandIndices(mScatterBandX->currentIndex(), i); });
    scBandRow->addWidget(mScatterBandY);
    scBandRow->addStretch();
    sv->addLayout(scBandRow);

    mScatter = new GrassScatterWidget(scatterPanel);
    sv->addWidget(mScatter, 1);
    mainSplit->addWidget(scatterPanel);
    mainSplit->setSizes({ 600, 460 });

    vbox->addWidget(mainSplit, 3);

    // ── Class toolbar ──
    auto *classBar = new QToolBar(this);
    classBar->setMovable(false);
    classBar->addAction("+ Add Class",    this, &GrassIclassDialog::addClass);
    classBar->addAction("✕ Delete Class", this, &GrassIclassDialog::deleteClass);
    classBar->addAction("● Color…",       this, &GrassIclassDialog::editClassColor);
    classBar->addSeparator();

    mDrawBtn = new QPushButton("✏ Draw Training Area", this);
    mDrawBtn->setCheckable(true);
    mDrawBtn->setEnabled(false);
    connect(mDrawBtn, &QPushButton::toggled, this, [this](bool on) {
        mDrawMode = on;
        mCanvas->setDrawMode(on);
        mStatusLabel->setText(on ? "Drag a box on the canvas for the selected class."
                                 : "");
    });
    classBar->addWidget(mDrawBtn);
    classBar->addAction("Clear Areas", this, &GrassIclassDialog::clearClassAreas);
    classBar->addSeparator();
    classBar->addAction("Compute Statistics", this, &GrassIclassDialog::computeStats);
    classBar->addAction("▶ Classify!", this, &GrassIclassDialog::runClassify);
    vbox->addWidget(classBar);

    // ── Class table ──
    mClassTable = new QTableWidget(0, 4, this);
    mClassTable->setHorizontalHeaderLabels({"Color", "Class Name", "Areas", "Pixels (1st band)"});
    mClassTable->horizontalHeader()->setStretchLastSection(true);
    mClassTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mClassTable->setEditTriggers(QAbstractItemView::DoubleClicked);
    mClassTable->setMaximumHeight(160);
    connect(mClassTable, &QTableWidget::itemSelectionChanged, this, [this] {
        selectClass(mClassTable->currentRow());
    });
    vbox->addWidget(mClassTable, 1);

    // ── Status / log ──
    auto *statRow = new QHBoxLayout;
    mStatusLabel = new QLabel("", this);
    mStatusLabel->setStyleSheet("color:#555;");
    statRow->addWidget(mStatusLabel, 1);
    mLogLabel = new QLabel("", this);
    mLogLabel->setWordWrap(true);
    mLogLabel->setStyleSheet("font-size:8pt; color:#333;");
    statRow->addWidget(mLogLabel, 2);
    auto *closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    statRow->addWidget(closeBtn);
    vbox->addLayout(statRow);
}

// ─── Group loading ────────────────────────────────────────────────────────────

void GrassIclassDialog::loadGroup()
{
    const QString grp = mGroupCombo->currentText().trimmed();
    if (grp.isEmpty()) return;

    // List bands in the group
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("i.group", {"-l", "group=" + grp});
    if (!p.waitForFinished(10000))
    { log("i.group -l timed out", true); return; }

    mBands.clear();
    for (const QString &line :
         QString::fromLocal8Bit(p.readAll()).split('\n', Qt::SkipEmptyParts))
    {
        const QString l = line.trimmed();
        if (l.contains('@') && !l.startsWith('-') && !l.startsWith("Group"))
            mBands.append(l);
    }

    if (mBands.isEmpty())
    {
        log("No bands found in group " + grp, true);
        return;
    }

    mDisplayBand->clear();
    mScatterBandX->clear();
    mScatterBandY->clear();
    QStringList labels;
    for (const QString &b : mBands)
    {
        const QString shortName = b.contains('@') ? b.left(b.indexOf('@')) : b;
        mDisplayBand->addItem(shortName, b);
        mScatterBandX->addItem(shortName);
        mScatterBandY->addItem(shortName);
        labels << shortName;
    }
    if (mBands.size() > 1) mScatterBandY->setCurrentIndex(1);
    mScatter->setBandLabels(labels);
    mScatter->setBandIndices(0, qMin(1, mBands.size() - 1));

    // Render the display band
    connect(mDisplayBand, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (i >= 0 && i < mBands.size())
        {
            mCanvas->setMap(mBands[i]);
            mCanvas->renderMap();
        }
    }, Qt::UniqueConnection);

    mCanvas->setMap(mBands[0]);
    mCanvas->renderMap();

    log(QString("Loaded group '%1': %2 bands").arg(grp).arg(mBands.size()));
}

// ─── Class management ─────────────────────────────────────────────────────────

void GrassIclassDialog::addClass()
{
    bool ok;
    const QString name = QInputDialog::getText(this, "Add Class",
        "Class name:", QLineEdit::Normal,
        QString("Class %1").arg(mNextClassId), &ok);
    if (!ok || name.isEmpty()) return;

    IClassDef cls;
    cls.id    = mNextClassId++;
    cls.name  = name;
    cls.color = kDefaultColors.value((mClasses.size()) % kDefaultColors.size());
    cls.stats.resize(mBands.size());
    mClasses.append(cls);

    const int row = mClassTable->rowCount();
    mClassTable->insertRow(row);
    auto *colorItem = new QTableWidgetItem("  ");
    colorItem->setBackground(cls.color);
    colorItem->setFlags(colorItem->flags() & ~Qt::ItemIsEditable);
    mClassTable->setItem(row, 0, colorItem);
    mClassTable->setItem(row, 1, new QTableWidgetItem(name));
    mClassTable->setItem(row, 2, new QTableWidgetItem("0"));
    mClassTable->setItem(row, 3, new QTableWidgetItem("—"));
    mClassTable->selectRow(row);
}

void GrassIclassDialog::deleteClass()
{
    const int row = mClassTable->currentRow();
    if (row < 0 || row >= mClasses.size()) return;
    mClasses.removeAt(row);
    mClassTable->removeRow(row);
    mSelectedClass = -1;
    updateBoxOverlay();
    mScatter->setClasses(mClasses);
}

void GrassIclassDialog::editClassColor()
{
    const int row = mClassTable->currentRow();
    if (row < 0 || row >= mClasses.size()) return;
    const QColor c = QColorDialog::getColor(mClasses[row].color, this);
    if (!c.isValid()) return;
    mClasses[row].color = c;
    auto *item = mClassTable->item(row, 0);
    if (item) item->setBackground(c);
    updateBoxOverlay();
    mScatter->setClasses(mClasses);
}

void GrassIclassDialog::selectClass(int row)
{
    mSelectedClass = (row >= 0 && row < mClasses.size()) ? row : -1;
    mDrawBtn->setEnabled(mSelectedClass >= 0 && !mBands.isEmpty());
}

void GrassIclassDialog::startDrawing()
{
    if (mSelectedClass < 0) return;
    mCanvas->setDrawMode(true);
    mDrawBtn->setChecked(true);
}

void GrassIclassDialog::onBoxDrawn(double n, double s, double e, double w)
{
    if (mSelectedClass < 0 || mSelectedClass >= mClasses.size()) return;

    IClassDef::BBox box{ n, s, e, w };
    mClasses[mSelectedClass].areas.append(box);

    // Update area count in table
    if (auto *item = mClassTable->item(mSelectedClass, 2))
        item->setText(QString::number(mClasses[mSelectedClass].areas.size()));

    // Turn off draw mode after each box (user clicks button again for next)
    mDrawBtn->setChecked(false);
    mCanvas->setDrawMode(false);

    updateBoxOverlay();
    mStatusLabel->setText(QString("Added training area %1 for '%2'")
                          .arg(mClasses[mSelectedClass].areas.size())
                          .arg(mClasses[mSelectedClass].name));
}

void GrassIclassDialog::clearClassAreas()
{
    if (mSelectedClass < 0 || mSelectedClass >= mClasses.size()) return;
    mClasses[mSelectedClass].areas.clear();
    if (auto *item = mClassTable->item(mSelectedClass, 2))
        item->setText("0");
    updateBoxOverlay();
}

void GrassIclassDialog::updateBoxOverlay()
{
    QVector<GrassTrainingCanvas::TrainingBox> boxes;
    for (const auto &cls : mClasses)
        for (const auto &area : cls.areas)
            boxes.append({ area.n, area.s, area.e, area.w, cls.color, cls.id });
    mCanvas->setBoxes(boxes);
}

// ─── Statistics ───────────────────────────────────────────────────────────────

BandStats GrassIclassDialog::statsForBand(const IClassDef::BBox &bbox,
                                           const QString &band)
{
    BandStats bs;

    // Set region to bbox, aligned to band
    {
        QProcess p;
        p.setProcessChannelMode(QProcess::MergedChannels);
        p.start("g.region",
                {QString("n=%1").arg(bbox.n, 0, 'f', 10),
                 QString("s=%1").arg(bbox.s, 0, 'f', 10),
                 QString("e=%1").arg(bbox.e, 0, 'f', 10),
                 QString("w=%1").arg(bbox.w, 0, 'f', 10),
                 "align=" + band});
        p.waitForFinished(10000);
    }

    // Get stats using r.univar -g
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("r.univar", {"-g", "map=" + band});
    if (!p.waitForFinished(30000)) return bs;

    for (const QString &line :
         QString::fromLocal8Bit(p.readAll()).split('\n', Qt::SkipEmptyParts))
    {
        if (line.startsWith("n="))          bs.count  = line.mid(2).toInt();
        if (line.startsWith("mean="))       bs.mean   = line.mid(5).toDouble();
        if (line.startsWith("population_stddev="))
            bs.stdDev = line.mid(18).toDouble();
        if (line.startsWith("stddev=") && bs.stdDev == 0)
            bs.stdDev = line.mid(7).toDouble();
    }
    return bs;
}

void GrassIclassDialog::computeStats()
{
    if (mBands.isEmpty())
    { log("Load an imagery group first.", true); return; }

    // Save current region
    QProcess().start("g.region", {"save=tmp_iclass_backup", "--overwrite"});

    int totalAreas = 0;
    for (auto &cls : mClasses)
    {
        cls.stats.resize(mBands.size());
        for (int b = 0; b < mBands.size(); ++b)
        {
            BandStats combined;
            for (const auto &area : cls.areas)
            {
                const BandStats s = statsForBand(area, mBands[b]);
                // Accumulate: simple average of means (approximate)
                if (s.count > 0)
                {
                    combined.count  += s.count;
                    combined.mean   += s.mean;
                    combined.stdDev += s.stdDev;
                }
            }
            if (!cls.areas.isEmpty() && combined.count > 0)
            {
                combined.mean   /= cls.areas.size();
                combined.stdDev /= cls.areas.size();
            }
            cls.stats[b] = combined;
        }
        totalAreas += cls.areas.size();

        // Update pixel count in table
        if (mSelectedClass >= 0 && mSelectedClass < mClasses.size())
        {
            const int row = &cls - mClasses.data();
            if (auto *item = mClassTable->item(row, 3))
                item->setText(cls.stats.value(0).count > 0
                    ? QString::number(cls.stats[0].count)
                    : "—");
        }
    }

    // Restore region
    {
        QProcess p;
        p.start("g.region", {"region=tmp_iclass_backup"});
        p.waitForFinished(5000);
    }
    QProcess().start("g.remove", {"type=region", "name=tmp_iclass_backup", "-f"});

    mScatter->setClasses(mClasses);
    mScatter->setBandIndices(mScatterBandX->currentIndex(),
                              mScatterBandY->currentIndex());

    // Update table
    for (int r = 0; r < mClasses.size(); ++r)
    {
        const auto &cls = mClasses[r];
        if (auto *item = mClassTable->item(r, 3))
            item->setText(cls.stats.value(0).count > 0
                ? QString::number(cls.stats[0].count) : "—");
    }

    log(QString("Statistics computed for %1 classes (%2 areas).")
        .arg(mClasses.size()).arg(totalAreas));
}

// ─── Classification ───────────────────────────────────────────────────────────

bool GrassIclassDialog::buildTrainingMap(const QString &mapName)
{
    // Create per-class bbox rasters and merge them
    QStringList classMaps;
    for (const auto &cls : mClasses)
    {
        if (cls.areas.isEmpty()) continue;
        for (int ai = 0; ai < cls.areas.size(); ++ai)
        {
            const auto &area = cls.areas[ai];
            const QString tmpMap = QString("tmp_iclass_c%1_a%2").arg(cls.id).arg(ai);

            // Set region to area, aligned to first band
            QProcess p1;
            p1.start("g.region",
                     {QString("n=%1").arg(area.n, 0, 'f', 10),
                      QString("s=%1").arg(area.s, 0, 'f', 10),
                      QString("e=%1").arg(area.e, 0, 'f', 10),
                      QString("w=%1").arg(area.w, 0, 'f', 10),
                      "align=" + mBands[0]});
            p1.waitForFinished(10000);

            // Create constant raster = class id
            QProcess p2;
            p2.setProcessChannelMode(QProcess::MergedChannels);
            p2.start("r.mapcalc",
                     {QString("expression=%1 = %2").arg(tmpMap).arg(cls.id),
                      "--overwrite"});
            if (!p2.waitForFinished(30000) || p2.exitCode() != 0)
            {
                log("r.mapcalc failed: " + QString::fromLocal8Bit(p2.readAll()), true);
                return false;
            }
            classMaps.append(tmpMap);
        }
    }

    if (classMaps.isEmpty())
    {
        log("No training areas defined.", true);
        return false;
    }

    // Restore original region (default) for merging
    QProcess().start("g.region", {"-d"}); // use default region
    QThread::msleep(200);

    // Build merge expression: patch all class maps
    QString expr = classMaps[0];
    for (int i = 1; i < classMaps.size(); ++i)
        expr = QString("if(!isnull(%1), %1, %2)").arg(classMaps[i - 1], expr);
    // Actually patch() would be cleaner, but r.mapcalc's if/isnull works
    // Correct chained merge: use r.patch
    QProcess pPatch;
    pPatch.setProcessChannelMode(QProcess::MergedChannels);
    pPatch.start("r.patch",
                 {"input=" + classMaps.join(","),
                  "output=" + mapName,
                  "--overwrite"});
    if (!pPatch.waitForFinished(60000) || pPatch.exitCode() != 0)
    {
        log("r.patch failed: " + QString::fromLocal8Bit(pPatch.readAll()), true);
        return false;
    }

    // Clean up temp maps
    QProcess().start("g.remove",
                     QStringList{"type=raster", "name=" + classMaps.join(","), "-f"});
    return true;
}

void GrassIclassDialog::runClassify()
{
    if (mBands.isEmpty() || mClasses.isEmpty())
    { log("Load a group and define classes first.", true); return; }

    const QString grp    = mGroupCombo->currentText().trimmed();
    const QString method = mMethodCombo->currentText();
    const QString output = mOutputEdit->text().trimmed();

    if (output.isEmpty())
    { log("Set an output map name.", true); return; }

    // Save + compute stats if not done
    computeStats();

    // Save current region, build training map
    QProcess().start("g.region", {"save=tmp_iclass_backup", "--overwrite"});
    QThread::msleep(300);

    const QString trainMap = "tmp_iclass_trainingmap";
    if (!buildTrainingMap(trainMap))
    {
        QProcess().start("g.region", {"region=tmp_iclass_backup"});
        return;
    }

    // Restore region
    QProcess p0; p0.start("g.region", {"region=tmp_iclass_backup"});
    p0.waitForFinished(5000);
    QProcess().start("g.remove", {"type=region", "name=tmp_iclass_backup", "-f"});

    // Generate signature file
    const QString sigFile = "iclass_sig_" + output;
    log("Generating signatures with i.gensig…");
    {
        QProcess p;
        p.setProcessChannelMode(QProcess::MergedChannels);
        p.start("i.gensig",
                {"trainingmap=" + trainMap,
                 "group="       + grp,
                 "subgroup="    + grp,
                 "signaturefile=" + sigFile});
        if (!p.waitForFinished(120000) || p.exitCode() != 0)
        {
            log("i.gensig failed: " + QString::fromLocal8Bit(p.readAll()).trimmed(), true);
            return;
        }
    }

    // Run classifier
    const QString modName = (method == "smap") ? "i.smap" : "i.maxlik";
    log("Running " + modName + "…");
    {
        QProcess p;
        p.setProcessChannelMode(QProcess::MergedChannels);
        QStringList args = {"group="         + grp,
                             "subgroup="      + grp,
                             "signaturefile=" + sigFile,
                             "output="        + output,
                             "--overwrite"};
        p.start(modName, args);
        if (!p.waitForFinished(600000))
        { log(modName + " timed out (>10 min)", true); return; }
        if (p.exitCode() != 0)
        { log(modName + " failed: " + QString::fromLocal8Bit(p.readAll()).trimmed(), true); return; }
    }

    // Clean up training map
    QProcess().start("g.remove", {"type=raster", "name=" + trainMap, "-f"});

    log("Classification complete → " + output);
    QMessageBox::information(this, "Classification complete",
        QString("Output map: %1\nMethod: %2\nClasses: %3")
        .arg(output, method).arg(mClasses.size()));
}

void GrassIclassDialog::log(const QString &msg, bool error)
{
    mLogLabel->setStyleSheet(error ? "font-size:8pt; color:red;"
                                   : "font-size:8pt; color:#333;");
    mLogLabel->setText(msg);
}
