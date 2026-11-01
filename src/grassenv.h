#pragma once

/*!
    \file grassenv.h
    \brief GRASS GIS session environment singleton.
*/

#include <QObject>
#include <QProcessEnvironment>
#include <QString>

/*!
    \class GrassEnv
    \brief Manages the GRASS GIS session environment.

    Wraps \c G_set_gisrc_mode() / \c G_no_gisinit() and the four core GRASS
    environment variables: \c GISBASE, \c GISDBASE, \c LOCATION_NAME and
    \c MAPSET.

    \b Module mode (\c --qtui): the variables are already set by the calling
    GRASS session; init() just validates them.

    \b Full-GUI mode: the startup wizard calls openMapset() once to establish
    the session, and again whenever the user switches mapsets.

    Access the singleton via instance().

    \sa GrassApp
*/
class GrassEnv : public QObject
{
    Q_OBJECT

  public:
    /*! \brief Returns the global GrassEnv singleton, creating it if needed. */
    static GrassEnv *instance();

    /*!
        \brief Initialise from the process environment.

        Reads \c GISBASE from the process environment and calls
        \c G_no_gisinit() to set up the GRASS C library without a \c GISRC
        file.  Also verifies that the compile-time GRASS header version matches
        the runtime \c libgrass_gis version.

        \return \c true on success; \c false if \c GISBASE is missing or the
                version check fails.  Check lastError() for a human-readable
                message on failure.
    */
    bool init();

    /*! \brief \c true after a successful init() or openMapset() call. */
    bool    isValid()    const { return mValid; }

    /*! \brief Human-readable error message from the last failed operation. */
    QString lastError()  const { return mError; }

    /*! \brief Path to the GRASS installation (\c $GISBASE). */
    QString gisBase()     const { return mGisBase; }

    /*! \brief Path to the GRASS database root (parent of all locations). */
    QString gisDatabase() const { return mGisDatabase; }

    /*! \brief Name of the current location. */
    QString location()    const { return mLocation; }

    /*! \brief Name of the current mapset within the location. */
    QString mapset()      const { return mMapset; }

    /*!
        \brief Switch to a different mapset without restarting the application.

        Updates the in-memory GRASS environment variables and emits
        mapsetChanged() so connected widgets can refresh their state.

        \param gisDatabase Absolute path to the GRASS database root.
        \param location    Location name inside \a gisDatabase.
        \param mapset      Mapset name inside \a location.
        \return \c true on success; \c false if the path does not exist or is
                not a valid GRASS mapset.
    */
    bool openMapset( const QString &gisDatabase,
                     const QString &location,
                     const QString &mapset );

    /*!
        \brief Build a QProcessEnvironment for child GRASS processes.

        Writes a temporary \c GISRC file to \c /tmp/qtgrass_\<pid\>.gisrc from
        the current in-memory session and returns a \c QProcessEnvironment with
        \c GISRC pointing to it.  Child processes call \c G_gisinit() which
        reads from \c GISRC, not from our in-memory state.

        The file persists for the lifetime of the process and is safely reused
        across repeated subprocess calls.

        \return Environment suitable for passing to \c QProcess::setProcessEnvironment().
    */
    QProcessEnvironment buildSubprocessEnv() const;

  signals:
    /*!
        \brief Emitted after a successful openMapset() call.
        \param gisDatabase New database path.
        \param location    New location name.
        \param mapset      New mapset name.
    */
    void mapsetChanged( const QString &gisDatabase,
                        const QString &location,
                        const QString &mapset );

  private:
    explicit GrassEnv( QObject *parent = nullptr );

    bool    mValid      = false;
    QString mError;
    QString mGisBase;
    QString mGisDatabase;
    QString mLocation;
    QString mMapset;

    static GrassEnv *sInstance;
};
