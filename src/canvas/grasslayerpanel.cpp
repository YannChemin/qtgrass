#include "grasslayerpanel.h"

#include "grassaddlayerdialog.h"
#include "grassdecorationpanel.h"
#include "grassmapcanvas.h"
#include "grassrastermaplayer.h"
#include "grassvectormaplayer.h"
#include "../attrib/grassattribtable.h"
#include "../colors/grasscolortabledialog.h"
#include "../grassenv.h"
#include "../layers/grasslayerprops.h"
#include "../layers/grasslayertree.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QProcess>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

// Prompt the user to pick a map (name@mapset) from a combobox populated via
// g.list, falling back to free-text entry if g.list finds nothing. Returns
// an empty string if the user cancels.
static QString pickMapRef( QWidget *parent, const QString &title,
                            const QString &grassType )
{
    const QString gb = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
    QStringList maps;
    if ( !gb.isEmpty() )
    {
        QProcess p;
        p.setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );
        p.start( gb + "/bin/g.list",
                 { "-m", "type=" + grassType, "mapset=*", "separator=newline" } );
        p.waitForFinished( 8000 );
        maps = QString::fromLocal8Bit( p.readAllStandardOutput() )
                   .split( '\n', Qt::SkipEmptyParts );
        for ( QString &m : maps ) m = m.trimmed();
    }

    if ( maps.isEmpty() )
    {
        bool ok;
        const QString ref = QInputDialog::getText( parent, title,
            "Map name (name or name@mapset):", QLineEdit::Normal, {}, &ok );
        return ( ok ? ref : QString() );
    }

    QDialog dlg( parent );
    dlg.setWindowTitle( title );
    auto *lay = new QVBoxLayout( &dlg );
    lay->addWidget( new QLabel( "Map:", &dlg ) );
    auto *combo = new QComboBox( &dlg );
    combo->addItems( maps );
    lay->addWidget( combo );
    auto *btns = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg );
    lay->addWidget( btns );
    QObject::connect( btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
    QObject::connect( btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );
    if ( dlg.exec() != QDialog::Accepted ) return {};
    return combo->currentText();
}

