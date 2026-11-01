#include "grassapp.h"
#include "grassenv.h"
#include "grassinterface.h"
#include "grassmessagebar.h"
#include "toolbox/grasstoolboxtree.h"
#include "module/grassmoduledialog.h"
#include "canvas/grassaddlayerdialog.h"
#include "canvas/grassmapcanvas.h"
#include "canvas/grasslayerpanel.h"
#include "canvas/grassdecorationpanel.h"
#include "layers/grasslayertree.h"
#include "canvas/grassrastermaplayer.h"
#include "vdigit/grassdigittoolbar.h"
#include "export/grassmapexport.h"
#include "colors/grasscolortabledialog.h"
#include "addon/grassaddonmanager.h"
#include "catalog/grassdatacat.h"
#include "region/grassregiondialog.h"
#include "help/grasshelpdialog.h"
#include "script/grassscriptrunner.h"
#include "startup/grassstartupdialog.h"
#include "workspace/grassworkspace.h"
#include "modeler/grassmodelerdialog.h"
#include "temporal/grassanimationdialog.h"
#include "layout/grasslayoutdialog.h"
#include "georect/grassgeorectdialog.h"
#include "imagery/grassiclassdialog.h"
#include "tools/grassmapswipe.h"
#include "tools/grasswmsbrowser.h"
#include "tools/grassnetanalysis.h"
#include "tools/grasshistorybrowser.h"
#include "tools/grasstplot.h"
#include "nviz/grassnvizdialog.h"
#include "mapcalc/grassmapcalcdialog.h"
#include "attrib/grassattribtable.h"
#include "attrib/grassdbmanager.h"
#include "profile/grassprofiledialog.h"
#include "profile/grasshistogramdialog.h"
#include "planetary/grassplanetarymenu.h"
#include "canvas/grassvectormaplayer.h"
#include "tiles/grasstilebackground.h"
#include "tiles/grasstilecache.h"
#include "tiles/grasstilefetcher.h"
#include "tiles/grasspreferencesdialog.h"
#include "grassicons.h"
#include "grassmenubuilder.h"
#include "grassstyle.h"
#include "grassaddonsmenu.h"

#include <QGuiApplication>
#include <QScreen>
#include <QInputDialog>
#include <QPalette>
#include <QRegularExpression>

extern "C" {
#include <grass/gis.h>   // G_projection(), PROJECTION_LL
}

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QTimer>
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcGrassApp, "qtgrass.app")

GrassApp::GrassApp( GrassEnv *env, QWidget *parent )
    : QMainWindow( parent )
    , mEnv( env )
{
    setWindowTitle( "GRASS GIS — qtgrass" );
    resize( 1280, 800 );

    // Apply the saved theme BEFORE building the UI so all widgets inherit it.
    applyTheme( QSettings().value( "ui/theme", "system" ).toString() );

    buildCentralArea();  // creates mCanvas, mLayerPanel, mToolbox, mMessageBar
    buildMenuBar();
    buildToolBar();
    buildStatusBar();

    // If the session was pre-initialised before this constructor ran (e.g.
    // main() parsed a .gxw and called openMapset() early), mapsetChanged was
    // never connected, so refresh the region cache explicitly now.
    if ( mEnv->isValid() && !mEnv->location().isEmpty() )
        mCanvas->refreshRegionCache();

    // Tile background — create shared cache + fetcher (always; disabled = no layer).
    mTileCache   = new GrassTileCache( "google_satellite", 512 );
    mTileFetcher = new GrassTileFetcher( "", mTileCache, this );
    applyTileSettingsFromQSettings();

    // GrassInterface: use the real canvas widget now
    mIface = new GrassInterface( this, mCanvas, mMessageBar, this );

    // Helper: parse "name@mapset" → (name, mapset)
    auto parseMapRef = []( const QString &ref ) -> QPair<QString, QString> {
        const int at = ref.indexOf( '@' );
        return at < 0 ? QPair<QString,QString>{ ref, {} }
                      : QPair<QString,QString>{ ref.left( at ), ref.mid( at+1 ) };
    };

    // Raster output from a module → add to canvas + layer panel
    connect( mIface, &GrassInterface::rasterLayerAdded,
             this, [this, parseMapRef]( const QString &, const QString &name, const QString & ) {
                 const auto [mapName, mapset] = parseMapRef( name );
                 mLayerPanel->addRasterLayer( mapName, mapset );
                 mMessageBar->pushMessage( "Raster added", name, GrassMessageBar::Info, 4 );
             } );

    // Vector output from a module → add to canvas + layer panel
    connect( mIface, &GrassInterface::vectorLayerAdded,
             this, [this, parseMapRef]( const QString &, const QString &name, const QString & ) {
                 const auto [mapName, mapset] = parseMapRef( name );
                 mLayerPanel->addVectorLayer( mapName, mapset );
                 mMessageBar->pushMessage( "Vector added", name, GrassMessageBar::Info, 4 );
             } );

    // Profile tool → open profile dialog
    connect( mCanvas, &GrassMapCanvas::profileLineReady,
             this, [this]( const QString &raster, const QString &mapset,
                           const QVector<QPointF> &coords ) {
                 auto *dlg = new GrassProfileDialog( this );
                 dlg->setAttribute( Qt::WA_DeleteOnClose );
                 dlg->show();
                 dlg->runProfile( raster, mapset, coords );
             } );

    // Toolbox → open module dialog, or native composite dialog for d.* types
    connect( mToolbox, &GrassToolboxTree::moduleActivated,
             this, [this]( const QString &moduleName ) {

        // Map GRASS display command names to their native composite layer type.
        // These open GrassAddLayerDialog instead of running the module as a subprocess.
        using LT = GrassAddLayerDialog::LayerType;
        static const QHash<QString, LT> kNativeDisplay = {
            { "d.rgb",           LT::Rgb           },
            { "d.ihs",           LT::Ihs           },
            { "d.shade",         LT::Shade         },
            { "d.rast.num",      LT::RastNum       },
            { "d.rast.arrow",    LT::RastArrow     },
            { "d.vect.thematic", LT::VectThematic  },
            { "d.vect.chart",    LT::VectChart     },
            { "d.label",         LT::Label         },
            { "d.geodesic",      LT::Geodesic      },
            { "d.rhumbline",     LT::Rhumbline     },
        };

        if ( kNativeDisplay.contains( moduleName ) )
        {
            auto *dlg = new GrassAddLayerDialog( mCanvas,
                                                  kNativeDisplay[moduleName], this );
            if ( dlg->exec() == QDialog::Accepted )
            {
                GrassMapLayer *layer = dlg->createLayer();
                if ( layer ) mLayerPanel->addCompositeLayer( layer );
            }
            delete dlg;
            return;
        }

        // All other modules → standard module dialog
        auto *dlg = new GrassModuleDialog( moduleName, {}, mIface, this );
        dlg->setAttribute( Qt::WA_DeleteOnClose );
        dlg->show();
    } );

    // Mapset changes → update title, status bar, refresh catalog and tiles
    connect( mEnv, &GrassEnv::mapsetChanged,
             mCatalog, &GrassDataCatalog::refresh );
    connect( mEnv, &GrassEnv::mapsetChanged,
             this, [this]( const QString &db, const QString &loc, const QString &ms ) {
                 setWindowTitle( QString( "GRASS GIS — %1/%2/%3 — qtgrass" )
                                 .arg( db, loc, ms ) );
                 if ( mLocationLabel )
                     mLocationLabel->setText( QString( " %1/%2/%3 " ).arg( db, loc, ms ) );
                 mMessageBar->pushMessage( "Ready",
                     QString( "Mapset %1 / %2 opened." ).arg( loc, ms ),
                     GrassMessageBar::Info, 4 );
                 mCanvas->refreshRegionCache();
                 // (Re-)initialise tile background now that a session is active.
                 // The first call from the constructor was a no-op because no
                 // session existed yet; this call actually creates the tile layer.
                 applyTileSettingsFromQSettings();
             } );

    if ( mEnv->isValid() && !mEnv->location().isEmpty() )
        setWindowTitle( QString( "GRASS GIS — %1/%2/%3 — qtgrass" )
                        .arg( mEnv->gisDatabase(), mEnv->location(), mEnv->mapset() ) );
}

