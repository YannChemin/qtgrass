#pragma once

/*!
    \file grassdatacat.h
    \brief Data catalog panel for browsing GRASS maps and regions.
*/

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class GrassMapCanvas;
class GrassLayerPanel;

/*!
    \class GrassDataCatalog
    \brief Data browser listing rasters, vectors, and regions in the current session.

    Shows all maps available in the current mapset and all accessible mapsets,
    grouped by type.  A search box filters the tree by name.

    Double-clicking a raster or vector entry adds it as a new layer on the canvas.

    Call refresh() after opening a new mapset so the tree reflects the updated
    map inventory.

    \sa GrassLayerPanel, GrassApp
*/
class GrassDataCatalog : public QWidget
{
    Q_OBJECT

  public:
    /*!
        \brief Constructs the catalog panel.
        \param canvas Canvas that receives new layers on double-click.
        \param panel  Layer panel that manages the layer stack.
        \param parent Parent widget.
    */
    explicit GrassDataCatalog( GrassMapCanvas  *canvas,
                                GrassLayerPanel *panel,
                                QWidget *parent = nullptr );

  public slots:
    /*!
        \brief Reload the map inventory from \c g.list.

        Call after openMapset() to reflect the new session.
    */
    void refresh();

  private slots:
    void onItemDoubleClicked( QTreeWidgetItem *item, int col );
    void onFilterChanged( const QString &text );

  private:
    void populate( const QString &type, QTreeWidgetItem *parent );
    QStringList listMaps( const QString &type ) const;

    GrassMapCanvas  *mCanvas = nullptr;
    GrassLayerPanel *mPanel  = nullptr;
    QTreeWidget     *mTree   = nullptr;
    QLineEdit       *mFilter = nullptr;
};
