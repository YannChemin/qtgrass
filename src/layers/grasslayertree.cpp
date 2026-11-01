#include "grasslayertree.h"

#include "../canvas/grassmapcanvas.h"
#include "../canvas/grassrastermaplayer.h"
#include "../canvas/grassvectormaplayer.h"
#include "../grassenv.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLoggingCategory>
#include <QPushButton>
#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProcess>
#include <QStandardPaths>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcLayer, "qtgrass.layer")

// ============================================================
// GrassLayerDelegate
// ============================================================
GrassLayerDelegate::GrassLayerDelegate( QObject *parent )
    : QStyledItemDelegate( parent )
{}

QRect GrassLayerDelegate::eyeRect(    const QRect &r ) const { return { r.left(),              r.top(), kEyeW,  r.height() }; }
QRect GrassLayerDelegate::iconRect(   const QRect &r ) const { return { r.left() + kEyeW,      r.top(), kIconW, r.height() }; }
QRect GrassLayerDelegate::opacityRect(const QRect &r ) const { return { r.right() - kOpacW,   r.top(), kOpacW, r.height() }; }
QRect GrassLayerDelegate::nameRect(   const QRect &r ) const {
    return { r.left() + kEyeW + kIconW + kPad, r.top(),
             r.width() - kEyeW - kIconW - kPad - kOpacW - kPad, r.height() };
}

QSize GrassLayerDelegate::sizeHint( const QStyleOptionViewItem &opt,
                                     const QModelIndex &idx ) const
{
    QSize s = QStyledItemDelegate::sizeHint( opt, idx );
    s.setHeight( std::max( s.height(), 22 ) );
    return s;
}

void GrassLayerDelegate::paint( QPainter *p,
                                 const QStyleOptionViewItem &opt,
                                 const QModelIndex &idx ) const
{
    // Draw selection background
    QApplication::style()->drawControl( QStyle::CE_ItemViewItem, &opt, p );

    const QString type    = idx.data( LayerRole::Type ).toString();
    const bool    visible = idx.data( LayerRole::Visible ).toBool();
    const double  opacity = idx.data( LayerRole::Opacity ).toDouble();
    const QString name    = idx.data( Qt::DisplayRole ).toString();

    const QRect row = opt.rect;

    // --- Eye icon (visibility) ---
    {
        const QRect er = eyeRect( row );
        p->save();
        p->setFont( QFont( "monospace", 10 ) );
        p->setPen( visible ? QColor( "#ddd" ) : QColor( "#555" ) );
        p->drawText( er, Qt::AlignCenter, visible ? "👁" : "⊘" );
        p->restore();
    }

    // --- Type icon ---
    {
        const QRect ir = iconRect( row );
        p->save();
        p->setFont( QFont( "monospace", 9 ) );
        // Icon glyph and colour per layer type
        struct { const char *type; const char *glyph; const char *colour; } icons[] = {
            { "raster",    "▦", "#4a9" },
            { "vector",    "▸", "#59e" },
            { "group",     "▼", "#fa0" },
            { "rgb",       "🎨", "#e88" },
            { "ihs",       "🌈", "#e8e" },
            { "shade",     "🏔", "#a8c" },
            { "rastnum",   "🔢", "#8ae" },
            { "rastarrow", "➡", "#ea8" },
            { "thematic",  "🗺", "#8e8" },
            { "chart",     "📊", "#e8a" },
            { "labels",    "🏷", "#aae" },
            { "geodesic",  "🌐", "#e88" },
            { "rhumb",     "🧭", "#8ae" },
            { "composite", "◈", "#888" },
        };
        QString glyph = "◈"; QColor col( "#888" );
        for ( const auto &ic : icons ) {
            if ( type == ic.type ) { glyph = ic.glyph; col = QColor(ic.colour); break; }
        }
        p->setPen( col );
        p->drawText( ir, Qt::AlignCenter, glyph );
        p->restore();
    }

    // --- Name ---
    {
        p->save();
        const QRect nr = nameRect( row );
        QFont f = opt.font;
        if ( type == "group" ) { f.setBold( true ); }
        if ( !visible )        { f.setItalic( true ); }
        p->setFont( f );
        p->setPen( visible ? opt.palette.text().color()
                           : opt.palette.text().color().darker( 150 ) );
        p->drawText( nr, Qt::AlignVCenter | Qt::AlignLeft,
                     p->fontMetrics().elidedText( name, Qt::ElideRight, nr.width() ) );
        p->restore();
    }

    // --- Opacity % (only for map layers) ---
    if ( type != "group" )
    {
        p->save();
        const QRect or_ = opacityRect( row );
        p->setFont( QFont( "sans", 8 ) );
        p->setPen( QColor( "#888" ) );
        p->drawText( or_, Qt::AlignCenter,
                     QString( "%1%" ).arg( qRound( opacity * 100 ) ) );
        p->restore();
    }
}