GrassLayerPanel::GrassLayerPanel( GrassMapCanvas *canvas, QWidget *parent )
    : QWidget( parent )
    , mCanvas( canvas )
{
    setMinimumWidth( 180 );
    setMaximumWidth( 300 );

    QVBoxLayout *lay = new QVBoxLayout( this );
    lay->setContentsMargins( 0, 0, 0, 0 );
    lay->setSpacing( 2 );

    // Header
    QHBoxLayout *hdr = new QHBoxLayout();
    hdr->setContentsMargins( 4, 2, 4, 0 );
    auto *hdrLbl = new QLabel( "<b>Layers</b>", this );
    hdr->addWidget( hdrLbl, 1 );

    // Add-group button
    QPushButton *addGrpBtn = new QPushButton( "+Group", this );
    addGrpBtn->setFixedHeight( 18 );
    addGrpBtn->setStyleSheet( "font-size:9px;" );
    addGrpBtn->setToolTip( "Add a new layer group" );
    hdr->addWidget( addGrpBtn );

    // Add Layer button — opens a drop-down menu with all layer types
    auto *addLayBtn = new QToolButton( this );
    addLayBtn->setText( "Add Layer ▾" );
    addLayBtn->setFixedHeight( 18 );
    addLayBtn->setStyleSheet( "font-size:9px;" );
    addLayBtn->setToolTip( "Add a raster, vector, or composite layer" );
    addLayBtn->setPopupMode( QToolButton::InstantPopup );

    auto *addMenu = new QMenu( addLayBtn );

    // Quick raster / vector
    auto *addRastAct  = addMenu->addAction( "Raster map…" );
    auto *addVectAct  = addMenu->addAction( "Vector map…" );
    addMenu->addSeparator();

    // Raster composites
    struct { const char *label; GrassAddLayerDialog::LayerType type; } items[] = {
        { "🎨  RGB Composite…",          GrassAddLayerDialog::LayerType::Rgb          },
        { "🌈  IHS Composite…",          GrassAddLayerDialog::LayerType::Ihs          },
        { "🏔   Shaded Relief…",          GrassAddLayerDialog::LayerType::Shade        },
        { "🔢  Cell Values…",             GrassAddLayerDialog::LayerType::RastNum      },
        { "➡  Flow / Aspect Arrows…",    GrassAddLayerDialog::LayerType::RastArrow    },
        { nullptr, {} },  // separator
        { "🗺  Thematic Choropleth…",     GrassAddLayerDialog::LayerType::VectThematic },
        { "📊  Feature Charts…",          GrassAddLayerDialog::LayerType::VectChart    },
        { "🏷  Attribute Labels…",        GrassAddLayerDialog::LayerType::Label        },
        { nullptr, {} },
        { "🌐  Great Circle…",            GrassAddLayerDialog::LayerType::Geodesic     },
        { "🧭  Rhumb Line…",              GrassAddLayerDialog::LayerType::Rhumbline    },
    };

    for ( const auto &item : items ) {
        if ( !item.label ) { addMenu->addSeparator(); continue; }
        const GrassAddLayerDialog::LayerType t = item.type;
        connect( addMenu->addAction( item.label ), &QAction::triggered,
                 this, [this, t] {
                     auto *dlg = new GrassAddLayerDialog( mCanvas, t, this );
                     if ( dlg->exec() != QDialog::Accepted ) { delete dlg; return; }
                     GrassMapLayer *layer = dlg->createLayer();
                     delete dlg;
                     if ( !layer ) return;
                     addCompositeLayer( layer );
                 } );
    }

    addLayBtn->setMenu( addMenu );
    hdr->addWidget( addLayBtn );
    lay->addLayout( hdr );

    // Layer tree (replaces old QListWidget)
    mTree = new GrassLayerTree( canvas, this );
    lay->addWidget( mTree, 1 );

    // Decoration panel
    mDecoPanel = new GrassDecorationPanel( canvas, this );
    lay->addWidget( mDecoPanel );

    // --- Connections ---

    connect( addGrpBtn, &QPushButton::clicked, this, [this] {
        mTree->layerModel()->addGroup( "Group" );
    } );

    // Quick raster / vector from the add-layer menu
    connect( addRastAct, &QAction::triggered, this, [this] {
        const QString ref = pickMapRef( this, "Add Raster", "raster" );
        if ( ref.isEmpty() ) return;
        const int at = ref.indexOf('@');
        addRasterLayer( at<0 ? ref : ref.left(at), at<0 ? "" : ref.mid(at+1) );
    } );
    connect( addVectAct, &QAction::triggered, this, [this] {
        const QString ref = pickMapRef( this, "Add Vector", "vector" );
        if ( ref.isEmpty() ) return;
        const int at = ref.indexOf('@');
        addVectorLayer( at<0 ? ref : ref.left(at), at<0 ? "" : ref.mid(at+1) );
    } );

    // Decoration changes
    connect( mDecoPanel, &GrassDecorationPanel::decorationsChanged,
             this, [this] {
                 mCanvas->setDecorations( mDecoPanel->activeDecorations() );
             } );

    // Top raster → update legend raster
    connect( mTree, &GrassLayerTree::topRasterChanged,
             this, [this]( const QString &ref ) {
                 mDecoPanel->setLegendRaster( ref );
                 // Update canvas decorations so legend shows correct raster
                 mCanvas->setDecorations( mDecoPanel->activeDecorations() );
             } );

    // Selection change → active layer for Identify
    connect( mTree, &GrassLayerTree::currentLayerChanged,
             mCanvas, &GrassMapCanvas::setActiveLayerName );

    // Visibility changes → canvas
    connect( mTree, &GrassLayerTree::layerVisibilityChanged,
             mCanvas, &GrassMapCanvas::setLayerVisible );

    // Opacity changes → canvas (canvas reads from mLayers directly)
    connect( mTree, &GrassLayerTree::layerOpacityChanged,
             mCanvas, [canvas]( const QString &, double ) {
                 canvas->scheduleRender();
             } );

    // Zoom to layer
    connect( mTree, &GrassLayerTree::requestZoomToLayer,
             this, [this]( const QString &name, const QString &, const QString & ) {
                 for ( GrassMapLayer *l : mCanvas->layers() )
                 {
                     if ( l->name() == name )
                     {
                         const GrassExtent e = l->extent();
                         if ( e.isValid() )
                             mCanvas->zoomToExtent( e );
                         else
                             mCanvas->zoomToFullExtent();
                         break;
                     }
                 }
             } );

    // Layer Properties dialog
    connect( mTree, &GrassLayerTree::requestLayerProperties,
             this, [this]( QStandardItem *item ) {
                 const QString type   = item->data( LayerRole::Type   ).toString();
                 const QString name   = item->data( LayerRole::Name   ).toString();
                 const QString mapset = item->data( LayerRole::Mapset ).toString();
                 QDialog *dlg = nullptr;
                 if ( type == "raster" )
                     dlg = new GrassRasterPropsDialog( name, mapset, mCanvas, this );
                 else if ( type == "vector" )
                     dlg = new GrassVectorPropsDialog( name, mapset, mCanvas, this );
                 if ( dlg ) { dlg->setAttribute( Qt::WA_DeleteOnClose ); dlg->show(); }
             } );

    // Attribute table
    connect( mTree, &GrassLayerTree::requestAttributeTable,
             this, [this]( const QString &name, const QString &mapset ) {
                 auto *dlg = new GrassAttribTable( name, mapset, 1, this );
                 dlg->setAttribute( Qt::WA_DeleteOnClose ); dlg->show();
             } );

    // Color table
    connect( mTree, &GrassLayerTree::requestColorTable,
             this, [this]( const QString &name, const QString &mapset ) {
                 auto *dlg = new GrassColorTableDialog( name, mapset, mCanvas, this );
                 dlg->setAttribute( Qt::WA_DeleteOnClose ); dlg->show();
             } );
}

// ---------------------------------------------------------------------------
void GrassLayerPanel::addRasterLayer( const QString &name,
                                       const QString &mapset )
{
    mTree->addRasterLayer( name, mapset );
}

void GrassLayerPanel::addVectorLayer( const QString &name,
                                       const QString &mapset )
{
    mTree->addVectorLayer( name, mapset );
}

void GrassLayerPanel::clear()
{
    mTree->clear();
}

void GrassLayerPanel::addCompositeLayer( GrassMapLayer *layer )
{
    if ( !layer ) return;
    // Insert at top of canvas stack (index 0 = bottom; insert before rasters
    // so it sits above the basemap but below user rasters by default — or just
    // append to the top)
    mCanvas->insertLayerAt( mCanvas->layers().size(), layer );
    mTree->addCompositeLayer( layer );
    mCanvas->scheduleRender();
}
