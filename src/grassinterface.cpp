#include "grassinterface.h"
#include "grassmessagebar.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcIface, "qtgrass.iface")

GrassInterface::GrassInterface( QWidget *mainWindow,
                                 QWidget *mapCanvas,
                                 GrassMessageBar *messageBar,
                                 QObject *parent )
    : QObject( parent )
    , mMainWindow( mainWindow )
    , mMapCanvas( mapCanvas )
    , mMessageBar( messageBar )
{}

bool GrassInterface::addVectorLayer( const QString &path,
                                      const QString &name,
                                      const QString &provider )
{
    qCDebug(lcIface) << "addVectorLayer:" << name << provider << path;
    emit vectorLayerAdded( path, name, provider );
    // Part 4 will register the layer with QgsLayerTree / our map canvas
    return true;
}

bool GrassInterface::addRasterLayer( const QString &url,
                                      const QString &name,
                                      const QString &provider )
{
    qCDebug(lcIface) << "addRasterLayer:" << name << provider << url;
    emit rasterLayerAdded( url, name, provider );
    // Part 4 will register the layer with QgsLayerTree / our map canvas
    return true;
}