void GrassApp::buildCentralArea()
{
    QWidget *central = new QWidget( this );
    QVBoxLayout *vbox = new QVBoxLayout( central );
    vbox->setContentsMargins( 0, 0, 0, 0 );
    vbox->setSpacing( 0 );

    mMessageBar = new GrassMessageBar( central );
    vbox->addWidget( mMessageBar );

    // Left panel: tabbed Modules tree + Data catalog
    QTabWidget *leftTabs = new QTabWidget( central );
    leftTabs->setMinimumWidth( 240 );
    leftTabs->setTabPosition( QTabWidget::South );

    mToolbox = new GrassToolboxTree( leftTabs );
    leftTabs->addTab( mToolbox, "Modules" );

    mCatalog = nullptr;

    // Map canvas (right-right)
    mCanvas = new GrassMapCanvas( central );
    mCanvas->setMinimumSize( 400, 300 );

    // Layer panel (right-left)
    mLayerPanel = new GrassLayerPanel( mCanvas, central );

    // Right side: layer panel + canvas side by side
    QSplitter *rightSplit = new QSplitter( Qt::Horizontal, central );
    rightSplit->addWidget( mLayerPanel );
    rightSplit->addWidget( mCanvas );
    rightSplit->setStretchFactor( 0, 0 );
    rightSplit->setStretchFactor( 1, 1 );
    rightSplit->setSizes( { 180, 900 } );

    // Data catalog tab (added after mLayerPanel exists)
    mCatalog = new GrassDataCatalog( mCanvas, mLayerPanel, leftTabs );
    leftTabs->addTab( mCatalog, "Data" );

    // Outer splitter: left tabs | right side
    QSplitter *outerSplit = new QSplitter( Qt::Horizontal, central );
    outerSplit->addWidget( leftTabs );
    outerSplit->addWidget( rightSplit );
    outerSplit->setStretchFactor( 0, 0 );
    outerSplit->setStretchFactor( 1, 1 );
    outerSplit->setSizes( { 240, 1040 } );

    vbox->addWidget( outerSplit, 1 );
    setCentralWidget( central );
}

// ---------------------------------------------------------------------------
// Apply / remove the dark stylesheet and persist the choice.
// mode: "dark" | "light" | "system"
// ---------------------------------------------------------------------------
void GrassApp::applyTheme( const QString &mode )
{
    QSettings s;
    s.setValue( "ui/theme", mode );

    if ( mode == "dark" )
    {
        qApp->setStyleSheet( GrassStyle::dark() );
    }
    else if ( mode == "light" )
    {
        qApp->setStyleSheet( "" );   // Qt default (light)
    }
    else // "system" — follow the OS palette
    {
        qApp->setStyleSheet( "" );
        const int lum = qApp->palette().color( QPalette::Window ).lightness();
        if ( lum < 128 )
            qApp->setStyleSheet( GrassStyle::dark() );
    }
}

