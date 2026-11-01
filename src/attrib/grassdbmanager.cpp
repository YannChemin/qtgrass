#include "grassdbmanager.h"
#include "grassfieldcalc.h"
#include "../grassinterface.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QToolBar>
#include <QVBoxLayout>

// ─── GrassSqlHighlighter ──────────────────────────────────────────────────────

GrassSqlHighlighter::GrassSqlHighlighter(QTextDocument *doc)
    : QSyntaxHighlighter(doc)
{
    auto addRule = [this](const QString &pattern,
                          const QColor &color, bool bold = false) {
        Rule r;
        r.re = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
        r.fmt.setForeground(color);
        if (bold) r.fmt.setFontWeight(QFont::Bold);
        mRules.append(r);
    };

    // Keywords
    const QString kws = "\\b(SELECT|FROM|WHERE|INSERT|INTO|VALUES|UPDATE|SET|"
                        "DELETE|CREATE|TABLE|DROP|ALTER|ADD|COLUMN|INDEX|VIEW|"
                        "JOIN|LEFT|RIGHT|INNER|OUTER|FULL|ON|AND|OR|NOT|NULL|"
                        "IS|IN|LIKE|BETWEEN|ORDER|BY|GROUP|HAVING|DISTINCT|AS|"
                        "UNION|ALL|LIMIT|OFFSET|CAST|CASE|WHEN|THEN|ELSE|END|"
                        "PRIMARY|KEY|UNIQUE|DEFAULT|REFERENCES|FOREIGN|CHECK|"
                        "COUNT|SUM|AVG|MIN|MAX|ABS|ROUND|UPPER|LOWER|LENGTH|"
                        "SUBSTR|TRIM|COALESCE|NULLIF|EXISTS|TRUE|FALSE)\\b";
    addRule(kws, QColor(30, 100, 200), true);

    // String literals
    addRule("'[^']*'", QColor(0, 130, 0));

    // Numbers
    addRule("\\b[0-9]+(\\.[0-9]+)?([eE][+-]?[0-9]+)?\\b", QColor(130, 0, 200));

    // -- comments
    addRule("--[^\n]*", QColor(100, 100, 100));
}

void GrassSqlHighlighter::highlightBlock(const QString &text)
{
    for (const Rule &r : mRules)
    {
        QRegularExpressionMatchIterator it = r.re.globalMatch(text);
        while (it.hasNext())
        {
            const auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), r.fmt);
        }
    }
}

// ─── GrassDbManagerDialog ────────────────────────────────────────────────────

GrassDbManagerDialog::GrassDbManagerDialog(GrassInterface *iface,
                                             const QString &mapName,
                                             const QString &mapset,
                                             int layer,
                                             QWidget *parent)
    : QDialog(parent)
    , mIface(iface)
{
    setWindowTitle("Database Manager");
    resize(1000, 680);

    auto *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(4);

    buildMapSelector(vbox);
    buildToolbar(vbox);

    mTabs = new QTabWidget(this);
    mTabs->addTab(buildDataTab(),   "Data");
    mTabs->addTab(buildSqlTab(),    "SQL Editor");
    mTabs->addTab(buildSchemaTab(), "Schema");
    vbox->addWidget(mTabs, 1);

    // Pre-load if a map was provided
    if (!mapName.isEmpty())
    {
        const QString ref = mapset.isEmpty() ? mapName : mapName + "@" + mapset;
        mMapCombo->setEditText(ref);
        mLayerSpin->setValue(layer);
        loadMap();
    }
    else
    {
        // Populate map combo from g.list
        QProcess p;
        p.start("g.list", {"type=vector", "mapset=."});
        if (p.waitForFinished(8000))
        {
            for (const QString &ln : QString::fromLocal8Bit(p.readAllStandardOutput())
                                     .split('\n', Qt::SkipEmptyParts))
                mMapCombo->addItem(ln.trimmed());
        }
    }
}

// ─── UI builders ─────────────────────────────────────────────────────────────

