#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

class QGroupBox;
class QListWidget;
class QLabel;
class QLineEdit;
class QPushButton;

// Simple 3-column dialog for opening an existing GRASS mapset:
//   [GISDBASE path]  [Location list]  [Mapset list]
//
// Returns GISDBASE, location, mapset on accept.
class GrassMapsetBrowser : public QDialog
{
    Q_OBJECT

  public:
    explicit GrassMapsetBrowser( QWidget *parent = nullptr );

    QString gisDatabase()    const { return mGisdbase; }
    QString location()       const { return mLocation; }
    QString mapset()         const { return mMapset; }
    // Non-empty when the user chose to open a workspace file instead of a mapset
    QString selectedWorkspace() const { return mSelectedWorkspace; }

    // Pre-fill with a known gisdbase/location/mapset
    void setInitial( const QString &gisdbase,
                     const QString &location,
                     const QString &mapset );

  private slots:
    void browseGisdbase();
    void validate();

  private:
    void populateLocations( const QString &gisdbase );
    void populateMapsets(   const QString &location  );
    void updateWorkspaceList( const QString &locationDir );

    QString mGisdbase;
    QString mLocation;
    QString mMapset;
    QString mSelectedWorkspace;  // path to .gxw if user picks one

    QLineEdit   *mGisdbEdit    = nullptr;
    QListWidget *mLocList      = nullptr;
    QListWidget *mMslist       = nullptr;
    QGroupBox   *mGxwBox       = nullptr;
    QListWidget *mGxwList      = nullptr;
    QPushButton *mOpenGxwBtn   = nullptr;
    QPushButton *mOpenBtn      = nullptr;
    QLabel      *mStatus       = nullptr;
};
