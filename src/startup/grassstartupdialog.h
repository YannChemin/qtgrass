#pragma once

#include <QDialog>
#include <QString>

class GrassEnv;
class QTableWidget;

// Shown on startup when no GRASS session is active, or from File → Change Project.
class GrassStartupDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit GrassStartupDialog( GrassEnv *env, QWidget *parent = nullptr );

    QString selectedWorkspace() const { return mSelectedWorkspace; }

  private slots:
    void openSelectedWorkspace();
    void openExisting();
    void createNew();

  private:
    GrassEnv     *mEnv            = nullptr;
    QTableWidget *mWorkspaceTable = nullptr;
    QString       mSelectedWorkspace;
};