void GrassApp::buildMenuBar()
{
    // ── module callback used by GrassMenuBuilder ──────────────────────────
    auto openModule = [this]( const QString &cmd )
    {
        auto *d = new GrassModuleDialog( cmd, {}, mIface, this );
        d->setAttribute( Qt::WA_DeleteOnClose );
        d->show();
    };

    // ── handler map: wxPython handler name → qtgrass lambda ───────────────
    GrassMenuBuilder::HandlerMap handlers = {
        { "OnAddRaster", [this] {
            bool ok;
            QString name = QInputDialog::getText(
                this, "Add Raster", "Raster map name [@mapset]:", QLineEdit::Normal, {}, &ok );
            if ( ok && !name.isEmpty() )
            {
                const int at = name.indexOf('@');
                mLayerPanel->addRasterLayer( at>=0 ? name.left(at) : name,
                                              at>=0 ? name.mid(at+1) : QString() );
            }
        } },
        { "OnAddVector", [this] {
            bool ok;
            QString name = QInputDialog::getText(
                this, "Add Vector", "Vector map name [@mapset]:", QLineEdit::Normal, {}, &ok );
            if ( ok && !name.isEmpty() )
            {
                const int at = name.indexOf('@');
                mLayerPanel->addVectorLayer( at>=0 ? name.left(at) : name,
                                              at>=0 ? name.mid(at+1) : QString() );
            }
        } },
    };

    // ─────────────────────────────────────────────────────────────────────
    // File — qtgrass-specific (workspace, project, export, quit)
    // ─────────────────────────────────────────────────────────────────────
    QMenu *fileMenu = menuBar()->addMenu( "&File" );

    // ── Workspace (mirrors wxPython File → Workspace) ────────────────────
    QMenu *wsMenu = fileMenu->addMenu( grassIcon("open"), "&Workspace" );
    wsMenu->addAction( "Ne&w", QKeySequence( "Ctrl+N" ), this, [this] {
        mLayerPanel->clear();
        mMessageBar->pushMessage( "Workspace",
            "Layers cleared — start adding layers for a new workspace.",
            GrassMessageBar::Info, 4 );
    } )->setToolTip( "Clear all layers to start a new workspace" );

    wsMenu->addAction( grassIcon("open"), "&Open…", QKeySequence( "Ctrl+O" ),
                       this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, "Open Workspace", QDir::homePath(),
            "GRASS Workspace (*.gxw);;All files (*)" );
        if ( !path.isEmpty() ) loadWorkspace( path );
    } );

    // Save: write to the last opened/saved workspace path; prompt only when none.
    // Capture the action so we can register it on the main window too — that
    // guarantees Ctrl+S works regardless of focus (submenu shortcuts can be
    // missed by Qt when the focus is in a non-action widget).
    QAction *saveAct = wsMenu->addAction(
        grassIcon("save"), "&Save", QKeySequence( "Ctrl+S" ),
        this, [this] {
            const QString lastPath = QSettings().value( "session/lastWorkspace" ).toString();
            const QString defaultDir =
                ( !mEnv->gisDatabase().isEmpty() && !mEnv->location().isEmpty() )
                ? mEnv->gisDatabase() + "/" + mEnv->location()
                : QDir::homePath();
            if ( lastPath.isEmpty() || !QFile::exists( lastPath ) )
            {
                const QString path = QFileDialog::getSaveFileName(
                    this, "Save Workspace", defaultDir,
                    "GRASS Workspace (*.gxw)" );
                if ( path.isEmpty() ) return;
                saveWorkspace( path );
            }
            else
            {
                saveWorkspace( lastPath );
            }
        } );
    saveAct->setShortcutContext( Qt::WindowShortcut );
    addAction( saveAct );   // ensure shortcut is active when focus is elsewhere

    wsMenu->addAction( "Save &As…", QKeySequence( "Ctrl+Shift+S" ), this, [this] {
        const QString defaultDir =
            ( !mEnv->gisDatabase().isEmpty() && !mEnv->location().isEmpty() )
            ? mEnv->gisDatabase() + "/" + mEnv->location()
            : QDir::homePath();
        const QString path = QFileDialog::getSaveFileName(
            this, "Save Workspace As", defaultDir,
            "GRASS Workspace (*.gxw)" );
        if ( !path.isEmpty() ) saveWorkspace( path );
    } );

    wsMenu->addSeparator();
    wsMenu->addAction( "&Close", this, [this] {
        mLayerPanel->clear();
        QSettings().remove( "session/lastWorkspace" );
        mMessageBar->pushMessage( "Workspace closed",
            "All layers removed.", GrassMessageBar::Info, 3 );
    } )->setToolTip( "Remove all layers from the current display" );

    // ── Change Project (open startup / location browser) ────────────────
    fileMenu->addSeparator();
    fileMenu->addAction( "Chan&ge Project…", QKeySequence( "Ctrl+Shift+O" ),
                         this, [this] {
        mLayerPanel->clear();
        GrassStartupDialog dlg( mEnv, this );
        if ( dlg.exec() != QDialog::Accepted ) return;
        if ( !dlg.selectedWorkspace().isEmpty() ) {
            loadWorkspace( dlg.selectedWorkspace() );
            return;
        }
        mCanvas->refreshRegionCache();
        mCanvas->resetViewportToRegion();
        if ( mCatalog ) mCatalog->refresh();
        if ( !mEnv->location().isEmpty() )
            mMessageBar->pushMessage( "Project changed",
                QString( "%1 / %2 / %3" )
                    .arg( mEnv->gisDatabase(), mEnv->location(), mEnv->mapset() ),
                GrassMessageBar::Info, 4 );
    } );

    // ── Map display — add layers (mirrors wxPython File → Map display) ───
    fileMenu->addSeparator();
    fileMenu->addAction( grassIcon("layer-raster-add"), "Add &Raster Layer",
                         QKeySequence( "Ctrl+Shift+R" ), this, [this] {
        bool ok;
        QString n = QInputDialog::getText( this, "Add Raster",
            "Raster map name [@mapset]:", QLineEdit::Normal, {}, &ok );
        if ( ok && !n.isEmpty() ) {
            const int at = n.indexOf('@');
            mLayerPanel->addRasterLayer( at>=0 ? n.left(at) : n,
                                          at>=0 ? n.mid(at+1) : QString() );
        }
    } );
    fileMenu->addAction( grassIcon("layer-vector-add"), "Add &Vector Layer",
                         QKeySequence( "Ctrl+Shift+V" ), this, [this] {
        bool ok;
        QString n = QInputDialog::getText( this, "Add Vector",
            "Vector map name [@mapset]:", QLineEdit::Normal, {}, &ok );
        if ( ok && !n.isEmpty() ) {
            const int at = n.indexOf('@');
            mLayerPanel->addVectorLayer( at>=0 ? n.left(at) : n,
                                          at>=0 ? n.mid(at+1) : QString() );
        }
    } );

    // ── Export / Print (qtgrass-specific) ────────────────────────────────
    fileMenu->addSeparator();
    fileMenu->addAction( grassIcon("layer-export"), "E&xport Map…",
                         QKeySequence( "Ctrl+E" ), this, [this] {
        auto *dlg = new GrassMapExportDialog( mCanvas, this );
        dlg->setAttribute( Qt::WA_DeleteOnClose );
        dlg->show();
    } );
    fileMenu->addAction( "Print &Composer…", QKeySequence( "Ctrl+P" ), this, [this] {
        auto *dlg = new GrassLayoutDialog( mIface, mCanvas, this );
        dlg->setAttribute( Qt::WA_DeleteOnClose );
        dlg->show();
    } );

    // ── Preferences (appearance + tile settings) ─────────────────────────
    fileMenu->addSeparator();
    QMenu *prefMenu = fileMenu->addMenu( grassIcon("settings"), "&Preferences" );

    QMenu *themeMenu = prefMenu->addMenu( "Appearance" );
    QActionGroup *themeGroup = new QActionGroup( this );
    themeGroup->setExclusive( true );
    const QString curTheme = QSettings().value( "ui/theme", "system" ).toString();
    auto addTheme = [&]( const QString &label, const QString &key ) {
        QAction *a = themeMenu->addAction( label );
        a->setCheckable( true );
        a->setChecked( curTheme == key );
        themeGroup->addAction( a );
        connect( a, &QAction::triggered, this,
                 [this, key] { applyTheme( key ); } );
    };
    addTheme( "Follow OS", "system" );
    addTheme( "Light",     "light"  );
    addTheme( "Dark",      "dark"   );

    prefMenu->addSeparator();
    prefMenu->addAction( grassIcon("layer-wms-add"), "Satellite / Tile Layer…",
                         QKeySequence( "Ctrl+," ), this, [this] {
        GrassPreferencesDialog dlg( this );
        connect( &dlg, &GrassPreferencesDialog::tileSettingsChanged,
                 this, &GrassApp::applyTileSettings );
        dlg.exec();
    } );

    // ── Auto-save Workspace submenu ──────────────────────────────────────
    prefMenu->addSeparator();
    QMenu *asMenu = prefMenu->addMenu( "Auto-save Workspace" );

    mAutoSaveEnableAct = asMenu->addAction( "Enable Auto-save" );
    mAutoSaveEnableAct->setCheckable( true );
    mAutoSaveEnableAct->setChecked(
        QSettings().value( "workspace/autosave/enabled", false ).toBool() );
    connect( mAutoSaveEnableAct, &QAction::toggled, this, [this]( bool on ) {
        QSettings().setValue( "workspace/autosave/enabled", on );
        updateAutoSaveTimer();
        if ( on )
            mMessageBar->pushMessage( "Auto-save",
                QString( "Workspace will be auto-saved every %1 minute(s)." )
                    .arg( QSettings().value( "workspace/autosave/intervalMinutes", 1 ).toInt() ),
                GrassMessageBar::Info, 4 );
    } );

    asMenu->addAction( "Set Interval…", this, [this] {
        bool ok;
        const int cur = QSettings().value( "workspace/autosave/intervalMinutes", 1 ).toInt();
        const int val = QInputDialog::getInt( this, "Auto-save Interval",
            "Auto-save every N minutes:", cur, 1, 60, 1, &ok );
        if ( !ok ) return;
        QSettings().setValue( "workspace/autosave/intervalMinutes", val );
        updateAutoSaveTimer();
        if ( mAutoSaveEnableAct->isChecked() )
            mMessageBar->pushMessage( "Auto-save",
                QString( "Interval set to %1 minute(s)." ).arg( val ),
                GrassMessageBar::Info, 3 );
    } );

    // Initialise timer after menu is built
    mAutoSaveTimer = new QTimer( this );
    mAutoSaveTimer->setSingleShot( false );
    connect( mAutoSaveTimer, &QTimer::timeout, this, &GrassApp::performAutoSave );
    updateAutoSaveTimer();

    fileMenu->addSeparator();
    fileMenu->addAction( "E&xit", QKeySequence( "Ctrl+Q" ),
                         qApp, &QApplication::quit );

    // ────────────────────────────────────────────────────────────────────
    // GRASS XML menus: Settings, Raster, Vector, Imagery, 3D raster,
    //                  Database, Temporal
    // GrassMenuBuilder parses $GISBASE/gui/wxpython/xml/menudata.xml and
    // adds every <menuitem> with a <command> tag as a module launcher.
    // ────────────────────────────────────────────────────────────────────
    // ── GRASS XML menus — fills prefixMap and commandActions ───────────
    GrassMenuBuilder::PrefixMenuMap    prefixMap;
    GrassMenuBuilder::CommandActionMap commandActions;
    GrassMenuBuilder::build( menuBar(), openModule, handlers,
                              prefixMap, commandActions );

    // ── Grass-Addons: insert into existing submenus via prefix map ──────
    // Find the Tools menu pointer (fallback for uncategorised addons).
    QMenu *toolsMenuPtr = nullptr;
    for ( QAction *a : menuBar()->actions() )
    {
        if ( a->menu() ) {
            QString lbl = a->text();
            lbl.remove( QRegularExpression( "&(?!amp;)" ) );
            if ( lbl.compare( "Tools", Qt::CaseInsensitive ) == 0 ) {
                toolsMenuPtr = a->menu();
                break;
            }
        }
    }
    GrassAddonMenu::populate( prefixMap, commandActions, toolsMenuPtr, openModule );

    // ── Tools (qtgrass-specific, appended after GRASS menus) ───────────
    QMenu *toolsMenu = menuBar()->addMenu( "&Tools" );
    toolsMenu->addAction( grassIcon("region"), "Computational &Region…",
                           QKeySequence( "Ctrl+R" ), this, [this] {
        auto *dlg = new GrassRegionDialog( mCanvas, this );
        dlg->setAttribute( Qt::WA_DeleteOnClose );
        dlg->show();
    } );
    toolsMenu->addSeparator();
    toolsMenu->addAction( "r.mapcalc…", QKeySequence( "Ctrl+M" ), this, [this] {
        auto *dlg = new GrassMapCalcDialog( mIface, this );
        dlg->setAttribute( Qt::WA_DeleteOnClose );
        dlg->show();
    } );
    toolsMenu->addAction( "Color &Table…", QKeySequence( "Ctrl+Shift+C" ), this, [this] {
        const QString ref = mLayerPanel->tree()->topRasterRef();
        if ( ref.isEmpty() ) {
            mMessageBar->pushMessage( "Color Table", "No visible raster layer.",
                                      GrassMessageBar::Warning, 3 );
            return;
        }
        const int at = ref.indexOf('@');
        const QString name   = at < 0 ? ref : ref.left(at);
        const QString mapset = at < 0 ? QString() : ref.mid(at+1);
        auto *dlg = new GrassColorTableDialog( name, mapset, mCanvas, this );
        dlg->setAttribute( Qt::WA_DeleteOnClose );
        dlg->show();
    } );
    toolsMenu->addSeparator();
    toolsMenu->addAction( "&Script Runner…", QKeySequence( "Ctrl+Shift+R" ), this, [this] {
        auto *dlg = new GrassScriptRunner( this );
        dlg->setAttribute( Qt::WA_DeleteOnClose );
        dlg->show();
    } );
    toolsMenu->addAction( "Addon &Manager…", this, [this] {
        auto *dlg = new GrassAddonManager( this );
        dlg->setAttribute( Qt::WA_DeleteOnClose );
        dlg->show();
    } );
    toolsMenu->addSeparator();
    toolsMenu->addAction( "Graphical &Modeler…", this, [this] {
        auto *dlg = new GrassModelerDialog( mIface, this );
        dlg->setAttribute( Qt::WA_DeleteOnClose );
        dlg->show();
    } );
    toolsMenu->addAction( "&Temporal Animation…", this, [this] {
        auto *dlg = new GrassAnimationDialog( mIface, this );
        dlg->setAttribute( Qt::WA_DeleteOnClose );
        dlg->show();
    } );
    toolsMenu->addAction( "Map &Swipe…", this, [this] {
        auto *dlg = new GrassMapSwipeDialog( this );
        dlg->setAttribute( Qt::WA_DeleteOnClose );
        dlg->show();
    } );
    toolsMenu->addAction( "&WMS/WMTS Browser…", this, [this] {
        auto *dlg = new GrassWmsBrowserDialog( this );
        dlg->setAttribute( Qt::WA_DeleteOnClose );
        dlg->show();
    } );
    toolsMenu->addSeparator();
    toolsMenu->addAction( "&3D Terrain View (nviz)…", QKeySequence( "Ctrl+3" ),
                           this, [this] {
        auto *dlg = new GrassNvizDialog( mIface, this );
        dlg->setAttribute( Qt::WA_DeleteOnClose );
        dlg->show();
    } );
    toolsMenu->addAction( "&Georectification…", this, [this] {
        auto *dlg = new GrassGeoRectDialog( mIface, this );
        dlg->setAttribute( Qt::WA_DeleteOnClose );
        dlg->show();
    } );
    toolsMenu->addAction( "Supervised &Classification (iclass)…", this, [this] {
        auto *dlg = new GrassIclassDialog( mIface, this );
        dlg->setAttribute( Qt::WA_DeleteOnClose );
        dlg->show();
    } );
    toolsMenu->addAction( "&Database Manager…", QKeySequence( "Ctrl+D" ),
                           this, [this] {
        auto *dlg = new GrassDbManagerDialog( mIface, {}, {}, 1, this );
        dlg->setAttribute( Qt::WA_DeleteOnClose );
        dlg->show();
    } );

    // ── Planetary ───────────────────────────────────────────────────────
    // The scan requires a valid GISRC (modules call G_gisinit before the
    // XML parser runs), so it is deferred until a mapset is opened.
    // rebuildPlanetaryMenu() is connected to GrassEnv::mapsetChanged and
    // also called once now in case a session is already active at startup.
    mOpenModuleFn = openModule;
    mPlanetMenu   = menuBar()->addMenu( "P&lanetary" );
    // Defer the first scan to the next event-loop tick so the constructor
    // has finished and mIface is valid before populate() runs.
    QTimer::singleShot( 0, this, &GrassApp::rebuildPlanetaryMenu );

    // Defer on mapsetChanged too: the scan spawns ~30 QProcess children and
    // blocks for several seconds, which must not happen synchronously inside
    // openMapset() → emit mapsetChanged() — that call stack may still hold
    // a setjmp frame or other GRASS C state that interacts badly with the scan.
    connect( mEnv, &GrassEnv::mapsetChanged, this, [this] {
        QTimer::singleShot( 0, this, &GrassApp::rebuildPlanetaryMenu );
    } );

    // ── Help ────────────────────────────────────────────────────────────
    QMenu *helpMenu = menuBar()->addMenu( "&Help" );
    helpMenu->addAction( grassIcon("help"), "GRASS &Manual", this, [this] {
        auto *dlg = new GrassHelpDialog( "index", this );
        dlg->setAttribute( Qt::WA_DeleteOnClose );
        dlg->show();
    } );
    helpMenu->addSeparator();
    helpMenu->addAction( "&About qtgrass", this, [this] {
        mMessageBar->pushMessage( "qtgrass",
            "Qt6 native GUI for GRASS GIS — github.com/yann/qtgrass",
            GrassMessageBar::Info, 8 );
    } );

    // Cap every menu to ≤70 % of screen height so Qt adds scroll arrows.
    applyMenuScrollLimits();
}

