#pragma once

/*!
    \file grassapp.h
    \brief Main application window for the qtgrass full-GUI mode.
*/

#include <QMainWindow>
#include <QString>
#include <functional>

class QLabel;
class QMenu;
class QSplitter;
class QTimer;
class GrassEnv;
class GrassMessageBar;
class GrassInterface;
class GrassToolboxTree;
class GrassMapCanvas;
class GrassLayerPanel;
class GrassDataCatalog;
class GrassTileBackground;
class GrassTileCache;
class GrassTileFetcher;
struct TileProvider;

/*!
    \class GrassApp
    \brief Main window of the qtgrass application.

    Hosts the full GRASS GUI: module toolbox on the left, a layer panel and
    map canvas in the centre, and a notification bar at the top.  Layout:

    \code
    ┌──────────────────────────────────────────────────┐
    │  GrassMessageBar (hidden when empty)             │
    ├────────────────┬─────────────────────────────────┤
    │  GrassToolbox  │  GrassLayerPanel │ GrassMapCanvas│
    │  (module tree) │  (layer list)    │ (map display) │
    └────────────────┴─────────────────────────────────┘
    │  Status bar: coordinates / location              │
    └──────────────────────────────────────────────────┘
    \endcode

    \sa GrassMapCanvas, GrassLayerPanel, GrassMessageBar
*/
class GrassApp : public QMainWindow
{
    Q_OBJECT

  public:
    /*!
        \brief Constructs the main window and initialises all sub-widgets.
        \param env   Active GRASS session — must be valid before calling this.
        \param parent Optional parent widget.
    */
    explicit GrassApp( GrassEnv *env, QWidget *parent = nullptr );
    ~GrassApp() override = default;

    /*!
        \brief Load a \c .gxw workspace file.

        Sets the mapset (if the workspace embeds a \c \<session\> block different
        from the current one), adds all layers to the canvas, and zooms to the
        saved geographic extent.

        \param path Absolute path to the \c .gxw file.
    */
    void loadWorkspace( const QString &path );

    /*!
        \brief Save the current canvas layers to a \c .gxw workspace file.
        \param path   Absolute path for the output file (overwritten if it exists).
        \param silent When true, no message bar notification is shown (used for auto-save).
    */
    void saveWorkspace( const QString &path, bool silent = false );

    /*! \brief Returns the plugin interface used by ported QGIS GRASS code. */
    GrassInterface  *iface()      const { return mIface; }

    /*! \brief Returns the notification bar at the top of the window. */
    GrassMessageBar *messageBar() const { return mMessageBar; }

    /*! \brief Returns the central map display widget. */
    GrassMapCanvas  *canvas()     const { return mCanvas; }

    /*! \brief Returns the layer-management panel on the left of the canvas. */
    GrassLayerPanel *layerPanel() const { return mLayerPanel; }

  protected:
    void closeEvent( QCloseEvent *event ) override;

  private:
    void buildMenuBar();
    void buildToolBar();
    void buildCentralArea();
    void buildStatusBar();

    void applyTileSettings( bool enabled, const TileProvider &provider );
    void applyTileSettingsFromQSettings();
    void applyTheme( const QString &mode );

    // Auto-save workspace
    QString autoSavePath() const;
    void    performAutoSave();
    void    updateAutoSaveTimer();

    // Cap all menus to 70 % of screen height so Qt shows scroll arrows for
    // long module lists.
    void applyMenuScrollLimits();

    // Planetary menu — cleared and rebuilt on every mapsetChanged signal
    void rebuildPlanetaryMenu();

    GrassEnv        *mEnv         = nullptr;
    GrassMessageBar *mMessageBar  = nullptr;
    GrassInterface  *mIface       = nullptr;

    GrassToolboxTree  *mToolbox   = nullptr;
    GrassDataCatalog  *mCatalog   = nullptr;
    GrassMapCanvas   *mCanvas     = nullptr;
    GrassLayerPanel  *mLayerPanel = nullptr;

    QLabel *mCoordLabel    = nullptr;
    QLabel *mLocationLabel = nullptr;

    GrassTileCache      *mTileCache      = nullptr;
    GrassTileFetcher    *mTileFetcher    = nullptr;
    GrassTileBackground *mTileBackground = nullptr;

    QTimer  *mAutoSaveTimer     = nullptr;
    QAction *mAutoSaveEnableAct = nullptr;

    QMenu  *mPlanetMenu    = nullptr;
    std::function<void(const QString &)> mOpenModuleFn;  // cached for menu rebuild
};
