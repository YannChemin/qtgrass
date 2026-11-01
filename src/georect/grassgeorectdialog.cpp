#include "grassgeorectdialog.h"
#include "../grassinterface.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
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
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>

// Column indices in the GCP table
static constexpr int COL_ID    = 0;
static constexpr int COL_SRCX  = 1;
static constexpr int COL_SRCY  = 2;
static constexpr int COL_REFX  = 3;
static constexpr int COL_REFY  = 4;
static constexpr int COL_RESX  = 5;
static constexpr int COL_RESY  = 6;
static constexpr int COL_RMS   = 7;
static constexpr int COL_ON    = 8;
static constexpr int NUM_COLS  = 9;

GrassGeoRectDialog::GrassGeoRectDialog(GrassInterface *iface, QWidget *parent)
    : QDialog(parent), mIface(iface)
{
    setWindowTitle("Georectification");
    resize(1100, 780);
    buildUi();
}

void GrassGeoRectDialog::buildUi()
{
    auto *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(6, 6, 6, 6);
    vbox->setSpacing(6);

    // ── Top: map selectors + group settings ──
    auto *topGroup = new QGroupBox("Maps & Settings", this);
    auto *tg = new QHBoxLayout(topGroup);

    tg->addWidget(new QLabel("Source raster:", topGroup));
    mSrcCombo = new QComboBox(topGroup);
    mSrcCombo->setEditable(true);
    mSrcCombo->setMinimumWidth(150);
    {
        QProcess p; p.start("g.list", {"type=raster", "mapset=."});
        if (p.waitForFinished(8000))
            for (const QString &l : QString::fromLocal8Bit(p.readAllStandardOutput())
                                    .split('\n', Qt::SkipEmptyParts))
                mSrcCombo->addItem(l.trimmed());
    }
    tg->addWidget(mSrcCombo, 1);

    auto *renderSrcBtn = new QPushButton("Render", topGroup);
    connect(renderSrcBtn, &QPushButton::clicked, this, [this] {
        if (!mSrcCombo->currentText().isEmpty())
        {
            mSrcCanvas->setMap(mSrcCombo->currentText());
            mSrcCanvas->renderMap();
            refreshMarkers();
        }
    });
    tg->addWidget(renderSrcBtn);

    tg->addSpacing(12);
    tg->addWidget(new QLabel("Reference raster:", topGroup));
    mRefCombo = new QComboBox(topGroup);
    mRefCombo->setEditable(true);
    mRefCombo->setMinimumWidth(150);
    mRefCombo->addItems(mSrcCombo->count() > 0
                        ? QStringList() : QStringList());
    // Populate ref from all visible rasters
    for (int i = 0; i < mSrcCombo->count(); ++i)
        mRefCombo->addItem(mSrcCombo->itemText(i));
    tg->addWidget(mRefCombo, 1);

    auto *renderRefBtn = new QPushButton("Render", topGroup);
    connect(renderRefBtn, &QPushButton::clicked, this, [this] {
        if (!mRefCombo->currentText().isEmpty())
        {
            mRefCanvas->setMap(mRefCombo->currentText());
            mRefCanvas->renderMap();
            refreshMarkers();
        }
    });
    tg->addWidget(renderRefBtn);

    tg->addSpacing(12);
    tg->addWidget(new QLabel("Group:", topGroup));
    mGroupEdit = new QLineEdit("rectify_group", topGroup);
    mGroupEdit->setFixedWidth(120);
    tg->addWidget(mGroupEdit);

    tg->addWidget(new QLabel("Order:", topGroup));
    mOrderCombo = new QComboBox(topGroup);
    mOrderCombo->addItem("1 (affine, min 3 GCPs)",   1);
    mOrderCombo->addItem("2 (quadratic, min 6 GCPs)", 2);
    mOrderCombo->addItem("3 (cubic, min 10 GCPs)",    3);
    mOrderCombo->setFixedWidth(180);
    connect(mOrderCombo, &QComboBox::currentIndexChanged,
            this, [this] { updateResiduals(); });
    tg->addWidget(mOrderCombo);

    tg->addWidget(new QLabel("Method:", topGroup));
    mMethodCombo = new QComboBox(topGroup);
    mMethodCombo->addItems({"nearest", "bilinear", "bicubic"});
    mMethodCombo->setFixedWidth(90);
    tg->addWidget(mMethodCombo);

    tg->addWidget(new QLabel("Extension:", topGroup));
    mExtensionEdit = new QLineEdit("_rect", topGroup);
    mExtensionEdit->setFixedWidth(70);
    tg->addWidget(mExtensionEdit);

    vbox->addWidget(topGroup);

    // ── Dual canvas ──
    auto *canvasSplit = new QSplitter(Qt::Horizontal, this);

    mSrcCanvas = new GrassGcpCanvas("Source (unrectified)", this);
    mRefCanvas = new GrassGcpCanvas("Reference (target CRS)", this);
    canvasSplit->addWidget(mSrcCanvas);
    canvasSplit->addWidget(mRefCanvas);
    canvasSplit->setSizes({500, 500});

    connect(mSrcCanvas, &GrassGcpCanvas::coordClicked,
            this, &GrassGeoRectDialog::onSrcCoordPicked);
    connect(mRefCanvas, &GrassGcpCanvas::coordClicked,
            this, &GrassGeoRectDialog::onRefCoordPicked);

    vbox->addWidget(canvasSplit, 3);

    // ── GCP toolbar ──
    auto *gcpBar = new QToolBar(this);
    gcpBar->setMovable(false);

    auto *addBtn = gcpBar->addAction("+ Add GCP", this,
                                      &GrassGeoRectDialog::startAddGcp);
    addBtn->setToolTip("Click source, then reference canvas to add a GCP");

    gcpBar->addAction("✕ Delete GCP", this,
                       &GrassGeoRectDialog::deleteSelectedGcp);

    gcpBar->addSeparator();
    gcpBar->addAction("Save GCPs…", this, &GrassGeoRectDialog::savePoints);
    gcpBar->addAction("Load GCPs…", this, &GrassGeoRectDialog::loadPoints);
    gcpBar->addSeparator();

    gcpBar->addAction("Create Group + Rectify!", this,
                       &GrassGeoRectDialog::rectify);

    mPickStatus = new QLabel("", this);
    mPickStatus->setStyleSheet("color: #b06000; font-weight: bold; padding: 0 8px;");
    gcpBar->addWidget(mPickStatus);

    vbox->addWidget(gcpBar);

    // ── GCP table ──
    mGcpTable = new QTableWidget(0, NUM_COLS, this);
    mGcpTable->setHorizontalHeaderLabels(
        {"#", "Src X", "Src Y", "Ref X", "Ref Y",
         "Res X", "Res Y", "RMS", "On"});
    mGcpTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    mGcpTable->horizontalHeader()->setStretchLastSection(false);
    mGcpTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mGcpTable->setEditTriggers(QAbstractItemView::DoubleClicked);
    mGcpTable->setMaximumHeight(200);

    connect(mGcpTable, &QTableWidget::cellChanged, this, [this](int row, int col) {
        if (mUpdatingTable || col == COL_ON) return;
        if (row >= mGcps.size()) return;
        bool ok;
        const double v = mGcpTable->item(row, col)->text().toDouble(&ok);
        if (!ok) return;
        switch (col) {
        case COL_SRCX: mGcps[row].srcX = v; break;
        case COL_SRCY: mGcps[row].srcY = v; break;
        case COL_REFX: mGcps[row].refX = v; break;
        case COL_REFY: mGcps[row].refY = v; break;
        default: break;
        }
        updateResiduals();
        refreshMarkers();
    });

    vbox->addWidget(mGcpTable, 1);

    // ── Status row ──
    auto *statRow = new QHBoxLayout;
    mRmsLabel    = new QLabel("RMS: —", this);
    mRmsLabel->setStyleSheet("font-weight:bold;");
    mStatusLabel = new QLabel("", this);
    statRow->addWidget(mRmsLabel);
    statRow->addWidget(mStatusLabel, 1);

    mLog = new QLabel("", this);
    mLog->setWordWrap(true);
    mLog->setStyleSheet("color:#555; font-size:8pt;");
    statRow->addWidget(mLog, 2);

    auto *closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    statRow->addWidget(closeBtn);
    vbox->addLayout(statRow);
}