bool GrassLayerDelegate::editorEvent( QEvent *event,
                                       QAbstractItemModel *model,
                                       const QStyleOptionViewItem &opt,
                                       const QModelIndex &idx )
{
    if ( event->type() != QEvent::MouseButtonPress ) return false;
    auto *me = static_cast<QMouseEvent *>( event );
    if ( me->button() != Qt::LeftButton ) return false;

    // Click on the eye icon → toggle visibility
    if ( eyeRect( opt.rect ).contains( me->pos() ) )
    {
        const bool vis = !idx.data( LayerRole::Visible ).toBool();
        model->setData( idx, vis, LayerRole::Visible );
        emit visibilityToggled( idx );
        return true;
    }
    return false;
}

// ============================================================
// GrassLayerTreeModel
// ============================================================
GrassLayerTreeModel::GrassLayerTreeModel( QObject *parent )
    : QStandardItemModel( parent )
{
    setHorizontalHeaderLabels( { "Layer" } );
}

QStandardItem *GrassLayerTreeModel::addLayer( const QString &name,
                                               const QString &mapset,
                                               const QString &type,
                                               QStandardItem *parentGroup )
{
    const QString display = mapset.isEmpty() ? name : name + "@" + mapset;

    auto *item = new QStandardItem( display );
    item->setEditable( false );
    item->setData( name,    LayerRole::Name   );
    item->setData( mapset,  LayerRole::Mapset );
    item->setData( type,    LayerRole::Type   );
    item->setData( 1.0,     LayerRole::Opacity );
    item->setData( true,    LayerRole::Visible );
    item->setDragEnabled( true );
    item->setDropEnabled( false );

    QStandardItem *parent = parentGroup ? parentGroup : invisibleRootItem();
    parent->insertRow( 0, item );  // top of list (newest on top)
    return item;
}

QStandardItem *GrassLayerTreeModel::addGroup( const QString &groupName )
{
    auto *group = new QStandardItem( groupName );
    group->setEditable( true );
    group->setData( groupName, LayerRole::Name   );
    group->setData( "group",   LayerRole::Type   );
    group->setData( true,      LayerRole::Visible );
    group->setDragEnabled( true );
    group->setDropEnabled( true );
    invisibleRootItem()->appendRow( group );
    return group;
}

void GrassLayerTreeModel::removeGroup( QStandardItem *group )
{
    if ( !group ) return;
    // Move all children to root before removing
    while ( group->rowCount() > 0 )
    {
        QList<QStandardItem *> row = group->takeRow( 0 );
        invisibleRootItem()->appendRow( row );
    }
    invisibleRootItem()->removeRow( group->row() );
}

QList<QStandardItem *> GrassLayerTreeModel::allLayers() const
{
    QList<QStandardItem *> result;
    collectLayers( invisibleRootItem(), result );
    return result;
}

void GrassLayerTreeModel::collectLayers( QStandardItem *parent,
                                          QList<QStandardItem *> &out ) const
{
    for ( int r = 0; r < parent->rowCount(); ++r )
    {
        auto *item = parent->child( r );
        if ( !item ) continue;
        const QString type = item->data( LayerRole::Type ).toString();
        if ( type == "group" )
            collectLayers( item, out );
        else
            out.append( item );
    }
}

Qt::DropActions GrassLayerTreeModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

Qt::ItemFlags GrassLayerTreeModel::flags( const QModelIndex &idx ) const
{
    Qt::ItemFlags f = QStandardItemModel::flags( idx );
    if ( !idx.isValid() ) return f | Qt::ItemIsDropEnabled;
    const QString type = idx.data( LayerRole::Type ).toString();
    f |= Qt::ItemIsDragEnabled;
    if ( type == "group" ) f |= Qt::ItemIsDropEnabled;
    return f;
}

