#pragma once

/*!
    \file grassinterface.h
    \brief Thin Qt-only replacement for QgisInterface used by ported GRASS plugin code.
*/

#include <QObject>
#include <QString>

class QWidget;
class GrassMessageBar;

/*!
    \class GrassInterface
    \brief Compatibility interface for ported QGIS GRASS plugin objects.

    Replaces \c QgisInterface* from the QGIS plugin architecture so that ported
    code (\c QgsGrassModule, \c QgsGrassTools, …) can be compiled without any
    QGIS runtime dependency.

    Only the four methods actually called by the ported GRASS plugin code have
    real implementations.  All others would be no-op stubs and are not
    exposed here.

    \sa GrassApp::iface()
*/
class GrassInterface : public QObject
{
    Q_OBJECT

  public:
    /*!
        \brief Constructs the interface object with references to the main widgets.
        \param mainWindow Main application window.
        \param mapCanvas  Map display widget.
        \param messageBar Notification bar.
        \param parent     Parent QObject.
    */
    explicit GrassInterface( QWidget *mainWindow,
                             QWidget *mapCanvas,
                             GrassMessageBar *messageBar,
                             QObject *parent = nullptr );

    /*! \brief Returns the main application window. */
    QWidget         *mainWindow()  const { return mMainWindow; }

    /*! \brief Returns the map canvas widget. */
    QWidget         *mapCanvas()   const { return mMapCanvas; }

    /*! \brief Returns the notification bar. */
    GrassMessageBar *messageBar()  const { return mMessageBar; }

    /*!
        \brief Register a vector output layer produced by a module run.

        Emits vectorLayerAdded() so GrassApp can add the layer to the canvas.

        \param path     Path or data source URI.
        \param name     Display name.
        \param provider Provider key (e.g. \c "ogr").
        \return Always \c true (registration is asynchronous via signal).
    */
    bool addVectorLayer( const QString &path,
                         const QString &name,
                         const QString &provider );

    /*!
        \brief Register a raster output layer produced by a module run.

        Emits rasterLayerAdded() so GrassApp can add the layer to the canvas.

        \param url      Data source URL or GRASS map reference.
        \param name     Display name.
        \param provider Provider key (e.g. \c "grassraster").
        \return Always \c true.
    */
    bool addRasterLayer( const QString &url,
                         const QString &name,
                         const QString &provider );

  signals:
    /*! \brief Emitted when a vector output layer is registered. */
    void vectorLayerAdded( const QString &path, const QString &name, const QString &provider );

    /*! \brief Emitted when a raster output layer is registered. */
    void rasterLayerAdded( const QString &url,  const QString &name, const QString &provider );

  private:
    QWidget         *mMainWindow  = nullptr;
    QWidget         *mMapCanvas   = nullptr;
    GrassMessageBar *mMessageBar  = nullptr;
};
