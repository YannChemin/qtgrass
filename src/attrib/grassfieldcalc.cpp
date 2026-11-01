#include "grassfieldcalc.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QVBoxLayout>

GrassFieldCalcDialog::GrassFieldCalcDialog(const GrassVectorConn &conn,
                                             const QStringList &columns,
                                             QWidget *parent)
    : QDialog(parent), mConn(conn), mColumns(columns)
{
    setWindowTitle("Field Calculator — " + conn.mapName);
    resize(640, 540);
    buildUi();
}

void GrassFieldCalcDialog::buildUi()
{
    auto *vbox = new QVBoxLayout(this);
    vbox->setSpacing(6);

    // ── Target column ──────────────────────────────────────────────────────
    auto *targetGroup = new QGroupBox("Target Column", this);
    auto *tform = new QFormLayout(targetGroup);

    mTargetCombo = new QComboBox(targetGroup);
    mTargetCombo->addItem("— New column —", "new");
    for (const QString &c : mColumns)
        if (c != mConn.key) mTargetCombo->addItem(c, c);
    tform->addRow("Column:", mTargetCombo);

    // New column controls (hidden unless "new" selected)
    mNewNameEdit = new QLineEdit(targetGroup);
    mNewNameEdit->setPlaceholderText("new_field_name");
    mNewNameEdit->setVisible(false);
    tform->addRow("New name:", mNewNameEdit);

    mNewTypeCombo = new QComboBox(targetGroup);
    mNewTypeCombo->addItems({"INTEGER", "DOUBLE PRECISION",
                             "VARCHAR(50)", "VARCHAR(100)", "TEXT"});
    mNewTypeCombo->setVisible(false);
    tform->addRow("Type:", mNewTypeCombo);

    connect(mTargetCombo, &QComboBox::currentIndexChanged,
            this, &GrassFieldCalcDialog::onTargetChanged);
    vbox->addWidget(targetGroup);

    // ── Expression ────────────────────────────────────────────────────────
    auto *exprGroup = new QGroupBox("Expression (SQL)", this);
    auto *ev = new QVBoxLayout(exprGroup);

    // Field reference buttons
    auto *fieldsRow = new QHBoxLayout;
    fieldsRow->addWidget(new QLabel("Fields:", exprGroup));
    for (const QString &col : mColumns)
    {
        auto *btn = new QPushButton(col, exprGroup);
        btn->setFixedHeight(22);
        btn->setToolTip("Insert field: " + col);
        connect(btn, &QPushButton::clicked, this,
                [this, col] { insertText(col); });
        fieldsRow->addWidget(btn);
    }
    fieldsRow->addStretch();
    ev->addLayout(fieldsRow);

    // Function / operator buttons
    auto *funcRow = new QHBoxLayout;
    const QStringList funcs = {"ABS()", "ROUND(,0)", "SQRT()",
                                "UPPER()", "LOWER()", "LENGTH()",
                                "SUBSTR(,1,1)", "CAST( AS INTEGER)",
                                "+", "-", "*", "/", "||"};
    for (const QString &f : funcs)
    {
        auto *btn = new QPushButton(f, exprGroup);
        btn->setFixedHeight(22);
        btn->setToolTip("Insert: " + f);
        connect(btn, &QPushButton::clicked, this,
                [this, f] { insertText(f.left(f.indexOf('(') < 0 ? f.size() : f.indexOf('(') + 1)); });
        funcRow->addWidget(btn);
    }
    funcRow->addStretch();
    ev->addLayout(funcRow);

    mExprEdit = new QPlainTextEdit(exprGroup);
    mExprEdit->setFont(QFont("Monospace", 10));
    mExprEdit->setMaximumHeight(100);
    mExprEdit->setPlaceholderText(
        "e.g.  area * 0.0001\n"
        "      ROUND(perimeter / area, 3)\n"
        "      UPPER(name)");
    connect(mExprEdit, &QPlainTextEdit::textChanged, this, [this] {
        const QString expr = mExprEdit->toPlainText().trimmed();
        mPreviewLabel->setText(
            "v.db.update … qcolumn=" + (expr.isEmpty() ? "<expression>" : expr.left(60)));
    });
    ev->addWidget(mExprEdit);
    vbox->addWidget(exprGroup);

    // ── WHERE clause ──────────────────────────────────────────────────────
    auto *whereGroup = new QGroupBox("WHERE clause (optional — leave empty for all rows)", this);
    auto *wform = new QFormLayout(whereGroup);
    mWhereEdit = new QLineEdit(whereGroup);
    mWhereEdit->setFont(QFont("Monospace", 10));
    mWhereEdit->setPlaceholderText("e.g.  cat > 10 AND name IS NOT NULL");
    wform->addRow("WHERE:", mWhereEdit);
    vbox->addWidget(whereGroup);

    // ── Preview ───────────────────────────────────────────────────────────
    mPreviewLabel = new QLabel("v.db.update … qcolumn=<expression>", this);
    mPreviewLabel->setWordWrap(true);
    mPreviewLabel->setStyleSheet("font-family:monospace; color:#555; font-size:9pt;");
    vbox->addWidget(mPreviewLabel);

    // ── Buttons ───────────────────────────────────────────────────────────
    auto *btnRow = new QHBoxLayout;
    auto *closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    mRunBtn = new QPushButton("▶ Run", this);
    mRunBtn->setDefault(true);
    connect(mRunBtn, &QPushButton::clicked, this, &GrassFieldCalcDialog::execute);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    btnRow->addWidget(mRunBtn);
    vbox->addLayout(btnRow);
}