// ============================================================
// GrassLayerTree
// ============================================================
GrassLayerTree::GrassLayerTree( GrassMapCanvas *canvas, QWidget *parent )
    : QTreeView( parent )
    , mCanvas( canvas )
{
    mModel    = new GrassLayerTreeModel( this );
    mDelegate = new GrassLayerDelegate( this );

    setModel( mModel );
    setItemDelegate( mDelegate );

    setHeaderHidden( true );
    setUniformRowHeights( true );
    setAnimated( true );
    setEditTriggers( QAbstractItemView::NoEditTriggers );
    setSelectionMode( QAbstractItemView::ExtendedSelection );
    setContextMenuPolicy( Qt::CustomContextMenu );

    // Drag-and-drop: reorder layers within the tree
    setDragEnabled( true );
    setAcceptDrops( true );
    setDragDropMode( QAbstractItemView::InternalMove );
    setDefaultDropAction( Qt::MoveAction );
    setDropIndicatorShown( true );

    connect( mDelegate, &GrassLayerDelegate::visibilityToggled,
             this, &GrassLayerTree::onVisibilityToggled );
    connect( mModel, &QStandardItemModel::itemChanged,
             this, &GrassLayerTree::onItemDataChanged );
    connect( this, &QTreeView::customContextMenuRequested,
             this, &GrassLayerTree::showContextMenu );
    connect( mModel, &QStandardItemModel::rowsMoved,
             this, &GrassLayerTree::syncCanvasOrder );

    // Track canvas cursor position for pixel-value queries
    connect( mCanvas, &GrassMapCanvas::coordsChanged,
             this, [this]( double e, double n ) { mLastE = e; mLastN = n; } );

    // Selection change → currentLayerChanged signal
    connect( selectionModel(), &QItemSelectionModel::currentChanged,
             this, [this]( const QModelIndex &cur, const QModelIndex & ) {
                 emit currentLayerChanged( cur.isValid()
                     ? cur.data( LayerRole::Name ).toString()
                     : QString{} );
             } );
}

// ---------------------------------------------------------------------------
// addRasterLayer / addVectorLayer
// ---------------------------------------------------------------------------
QStandardItem *GrassLayerTree::addRasterLayer( const QString &name,
                                                const QString &mapset )
{
    mCanvas->addRasterLayer( name, mapset );
    auto *item = mModel->addLayer( name, mapset, "raster" );
    setExpanded( mModel->indexFromItem( item->parent()
                 ? item->parent() : mModel->invisibleRootItem() ), true );
    scrollTo( mModel->indexFromItem( item ) );
    emit topRasterChanged( topRasterRef() );
    return item;
}

QStandardItem *GrassLayerTree::addVectorLayer( const QString &name,
                                                const QString &mapset )
{
    mCanvas->addVectorLayer( name, mapset );
    auto *item = mModel->addLayer( name, mapset, "vector" );
    scrollTo( mModel->indexFromItem( item ) );
    return item;
}

QStandardItem *GrassLayerTree::addCompositeLayer( GrassMapLayer *layer )
{
    // Determine type tag from the layer's display name prefix
    const QString n = layer->name();
    QString type = "composite";
    if      ( n.startsWith( "RGB:"      ) ) type = "rgb";
    else if ( n.startsWith( "IHS:"      ) ) type = "ihs";
    else if ( n.startsWith( "Shade:"    ) ) type = "shade";
    else if ( n.startsWith( "Values:"   ) ) type = "rastnum";
    else if ( n.startsWith( "Arrows:"   ) ) type = "rastarrow";
    else if ( n.startsWith( "Thematic:" ) ) type = "thematic";
    else if ( n.startsWith( "Chart:"    ) ) type = "chart";
    else if ( n.startsWith( "Labels:"   ) ) type = "labels";
    else if ( n.startsWith( "Geodesic"  ) ) type = "geodesic";
    else if ( n.startsWith( "Rhumb"     ) ) type = "rhumb";

    auto *item = mModel->addLayer( n, {}, type );
    scrollTo( mModel->indexFromItem( item ) );
    return item;
}

void GrassLayerTree::clear()
{
    mModel->removeRows( 0, mModel->rowCount() );
    mCanvas->clearLayers();
}

// ---------------------------------------------------------------------------
// currentLayerName
// ---------------------------------------------------------------------------
QString GrassLayerTree::currentLayerName() const
{
    const QModelIndex idx = currentIndex();
    if ( !idx.isValid() ) return {};
    return idx.data( LayerRole::Name ).toString();
}