// ---------------------------------------------------------------------------
// applyMenuScrollLimits — walk every QMenu reachable from the menu bar and
// cap its maximum height.  When a QMenu's sizeHint().height() exceeds
// maximumHeight(), Qt automatically inserts its built-in up/down scroll
// arrow items — no manual scrollbar management needed.
// ---------------------------------------------------------------------------
static void limitMenuHeight( QMenu *menu, int maxH )
{
    if ( !menu ) return;
    menu->setMaximumHeight( maxH );
    for ( QAction *a : menu->actions() )
        if ( a->menu() )
            limitMenuHeight( a->menu(), maxH );
}

void GrassApp::rebuildPlanetaryMenu()
{
    if ( !mPlanetMenu ) return;
    if ( !mEnv->isValid() || mEnv->location().isEmpty() )
        return;   // no session yet — wait for the next mapsetChanged

    qCDebug(lcGrassApp) << "[rebuildPlanetaryMenu] clearing menu, mIface=" << (void*)mIface;
    mPlanetMenu->clear();

    qCDebug(lcGrassApp) << "[rebuildPlanetaryMenu] starting scan";
    const auto modules = GrassPlanetaryMenu::scanAvailableModules(
        mEnv->buildSubprocessEnv() );
    qCDebug(lcGrassApp) << "[rebuildPlanetaryMenu] scan done, modules=" << modules.size()
                   << "mPlanetMenu=" << (void*)mPlanetMenu
                   << "mIface=" << (void*)mIface;
    GrassPlanetaryMenu::populate( mPlanetMenu, mOpenModuleFn, modules, mIface, this );
    qCDebug(lcGrassApp) << "[rebuildPlanetaryMenu] populate done";
    applyMenuScrollLimits();
    qCDebug(lcGrassApp) << "[rebuildPlanetaryMenu] complete";
}