// ─── GCP picking ─────────────────────────────────────────────────────────────

void GrassGeoRectDialog::startAddGcp()
{
    mWaitSrc = true;
    mWaitRef = false;
    mSrcCanvas->setPickMode(true);
    mRefCanvas->setPickMode(false);
    mPickStatus->setText("Step 1/2: Click source canvas (unrectified)");
}

void GrassGeoRectDialog::onSrcCoordPicked(double east, double north)
{
    if (!mWaitSrc) return;
    mPendSrcX = east;
    mPendSrcY = north;
    mWaitSrc = false;
    mWaitRef = true;
    mSrcCanvas->setPickMode(false);
    mRefCanvas->setPickMode(true);
    mPickStatus->setText(
        QString("Step 2/2: Click reference canvas — src=(%1, %2)")
        .arg(east, 0, 'f', 4).arg(north, 0, 'f', 4));
}

void GrassGeoRectDialog::onRefCoordPicked(double east, double north)
{
    if (!mWaitRef) return;
    mWaitRef = false;
    mRefCanvas->setPickMode(false);
    mPickStatus->setText("");
    addGcpToList(mPendSrcX, mPendSrcY, east, north);
}

void GrassGeoRectDialog::addGcpToList(double srcX, double srcY,
                                       double refX, double refY)
{
    GcpPoint g;
    g.id   = mNextId++;
    g.srcX = srcX; g.srcY = srcY;
    g.refX = refX; g.refY = refY;
    mGcps.append(g);
    updateGcpTable();
    updateResiduals();
    refreshMarkers();
}

