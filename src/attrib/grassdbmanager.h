#pragma once

#include <QDialog>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVBoxLayout>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class GrassInterface;

// ── Connection info for one vector layer's attribute table ────────────────────
struct GrassVectorConn {
    int     layer    = 1;
    QString mapName;
    QString table;
    QString database;
    QString key      = "cat";
    QString driver;
    bool    valid    = false;
};

// ── SQL syntax highlighter ────────────────────────────────────────────────────
class GrassSqlHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit GrassSqlHighlighter(QTextDocument *doc);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule { QRegularExpression re; QTextCharFormat fmt; };
    QVector<Rule> mRules;
};

// ── Comprehensive database manager dialog ─────────────────────────────────────
class GrassDbManagerDialog : public QDialog
{
    Q_OBJECT
public:
    // Opens the map selector in the specified mapset context.
    // Pass a pre-selected map name to load immediately.
    explicit GrassDbManagerDialog(GrassInterface *iface,
                                   const QString &mapName = {},
                                   const QString &mapset  = {},
                                   int layer = 1,
                                   QWidget *parent = nullptr);

private:
    // ── UI builders ───────────────────────────────────────────────────────
    void buildToolbar(QVBoxLayout *vbox);
    void buildMapSelector(QVBoxLayout *vbox);
    QWidget *buildDataTab();
    QWidget *buildSqlTab();
    QWidget *buildSchemaTab();

    // ── Operations ────────────────────────────────────────────────────────
    void loadMap();
    GrassVectorConn queryConnection(const QString &map, int layer);
    void refreshData();
    void refreshSchema();
    QStringList runVDbSelect(bool withHeaders = true);

    void onCellChanged(int row, int col);
    void applyFilter(const QString &text);

    void executeSql();
    void populateResults(const QStringList &lines);

    void addColumn();
    void dropColumn();
    void exportCsv();
    void importCsv();
    void openFieldCalc();

    // ── Widgets ───────────────────────────────────────────────────────────
    QComboBox      *mMapCombo    = nullptr;
    QSpinBox       *mLayerSpin   = nullptr;

    // Data tab
    QTableWidget   *mDataTable   = nullptr;
    QLineEdit      *mFilterEdit  = nullptr;
    QLabel         *mDataStatus  = nullptr;

    // SQL tab
    QPlainTextEdit *mSqlEdit     = nullptr;
    QComboBox      *mHistCombo   = nullptr;
    QTableWidget   *mResultTable = nullptr;
    QLabel         *mSqlStatus   = nullptr;

    // Schema tab
    QTableWidget   *mSchemaTable = nullptr;
    QLabel         *mConnLabel   = nullptr;

    QTabWidget     *mTabs        = nullptr;

    // ── State ─────────────────────────────────────────────────────────────
    GrassInterface  *mIface      = nullptr;
    GrassVectorConn  mConn;
    QStringList      mHeaders;      // column names in current data view
    int              mKeyCol = 0;   // index of key column in mHeaders
    bool             mEditGuard = false;   // suppress cell-change during load
    QStringList      mSqlHistory;
};