// ---------------------------------------------------------------------------
// topRasterRef
// ---------------------------------------------------------------------------
QString GrassLayerTree::topRasterRef() const
{
    const auto layers = mModel->allLayers();
    for ( const QStandardItem *item : layers )
    {
        if ( item->data( LayerRole::Type ).toString() == "raster" &&
             item->data( LayerRole::Visible ).toBool() )
        {
            const QString n = item->data( LayerRole::Name   ).toString();
            const QString m = item->data( LayerRole::Mapset ).toString();
            return m.isEmpty() ? n : n + "@" + m;
        }
    }
    return {};
}

// ---------------------------------------------------------------------------
// syncCanvasOrder — re-stack canvas layers to match tree model order after
// a drag-drop reorder.  Tree row 0 = topmost visual layer; canvas mLayers[0]
// = bottom-most (rendered first), so canvas order is tree order reversed.
// ---------------------------------------------------------------------------
void GrassLayerTree::syncCanvasOrder()
{
    const auto items = mModel->allLayers();

    QList<GrassMapLayer *> newOrder;
    newOrder.reserve( items.size() );

    // Build canvas order bottom-to-top (reverse of tree top-to-bottom)
    for ( int i = items.size() - 1; i >= 0; --i )
    {
        const QString name = items[i]->data( LayerRole::Name ).toString();
        for ( GrassMapLayer *l : mCanvas->layers() )
        {
            if ( l->name() == name ) { newOrder << l; break; }
        }
    }

    if ( newOrder.size() == mCanvas->layers().size() )
    {
        mCanvas->setLayerOrder( newOrder );
        mCanvas->scheduleRender();
        emit topRasterChanged( topRasterRef() );
    }
}

// ---------------------------------------------------------------------------
// onVisibilityToggled — sync with canvas
// ---------------------------------------------------------------------------
void GrassLayerTree::onVisibilityToggled( const QModelIndex &idx )
{
    const QString name = idx.data( LayerRole::Name ).toString();
    const bool vis     = idx.data( LayerRole::Visible ).toBool();
    mCanvas->setLayerVisible( name, vis );
    emit layerVisibilityChanged( name, vis );
    update( idx );

    if ( idx.data( LayerRole::Type ).toString() == "raster" )
        emit topRasterChanged( topRasterRef() );
}

// ---------------------------------------------------------------------------
// onItemDataChanged — e.g. rename (editing a group name)
// ---------------------------------------------------------------------------
void GrassLayerTree::onItemDataChanged( QStandardItem *item )
{
    if ( item->data( LayerRole::Type ).toString() == "group" )
        item->setData( item->text(), LayerRole::Name );
}

// ---------------------------------------------------------------------------
// mouseDoubleClickEvent
// Double-click on a layer item opens Properties; on a group item starts rename.
// Clicks on the eye-icon column are ignored (delegate handles visibility toggle).
// ---------------------------------------------------------------------------
void GrassLayerTree::mouseDoubleClickEvent( QMouseEvent *e )
{
    const QModelIndex idx = indexAt( e->pos() );
    if ( !idx.isValid() ) { QTreeView::mouseDoubleClickEvent(e); return; }

    auto *item = mModel->itemFromIndex( idx );
    if ( !item ) { QTreeView::mouseDoubleClickEvent(e); return; }

    const QString type = item->data( LayerRole::Type ).toString();

    // Eye-icon column: let the delegate handle it, don't open properties.
    const QRect row = visualRect( idx );
    const QRect eye { row.left(), row.top(), 20, row.height() };
    if ( eye.contains( e->pos() ) ) return;

    if ( type == "group" )
    {
        // Rename group inline
        edit( idx );
    }
    else
    {
        emit requestLayerProperties( item );
    }
}

// ---------------------------------------------------------------------------
// showContextMenu
// ---------------------------------------------------------------------------
void GrassLayerTree::showContextMenu( const QPoint &pos )
{
    const QModelIndex idx = indexAt( pos );

    QMenu menu( this );

    if ( idx.isValid() )
    {
        auto *item = mModel->itemFromIndex( idx );
        buildContextMenu( item, menu );
    }
    else
    {
        // Clicked on blank area
        menu.addAction( "Add Group", this, [this] {
            bool ok;
            const QString name = QInputDialog::getText(
                this, "New Group", "Group name:", QLineEdit::Normal, "Group", &ok );
            if ( ok && !name.isEmpty() )
                mModel->addGroup( name );
        } );
    }

    if ( !menu.isEmpty() ) menu.exec( viewport()->mapToGlobal( pos ) );
}