void GrassDbManagerDialog::buildMapSelector(QVBoxLayout *vbox)
{
    auto *row = new QHBoxLayout;
    row->addWidget(new QLabel("Vector map:", this));

    mMapCombo = new QComboBox(this);
    mMapCombo->setEditable(true);
    mMapCombo->setMinimumWidth(200);
    row->addWidget(mMapCombo, 1);

    row->addWidget(new QLabel("Layer:", this));
    mLayerSpin = new QSpinBox(this);
    mLayerSpin->setRange(1, 99);
    mLayerSpin->setValue(1);
    mLayerSpin->setFixedWidth(52);
    row->addWidget(mLayerSpin);

    auto *loadBtn = new QPushButton("Load", this);
    connect(loadBtn, &QPushButton::clicked, this, &GrassDbManagerDialog::loadMap);
    row->addWidget(loadBtn);

    vbox->addLayout(row);
}

void GrassDbManagerDialog::buildToolbar(QVBoxLayout *vbox)
{
    auto *tb = new QToolBar(this);
    tb->setMovable(false);
    tb->setIconSize({16, 16});

    tb->addAction("Add Column",  this, &GrassDbManagerDialog::addColumn);
    tb->addAction("Drop Column", this, &GrassDbManagerDialog::dropColumn);
    tb->addSeparator();
    tb->addAction("Field Calculator…", this, &GrassDbManagerDialog::openFieldCalc);
    tb->addSeparator();
    tb->addAction("Export CSV…", this, &GrassDbManagerDialog::exportCsv);
    tb->addAction("Import CSV…", this, &GrassDbManagerDialog::importCsv);
    tb->addSeparator();
    tb->addAction("Refresh", this, [this] { loadMap(); });

    vbox->addWidget(tb);
}

QWidget *GrassDbManagerDialog::buildDataTab()
{
    auto *w    = new QWidget(this);
    auto *vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(0, 4, 0, 0);

    mDataTable = new QTableWidget(w);
    mDataTable->setAlternatingRowColors(true);
    mDataTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mDataTable->horizontalHeader()->setStretchLastSection(false);
    mDataTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    mDataTable->setSortingEnabled(true);

    connect(mDataTable, &QTableWidget::cellChanged,
            this, &GrassDbManagerDialog::onCellChanged);

    vbox->addWidget(mDataTable, 1);

    auto *btmRow = new QHBoxLayout;
    btmRow->addWidget(new QLabel("Filter:", w));
    mFilterEdit = new QLineEdit(w);
    mFilterEdit->setPlaceholderText("Type to filter any column…");
    connect(mFilterEdit, &QLineEdit::textChanged,
            this, &GrassDbManagerDialog::applyFilter);
    btmRow->addWidget(mFilterEdit, 1);

    mDataStatus = new QLabel("No data loaded.", w);
    btmRow->addWidget(mDataStatus);
    vbox->addLayout(btmRow);

    return w;
}

QWidget *GrassDbManagerDialog::buildSqlTab()
{
    auto *w    = new QWidget(this);
    auto *vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(0, 4, 0, 0);

    // History + execute bar
    auto *topRow = new QHBoxLayout;
    topRow->addWidget(new QLabel("History:", w));
    mHistCombo = new QComboBox(w);
    mHistCombo->setEditable(false);
    mHistCombo->setMinimumWidth(200);
    connect(mHistCombo, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (i >= 0 && i < mSqlHistory.size())
            mSqlEdit->setPlainText(mSqlHistory[i]);
    });
    topRow->addWidget(mHistCombo, 1);
    auto *execBtn = new QPushButton("▶ Execute", w);
    execBtn->setShortcut(QKeySequence("Ctrl+Return"));
    connect(execBtn, &QPushButton::clicked, this, &GrassDbManagerDialog::executeSql);
    topRow->addWidget(execBtn);
    vbox->addLayout(topRow);

    auto *splitter = new QSplitter(Qt::Vertical, w);

    // SQL editor
    mSqlEdit = new QPlainTextEdit(splitter);
    mSqlEdit->setFont(QFont("Monospace", 10));
    mSqlEdit->setPlaceholderText(
        "SELECT * FROM roads WHERE area > 100\n"
        "-- Ctrl+Return to execute");
    new GrassSqlHighlighter(mSqlEdit->document());
    splitter->addWidget(mSqlEdit);

    // Results
    auto *resWidget = new QWidget(splitter);
    auto *rv = new QVBoxLayout(resWidget);
    rv->setContentsMargins(0, 0, 0, 0);
    mResultTable = new QTableWidget(resWidget);
    mResultTable->setAlternatingRowColors(true);
    mResultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mResultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    mResultTable->setSortingEnabled(true);
    rv->addWidget(mResultTable, 1);
    mSqlStatus = new QLabel("Ready.", resWidget);
    rv->addWidget(mSqlStatus);
    splitter->addWidget(resWidget);
    splitter->setSizes({200, 300});

    vbox->addWidget(splitter, 1);
    return w;
}

