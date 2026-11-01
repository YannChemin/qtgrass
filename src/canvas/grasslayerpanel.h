#pragma once

/*!
    \file grasslayerpanel.h
    \brief Container widget combining the layer tree and decoration panel.
*/

#include <QWidget>

class GrassMapCanvas;
class GrassDecorationPanel;
class GrassLayerTree;
class GrassMapLayer;
class QStandardItem;

/*!
    \class GrassLayerPanel
    \brief Outer container for the layer management panel.

    Hosts a GrassLayerTree (top section) and a GrassDecorationPanel (bottom
    section).  All canvas-state changes (add/remove/reorder layers, visibility,
    opacity) are mediated through this class so callers do not need to interact
    with the tree or canvas directly.

    \sa GrassLayerTree, GrassDecorationPanel, GrassMapCanvas
*/
class GrassLayerPanel : public QWidget
{
    Q_OBJECT

  public:
    /*!
        \brief Constructs the layer panel and wires it to the given canvas.
        \param canvas The map canvas that this panel controls.
        \param parent Optional parent widget.
    */
    explicit GrassLayerPanel( GrassMapCanvas *canvas,
                               QWidget *parent = nullptr );

    /*!
        \brief Add a raster layer to the top of the layer stack.
        \param name   GRASS raster map name.
        \param mapset Mapset that owns the map (empty = current mapset).
    */
    void addRasterLayer( const QString &name, const QString &mapset = {} );

    /*!
        \brief Add a vector layer to the top of the layer stack.
        \param name   GRASS vector map name.
        \param mapset Mapset that owns the map (empty = current mapset).
    */
    void addVectorLayer( const QString &name, const QString &mapset = {} );

    /*!
        \brief Add a pre-built composite layer (RGB, IHS, Shade, etc.) to the canvas and tree.
        \param layer Heap-allocated layer; the canvas takes ownership.
    */
    void addCompositeLayer( GrassMapLayer *layer );

    /*!
        \brief Remove all layers from the tree and the canvas.

        Waits for any running async render to finish before deleting layer
        objects to avoid use-after-free in the render thread.
    */
    void clear();

    /*! \brief Returns the layer tree widget. */
    GrassLayerTree      *tree()      const { return mTree; }

    /*! \brief Returns the decoration panel at the bottom of this widget. */
    GrassDecorationPanel *decoPanel() const { return mDecoPanel; }

  private:
    GrassMapCanvas       *mCanvas    = nullptr;
    GrassLayerTree       *mTree      = nullptr;
    GrassDecorationPanel *mDecoPanel = nullptr;
};