void GrassApp::applyMenuScrollLimits()
{
    // Use 70 % of the primary screen's usable height.
    // Minimum 400 px so tiny screens still get a usable menu.
    QScreen *ps = QGuiApplication::primaryScreen();
    if ( !ps ) return;   // no screen yet (headless / very early startup)
    const QRect screen = ps->availableGeometry();
    const int maxH = qMax( 400, static_cast<int>( screen.height() * 0.70 ) );

    for ( QAction *a : menuBar()->actions() )
        if ( a->menu() )
            limitMenuHeight( a->menu(), maxH );
}

void GrassApp::buildToolBar()
{
    QToolBar *tb = addToolBar( "Map Tools" );
    tb->setMovable( false );
    tb->setIconSize( QSize( 22, 22 ) );
    tb->setToolButtonStyle( Qt::ToolButtonIconOnly );

    // ── Zoom to full extent ─────────────────────────────────────────────
    QAction *extAct = tb->addAction(
        grassToolIcon( "zoom-extent" ), "Full Extent" );
    extAct->setToolTip( "Zoom to full extent of all layers  (Ctrl+Shift+F)" );
    extAct->setShortcut( QKeySequence( "Ctrl+Shift+F" ) );
    connect( extAct, &QAction::triggered, this,
             [this] { mCanvas->zoomToFullExtent(); } );
    tb->addSeparator();

    // ── Exclusive map-tool group ────────────────────────────────────────
    QActionGroup *toolGroup = new QActionGroup( this );
    toolGroup->setExclusive( true );

    auto addTool = [&]( const QString &iconName, const QString &text,
                        const QString &tip, MapTool t ) -> QAction * {
        QIcon ico = grassToolIcon( iconName );
        QAction *a = ico.isNull() ? tb->addAction( text )
                                  : tb->addAction( ico, text );
        a->setToolTip( tip );
        a->setCheckable( true );
        toolGroup->addAction( a );
        connect( a, &QAction::toggled, this, [this, t]( bool on ) {
            if ( on ) mCanvas->setTool( t );
        } );
        return a;
    };

    QAction *panAct = addTool( "pan",                "Pan",
        "Pan — drag to scroll the map",                         MapTool::Pan );
    addTool( "zoom-in",              "Zoom In",
        "Zoom in — single click",                               MapTool::ZoomIn );
    addTool( "zoom-out",             "Zoom Out",
        "Zoom out — single click",                              MapTool::ZoomOut );
    addTool( "layer-info",           "Identify",
        "Query raster/vector value at cursor",                  MapTool::Identify );
    addTool( "layer-raster-profile", "Profile",
        "Draw profile line  (left = add point, right = run)",  MapTool::Profile );
    panAct->setChecked( true );

    tb->addSeparator();

    // ── Region overlay toggle ───────────────────────────────────────────
    QIcon regIco = grassToolIcon( "region" );
    QAction *regionAct = regIco.isNull()
        ? tb->addAction( "Region" )
        : tb->addAction( regIco, "Region" );
    regionAct->setToolTip( "Show / hide GRASS computational region box" );
    regionAct->setCheckable( true );
    regionAct->setChecked( true );
    connect( regionAct, &QAction::toggled, this,
             [this]( bool on ) { mCanvas->setShowRegion( on ); } );

    tb->addSeparator();

    // ── r.mapcalc ──────────────────────────────────────────────────────
    QAction *calcAct = tb->addAction(
        grassToolIcon( "raster-calculator" ), "Map Calc" );
    calcAct->setToolTip( "Open r.mapcalc expression editor" );
    connect( calcAct, &QAction::triggered, this, [this] {
        auto *dlg = new GrassMapCalcDialog( mIface, this );
        dlg->setAttribute( Qt::WA_DeleteOnClose );
        dlg->show();
    } );

    tb->addSeparator();

    // ── Digitize toggle ─────────────────────────────────────────────────
    auto *digitBar = new GrassDigitToolbar( mCanvas, mLayerPanel, this );
    digitBar->setVisible( false );
    addToolBar( Qt::BottomToolBarArea, digitBar );

    QAction *digitAct = tb->addAction(
        grassToolIcon( "edit" ), "Digitize" );
    digitAct->setToolTip( "Show / hide the vector digitizing toolbar" );
    digitAct->setCheckable( true );
    connect( digitAct, &QAction::toggled, digitBar, &QToolBar::setVisible );
    connect( digitBar, &GrassDigitToolbar::editingStarted,
             this, [this]( const QString &name ) {
        mMessageBar->pushMessage( "Editing",
            "Editing started: " + name, GrassMessageBar::Info, 4 );
    } );
    connect( digitBar, &GrassDigitToolbar::editingStopped, this, [this] {
        mMessageBar->pushMessage( "Editing",
            "Editing stopped — topology rebuilt.", GrassMessageBar::Info, 4 );
    } );

    tb->addSeparator();

    // ── Display-driver (quality) toggle ────────────────────────────────
    QAction *driverAct = tb->addAction(
        grassToolIcon( "monitor-settings" ), "Display Quality" );
    driverAct->setToolTip(
        "Toggle Direct (fast) / GRASS Display Driver (full d.* symbology)" );
    driverAct->setCheckable( true );
    driverAct->setChecked( false );
    connect( driverAct, &QAction::toggled, this, [this]( bool on ) {
        mCanvas->setRenderMode( on ? GrassMapCanvas::RenderMode::DisplayDriver
                                   : GrassMapCanvas::RenderMode::Direct );
        mMessageBar->pushMessage(
            on ? "Display Driver mode" : "Direct render mode",
            on ? "GRASS Cairo driver — full d.* symbology."
               : "Direct C API — fast interactive mode.",
            GrassMessageBar::Info, 4 );
    } );

    // ── Histogram ──────────────────────────────────────────────────────
    QAction *histAct = tb->addAction(
        grassToolIcon( "layer-raster-histogram" ), "Histogram" );
    histAct->setToolTip( "Histogram of the top visible raster layer" );
    connect( histAct, &QAction::triggered, this, [this] {
        const QString ref = mLayerPanel->tree()->topRasterRef();
        if ( ref.isEmpty() ) {
            mMessageBar->pushMessage( "Histogram", "No visible raster layer.",
                                      GrassMessageBar::Warning, 3 );
            return;
        }
        const int at = ref.indexOf('@');
        const QString name   = at < 0 ? ref : ref.left(at);
        const QString mapset = at < 0 ? QString() : ref.mid(at+1);
        auto *dlg = new GrassHistogramDialog( name, mapset, this );
        dlg->setAttribute( Qt::WA_DeleteOnClose );
        dlg->show();
    } );

    // ── Rendering status signals ────────────────────────────────────────
    connect( mCanvas, &GrassMapCanvas::renderingStarted, this,
             [this] { statusBar()->showMessage( "Rendering…" ); } );
    connect( mCanvas, &GrassMapCanvas::renderingDone,    this,
             [this] { statusBar()->clearMessage(); } );
}

