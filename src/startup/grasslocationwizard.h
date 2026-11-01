#pragma once

#include <QWizard>
#include <QString>

class GrassEnv;
class GrassCrsModel;

// ============================================================
// Page IDs
// ============================================================
enum WizardPageId {
    PageGisdb        = 0,
    PageCrsChoice    = 1,
    PageEpsg         = 2,
    PageIau          = 3,
    PageLocationName = 4,
    PageSummary      = 5,
};

// ============================================================
// GrassLocationWizard
// ============================================================
// QWizard that guides the user through:
//   1. Choosing / creating a GISDBASE directory
//   2. Choosing a CRS (Earth EPSG or Planetary IAU_2015)
//   3. Naming the new location (+ mapset)
//   4. Review + create (runs g.proj -c)
//
// On accept, GrassEnv::openMapset() has been called with the
// newly created GISDBASE / location / PERMANENT.
class GrassLocationWizard : public QWizard
{
    Q_OBJECT

  public:
    explicit GrassLocationWizard( GrassEnv *env, QWidget *parent = nullptr );

    // Called by the summary page to actually create the location
    bool createLocation( QString *errorOut = nullptr );

    // Fields accessible across pages
    QString selectedSrid()         const;
    QString selectedSridLabel()    const;
    QString gisdbase()             const;
    QString locationName()         const;
    QString mapsetName()           const;

  private:
    GrassEnv *mEnv = nullptr;
};
