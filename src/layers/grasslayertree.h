#pragma once

/*!
    \file grasslayertree.h
    \brief Layer tree widget with custom delegate, model, and context menus.
*/

#include "../canvas/grassmaplayer.h"

#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTreeView>

class GrassMapCanvas;
class GrassDecorationPanel;

// ── LayerRole — data roles stored on every QStandardItem ──────────────────

/*!
    \namespace LayerRole
    \brief Custom Qt::ItemDataRole values used by GrassLayerTreeModel items.
*/
namespace LayerRole {
    enum {
        Name    = Qt::UserRole,       /*!< \c QString — GRASS map name, e.g. \c "dem". */
        Mapset  = Qt::UserRole + 1,   /*!< \c QString — owning mapset (empty = current). */
        Type    = Qt::UserRole + 2,   /*!< \c QString — \c "raster", \c "vector", or \c "group". */
        Opacity = Qt::UserRole + 3,   /*!< \c double  — layer opacity in [0, 1]. */
        Visible = Qt::UserRole + 4,   /*!< \c bool    — whether the layer is shown. */
    };
}

// ── GrassLayerDelegate ────────────────────────────────────────────────────────

/*!
    \class GrassLayerDelegate
    \brief Custom item delegate that draws layer rows with an eye icon and opacity label.

    Row layout: \c [eye] [type icon] [name …] [NNN%]

    Handles click events on the eye column to toggle layer visibility without
    entering edit mode.

    \sa GrassLayerTree
*/
class GrassLayerDelegate : public QStyledItemDelegate
{
    Q_OBJECT
  public:
    /*! \brief Constructs the delegate. */
    explicit GrassLayerDelegate( QObject *parent = nullptr );

    void paint( QPainter *p,
                const QStyleOptionViewItem &opt,
                const QModelIndex &idx ) const override;

    QSize sizeHint( const QStyleOptionViewItem &opt,
                    const QModelIndex &idx ) const override;

    bool editorEvent( QEvent *event,
                      QAbstractItemModel *model,
                      const QStyleOptionViewItem &opt,
                      const QModelIndex &idx ) override;

  signals:
    /*!
        \brief Emitted when the user clicks the eye icon for \a idx.
        \param idx Model index of the toggled item.
    */
    void visibilityToggled( const QModelIndex &idx );

  private:
    QRect eyeRect(     const QRect &rowRect ) const;
    QRect iconRect(    const QRect &rowRect ) const;
    QRect nameRect(    const QRect &rowRect ) const;
    QRect opacityRect( const QRect &rowRect ) const;

    static constexpr int kEyeW  = 20;  /*!< Eye icon column width in pixels. */
    static constexpr int kIconW = 20;  /*!< Type icon column width in pixels. */
    static constexpr int kOpacW = 38;  /*!< Opacity label width in pixels (\c " NNN%"). */
    static constexpr int kPad   = 3;   /*!< Padding between sub-rects in pixels. */
};

// ── GrassLayerTreeModel ───────────────────────────────────────────────────────

/*!
    \class GrassLayerTreeModel
    \brief QStandardItemModel subclass with helpers for adding layers and groups.

    Items carry LayerRole data roles for name, mapset, type, opacity, and
    visibility.  Drag-and-drop reordering is enabled.

    \sa GrassLayerTree, LayerRole
*/
class GrassLayerTreeModel : public QStandardItemModel
{
    Q_OBJECT
  public:
    /*! \brief Constructs an empty model. */
    explicit GrassLayerTreeModel( QObject *parent = nullptr );

    /*!
        \brief Add a layer item to the model.
        \param name        GRASS map name.
        \param mapset      Owning mapset (empty = current).
        \param type        Layer type string: \c "raster" or \c "vector".
        \param parentGroup Parent group item; \c nullptr inserts at the top level.
        \return The newly created item.
    */
    QStandardItem *addLayer( const QString &name,
                             const QString &mapset,
                             const QString &type,
                             QStandardItem *parentGroup = nullptr );

    /*!
        \brief Add a named layer group.
        \param groupName Display name of the group.
        \return The new group item.
    */
    QStandardItem *addGroup( const QString &groupName );

    /*!
        \brief Remove a layer group and all its children.
        \param group Group item to remove.
    */
    void removeGroup( QStandardItem *group );

    /*!
        \brief Returns all layer items in the model (flat, any depth).
    */
    QList<QStandardItem *> allLayers() const;

