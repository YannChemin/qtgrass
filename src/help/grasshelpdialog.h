#pragma once

#include <QDialog>

class QTextBrowser;
class QLineEdit;
class QLabel;

// Displays the GRASS HTML manual page for a module in a QTextBrowser.
// Falls back gracefully when the HTML file is not found.
class GrassHelpDialog : public QDialog
{
    Q_OBJECT

  public:
    // Open help for a specific module (e.g. "r.slope.aspect")
    explicit GrassHelpDialog( const QString &moduleName,
                               QWidget *parent = nullptr );

    // Set module after construction
    void showHelp( const QString &moduleName );

  private slots:
    void onSearch( const QString &text );

  private:
    QString htmlPath( const QString &moduleName ) const;

    QTextBrowser *mBrowser  = nullptr;
    QLineEdit    *mSearch   = nullptr;
    QLabel       *mTitle    = nullptr;
    QString       mCurrentModule;
};
