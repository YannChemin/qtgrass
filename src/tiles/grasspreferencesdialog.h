#pragma once

/*!
    \file grasspreferencesdialog.h
    \brief Preferences dialog for the satellite / basemap tile background.
*/

#include <QDialog>
#include <QList>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;

/*!
    \enum PlanetaryBody
    \brief Body the GRASS location's CRS refers to.

    Detected from the location's PROJ_INFO file (semi-major axis + name match).
    Each TileProvider is tagged with the body it covers; the preferences dialog
    only offers providers that match the detected body.
*/
enum class PlanetaryBody { Earth, Moon, Mars, Other, Unknown };

/*!
    \struct TileProvider
    \brief Descriptor for one XYZ tile provider.
*/
struct TileProvider {
    QString       id;            /*!< Unique key stored in QSettings. */
    QString       name;          /*!< Human-readable display name. */
    QString       urlTemplate;   /*!< XYZ URL template with \c {x} \c {y} \c {z} placeholders. */
    QString       attribution;   /*!< Attribution text required by the provider's TOS. */
    PlanetaryBody body = PlanetaryBody::Earth; /*!< Planetary body this provider covers. */
};

/*!
    \class GrassPreferencesDialog
    \brief Tile background configuration dialog.

    On open, detects the planetary body from the active GRASS location's
    PROJ_INFO file and restricts the provider list to those that cover that
    body (Earth → Google/Bing/OSM, Moon → OPM Moon basemaps, Mars → OPM Mars
    basemaps, Other → nothing offered).
*/
class GrassPreferencesDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit GrassPreferencesDialog( QWidget *parent = nullptr );

    /*! \brief Read the persisted enabled/provider-index settings. */
    static void loadSettings( bool &enabled, int &providerIndex );

    /*! \brief Full provider catalogue (all bodies). */
    static const QList<TileProvider> &providers();

    /*!
        \brief Detect the planetary body from the active GRASS location.

        Reads \c $GISDBASE/$LOCATION/PERMANENT/PROJ_INFO and looks for body
        name keywords (\c moon, \c mars) plus the ellipsoid semi-major axis
        \c a.  Defaults to Earth when no GRASS session is active.
    */
    static PlanetaryBody detectBody();

    /*! \brief Human-readable name for a PlanetaryBody value. */
    static QString bodyName( PlanetaryBody b );

  signals:
    /*! \brief Emitted on Apply with the new state. */
    void tileSettingsChanged( bool enabled, const TileProvider &provider );

  private slots:
    void apply();
    void rebuildProviderCombo();

  private:
    QCheckBox *mEnableCheck   = nullptr;
    QComboBox *mProviderCombo = nullptr;
    QLabel    *mBodyLabel     = nullptr;

    PlanetaryBody mBody = PlanetaryBody::Earth;
};