void GrassGeoRectDialog::deleteSelectedGcp()
{
    const int row = mGcpTable->currentRow();
    if (row < 0 || row >= mGcps.size()) return;
    mGcps.removeAt(row);
    updateGcpTable();
    updateResiduals();
    refreshMarkers();
}

// ─── Table management ─────────────────────────────────────────────────────────

void GrassGeoRectDialog::updateGcpTable()
{
    mUpdatingTable = true;
    mGcpTable->setRowCount(mGcps.size());

    for (int r = 0; r < mGcps.size(); ++r)
    {
        const auto &g = mGcps[r];
        auto setItem = [&](int col, const QString &text, bool editable = false) {
            auto *item = new QTableWidgetItem(text);
            if (!editable)
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            mGcpTable->setItem(r, col, item);
        };

        setItem(COL_ID,   QString::number(g.id));
        setItem(COL_SRCX, QString::number(g.srcX, 'f', 4), true);
        setItem(COL_SRCY, QString::number(g.srcY, 'f', 4), true);
        setItem(COL_REFX, QString::number(g.refX, 'f', 4), true);
        setItem(COL_REFY, QString::number(g.refY, 'f', 4), true);
        setItem(COL_RESX, QString::number(g.resX, 'f', 4));
        setItem(COL_RESY, QString::number(g.resY, 'f', 4));

        // RMS cell — coloured by quality
        auto *rmsItem = new QTableWidgetItem(
            g.enabled ? QString::number(g.rms, 'f', 4) : "—");
        rmsItem->setFlags(rmsItem->flags() & ~Qt::ItemIsEditable);
        if (g.enabled && g.rms > 0)
        {
            // Red if high residual (above 1 px), green if low
            const int red   = qMin(255, static_cast<int>(g.rms * 100));
            const int green = qMax(0,   255 - red);
            rmsItem->setBackground(QColor(red, green, 0, 80));
        }
        mGcpTable->setItem(r, COL_RMS, rmsItem);

        // Enable checkbox
        auto *onItem = new QTableWidgetItem;
        onItem->setCheckState(g.enabled ? Qt::Checked : Qt::Unchecked);
        onItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        mGcpTable->setItem(r, COL_ON, onItem);
    }

    // Connect enable toggling
    connect(mGcpTable, &QTableWidget::itemChanged, this,
            [this](QTableWidgetItem *item) {
                if (item->column() != COL_ON || mUpdatingTable) return;
                const int r = item->row();
                if (r < mGcps.size())
                {
                    mGcps[r].enabled = (item->checkState() == Qt::Checked);
                    updateResiduals();
                    refreshMarkers();
                }
            });

    mUpdatingTable = false;
}