QWidget *GrassDbManagerDialog::buildSchemaTab()
{
    auto *w    = new QWidget(this);
    auto *vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(0, 4, 0, 0);

    mConnLabel = new QLabel("Not connected.", w);
    mConnLabel->setWordWrap(true);
    mConnLabel->setStyleSheet("font-family:monospace; color:#444;");
    vbox->addWidget(mConnLabel);

    mSchemaTable = new QTableWidget(0, 3, w);
    mSchemaTable->setHorizontalHeaderLabels({"Column", "Type", "Key"});
    mSchemaTable->horizontalHeader()->setStretchLastSection(true);
    mSchemaTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mSchemaTable->setAlternatingRowColors(true);
    vbox->addWidget(mSchemaTable, 1);

    auto *btnRow = new QHBoxLayout;
    auto *addColBtn = new QPushButton("Add Column…", w);
    connect(addColBtn, &QPushButton::clicked, this, &GrassDbManagerDialog::addColumn);
    auto *dropColBtn = new QPushButton("Drop Column…", w);
    connect(dropColBtn, &QPushButton::clicked, this, &GrassDbManagerDialog::dropColumn);
    btnRow->addWidget(addColBtn);
    btnRow->addWidget(dropColBtn);
    btnRow->addStretch();
    vbox->addLayout(btnRow);

    return w;
}

// ─── Map loading ──────────────────────────────────────────────────────────────

GrassVectorConn GrassDbManagerDialog::queryConnection(const QString &map, int layer)
{
    GrassVectorConn conn;
    conn.mapName = map;
    conn.layer   = layer;

    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("v.db.connect",
            {"-g", "map=" + map, "layer=" + QString::number(layer)});
    if (!p.waitForFinished(10000)) return conn;

    for (const QString &line :
         QString::fromLocal8Bit(p.readAll()).split('\n', Qt::SkipEmptyParts))
    {
        const QStringList cols = line.split('|');
        if (cols.size() < 5) continue;
        if (cols[0].trimmed().toInt() != layer) continue;
        conn.table    = cols[1].trimmed();
        conn.database = cols[2].trimmed();
        conn.key      = cols[3].trimmed();
        conn.driver   = cols[4].trimmed();
        conn.valid    = true;
        break;
    }
    return conn;
}

QStringList GrassDbManagerDialog::runVDbSelect(bool withHeaders)
{
    if (!mConn.valid) return {};
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    QStringList args = {"map=" + mConn.mapName,
                         "layer=" + QString::number(mConn.layer),
                         "separator=pipe"};
    if (!withHeaders) args << "-c";
    p.start("v.db.select", args);
    if (!p.waitForFinished(60000)) return {};
    return QString::fromLocal8Bit(p.readAll())
           .split('\n', Qt::SkipEmptyParts);
}

void GrassDbManagerDialog::loadMap()
{
    const QString map   = mMapCombo->currentText().trimmed();
    const int     layer = mLayerSpin->value();
    if (map.isEmpty()) return;

    mConn = queryConnection(map, layer);
    if (!mConn.valid)
    {
        mDataStatus->setText("No attribute table for this layer.");
        mDataTable->clearContents();
        mDataTable->setRowCount(0);
        mDataTable->setColumnCount(0);
        mConnLabel->setText("No connection found.");
        return;
    }

    setWindowTitle(QString("Database Manager — %1 (layer %2)")
                   .arg(map).arg(layer));
    mConnLabel->setText(QString("driver=%1  database=%2  table=%3  key=%4")
                        .arg(mConn.driver, mConn.database, mConn.table, mConn.key));

    refreshData();
    refreshSchema();
}