void GrassFieldCalcDialog::insertText(const QString &text)
{
    mExprEdit->insertPlainText(text);
    mExprEdit->setFocus();
}

void GrassFieldCalcDialog::onTargetChanged()
{
    const bool isNew = (mTargetCombo->currentData().toString() == "new");
    mNewNameEdit->setVisible(isNew);
    mNewTypeCombo->setVisible(isNew);
}

bool GrassFieldCalcDialog::createColumn(const QString &name, const QString &type)
{
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
        return false;
    }
    return true;
}

void GrassFieldCalcDialog::execute()
{
    const QString expr = mExprEdit->toPlainText().trimmed();
    if (expr.isEmpty())
    {
        QMessageBox::warning(this, "Field Calculator", "Expression is empty.");
        return;
    }

    // Determine target column
    QString targetCol;
    if (mTargetCombo->currentData().toString() == "new")
    {
        targetCol = mNewNameEdit->text().trimmed();
        if (targetCol.isEmpty())
        {
            QMessageBox::warning(this, "Field Calculator",
                "Enter a name for the new column.");
            return;
        }
        if (!createColumn(targetCol, mNewTypeCombo->currentText()))
            return;
        // Add the new column to the list
        mColumns.append(targetCol);
    }
    else
    {
        targetCol = mTargetCombo->currentData().toString();
    }

    // Build v.db.update args
    QStringList args = {"map="     + mConn.mapName,
                         "layer="   + QString::number(mConn.layer),
                         "column="  + targetCol,
                         "qcolumn=" + expr};

    const QString where = mWhereEdit->text().trimmed();
    if (!where.isEmpty())
        args << "where=" + where;

    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("v.db.update", args);

    mRunBtn->setEnabled(false);
    mRunBtn->setText("Running…");

    const bool finished = p.waitForFinished(120000);
    mRunBtn->setEnabled(true);
    mRunBtn->setText("▶ Run");

    if (!finished || p.exitCode() != 0)
    {
        const QString err = QString::fromLocal8Bit(p.readAll()).trimmed();
        QMessageBox::warning(this, "Field Calculator failed",
                             err.isEmpty() ? "v.db.update returned non-zero exit code." : err);
        return;
    }

    QMessageBox::information(this, "Field Calculator",
        QString("Column '%1' updated successfully.").arg(targetCol));
    emit calculationDone();
}