void GrassApp::loadWorkspace( const QString &path )
{
    GrassWorkspace ws;
    if ( !ws.load( path ) )
    {
        mMessageBar->pushMessage( "Workspace",
            "Cannot load " + path + ": " + ws.lastError(),
            GrassMessageBar::Critical, 6 );
        return;
    }

    // Clear tree + canvas together before switching session.
    // mLayerPanel->clear() removes tree items and canvas layers atomically —
    // no dangling pointers in the tree, and waitForFinished() ensures no render
    // is running when openMapset() changes the GRASS global state.
    mLayerPanel->clear();

    // Switch session if the workspace encodes a different one
    if ( !ws.database().isEmpty() && !ws.location().isEmpty() && !ws.mapset().isEmpty() )
    {
        if ( ws.database() != mEnv->gisDatabase() ||
             ws.location() != mEnv->location()    ||
             ws.mapset()   != mEnv->mapset() )
        {
            mEnv->openMapset( ws.database(), ws.location(), ws.mapset() );
            // Reset viewport to the new session's region so the first render
            // of the new session does not use the old session's coordinates
            // (e.g. projected meters passed to a geographic location → crash).
            mCanvas->refreshRegionCache();
            mCanvas->resetViewportToRegion();
        }
    }

    // Add layers from the first display (others are ignored for now)
    if ( !ws.displays().isEmpty() )
    {
        const GwxDisplay &disp = ws.displays().first();

        // Add in reverse order so the first layer ends up on top
        for ( int i = disp.layers.size() - 1; i >= 0; --i )
        {
            const GwxLayer &l = disp.layers[i];
            // Parse "map@mapset" → name, mapset
            const int at = l.name.indexOf('@');
            const QString mapName   = (at >= 0) ? l.name.left(at)  : l.name;
            const QString mapMapset = (at >= 0) ? l.name.mid(at+1) : mEnv->mapset();

            if ( l.type == "raster" || l.displayCmd == "d.rast" )
                mLayerPanel->addRasterLayer( mapName, mapMapset );
            else if ( l.type == "vector" || l.displayCmd == "d.vect" )
                mLayerPanel->addVectorLayer( mapName, mapMapset );
        }

        // Restore decoration panel state if the workspace contains one.
        if ( !disp.decorations.isEmpty() ) {
            QList<GrassDecorationPanel::DecoState> states;
            for ( const GwxDecoration &gd : disp.decorations )
                states.append( { gd.prog, gd.args, gd.enabled } );
            mLayerPanel->decoPanel()->restoreState( states );
        }

        // Restore the saved display viewport from the .gxw rather than
        // zooming to layer extents.  Layer auto-zoom (addLayer for size==1)
        // runs first, so we override it with the saved extent afterwards.
        //
        // Priority:
        //   1. Saved extent in .gxw  (west,south,east,north from the display element)
        //   2. GRASS computational region (WIND) — the session's working area
        //   3. zoomToFullExtent() — last resort, dominated by largest raster
        {
            GrassExtent savedVp { disp.north, disp.south, disp.east, disp.west };
            if ( savedVp.isValid() )
            {
                // Apply after event loop processes addLayer() signals (50 ms)
                QTimer::singleShot( 50, mCanvas, [this, savedVp] {
                    mCanvas->zoomToExtent( savedVp );
                } );
            }
            else
            {
                // No saved extent — use the GRASS computational region (WIND).
                // resetViewportToRegion() is a no-op if mRegion is not yet valid,
                // so the layer auto-zoom from addLayer() is kept as last resort.
                QTimer::singleShot( 50, mCanvas, [this] {
                    mCanvas->resetViewportToRegion();
                    mCanvas->scheduleRender();
                } );
            }
        }
    }

    // Explicitly refresh the data catalog after session switch.
    // mapsetChanged already connects to refresh() but an explicit call ensures
    // the catalog shows the new location even if the signal fired before the
    // OS environment variable was fully visible to qgetenv().
    if ( mCatalog ) mCatalog->refresh();

    // Remember the loaded workspace path so Ctrl+S overwrites it (rather than
    // prompting for Save As every time, or saving to a stale leftover path).
    QSettings().setValue( "session/lastWorkspace", path );

    mMessageBar->pushMessage( "Workspace loaded",
        QFileInfo( path ).fileName() +
        QString( " — %1 layer(s)" ).arg(
            ws.displays().isEmpty() ? 0 : ws.displays().first().layers.size() ),
        GrassMessageBar::Info, 5 );
}