void GrassDbManagerDialog::refreshData()
{
    const QStringList lines = runVDbSelect(true);
    if (lines.isEmpty())
    {
        mDataStatus->setText("No data.");
        return;
    }

    mEditGuard = true;
    mDataTable->setSortingEnabled(false);

    // Header row
    mHeaders = lines[0].split('|');
    mKeyCol  = mHeaders.indexOf(mConn.key);
    if (mKeyCol < 0) mKeyCol = 0;

    mDataTable->setColumnCount(mHeaders.size());
    mDataTable->setHorizontalHeaderLabels(mHeaders);

    // Data rows
    mDataTable->setRowCount(lines.size() - 1);
    for (int r = 0; r < lines.size() - 1; ++r)
    {
        const QStringList vals = lines[r + 1].split('|');
        for (int c = 0; c < mHeaders.size(); ++c)
        {
            const QString v = (c < vals.size()) ? vals[c] : QString();
            auto *item = new QTableWidgetItem(v);
            if (c == mKeyCol)
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            mDataTable->setItem(r, c, item);
        }
    }

    mDataTable->setSortingEnabled(true);
    mDataTable->resizeColumnsToContents();
    mEditGuard = false;

    mDataStatus->setText(QString("%1 rows | key: %2")
                         .arg(mDataTable->rowCount()).arg(mConn.key));
}

void GrassDbManagerDialog::refreshSchema()
{
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("db.describe",
            {"-c",
             "table="    + mConn.table,
             "database=" + mConn.database,
             "driver="   + mConn.driver});
    if (!p.waitForFinished(10000)) return;

    mSchemaTable->setRowCount(0);
    const QRegularExpression colRe(
        R"(Col\s+\d+:\s+(\S+)\s+(.+))", QRegularExpression::CaseInsensitiveOption);

    for (const QString &line :
         QString::fromLocal8Bit(p.readAll()).split('\n', Qt::SkipEmptyParts))
    {
        const auto m = colRe.match(line);
        if (!m.hasMatch()) continue;

        const QString colName = m.captured(1);
        const QString colType = m.captured(2).trimmed();
        const int row = mSchemaTable->rowCount();
        mSchemaTable->insertRow(row);
        mSchemaTable->setItem(row, 0, new QTableWidgetItem(colName));
        mSchemaTable->setItem(row, 1, new QTableWidgetItem(colType));
        mSchemaTable->setItem(row, 2,
            new QTableWidgetItem(colName == mConn.key ? "✓" : ""));
    }
    mSchemaTable->resizeColumnsToContents();
}

// ─── Data tab interactions ───────────────────────────────────────────────────

void GrassDbManagerDialog::onCellChanged(int row, int col)
{
    if (mEditGuard || !mConn.valid) return;
    if (col == mKeyCol) return;   // key column is read-only

    auto *keyItem = mDataTable->item(row, mKeyCol);
    auto *valItem = mDataTable->item(row, col);
    if (!keyItem || !valItem) return;

    const QString colName  = mHeaders.value(col);
    const QString keyValue = keyItem->text();
    const QString value    = valItem->text();

    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("v.db.update",
            {"map="    + mConn.mapName,
             "layer="  + QString::number(mConn.layer),
             "column=" + colName,
             "value="  + value,
             "where="  + mConn.key + "=" + keyValue});
    if (!p.waitForFinished(15000) || p.exitCode() != 0)
    {
        const QString err = QString::fromLocal8Bit(p.readAll()).trimmed();
        QMessageBox::warning(this, "Update failed",
                             QString("Could not update %1=%2 for %3=%4\n\n%5")
                             .arg(colName, value, mConn.key, keyValue, err));
        // Revert cell
        mEditGuard = true;
        valItem->setText(QString()); // clear to show error
        mEditGuard = false;
    }
}

void GrassDbManagerDialog::applyFilter(const QString &text)
{
    for (int r = 0; r < mDataTable->rowCount(); ++r)
    {
        bool match = text.isEmpty();
        if (!match)
        {
            for (int c = 0; c < mDataTable->columnCount() && !match; ++c)
            {
                const auto *item = mDataTable->item(r, c);
                if (item && item->text().contains(text, Qt::CaseInsensitive))
                    match = true;
            }
        }
        mDataTable->setRowHidden(r, !match);
    }
}

// ─── SQL Editor ──────────────────────────────────────────────────────────────