void GrassLayerTree::buildContextMenu( QStandardItem *item, QMenu &menu )
{
    const QString type   = item->data( LayerRole::Type   ).toString();
    const QString name   = item->data( LayerRole::Name   ).toString();
    const QString mapset = item->data( LayerRole::Mapset ).toString();

    if ( type == "group" )
    {
        menu.addAction( "Rename Group", this, [item] {
            // Trigger inline edit
            item->setEditable( true );
        } );
        menu.addAction( "Ungroup (move layers to root)", this, [this, item] {
            mModel->removeGroup( item );
        } );
        return;
    }

    // ── GRASS access model: read-only check ───────────────────────────────────
    // A map is writeable only when it lives in the active mapset.  Maps from
    // PERMANENT (or any other mapset) shown alongside the active mapset are
    // read-only to enforce the location-wide isolation that GRASS depends on.
    const QString activeMapset =
        QString::fromLocal8Bit( qgetenv( "MAPSET" ) );
    const QString effectiveMapset =
        mapset.isEmpty() ? activeMapset : mapset;
    const bool readOnly = ( !activeMapset.isEmpty()
                            && effectiveMapset != activeMapset );

    const QString roSuffix = readOnly
        ? QString( "  (read-only — %1)" ).arg( effectiveMapset )
        : QString();
    auto roTooltip = [&]( const QString &action ) -> QString {
        if ( !readOnly ) return action;
        return QString( "%1 — disabled: map lives in %2 which is read-only "
                        "while %3 is the active mapset." )
                .arg( action, effectiveMapset, activeMapset );
    };

    // Layer-specific items — Zoom and Properties are read-only safe.
    menu.addAction( "Zoom to Layer", this, [this, name, mapset, type] {
        emit requestZoomToLayer( name, mapset, type );
    } );

    menu.addAction( "Layer Properties…", this, [this, item] {
        emit requestLayerProperties( item );
    } );

    if ( type == "raster" )
    {
        menu.addAction( "Query pixel value…", this,
                        [this, name, mapset]() {
            const QString mapRef = mapset.isEmpty() ? name : name + "@" + mapset;

            QDialog dlg( this );
            dlg.setWindowTitle( "Query pixel value — " + mapRef );
            auto *form  = new QFormLayout( &dlg );

            auto *eEdit = new QDoubleSpinBox( &dlg );
            eEdit->setRange( -1e12, 1e12 );
            eEdit->setDecimals( 6 );
            eEdit->setValue( mLastE );
            form->addRow( "Easting:", eEdit );

            auto *nEdit = new QDoubleSpinBox( &dlg );
            nEdit->setRange( -1e12, 1e12 );
            nEdit->setDecimals( 6 );
            nEdit->setValue( mLastN );
            form->addRow( "Northing:", nEdit );

            auto *outEdit = new QPlainTextEdit( &dlg );
            outEdit->setReadOnly( true );
            outEdit->setFont( QFont( "Monospace", 9 ) );
            outEdit->setMinimumHeight( 60 );
            form->addRow( "Value:", outEdit );

            auto *btnBox = new QDialogButtonBox( QDialogButtonBox::Close, &dlg );
            auto *queryBtn = new QPushButton( "Query", &dlg );
            btnBox->addButton( queryBtn, QDialogButtonBox::ActionRole );
            form->addRow( btnBox );

            auto runQ = [&]() {
                const QString gb = QString::fromLocal8Bit( qgetenv("GISBASE") );
                if ( gb.isEmpty() ) { outEdit->setPlainText("(GISBASE not set)"); return; }
                const QString coord = QString("%1,%2").arg(eEdit->value(), 0,'f',6)
                                                      .arg(nEdit->value(), 0,'f',6);
                QProcess p;
                p.setProcessChannelMode( QProcess::MergedChannels );
                p.setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );
                p.start( gb + "/bin/r.what",
                         { "map=" + mapRef, "coordinates=" + coord } );
                if ( !p.waitForFinished(10000) ) { p.kill(); outEdit->setPlainText("(timeout)"); return; }
                const QString out = QString::fromLocal8Bit( p.readAllStandardOutput() ).trimmed();
                outEdit->setPlainText( out.isEmpty() ? "(no output)" : out );
            };

            connect( queryBtn, &QPushButton::clicked, &dlg, runQ );
            connect( btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::accept );
            runQ();
            dlg.exec();
        } );

        QAction *ctAct = menu.addAction( "Color Table…" + roSuffix,
                                          this, [this, name, mapset] {
            emit requestColorTable( name, mapset );
        } );
        ctAct->setEnabled( !readOnly );
        ctAct->setToolTip( roTooltip( "Color Table" ) );

        QAction *ovAct = menu.addAction( "Build Overviews…" + roSuffix,
                                          this, [this, name, mapset] {
            buildRasterOverviews( name, mapset );
        } );
        ovAct->setEnabled( !readOnly );
        ovAct->setToolTip( roTooltip( "Build Overviews" ) );
    }

    if ( type == "vector" )
    {
        // Attribute table viewer is read-only safe (it just SELECTs).
        menu.addAction( "Attribute Table…", this, [this, name, mapset] {
            emit requestAttributeTable( name, mapset );
        } );

        QAction *rbAct = menu.addAction( "Rebuild Topology" + roSuffix,
                                          this, [this, name, mapset] {
            const QString ref = mapset.isEmpty() ? name : name + "@" + mapset;
            const QString gb  = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
            if ( gb.isEmpty() )
            {
                QMessageBox::warning( this, "Rebuild Topology",
                    "GISBASE is not set — cannot locate v.build." );
                return;
            }

            QProcess *proc = new QProcess( this );
            proc->setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );
            proc->setProgram( gb + "/bin/v.build" );
            proc->setArguments( { "map=" + ref, "option=build" } );

            connect( proc, &QProcess::finished,
                     this, [this, proc, name, ref]( int exitCode, QProcess::ExitStatus ) {
                const QString out = QString::fromLocal8Bit( proc->readAllStandardOutput() ).trimmed();
                const QString err = QString::fromLocal8Bit( proc->readAllStandardError()  ).trimmed();
                proc->deleteLater();

                if ( exitCode != 0 )
                {
                    QMessageBox::warning( this, "Rebuild Topology",
                        QString( "v.build failed for %1:\n%2" ).arg( ref, err.isEmpty() ? out : err ) );
                    return;
                }

                // Invalidate cached render and reload on next paint
                for ( GrassMapLayer *l : mCanvas->layers() )
                {
                    if ( l->name() == name )
                    {
                        auto *vl = dynamic_cast<GrassVectorMapLayer *>( l );
                        if ( vl ) { vl->reload(); l->invalidateCache(); }
                        break;
                    }
                }
                mCanvas->scheduleRender();
            } );

            proc->start();
            if ( !proc->waitForStarted( 3000 ) )
            {
                QMessageBox::warning( this, "Rebuild Topology",
                    "Could not start v.build — check GISBASE." );
                proc->deleteLater();
            }
        } );
        rbAct->setEnabled( !readOnly );
        rbAct->setToolTip( roTooltip( "Rebuild Topology" ) );
    }

    menu.addSeparator();

    // Opacity sub-menu
    menu.addAction( "Set Opacity…", this, [this, item] {
        bool ok;
        const int pct = QInputDialog::getInt(
            this, "Layer Opacity",
            "Opacity (0 = transparent, 100 = opaque):",
            qRound( item->data( LayerRole::Opacity ).toDouble() * 100 ),
            0, 100, 5, &ok );
        if ( !ok ) return;
        const double op = pct / 100.0;
        item->setData( op, LayerRole::Opacity );
        const QString n = item->data( LayerRole::Name ).toString();
        // Update canvas layer opacity (Direct mode)
        for ( GrassMapLayer *l : mCanvas->layers() )
            if ( l->name() == n ) { l->setOpacity( op ); break; }
        mCanvas->scheduleRender();
        emit layerOpacityChanged( n, op );
    } );

    menu.addAction( "Move to Group…", this, [this, item] {
        // Collect available groups
        QStringList groups;
        groups << "(root — no group)";
        for ( int r = 0; r < mModel->rowCount(); ++r )
        {
            auto *g = mModel->item( r );
            if ( g && g->data( LayerRole::Type ).toString() == "group" )
                groups << g->text();
        }
        bool ok;
        const QString chosen = QInputDialog::getItem(
            this, "Move to Group", "Group:", groups, 0, false, &ok );
        if ( !ok ) return;

        // Remove from current parent
        QStandardItem *parent = item->parent() ? item->parent()
                                                : mModel->invisibleRootItem();
        QList<QStandardItem *> row = parent->takeRow( item->row() );

        if ( chosen == "(root — no group)" )
        {
            mModel->invisibleRootItem()->appendRow( row );
        }
        else
        {
            for ( int r = 0; r < mModel->rowCount(); ++r )
            {
                auto *g = mModel->item( r );
                if ( g && g->text() == chosen )
                { g->appendRow( row ); break; }
            }
        }
    } );

    menu.addSeparator();
    menu.addAction( "Remove", this, [this, item] {
        const QString name = item->data( LayerRole::Name ).toString();
        const QString type = item->data( LayerRole::Type ).toString();
        mCanvas->setLayerVisible( name, false );
        QStandardItem *parent = item->parent() ? item->parent()
                                                : mModel->invisibleRootItem();
        parent->removeRow( item->row() );   // deletes item — don't touch it after this
        if ( type == "raster" )
            emit topRasterChanged( topRasterRef() );
    } );
}

