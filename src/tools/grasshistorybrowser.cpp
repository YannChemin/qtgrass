#include "grasshistorybrowser.h"
#include "../grassinterface.h"
#include "../module/grassmoduledialog.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QVBoxLayout>
#include <QRegularExpression>

GrassHistoryBrowserDialog::GrassHistoryBrowserDialog(GrassInterface *iface,
                                                      QWidget *parent)
    : QDialog(parent), mIface(iface)
{
    setWindowTitle("Map History Browser");
    resize(700, 500);

    auto *vbox = new QVBoxLayout(this);

    // ── Map selector ──
    auto *row = new QHBoxLayout;
    row->addWidget(new QLabel("Type:", this));
    mTypeCombo = new QComboBox(this);
    mTypeCombo->addItems({"raster", "vector", "raster_3d"});
    row->addWidget(mTypeCombo);

    row->addWidget(new QLabel("Map:", this));
    mMapCombo = new QComboBox(this);
    mMapCombo->setEditable(true);
    mMapCombo->setMinimumWidth(200);
    row->addWidget(mMapCombo, 1);

    auto *loadMapsBtn = new QPushButton("⟳", this);
    loadMapsBtn->setFixedWidth(28);
    loadMapsBtn->setToolTip("Refresh map list");
    connect(loadMapsBtn, &QPushButton::clicked, this, [this] {
        const QString type = mTypeCombo->currentText();
        const QString cur  = mMapCombo->currentText();
        mMapCombo->clear();
        QProcess p;
        p.start("g.list", {"type=" + type, "mapset=*"});
        if (p.waitForFinished(8000))
            for (const QString &l : QString::fromLocal8Bit(p.readAllStandardOutput())
                                    .split('\n', Qt::SkipEmptyParts))
                mMapCombo->addItem(l.trimmed());
        const int idx = mMapCombo->findText(cur);
        if (idx >= 0) mMapCombo->setCurrentIndex(idx);
        else if (!cur.isEmpty()) mMapCombo->setEditText(cur);
    });
    row->addWidget(loadMapsBtn);

    auto *showBtn = new QPushButton("Show History", this);
    connect(showBtn, &QPushButton::clicked, this, &GrassHistoryBrowserDialog::loadHistory);
    row->addWidget(showBtn);
    vbox->addLayout(row);

    // ── History text ──
    mHistoryEdit = new QPlainTextEdit(this);
    mHistoryEdit->setReadOnly(true);
    mHistoryEdit->setFont(QFont("Monospace", 9));
    mHistoryEdit->setPlaceholderText(
        "Select a map type and name, then click Show History.");
    vbox->addWidget(mHistoryEdit, 1);

    // ── Re-run ──
    auto *reRunRow = new QHBoxLayout;
    mStatusLabel = new QLabel("", this);
    mStatusLabel->setStyleSheet("font-size:8pt; color:#555;");
    reRunRow->addWidget(mStatusLabel, 1);

    auto *reRunBtn = new QPushButton("Re-run Creation Command…", this);
    reRunBtn->setToolTip(
        "Parse the creation command from history and open its module dialog");
    connect(reRunBtn, &QPushButton::clicked,
            this, &GrassHistoryBrowserDialog::reRunSelected);
    reRunRow->addWidget(reRunBtn);

    auto *closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    reRunRow->addWidget(closeBtn);
    vbox->addLayout(reRunRow);

    // Initial map list load
    loadMapsBtn->click();
}

void GrassHistoryBrowserDialog::loadHistory()
{
    const QString type = mTypeCombo->currentText();
    const QString map  = mMapCombo->currentText().trimmed();
    if (map.isEmpty()) return;

    const QString prog = (type == "vector")    ? "v.info"
                       : (type == "raster_3d") ? "r3.info"
                       :                          "r.info";

    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(prog, {"-h", "map=" + map});
    if (!p.waitForFinished(15000))
    { mStatusLabel->setText("Timed out."); return; }

    const QString out = QString::fromLocal8Bit(p.readAll()).trimmed();
    if (p.exitCode() != 0 || out.isEmpty())
    {
        mStatusLabel->setText("No history for map: " + map);
        mHistoryEdit->clear();
        return;
    }

    mHistoryEdit->setPlainText(out);
    mStatusLabel->setText(QString("History for %1 '%2'").arg(type, map));
}

void GrassHistoryBrowserDialog::reRunSelected()
{
    const QString text = mHistoryEdit->toPlainText().trimmed();
    if (text.isEmpty()) { mStatusLabel->setText("Load history first."); return; }

    // Find the first non-comment line that looks like a GRASS command
    // History lines typically start with a module name like "r.slope.aspect"
    static const QRegularExpression moduleRe(
        R"(^\s*(r\.|v\.|g\.|d\.|i\.|t\.|db\.|r3\.|m\.|ps\.|s\.\w+))",
        QRegularExpression::MultilineOption);

    QString cmd;
    const auto match = moduleRe.match(text);
    if (match.hasMatch())
    {
        // Extract the line containing the match
        const int lineStart = text.lastIndexOf('\n', match.capturedStart()) + 1;
        int lineEnd   = text.indexOf('\n', match.capturedStart());
        if (lineEnd < 0) lineEnd = text.size();
        cmd = text.mid(lineStart, lineEnd - lineStart).trimmed();
    }

    if (cmd.isEmpty())
    {
        mStatusLabel->setText("No recognisable GRASS command found in history.");
        return;
    }

    // Parse: first token = module name, rest = args
    const QStringList parts = cmd.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return;

    const QString module = parts[0];
    const QStringList args = parts.mid(1);

    mStatusLabel->setText("Opening: " + cmd.left(80));
    auto *dlg = new GrassModuleDialog(module, args, mIface, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}