void GrassDbManagerDialog::executeSql()
{
    if (!mConn.valid)
    {
        mSqlStatus->setText("Load a map first.");
        return;
    }

    const QString sql = mSqlEdit->toPlainText().trimmed();
    if (sql.isEmpty()) return;

    // Add to history
    if (!mSqlHistory.contains(sql))
    {
        mSqlHistory.prepend(sql);
        if (mSqlHistory.size() > 50) mSqlHistory.removeLast();
        mHistCombo->blockSignals(true);
        mHistCombo->insertItem(0, sql.left(60).replace('\n', ' '));
        mHistCombo->blockSignals(false);
    }

    const bool isSelect = sql.startsWith("SELECT", Qt::CaseInsensitive) ||
                          sql.startsWith("WITH",   Qt::CaseInsensitive);

    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    QElapsedTimer timer; timer.start();

    if (isSelect)
    {
        p.start("db.select",
                {"sql="      + sql,
                 "database=" + mConn.database,
                 "driver="   + mConn.driver,
                 "separator=pipe"});
    }
    else
    {
        p.start("db.execute",
                {"sql="      + sql,
                 "database=" + mConn.database,
                 "driver="   + mConn.driver});
    }

    if (!p.waitForFinished(60000))
    {
        mSqlStatus->setText("Query timed out (>60s).");
        return;
    }

    const qint64 ms = timer.elapsed();
    const QStringList lines = QString::fromLocal8Bit(p.readAll())
                              .split('\n', Qt::SkipEmptyParts);

    if (p.exitCode() != 0)
    {
        mSqlStatus->setText("ERROR: " + lines.join(" | ").left(200));
        mResultTable->setRowCount(0);
        mResultTable->setColumnCount(0);
        return;
    }

    if (isSelect)
    {
        populateResults(lines);
        mSqlStatus->setText(QString("%1 rows returned in %2 ms")
                            .arg(mResultTable->rowCount()).arg(ms));
    }
    else
    {
        mResultTable->setRowCount(0);
        mResultTable->setColumnCount(0);
        mSqlStatus->setText(QString("OK — executed in %1 ms").arg(ms));
        // Refresh data tab if it's a modifying query
        refreshData();
    }
}

void GrassDbManagerDialog::populateResults(const QStringList &lines)
{
    mResultTable->setSortingEnabled(false);
    mResultTable->clearContents();
    if (lines.isEmpty()) { mResultTable->setRowCount(0); mResultTable->setColumnCount(0); return; }

    const QStringList headers = lines[0].split('|');
    mResultTable->setColumnCount(headers.size());
    mResultTable->setHorizontalHeaderLabels(headers);
    mResultTable->setRowCount(lines.size() - 1);

    for (int r = 0; r < lines.size() - 1; ++r)
    {
        const QStringList vals = lines[r + 1].split('|');
        for (int c = 0; c < headers.size(); ++c)
        {
            auto *item = new QTableWidgetItem(c < vals.size() ? vals[c] : QString());
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            mResultTable->setItem(r, c, item);
        }
    }
    mResultTable->setSortingEnabled(true);
    mResultTable->resizeColumnsToContents();
}

// ─── Column management ────────────────────────────────────────────────────────

void GrassDbManagerDialog::addColumn()
{
    if (!mConn.valid) return;

    QDialog dlg(this);
    dlg.setWindowTitle("Add Column");
    auto *form = new QFormLayout(&dlg);

    auto *nameEdit  = new QLineEdit(&dlg);
    auto *typeCombo = new QComboBox(&dlg);
    typeCombo->addItems({"INTEGER", "DOUBLE PRECISION", "VARCHAR(50)",
                         "VARCHAR(100)", "VARCHAR(255)", "TEXT"});
    typeCombo->setEditable(true);
    form->addRow("Name:",  nameEdit);
    form->addRow("Type:",  typeCombo);

    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() != QDialog::Accepted) return;
    const QString name = nameEdit->text().trimmed();
    const QString type = typeCombo->currentText().trimmed();
    if (name.isEmpty()) return;

    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("v.db.addcolumn",
            {"map="     + mConn.mapName,
             "layer="   + QString::number(mConn.layer),
             "columns=" + name + " " + type});
    if (!p.waitForFinished(15000) || p.exitCode() != 0)
    {
        QMessageBox::warning(this, "Add column failed",
            QString::fromLocal8Bit(p.readAll()).trimmed());
        return;
    }
    loadMap();
}