// ---------------------------------------------------------------------------
// buildRasterOverviews — right-click action for raster layers.
//
// Creates cascaded reduced-resolution copies of the raster entirely within
// the GRASS storage structure (no GeoTIFF export):
//   L2 = r.resamp.stats input=MAP    output=MAP_L2   (2× coarser)
//   L4 = r.resamp.stats input=MAP_L2 output=MAP_L4   (cascade, cheap)
//   L8 = r.resamp.stats input=MAP_L4 output=MAP_L8   (cascade, very cheap)
//   …
// Total extra I/O ≈ 33% of the original raster (geometric series).
//
// Results are stored as native GRASS rasters in the same mapset.
// A marker file cell_misc/<name>/qtgrass_ovr lists the available levels.
// The renderer reads this marker and selects the finest qualifying level.
// ---------------------------------------------------------------------------
void GrassLayerTree::buildRasterOverviews( const QString &name,
                                            const QString &mapset )
{
    // --- Gather session info ---
    // The 'grass' startup script lives outside GISBASE (e.g. /usr/local/bin or
    // /usr/bin) — find it via PATH, not by constructing from GISBASE.
    const QString grassBin = QStandardPaths::findExecutable( "grass" );
    const QString gisdb    = QString::fromLocal8Bit( qgetenv( "GISDBASE" ) );
    const QString loc      = QString::fromLocal8Bit( qgetenv( "LOCATION_NAME" ) );
    const QString ms       = mapset.isEmpty()
                             ? QString::fromLocal8Bit( qgetenv( "MAPSET" ) )
                             : mapset;

    if ( grassBin.isEmpty() )
    {
        QMessageBox::warning( this, "Build Overviews",
            "Cannot find the 'grass' executable in PATH.\n"
            "Make sure GRASS GIS is installed and /usr/local/bin is in PATH." );
        return;
    }
    if ( gisdb.isEmpty() || loc.isEmpty() || ms.isEmpty() )
    {
        QMessageBox::warning( this, "Build Overviews",
            "No active GRASS session (GISDBASE/LOCATION_NAME/MAPSET not set).\n"
            "Open a mapset first." );
        return;
    }

    // --- Get native resolution from the layer ---
    double ewRes = 0, nsRes = 0;
    int    nRows = 0, nCols = 0;
    for ( GrassMapLayer *l : mCanvas->layers() )
    {
        if ( l->name() == name )
        {
            auto *rl = dynamic_cast<GrassRasterMapLayer *>( l );
            if ( rl ) { ewRes = rl->ewRes(); nsRes = rl->nsRes(); }
            break;
        }
    }
    if ( ewRes <= 0 || nsRes <= 0 )
    {
        QMessageBox::warning( this, "Build Overviews",
            QString( "Cannot determine resolution for '%1'." ).arg( name ) );
        return;
    }

    // --- Decide which levels to build ---
    // Build as many levels as make sense (output still > 64 cells wide/tall)
    const GrassExtent ext = [&]() -> GrassExtent {
        for ( GrassMapLayer *l : mCanvas->layers() )
            if ( l->name() == name ) return l->extent();
        return {};
    }();
    nCols = ext.isValid() ? static_cast<int>( ext.width()  / ewRes ) : 0;
    nRows = ext.isValid() ? static_cast<int>( ext.height() / nsRes ) : 0;

    QList<int> levels;
    for ( int L : { 2, 4, 8, 16, 32, 64 } )
        if ( nCols / L >= 64 && nRows / L >= 64 )
            levels << L;

    if ( levels.isEmpty() )
    {
        QMessageBox::information( this, "Build Overviews",
            "Raster is too small to benefit from overview levels." );
        return;
    }

    // --- Build the shell script (cascaded resample) ---
    // Each level reads from the PREVIOUS level (not the original), so total
    // I/O is O(1) × original size (geometric series: 1 + 1/4 + 1/16 + …).
    const QString ref = name + "@" + ms;
    QStringList cmds;
    cmds << "set -e";   // abort on first error

    QString prevMap = name + "@" + ms;
    QStringList levelLines;   // for the marker file

    for ( int L : levels )
    {
        const QString ovrName = name + "_L" + QString::number( L );
        const double  ovrEwRes = ewRes * L;
        const double  ovrNsRes = nsRes * L;

        // Set region to the source map's extent at the overview resolution
        cmds << QString( "g.region raster=%1 ewres=%2 nsres=%3 --quiet" )
                .arg( prevMap )
                .arg( ovrEwRes, 0, 'g', 10 )
                .arg( ovrNsRes, 0, 'g', 10 );

        cmds << QString( "r.resamp.stats input=%1 output=%2@%3 method=average --overwrite --quiet" )
                .arg( prevMap, ovrName, ms );

        prevMap     = ovrName + "@" + ms;
        levelLines << QString( "%1 %2" ).arg( L ).arg( ovrName );
    }

    // Restore region to original raster
    cmds << "g.region raster=" + ref + " --quiet";

    const QString script = cmds.join( "\n" );

    // --- Progress dialog ---
    QDialog dlg( this );
    dlg.setWindowTitle( QString( "Building overviews for %1" ).arg( name ) );
    dlg.resize( 600, 400 );

    auto *vbox   = new QVBoxLayout( &dlg );
    auto *status = new QLabel( "Starting…", &dlg );
    auto *log    = new QPlainTextEdit( &dlg );
    log->setReadOnly( true );
    log->setFont( QFont( "monospace", 9 ) );
    auto *btns = new QDialogButtonBox( QDialogButtonBox::Cancel, &dlg );
    vbox->addWidget( status );
    vbox->addWidget( log, 1 );
    vbox->addWidget( btns );

    QProcess *proc = new QProcess( &dlg );
    proc->setProcessChannelMode( QProcess::MergedChannels );
    proc->setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );

    connect( proc, &QProcess::readyRead, &dlg, [proc, log] {
        log->appendPlainText( QString::fromLocal8Bit(
            proc->readAll() ).trimmed() );
    } );

    bool succeeded = false;
    connect( proc,
             QOverload<int, QProcess::ExitStatus>::of( &QProcess::finished ),
             &dlg, [&]( int code, QProcess::ExitStatus ) {
        succeeded = ( code == 0 );
        status->setText( succeeded ? "Done." : "Failed — see log above." );
        btns->setStandardButtons( QDialogButtonBox::Ok );
    } );

    connect( btns, &QDialogButtonBox::rejected, &dlg, [&] {
        proc->kill();
        dlg.reject();
    } );
    connect( btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );

    // Run: grass /gisdb/loc/ms --exec bash -c "script"
    const QString mapsetPath = gisdb + "/" + loc + "/" + ms;
    proc->start( grassBin, { mapsetPath, "--exec", "bash", "-c", script } );

    if ( !proc->waitForStarted( 8000 ) )
    {
        QMessageBox::warning( this, "Build Overviews",
            QString( "Could not start '%1'.\n"
                     "Session: %2" ).arg( grassBin, mapsetPath ) );
        return;
    }

    status->setText( QString( "Building %1 overview levels…" ).arg( levels.size() ) );
    dlg.exec();

    if ( !succeeded ) return;

    // --- Write the marker file ---
    const QString miscDir = gisdb + "/" + loc + "/" + ms +
                            "/cell_misc/" + name;
    QDir().mkpath( miscDir );
    QFile marker( miscDir + "/qtgrass_ovr" );
    if ( marker.open( QIODevice::WriteOnly | QIODevice::Text ) )
    {
        QTextStream ts( &marker );
        ts << "# qtgrass overview levels for " << name << "@" << ms << "\n";
        for ( const QString &line : std::as_const( levelLines ) )
            ts << line << "\n";
        marker.close();
        qCInfo(lcLayer) << "GrassLayerTree: wrote" << marker.fileName();
    }

    // --- Invalidate layer cache and reload overview info ---
    for ( GrassMapLayer *l : mCanvas->layers() )
    {
        if ( l->name() == name )
        {
            auto *rl = dynamic_cast<GrassRasterMapLayer *>( l );
            if ( rl ) { rl->reloadOverviews(); rl->invalidateCache(); }
            break;
        }
    }
    mCanvas->scheduleRender();

    QMessageBox::information( this, "Build Overviews",
        QString( "Built %1 overview levels for '%2'.\n"
                 "Zoomed-out views will now load significantly faster." )
        .arg( levels.size() ).arg( name ) );
}