    Qt::DropActions supportedDropActions() const override;
    Qt::ItemFlags   flags( const QModelIndex &idx ) const override;

  private:
    void collectLayers( QStandardItem *parent,
                        QList<QStandardItem *> &out ) const;
};

// ── GrassLayerTree ────────────────────────────────────────────────────────────

/*!
    \class GrassLayerTree
    \brief QTreeView for the layer stack with drag-drop, visibility, and context menus.

    Integrates GrassLayerTreeModel and GrassLayerDelegate.  Context menus
    provide per-layer actions: zoom, properties, colour table, attribute table,
    build overviews, and remove.

    Visibility toggles update the canvas via GrassMapCanvas::setLayerVisible();
    opacity changes update the layer directly and schedule a re-render.

    \sa GrassLayerTreeModel, GrassLayerDelegate, GrassLayerPanel
*/
class GrassLayerTree : public QTreeView
{
    Q_OBJECT
  public:
    /*!
        \brief Constructs the tree, wired to the given canvas.
        \param canvas Map canvas to update on layer changes.
        \param parent Parent widget.
    */
    explicit GrassLayerTree( GrassMapCanvas *canvas,
                              QWidget *parent = nullptr );

    /*!
        \brief Add a raster layer item to the top of the stack.
        \param name   GRASS raster map name.
        \param mapset Owning mapset (empty = current).
        \return The new model item.
    */
    QStandardItem *addRasterLayer( const QString &name,
                                    const QString &mapset = {} );

    /*!
        \brief Add a vector layer item to the top of the stack.
        \param name   GRASS vector map name.
        \param mapset Owning mapset (empty = current).
        \return The new model item.
    */
    QStandardItem *addVectorLayer( const QString &name,
                                    const QString &mapset = {} );

    /*! \brief Remove all items from the tree. */
    void clear();

    /*!
        \brief Add a composite layer item to the tree.

        The layer must already be inserted into the canvas (GrassLayerPanel::addCompositeLayer()
        does both in one call).  The type tag is derived from the layer name prefix.

        \param layer The composite layer that was added to the canvas.
        \return The newly created model item.
    */
    QStandardItem *addCompositeLayer( GrassMapLayer *layer );

    /*!
        \brief Returns the GRASS map reference of the topmost visible raster.
        \return \c "name\@mapset" string, or an empty string when no raster is visible.
    */
    QString topRasterRef() const;

    /*! \brief Returns the underlying tree model. */
    GrassLayerTreeModel *layerModel() const { return mModel; }

    /*!
        \brief Returns the GRASS map name of the currently selected item.
        \return Layer name, or empty string when nothing is selected.
    */
    QString currentLayerName() const;

  signals:
    void layerVisibilityChanged( const QString &name, bool visible );    /*!< Emitted when a layer is shown or hidden. */
    void layerOpacityChanged(    const QString &name, double opacity );  /*!< Emitted when a layer's opacity changes. */
    void topRasterChanged( const QString &rasterRef );                   /*!< Emitted when the topmost visible raster changes. */
    void currentLayerChanged( const QString &name );                     /*!< Emitted when the selected layer changes. */
    void requestZoomToLayer( const QString &name, const QString &mapset,
                              const QString &type );                     /*!< Emitted when "Zoom to Layer" is chosen. */
    void requestAttributeTable( const QString &name, const QString &mapset ); /*!< Emitted when "Attribute Table" is chosen. */
    void requestLayerProperties( QStandardItem *item );                  /*!< Emitted when "Properties" is chosen. */
    void requestColorTable( const QString &name, const QString &mapset );     /*!< Emitted when "Color Table" is chosen. */

  protected:
    void mouseDoubleClickEvent( QMouseEvent *e ) override;

  private slots:
    void onVisibilityToggled( const QModelIndex &idx );
    void showContextMenu( const QPoint &pos );
    void onItemDataChanged( QStandardItem *item );
    void syncCanvasOrder();

  private:
    void buildContextMenu( QStandardItem *item, QMenu &menu );
    void buildRasterOverviews( const QString &name, const QString &mapset );

    GrassMapCanvas      *mCanvas   = nullptr;
    GrassLayerTreeModel *mModel    = nullptr;
    GrassLayerDelegate  *mDelegate = nullptr;
    double               mLastE    = 0.0;
    double               mLastN    = 0.0;
};