void GrassDbManagerDialog::dropColumn()
{
    if (!mConn.valid) return;

    QStringList cols = mHeaders;
    cols.removeAll(mConn.key);   // cannot drop key column

    bool ok;
    const QString col = QInputDialog::getItem(
        this, "Drop Column", "Select column to drop:", cols, 0, false, &ok);
    if (!ok || col.isEmpty()) return;

    if (QMessageBox::warning(this, "Drop column",
            QString("Drop column '%1' permanently from '%2'?")
            .arg(col, mConn.table),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;

    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("v.db.dropcolumn",
            {"map="    + mConn.mapName,
             "layer="  + QString::number(mConn.layer),
             "column=" + col});
    if (!p.waitForFinished(15000) || p.exitCode() != 0)
    {
        QMessageBox::warning(this, "Drop column failed",
            QString::fromLocal8Bit(p.readAll()).trimmed());
        return;
    }
    loadMap();
}

// ─── Field calculator ────────────────────────────────────────────────────────

void GrassDbManagerDialog::openFieldCalc()
{
    if (!mConn.valid) { QMessageBox::information(this, "Field Calculator",
        "Load a vector map first."); return; }

    auto *dlg = new GrassFieldCalcDialog(mConn, mHeaders, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &GrassFieldCalcDialog::calculationDone,
            this, &GrassDbManagerDialog::loadMap);
    dlg->show();
}

// ─── Import / Export ─────────────────────────────────────────────────────────

void GrassDbManagerDialog::exportCsv()
{
    if (mDataTable->rowCount() == 0)
    { QMessageBox::information(this, "Export CSV", "No data to export."); return; }

    const QString path = QFileDialog::getSaveFileName(
        this, "Export CSV", {}, "CSV files (*.csv);;All Files (*)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Export failed", "Cannot write: " + path);
        return;
    }
    QTextStream out(&f);

    // Headers
    QStringList hdr;
    for (int c = 0; c < mDataTable->columnCount(); ++c)
        hdr << mDataTable->horizontalHeaderItem(c)->text();
    out << hdr.join(",") << "\n";

    // Rows (visible only)
    int exported = 0;
    for (int r = 0; r < mDataTable->rowCount(); ++r)
    {
        if (mDataTable->isRowHidden(r)) continue;
        QStringList row;
        for (int c = 0; c < mDataTable->columnCount(); ++c)
        {
            const auto *item = mDataTable->item(r, c);
            QString v = item ? item->text() : QString();
            if (v.contains(',') || v.contains('"') || v.contains('\n'))
                v = '"' + v.replace('"', "\"\"") + '"';
            row << v;
        }
        out << row.join(",") << "\n";
        ++exported;
    }

    QMessageBox::information(this, "CSV exported",
        QString("Saved %1 rows to:\n%2").arg(exported).arg(path));
}

void GrassDbManagerDialog::importCsv()
{
    if (!mConn.valid) return;

    const QString path = QFileDialog::getOpenFileName(
        this, "Import CSV", {}, "CSV files (*.csv);;All Files (*)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Import failed", "Cannot open: " + path);
        return;
    }
    QTextStream in(&f);

    // Read header
    const QString headerLine = in.readLine();
    const QStringList csvCols = headerLine.split(',');

    // Check columns match (basic check)
    int matched = 0;
    for (const QString &c : csvCols)
        if (mHeaders.contains(c.trimmed(), Qt::CaseInsensitive)) ++matched;

    if (matched == 0)
    {
        QMessageBox::warning(this, "Import CSV",
            "No matching columns found between CSV and table.\n"
            "CSV headers: " + csvCols.join(", ") + "\n"
            "Table columns: " + mHeaders.join(", "));
        return;
    }

    // Build and execute INSERT statements via db.execute
    int imported = 0, errors = 0;
    while (!in.atEnd())
    {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        const QStringList vals = line.split(',');

        QStringList colList, valList;
        for (int i = 0; i < csvCols.size() && i < vals.size(); ++i)
        {
            colList << csvCols[i].trimmed();
            QString v = vals[i].trimmed();
            v.remove(QRegularExpression("^\"|\"$"));
            v.replace("'", "''");
            valList << "'" + v + "'";
        }

        const QString sql = QString("INSERT INTO %1 (%2) VALUES (%3)")
                            .arg(mConn.table,
                                 colList.join(", "),
                                 valList.join(", "));
        QProcess p;
        p.start("db.execute",
                {"sql="      + sql,
                 "database=" + mConn.database,
                 "driver="   + mConn.driver});
        if (p.waitForFinished(10000) && p.exitCode() == 0) ++imported;
        else ++errors;
    }

    QMessageBox::information(this, "Import complete",
        QString("Imported: %1 rows\nErrors: %2").arg(imported).arg(errors));
    loadMap();
}