void GrassGeoRectDialog::updateResiduals()
{
    const int order = mOrderCombo->currentData().toInt();
    const bool ok   = mTransform.fit(mGcps, order);

    if (ok)
    {
        mTransform.computeResiduals(mGcps);
        const double totalRms = mTransform.totalRms(mGcps);
        mRmsLabel->setText(QString("Total RMS: %1  (order %2, %3 active GCPs)")
                           .arg(totalRms, 0, 'f', 4)
                           .arg(order)
                           .arg([&]{ int n=0; for(auto &g:mGcps) if(g.enabled) ++n; return n; }()));
    }
    else
    {
        for (auto &g : mGcps) g.resX = g.resY = g.rms = 0;
        mRmsLabel->setText(
            QString("RMS: — (need ≥ %1 active GCPs for order %2)")
            .arg(mTransform.minPoints()).arg(order));
    }

    // Refresh residual columns without full rebuild
    mUpdatingTable = true;
    for (int r = 0; r < mGcps.size(); ++r)
    {
        const auto &g = mGcps[r];
        if (auto *it = mGcpTable->item(r, COL_RESX))
            it->setText(QString::number(g.resX, 'f', 4));
        if (auto *it = mGcpTable->item(r, COL_RESY))
            it->setText(QString::number(g.resY, 'f', 4));
        if (auto *it = mGcpTable->item(r, COL_RMS))
        {
            it->setText(g.enabled ? QString::number(g.rms, 'f', 4) : "—");
            if (g.enabled && g.rms > 0)
            {
                const int red   = qMin(255, static_cast<int>(g.rms * 100));
                const int green = qMax(0, 255 - red);
                it->setBackground(QColor(red, green, 0, 80));
            }
        }
    }
    mUpdatingTable = false;
}

void GrassGeoRectDialog::refreshMarkers()
{
    mSrcCanvas->setGcps(mGcps, true);
    mRefCanvas->setGcps(mGcps, false);
}

// ─── POINTS file I/O ─────────────────────────────────────────────────────────

QString GrassGeoRectDialog::pointsFilePath() const
{
    // Query GRASS environment for the group POINTS file path
    QProcess p;
    p.start("g.gisenv", {"get=GISDBASE,LOCATION_NAME,MAPSET", "sep=,"});
    if (!p.waitForFinished(5000)) return {};
    const QStringList parts =
        QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed().split(',');
    if (parts.size() < 3) return {};
    return QString("%1/%2/%3/group/%4/POINTS")
           .arg(parts[0].trimmed(), parts[1].trimmed(),
                parts[2].trimmed(), mGroupEdit->text().trimmed());
}

void GrassGeoRectDialog::savePoints()
{
    QString path = pointsFilePath();
    if (path.isEmpty())
        path = QFileDialog::getSaveFileName(this, "Save POINTS file", {},
                                             "GRASS POINTS (*)");
    if (path.isEmpty()) return;

    // Ensure parent directory exists
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Save failed", "Cannot write: " + path);
        return;
    }
    QTextStream out(&f);
    out << "# Ground Control Points File\n";
    out << "# ref_east ref_north src_east src_north status(1=active)\n";
    for (const GcpPoint &g : mGcps)
        out << QString("%1 %2 %3 %4 %5\n")
               .arg(g.refX, 0, 'f', 8).arg(g.refY, 0, 'f', 8)
               .arg(g.srcX, 0, 'f', 8).arg(g.srcY, 0, 'f', 8)
               .arg(g.enabled ? 1 : 0);

    log("Saved " + QString::number(mGcps.size()) + " GCPs → " + path);
}