void GrassApp::saveWorkspace( const QString &path, bool silent )
{
    // Write a minimal GRASS .gxw XML that wxPython and qtgrass can both open.
    // Format:
    //   <gxw>
    //     <session> database / location / mapset </session>
    //     <display extent="W,S,E,N,0,1">
    //       <layer type="raster|vector" name="map@mapset" checked="1" opacity="1.0">
    //         <task name="d.rast|d.vect">
    //           <parameter name="map"><value>map@mapset</value></parameter>
    //         </task>
    //       </layer>
    //     </display>
    //   </gxw>
    QFile f( path );
    if ( !f.open( QIODevice::WriteOnly | QIODevice::Text ) )
    {
        mMessageBar->pushMessage( "Save Workspace",
            "Cannot write to: " + path, GrassMessageBar::Critical, 6 );
        return;
    }

    const GrassExtent vp = mCanvas->viewportExtent();
    QTextStream out( &f );
    out.setEncoding( QStringConverter::Utf8 );

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<!DOCTYPE gxw SYSTEM \"grass-gxw.dtd\">\n"
        << "<gxw>\n"
        << "    <session>\n"
        << "        <database>" << mEnv->gisDatabase() << "</database>\n"
        << "        <location>" << mEnv->location()    << "</location>\n"
        << "        <mapset>"   << mEnv->mapset()      << "</mapset>\n"
        << "    </session>\n"
        << "    <display name=\"Map Display 1\" render=\"1\" mode=\"0\" "
        << "showCompExtent=\"1\" alignExtent=\"1\" constrainRes=\"0\" "
        << "showStatusbar=\"1\" showToolbars=\"1\" isDocked=\"1\" "
        << QString( "extent=\"%1,%2,%3,%4,0.000000,1.000000\" " )
             .arg( vp.west, 0, 'f', 6 ).arg( vp.south, 0, 'f', 6 )
             .arg( vp.east, 0, 'f', 6 ).arg( vp.north, 0, 'f', 6 )
        << "viewMode=\"2d\">\n";

    // Write layers in reverse order (bottom layer first in the XML, matching
    // wxPython convention — qtgrass loadWorkspace() re-reverses on load)
    const auto &layers = mCanvas->layers();
    for ( int i = layers.size() - 1; i >= 0; --i )
    {
        const GrassMapLayer *l = layers[i];
        const bool isRaster = dynamic_cast<const GrassRasterMapLayer *>( l ) != nullptr;
        const QString type   = isRaster ? "raster" : "vector";
        const QString dcmd   = isRaster ? "d.rast" : "d.vect";
        // Build full map@mapset name
        const QString fullName = l->name().contains('@')
            ? l->name()
            : l->name() + "@" + mEnv->mapset();
        const int checked = l->visible() ? 1 : 0;
        const double opacity = l->opacity();

        out << QString( "        <layer type=\"%1\" name=\"%2\" "
                        "checked=\"%3\" opacity=\"%4\">\n" )
               .arg( type ).arg( fullName ).arg( checked ).arg( opacity, 0, 'f', 6 );
        out << QString( "            <task name=\"%1\">\n" ).arg( dcmd );
        out << "                <parameter name=\"map\">\n"
            << "                    <value>" << fullName << "</value>\n"
            << "                </parameter>\n"
            << "            </task>\n"
            << "        </layer>\n";
    }

    // Write decoration states (qtgrass extension — ignored by wxPython).
    auto xmlEsc = []( const QString &s ) -> QString {
        QString r = s;
        r.replace( "&", "&amp;" );
        r.replace( "<", "&lt;"  );
        r.replace( ">", "&gt;"  );
        return r;
    };
    const auto decoStates = mLayerPanel->decoPanel()->savedState();
    if ( !decoStates.isEmpty() ) {
        out << "        <qtgrass-decorations>\n";
        for ( const auto &ds : decoStates ) {
            out << QString( "            <deco prog=\"%1\" enabled=\"%2\">\n" )
                   .arg( ds.prog ).arg( ds.enabled ? 1 : 0 );
            for ( const QString &a : ds.args )
                out << "                <a>" << xmlEsc(a) << "</a>\n";
            out << "            </deco>\n";
        }
        out << "        </qtgrass-decorations>\n";
    }

    out << "    </display>\n"
        << "</gxw>\n";

    f.close();

    QSettings().setValue( "session/lastWorkspace", path );
    if ( !silent )
        mMessageBar->pushMessage( "Workspace saved",
            QFileInfo( path ).fileName() +
                QString( " — %1 layer(s)" ).arg( layers.size() ),
            GrassMessageBar::Info, 4 );
}

