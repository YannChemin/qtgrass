#pragma once

/*!
    \file grassworkspace.h
    \brief Parser for GRASS GIS workspace files (\c .gxw).
*/

#include <QMap>
#include <QString>
#include <QVector>

/*!
    \struct GwxLayer
    \brief One map layer entry from a \c .gxw display block.
*/
struct GwxLayer {
    QString type;        /*!< Layer type: \c "raster", \c "vector", \c "raster_3d", \c "rgb", … */
    QString name;        /*!< Map reference in \c name\@mapset form. */
    bool    checked = true;  /*!< Visibility state as saved in the workspace. */
    double  opacity = 1.0;   /*!< Layer opacity in [0, 1]. */
    QString displayCmd;  /*!< Display command: \c "d.rast", \c "d.vect", … */
    QMap<QString,QString> params; /*!< Parameter name → value map. */
};

/*!
    \struct GwxDecoration
    \brief One decoration entry from a \c <qtgrass-decorations> block.
*/
struct GwxDecoration {
    QString     prog;            /*!< Display module name: \c "d.legend", \c "d.grid", … */
    QStringList args;            /*!< Module arguments as stored in GrassDisplayCmd::args. */
    bool        enabled = false; /*!< Whether the decoration checkbox was checked. */
};

/*!
    \struct GwxDisplay
    \brief One map display window stored in a \c .gxw workspace.
*/
struct GwxDisplay {
    QString name;    /*!< Display window name. */
    double west  = 0; /*!< Saved viewport west boundary (0 if not recorded). */
    double south = 0; /*!< Saved viewport south boundary. */
    double east  = 0; /*!< Saved viewport east boundary. */
    double north = 0; /*!< Saved viewport north boundary. */
    QVector<GwxLayer>      layers;      /*!< Ordered layer stack (bottom to top). */
    QVector<GwxDecoration> decorations; /*!< Decoration overlay states. */
};

/*!
    \class GrassWorkspace
    \brief Parser for GRASS GIS workspace files (\c .gxw).

    Reads only the fields used by qtgrass — the wxPython AUI layout blob and
    monitor-specific settings are silently ignored.

    Usage:
    \code
    GrassWorkspace ws;
    if ( !ws.load( "/path/to/file.gxw" ) )
        qWarning() << ws.lastError();
    for ( const GwxDisplay &d : ws.displays() )
        for ( const GwxLayer &l : d.layers )
            canvas->addRasterLayer( l.name );
    \endcode

    \sa GrassApp::loadWorkspace()
*/
class GrassWorkspace
{
public:
    GrassWorkspace() = default;

    /*!
        \brief Parse a \c .gxw file.
        \param path Absolute path to the workspace file.
        \return \c true on success; \c false on XML parse error or missing file.
                Check lastError() for a human-readable message.
    */
    bool load( const QString &path );

    /*!
        \brief GRASS database path from the embedded \c \<session\> block.

        Empty when the workspace does not embed session information.
    */
    QString database() const { return mDatabase; }

    /*! \brief Location name from the \c \<session\> block (may be empty). */
    QString location() const { return mLocation; }

    /*! \brief Mapset name from the \c \<session\> block (may be empty). */
    QString mapset()   const { return mMapset;   }

    /*! \brief Ordered list of display windows parsed from the workspace. */
    const QVector<GwxDisplay> &displays() const { return mDisplays; }

    /*! \brief Absolute path of the file that was passed to load(). */
    QString filePath()  const { return mPath;  }

    /*! \brief Human-readable error message from the last failed load(). */
    QString lastError() const { return mError; }

private:
    QString mPath, mError;
    QString mDatabase, mLocation, mMapset;
    QVector<GwxDisplay> mDisplays;
};