void GrassGeoRectDialog::loadPoints()
{
    QString path = pointsFilePath();
    if (!QFile::exists(path))
        path = QFileDialog::getOpenFileName(this, "Load POINTS file", {},
                                             "GRASS POINTS (*);;All Files (*)");
    if (path.isEmpty() || !QFile::exists(path)) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    mGcps.clear();
    mNextId = 1;
    QTextStream in(&f);
    while (!in.atEnd())
    {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        const QStringList cols = line.split(' ', Qt::SkipEmptyParts);
        if (cols.size() < 4) continue;
        GcpPoint g;
        g.id      = mNextId++;
        g.refX    = cols[0].toDouble();
        g.refY    = cols[1].toDouble();
        g.srcX    = cols[2].toDouble();
        g.srcY    = cols[3].toDouble();
        g.enabled = (cols.size() < 5 || cols[4].toInt() != 0);
        mGcps.append(g);
    }
    updateGcpTable();
    updateResiduals();
    refreshMarkers();
    log("Loaded " + QString::number(mGcps.size()) + " GCPs from " + path);
}

// ─── Rectification ────────────────────────────────────────────────────────────

void GrassGeoRectDialog::createGroup()
{
    const QString grp = mGroupEdit->text().trimmed();
    const QString src = mSrcCombo->currentText().trimmed();
    if (grp.isEmpty() || src.isEmpty()) return;

    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("i.group",
            {"group=" + grp, "input=" + src});
    if (!p.waitForFinished(15000) || p.exitCode() != 0)
    {
        log("i.group failed: " + QString::fromLocal8Bit(p.readAll()).trimmed(),
            true);
        return;
    }
    log("Group '" + grp + "' created/updated.");
}

void GrassGeoRectDialog::rectify()
{
    if (mGcps.size() < 3)
    {
        QMessageBox::warning(this, "Rectify",
            "Add at least 3 GCPs before rectifying.");
        return;
    }

    const int order = mOrderCombo->currentData().toInt();
    if (!mTransform.fit(mGcps, order))
    {
        QMessageBox::warning(this, "Rectify",
            QString("Need ≥ %1 active GCPs for order %2 polynomial.")
            .arg(mTransform.minPoints()).arg(order));
        return;
    }

    // Save the POINTS file first
    savePoints();

    // Create group
    createGroup();

    // Run i.rectify
    const QString grp    = mGroupEdit->text().trimmed();
    const QString ext    = mExtensionEdit->text().trimmed();
    const QString method = mMethodCombo->currentText();

    QStringList args = {"group="     + grp,
                         "extension=" + ext,
                         "order="     + QString::number(order),
                         "method="    + method,
                         "-c"};   // -c = use the group's POINTS file

    log("Running: i.rectify " + args.join(' '));

    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("i.rectify", args);

    if (!p.waitForFinished(300000))
    {
        log("i.rectify timed out.", true);
        return;
    }

    const QString out = QString::fromLocal8Bit(p.readAll()).trimmed();
    if (p.exitCode() != 0)
    {
        log("i.rectify failed:\n" + out, true);
        QMessageBox::critical(this, "Rectification failed", out.left(400));
        return;
    }

    log("Rectification complete. Output maps have extension '" + ext + "'.");
    QMessageBox::information(this, "Rectification complete",
        QString("Maps rectified with extension '%1'.\n"
                "Total RMS: %2\n\nOutput maps added to '%3'.")
        .arg(ext).arg(mTransform.totalRms(mGcps), 0, 'f', 4).arg(grp));
}

void GrassGeoRectDialog::log(const QString &msg, bool error)
{
    mLog->setStyleSheet(error ? "color:red; font-size:8pt;"
                               : "color:#555; font-size:8pt;");
    mLog->setText(msg);
}