void GrassApp::closeEvent( QCloseEvent *event )
{
    // Auto-save before closing: write to the auto-save default file, not to
    // the user's active workspace (unless its path IS the auto-save path).
    if ( QSettings().value( "workspace/autosave/enabled", false ).toBool() )
        performAutoSave();

    QMainWindow::closeEvent( event );
    if ( event->isAccepted() )
        QApplication::quit();
}

// ---------------------------------------------------------------------------
// Auto-save workspace helpers
// ---------------------------------------------------------------------------
QString GrassApp::autoSavePath() const
{
    const QString gisdb = mEnv->gisDatabase();
    const QString loc   = mEnv->location();
    const QString ms    = mEnv->mapset();
    if ( gisdb.isEmpty() || loc.isEmpty() ) return {};

    const QString dir  = gisdb + "/" + loc;
    const QString file = ( ms.isEmpty() || ms == "PERMANENT" )
                         ? "default.gxw"
                         : "default_" + ms + ".gxw";
    return dir + "/" + file;
}

void GrassApp::performAutoSave()
{
    const QString path = autoSavePath();
    if ( path.isEmpty() ) return;

    // Capture the user's active workspace before saving so we can restore it
    const QString userWs = QSettings().value( "session/lastWorkspace" ).toString();

    saveWorkspace( path, /*silent=*/true );

    // Restore lastWorkspace so Ctrl+S still targets the user's file, not ours
    if ( !userWs.isEmpty() && userWs != path )
        QSettings().setValue( "session/lastWorkspace", userWs );
}

void GrassApp::updateAutoSaveTimer()
{
    const bool enabled = QSettings().value( "workspace/autosave/enabled", false ).toBool();
    const int  minutes = QSettings().value( "workspace/autosave/intervalMinutes", 1 ).toInt();
    if ( !mAutoSaveTimer ) return;
    if ( enabled )
        mAutoSaveTimer->start( minutes * 60 * 1000 );
    else
        mAutoSaveTimer->stop();
}

// ---------------------------------------------------------------------------
// Tile background management
// ---------------------------------------------------------------------------
void GrassApp::applyTileSettingsFromQSettings()
{
    bool enabled; int idx;
    GrassPreferencesDialog::loadSettings( enabled, idx );
    const QList<TileProvider> &provs = GrassPreferencesDialog::providers();
    const TileProvider &prov = ( idx >= 0 && idx < provs.size() )
                               ? provs[idx] : provs[0];
    applyTileSettings( enabled, prov );
}

void GrassApp::applyTileSettings( bool enabled, const TileProvider &prov )
{
    // Remove existing tile layer from canvas if present
    if ( mTileBackground )
    {
        mCanvas->setLayerVisible( mTileBackground->name(), false );
        // Remove it from mCanvas's internal layer list.
        // We use clearLayers() + re-add is too disruptive; instead we rely on
        // the canvas ignoring invisible layers. For a clean swap, re-insert.
        // Simplest: keep the layer object, just update provider and visibility.
    }

    if ( !enabled )
    {
        if ( mTileBackground )
        {
            mTileBackground->setVisible( false );
            mTileBackground->invalidateCache();
        }
        mCanvas->scheduleRender();
        return;
    }

    // G_projection() requires a valid GRASS session (MAPSET must be set).
    // If no session is active yet (startup before the wizard), defer — the
    // tile layer will be created properly when mapsetChanged fires.
    if ( !mEnv->isValid() || mEnv->location().isEmpty() )
        return;

    // Don't create a tile layer when the provider's target body doesn't match
    // the current location's body — e.g. Moon tiles for a Mars session.
    // GPJ_grass_to_osr (used internally during layer creation) crashes for
    // non-Earth spherical CRS; body-mismatch tiles would be meaningless anyway.
    const PlanetaryBody locBody = GrassPreferencesDialog::detectBody();
    if ( prov.body != locBody )
    {
        if ( mTileBackground )
        {
            mTileBackground->setVisible( false );
            mTileBackground->invalidateCache();
        }
        mCanvas->scheduleRender();
        return;
    }

    // Detect if location is geographic (lat/lon) — needed for tile math.
    const bool isGeo = ( G_projection() == PROJECTION_LL );

    mTileCache->setProviderId( prov.id );
    mTileFetcher->setUrlTemplate( prov.urlTemplate );

    if ( !mTileBackground )
    {
        // First time: create the layer and insert at bottom of stack.
        mTileBackground = new GrassTileBackground(
            mTileCache, mTileFetcher, isGeo, prov.attribution );
        mTileBackground->setShowAttribution( true );

        // Insert at index 0 (bottom-most) in the canvas.
        mCanvas->insertLayerAt( 0, mTileBackground );

        // When tiles arrive → invalidate tile layer cache + re-render.
        connect( mTileFetcher, &GrassTileFetcher::tileFetched,
                 this, [this]( GrassTileKey ) {
                     if ( mTileBackground )
                         mTileBackground->invalidateCache();
                     mCanvas->scheduleRender();
                 } );
    }
    else
    {
        mTileBackground->setAttribution( prov.attribution );
        mTileBackground->setVisible( true );
        mTileBackground->invalidateCache();
    }

    if ( !isGeo )
    {
        mMessageBar->pushMessage( "Basemap",
            "Projected CRS — tiles will be reprojected on-the-fly to the current location's coordinate system.",
            GrassMessageBar::Info, 5 );
    }

    mCanvas->scheduleRender();
}

void GrassApp::buildStatusBar()
{
    QStatusBar *sb = statusBar();
    sb->setStyleSheet( "QStatusBar { border-top: 1px solid #ccc; }" );

    mLocationLabel = new QLabel( " No mapset open ", this );
    mLocationLabel->setToolTip( "Active GRASS location / mapset" );
    sb->addPermanentWidget( mLocationLabel );

    mCoordLabel = new QLabel( " — ", this );
    mCoordLabel->setToolTip( "Cursor coordinates in map units" );
    sb->addWidget( mCoordLabel );

    // Live coordinate display
    connect( mCanvas, &GrassMapCanvas::coordsChanged,
             this, [this]( double e, double n ) {
                 mCoordLabel->setText( QString( " E: %1  N: %2 " )
                                       .arg( e, 0, 'f', 4 )
                                       .arg( n, 0, 'f', 4 ) );
             } );

    // Identify results — displayed as a canvas popup overlay (showIdentifyPopup)
    // No additional status-bar or message-bar duplicate needed.

    if ( mEnv->isValid() && !mEnv->location().isEmpty() )
        mLocationLabel->setText( QString( " %1/%2/%3 " )
                                 .arg( mEnv->gisDatabase(),
                                       mEnv->location(),
                                       mEnv->mapset() ) );
}
