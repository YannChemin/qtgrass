#pragma once

#include "grassdbmanager.h"
#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

// ── Field Calculator ──────────────────────────────────────────────────────────
// Computes or updates a column's value using an SQL expression via v.db.update.
// Supports: existing column update, new column creation.
class GrassFieldCalcDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GrassFieldCalcDialog(const GrassVectorConn &conn,
                                   const QStringList &columns,
                                   QWidget *parent = nullptr);

signals:
    void calculationDone();

private:
    void buildUi();
    void insertText(const QString &text);
    void onTargetChanged();
    void execute();
    bool createColumn(const QString &name, const QString &type);

    GrassVectorConn  mConn;
    QStringList      mColumns;

    // Widgets
    QComboBox      *mTargetCombo   = nullptr;
    QLineEdit      *mNewNameEdit   = nullptr;
    QComboBox      *mNewTypeCombo  = nullptr;
    QPlainTextEdit *mExprEdit      = nullptr;
    QLineEdit      *mWhereEdit     = nullptr;
    QLabel         *mPreviewLabel  = nullptr;
    QPushButton    *mRunBtn        = nullptr;
};
