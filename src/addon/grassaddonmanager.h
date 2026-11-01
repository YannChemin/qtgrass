#pragma once

#include <QDialog>

class QTabWidget;
class QTableWidget;
class QLineEdit;
class QLabel;
class QPlainTextEdit;
class QPushButton;

// GUI for g.extension: search, install, and remove GRASS addons.
//
// Tabs:
//   • Installed — from `g.extension -a`
//   • Available — from `g.extension -c -l` (name + description), searchable
//
// Install / Remove run g.extension via QProcess with live output log.
class GrassAddonManager : public QDialog
{
    Q_OBJECT

  public:
    explicit GrassAddonManager( QWidget *parent = nullptr );

  private slots:
    void loadInstalled();
    void loadAvailable();
    void installSelected();
    void removeSelected();

  private:
    // Runs g.extension with given args; streams output to mLog.
    // Returns exit code.
    int runGExtension( const QStringList &args );

    QTabWidget    *mTabs       = nullptr;

    // Installed tab
    QTableWidget  *mInstTable  = nullptr;
    QPushButton   *mRemoveBtn  = nullptr;

    // Available tab
    QLineEdit     *mSearch     = nullptr;
    QTableWidget  *mAvailTable = nullptr;
    QPushButton   *mInstallBtn = nullptr;
    QPushButton   *mRefreshBtn = nullptr;

    // Shared log
    QPlainTextEdit *mLog       = nullptr;
    QLabel         *mStatus    = nullptr;
};
