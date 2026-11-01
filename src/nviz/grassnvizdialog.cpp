#include "grassnvizdialog.h"
#include "grassnvizwidget.h"
#include "../grassinterface.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

GrassNvizDialog::GrassNvizDialog(GrassInterface *iface, QWidget *parent)
    : QDialog(parent), mIface(iface)
{
    setWindowTitle("3D View (nviz)");
    resize(1000, 680);

    auto *hbox = new QHBoxLayout(this);

    // ── 3D viewport (left) ──
    mView = new GrassNvizWidget(this);
    connect(mView, &GrassNvizWidget::loadingDone, this,
            [this](bool ok, const QString &msg) {
                mStatusLabel->setText(ok ? msg : "Error: " + mView->lastError());
                mStatusLabel->setStyleSheet(ok ? "color:#333;" : "color:red;");
            });

    // ── Controls (right) ──
    auto *ctrlWidget = new QWidget(this);
    ctrlWidget->setFixedWidth(230);
    auto *vbox = new QVBoxLayout(ctrlWidget);
    vbox->setSpacing(8);

    // Map selectors
    auto *mapsGroup = new QGroupBox("Maps", ctrlWidget);
    auto *mf = new QFormLayout(mapsGroup);

    mElevCombo = new QComboBox(mapsGroup);
    mElevCombo->setEditable(true);
    mf->addRow("Elevation:", mElevCombo);

    mColorCombo = new QComboBox(mapsGroup);
    mColorCombo->setEditable(true);
    mColorCombo->addItem("(same as elevation)", "");
    mf->addRow("Color drape:", mColorCombo);

    // Populate from g.list
    {
        QProcess p;
        p.start("g.list", {"type=raster", "mapset=*"});
        if (p.waitForFinished(8000))
            for (const QString &l : QString::fromLocal8Bit(p.readAllStandardOutput())
                                    .split('\n', Qt::SkipEmptyParts))
            {
                mElevCombo->addItem(l.trimmed());
                mColorCombo->addItem(l.trimmed());
            }
    }

    auto *loadBtn = new QPushButton("▶ Load / Render", mapsGroup);
    loadBtn->setDefault(true);
    connect(loadBtn, &QPushButton::clicked, this, &GrassNvizDialog::loadMap);
    mf->addRow(loadBtn);
    vbox->addWidget(mapsGroup);

    // Display controls
    auto *dispGroup = new QGroupBox("Display", ctrlWidget);
    auto *df = new QFormLayout(dispGroup);

    mExagSpin = new QDoubleSpinBox(dispGroup);
    mExagSpin->setRange(0.1, 100.0);
    mExagSpin->setValue(1.0);
    mExagSpin->setSingleStep(0.5);
    mExagSpin->setSuffix("×");
    mExagSpin->setToolTip("Vertical exaggeration factor");
    connect(mExagSpin, &QDoubleSpinBox::valueChanged,
            this, [this](double v) { mView->setExaggeration(v); });
    df->addRow("Z exaggeration:", mExagSpin);

    mWireCheck = new QCheckBox("Wireframe", dispGroup);
    connect(mWireCheck, &QCheckBox::toggled,
            mView, &GrassNvizWidget::setWireframe);
    df->addRow(mWireCheck);

    auto *resetBtn = new QPushButton("Reset View", dispGroup);
    connect(resetBtn, &QPushButton::clicked, mView, &GrassNvizWidget::resetView);
    df->addRow(resetBtn);
    vbox->addWidget(dispGroup);

    // Navigation hint
    auto *hintBox = new QGroupBox("Navigation", ctrlWidget);
    auto *hv = new QVBoxLayout(hintBox);
    auto *hintLabel = new QLabel(
        "<small>"
        "• Left-drag: orbit<br/>"
        "• Right-drag: pan<br/>"
        "• Scroll wheel: zoom in/out<br/>"
        "• Z exaggeration: vertical scale"
        "</small>", hintBox);
    hintLabel->setTextFormat(Qt::RichText);
    hintLabel->setWordWrap(true);
    hv->addWidget(hintLabel);
    vbox->addWidget(hintBox);

    vbox->addStretch();

    // Export
    auto *expBtn = new QPushButton("Export PNG…", ctrlWidget);
    connect(expBtn, &QPushButton::clicked, this, &GrassNvizDialog::exportPng);
    vbox->addWidget(expBtn);

    auto *closeBtn = new QPushButton("Close", ctrlWidget);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    vbox->addWidget(closeBtn);

    mStatusLabel = new QLabel("Load a raster to begin.", ctrlWidget);
    mStatusLabel->setWordWrap(true);
    mStatusLabel->setStyleSheet("color:#555; font-size:8pt;");
    vbox->addWidget(mStatusLabel);

    hbox->addWidget(mView, 1);
    hbox->addWidget(ctrlWidget);
}

void GrassNvizDialog::loadMap()
{
    const QString elev  = mElevCombo->currentText().trimmed();
    if (elev.isEmpty())
    { mStatusLabel->setText("Select an elevation raster."); return; }

    const QString color = mColorCombo->currentData().toString().isEmpty()
                          ? mColorCombo->currentText().trimmed()
                          : QString();   // empty = same as elevation

    mStatusLabel->setText("Loading " + elev + "…");
    QApplication::processEvents();

    mView->loadElevation(elev, color);
}

void GrassNvizDialog::exportPng()
{
    const QString path = QFileDialog::getSaveFileName(
        this, "Export 3D View", {}, "PNG Image (*.png)");
    if (path.isEmpty()) return;

    const QPixmap pix = mView->grab();
    if (!pix.save(path, "PNG"))
        QMessageBox::warning(this, "Export failed", "Cannot write: " + path);
    else
        mStatusLabel->setText("Exported → " + path);
}
