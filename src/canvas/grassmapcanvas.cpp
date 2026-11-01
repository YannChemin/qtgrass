#include "grassmapcanvas.h"
#include "grassrastermaplayer.h"
#include "grassvectormaplayer.h"
#include "grassdisplaydriver.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QtConcurrent/QtConcurrent>
#include <QDebug>
#include <cmath>
#include <algorithm>

extern "C" {
#include <grass/gis.h>
#include <grass/raster.h>
}

GrassMapCanvas::GrassMapCanvas( QWidget *parent )
    : QWidget( parent )
{
    setMouseTracking( true );
    setAttribute( Qt::WA_OpaquePaintEvent );
    setMinimumSize( 200, 150 );
    setStyleSheet( "background: #1a1a1a;" );
    setCursor( Qt::OpenHandCursor );
    mCenter = { 0.0, 0.0 };

    // Debounce timer — coalesces rapid viewport changes (pan, zoom, resize)
    // before firing an async render.
    mDebounceTimer = new QTimer( this );
    mDebounceTimer->setSingleShot( true );
    mDebounceTimer->setInterval( 10 );
    connect( mDebounceTimer, &QTimer::timeout,
             this, &GrassMapCanvas::startAsyncRender );

    // FutureWatcher — notifies us when the background render is done.
    mFutureWatcher = new QFutureWatcher<QImage>( this );
    connect( mFutureWatcher, &QFutureWatcher<QImage>::finished,
             this, &GrassMapCanvas::onRenderFinished );

    // Display driver (for DisplayDriver mode)
    mDriver = new GrassDisplayDriver( this );
    connect( mDriver, &GrassDisplayDriver::renderComplete,
             this, &GrassMapCanvas::onDriverRenderComplete );
    connect( mDriver, &GrassDisplayDriver::renderError,
             this, [this]( const QString &msg ) {
                 mDriverBusy = false;
                 mBusyMsg    = "Render error: " + msg;
                 update();
                 emit renderingDone();
             } );
}

GrassMapCanvas::~GrassMapCanvas()
{
    // Block until any in-progress background render completes so that the
    // layer objects are not deleted while the render thread uses them.
    if ( mFutureWatcher->isRunning() )
        mFutureWatcher->waitForFinished();
}

// ---------------------------------------------------------------------------
// Tool mode
// ---------------------------------------------------------------------------
void GrassMapCanvas::setTool( MapTool t )
{
    mTool = t;
    switch ( t )
    {
        case MapTool::Pan:      setCursor( Qt::OpenHandCursor );  break;
        case MapTool::ZoomIn:   setCursor( Qt::ArrowCursor );     break;
        case MapTool::ZoomOut:  setCursor( Qt::ArrowCursor );     break;
        case MapTool::Identify: setCursor( Qt::CrossCursor );     break;
        case MapTool::Profile:
            setCursor( Qt::CrossCursor );
            mProfilePts.clear();
            mProfilePxPts.clear();
            break;
        case MapTool::Digitize:
            setCursor( Qt::CrossCursor );
            break;
    }
}

// ---------------------------------------------------------------------------
// Layer management
// ---------------------------------------------------------------------------
GrassRasterMapLayer *GrassMapCanvas::addRasterLayer( const QString &name,
                                                      const QString &mapset )
{
    auto *layer = new GrassRasterMapLayer( name, mapset );
    mLayers.append( layer );
    zoomToLayerIfNeeded( layer->extent() );
    return layer;
}

GrassVectorMapLayer *GrassMapCanvas::addVectorLayer( const QString &name,
                                                      const QString &mapset )
{
    auto *layer = new GrassVectorMapLayer( name, mapset );
    mLayers.append( layer );
    zoomToLayerIfNeeded( layer->extent() );
    return layer;
}

void GrassMapCanvas::zoomToLayerIfNeeded( const GrassExtent &layerExt )
{
    // Decide whether to zoom based on how visible the new layer is in the
    // current viewport, regardless of layer count.
    //
    // Zoom if:
    //   - viewport is invalid (not yet initialised)
    //   - layer extent is invalid (load failed — defer)
    //   - layer does not overlap the viewport at all
    //   - layer covers < 1% of the viewport area (e.g. a 15 km raster in a
    //     500 km NC state-wide view)
    const GrassExtent vp = viewportExtent();

    if ( !layerExt.isValid() )
    {
        // Extent not loaded yet — defer and retry
        QTimer::singleShot( 80, this, [this] { zoomToFullExtent(); } );
        return;
    }

    bool needZoom = !vp.isValid();

    if ( !needZoom )
    {
        const double olN = std::min( layerExt.north, vp.north );
        const double olS = std::max( layerExt.south, vp.south );
        const double olE = std::min( layerExt.east,  vp.east  );
        const double olW = std::max( layerExt.west,  vp.west  );

        if ( olN <= olS || olE <= olW )
        {
            needZoom = true;   // no overlap at all
        }
        else
        {
            // Overlap exists — zoom if layer is a tiny fraction of the viewport
            const double overlapFrac = ( (olN-olS) * (olE-olW) )
                                     / ( vp.height() * vp.width() );
            needZoom = ( overlapFrac < 0.01 );
        }
    }

    if ( needZoom )
    {
        zoomToFullExtent();
        QTimer::singleShot( 100, this, [this] { zoomToFullExtent(); } );
    }
    else
    {
        scheduleRender();
    }
}

void GrassMapCanvas::insertLayerAt( int index, GrassMapLayer *layer )
{
    if ( mFutureWatcher->isRunning() )
        mFutureWatcher->waitForFinished();
    const int pos = std::max( 0, std::min( index, static_cast<int>( mLayers.size() ) ) );
    mLayers.insert( pos, layer );
    scheduleRender();
}

void GrassMapCanvas::setLayerOrder( const QList<GrassMapLayer *> &orderedLayers )
{
    if ( orderedLayers.size() != mLayers.size() ) return;
    mLayers = orderedLayers;
}

void GrassMapCanvas::clearLayers()
{
    // Wait for any in-progress render so layer pointers remain valid.
    if ( mFutureWatcher->isRunning() )
        mFutureWatcher->waitForFinished();
    qDeleteAll( mLayers );
    mLayers.clear();
    scheduleRender();
}

void GrassMapCanvas::setLayerVisible( const QString &name, bool visible )
{
    for ( GrassMapLayer *l : mLayers )
        if ( l->name() == name ) { l->setVisible( visible ); scheduleRender(); return; }
}

// ---------------------------------------------------------------------------
// Render scheduling
// ---------------------------------------------------------------------------
void GrassMapCanvas::scheduleRender()
{
    if ( mRenderMode == RenderMode::DisplayDriver )
    {
        triggerDisplayDriverRender();
        return;
    }
    mDebounceTimer->start(); // restart the 10 ms one-shot timer
}

// ---------------------------------------------------------------------------
// Viewport control
// ---------------------------------------------------------------------------
void GrassMapCanvas::zoomToFullExtent()
{
    // Guard: if the canvas hasn't been laid out yet (width/height still 0),
    // defer until resizeEvent gives us real dimensions.  Computing
    // mPixelsPerUnit with W=1 or H=1 produces a near-zero scale that makes
    // the raster invisible until the user manually zooms.
    if ( width() <= 0 || height() <= 0 )
    {
        QTimer::singleShot( 50, this, [this] { zoomToFullExtent(); } );
        return;
    }

    // Determine whether the current GRASS session is geographic (degrees) or
    // projected (metres).  Use the cached region north value as a proxy: if
    // |north| ≤ 90 the session is likely geographic, otherwise projected.
    // This guards against mixing a global basemap (±180°/±85°) with projected
    // rasters (N > 100 000 m) in the combined extent, which would produce a
    // nonsensically huge viewport.
    const bool sessionIsGeo = mRegion.valid &&
                              std::abs( mRegion.north ) <= 90.0 &&
                              std::abs( mRegion.east  ) <= 360.0;

    GrassExtent combined;
    bool any = false;
    for ( const GrassMapLayer *l : mLayers )
    {
        if ( !l->visible() ) continue;
        const GrassExtent e = l->extent();
        if ( !e.isValid() ) continue;

        // Skip layers whose extent scale doesn't match the session projection.
        const bool extIsGeo = std::abs( e.north ) <= 90.0 &&
                              std::abs( e.west  ) <= 360.0;
        if ( extIsGeo != sessionIsGeo ) continue;

        if ( !any ) { combined = e; any = true; }
        else
        {
            combined.north = std::max( combined.north, e.north );
            combined.south = std::min( combined.south, e.south );
            combined.east  = std::max( combined.east,  e.east  );
            combined.west  = std::min( combined.west,  e.west  );
        }
    }
    if ( !any || !combined.isValid() ) { scheduleRender(); return; }
    const int W = width();
    const int H = height();
    mPixelsPerUnit = std::min( static_cast<double>(W) / ( combined.width()  * 1.05 ),
                               static_cast<double>(H) / ( combined.height() * 1.05 ) );
    mCenter = { ( combined.east + combined.west ) / 2.0,
                ( combined.north + combined.south ) / 2.0 };
    scheduleRender();
}

void GrassMapCanvas::zoomToExtent( const GrassExtent &ext )
{
    if ( !ext.isValid() ) return;
    const int W = std::max( width(),  1 );
    const int H = std::max( height(), 1 );
    mPixelsPerUnit = std::min( W / ( ext.width()  * 1.05 ),
                               H / ( ext.height() * 1.05 ) );
    mCenter = { ( ext.east + ext.west ) / 2.0,
                ( ext.north + ext.south ) / 2.0 };
    scheduleRender();
}

void GrassMapCanvas::zoomIn()
{
    zoomToward( QPointF( width() / 2.0, height() / 2.0 ), 2.0 );
}

void GrassMapCanvas::zoomOut()
{
    zoomToward( QPointF( width() / 2.0, height() / 2.0 ), 0.5 );
}

GrassExtent GrassMapCanvas::viewportExtent() const
{
    const int W = std::max( width(),  1 );
    const int H = std::max( height(), 1 );
    const double halfW = W / ( 2.0 * mPixelsPerUnit );
    const double halfH = H / ( 2.0 * mPixelsPerUnit );
    return { mCenter.y() + halfH, mCenter.y() - halfH,
             mCenter.x() + halfW, mCenter.x() - halfW };
}

// ---------------------------------------------------------------------------
// Coordinate conversion
// ---------------------------------------------------------------------------
QPointF GrassMapCanvas::pixelToMap( const QPointF &px ) const
{
    return { mCenter.x() + ( px.x() - width()  / 2.0 ) / mPixelsPerUnit,
             mCenter.y() - ( px.y() - height() / 2.0 ) / mPixelsPerUnit };
}

QPointF GrassMapCanvas::mapToPixel( const QPointF &map ) const
{
    return { width()  / 2.0 + ( map.x() - mCenter.x() ) * mPixelsPerUnit,
             height() / 2.0 - ( map.y() - mCenter.y() ) * mPixelsPerUnit };
}

void GrassMapCanvas::setDigitPoints( const QVector<QPointF> &geoPts )
{
    mDigitGeo = geoPts;
    update();
}

void GrassMapCanvas::zoomToward( const QPointF &mousePos, double factor )
{
    const QPointF before = pixelToMap( mousePos );
    mPixelsPerUnit *= factor;
    const QPointF after  = pixelToMap( mousePos );
    mCenter.rx() -= ( after.x() - before.x() );
    mCenter.ry() -= ( after.y() - before.y() );

    if ( mRenderMode == RenderMode::DisplayDriver )
        mDriver->cancel();

    // Show the old pixmap immediately (may be slightly stale), then re-render.
    update();
    scheduleRender();
}

// ---------------------------------------------------------------------------
// Identify
// ---------------------------------------------------------------------------
QString GrassMapCanvas::identifyRaster( GrassRasterMapLayer *layer,
                                         double east, double north ) const
{
    const GrassExtent e = layer->extent();
    if ( !e.isValid() || east < e.west || east > e.east ||
         north > e.north || north < e.south )
        return "outside extent";

    const QByteArray nameBytes   = layer->name().toLocal8Bit();
    const QByteArray mapsetBytes = layer->mapset().toLocal8Bit();
    const char *mapset = mapsetBytes.isEmpty() ? "" : mapsetBytes.constData();

    QByteArray mutableName = nameBytes;
    if ( !G_find_raster( mutableName.data(), mapset ) )
        return "map not found";

    struct Cell_head hd;
    Rast_get_cellhd( nameBytes.constData(), mapset, &hd );

    const int row = static_cast<int>( ( hd.north - north ) / hd.ns_res );
    const int col = static_cast<int>( ( east - hd.west   ) / hd.ew_res );
    if ( row < 0 || row >= hd.rows || col < 0 || col >= hd.cols )
        return "outside raster";

    const int fd = Rast_open_old( nameBytes.constData(), mapset );
    if ( fd < 0 ) return "cannot open";

    const RASTER_MAP_TYPE type = Rast_get_map_type( fd );
    void *buf = Rast_allocate_buf( type );

    Rast_get_row( fd, buf, row, type );
    Rast_close( fd );

    QString result;
    if ( Rast_is_null_value( reinterpret_cast<char *>( buf ) + col * Rast_cell_size( type ),
                              type ) )
    {
        result = "null";
    }
    else
    {
        switch ( type )
        {
            case CELL_TYPE:
            {
                const CELL v = reinterpret_cast<CELL *>( buf )[col];
                result = QString::number( v );
                break;
            }
            case FCELL_TYPE:
            {
                const FCELL v = reinterpret_cast<FCELL *>( buf )[col];
                result = QString::number( static_cast<double>( v ), 'g', 8 );
                break;
            }
            case DCELL_TYPE:
            {
                const DCELL v = reinterpret_cast<DCELL *>( buf )[col];
                result = QString::number( v, 'g', 12 );
                break;
            }
        }
    }

    G_free( buf );
    return result;
}

// ---------------------------------------------------------------------------
// Region overlay — uses a cached Cell_head so paintEvent never calls GRASS API
// ---------------------------------------------------------------------------
void GrassMapCanvas::refreshRegionCache()
{
    // Called from the main thread after a mapset change.
    // G_get_set_window calls G_set_window() which writes GRASS global state.
    // The render thread also calls G_set_window() per raster layer — wait for
    // any in-progress render to finish before touching the shared GRASS window.
    if ( mFutureWatcher->isRunning() )
        mFutureWatcher->waitForFinished();

    if ( !G_getenv_nofatal( "MAPSET" ) ) { mRegion.valid = false; return; }
    struct Cell_head w;
    G_get_set_window( &w );
    mRegion = { w.north, w.south, w.east, w.west,
                ( w.rows > 0 && w.cols > 0 ) };
}

void GrassMapCanvas::resetViewportToRegion()
{
    if ( !mRegion.valid ) return;
    const GrassExtent ext { mRegion.north, mRegion.south,
                            mRegion.east,  mRegion.west };
    if ( !ext.isValid() ) return;
    const int W = std::max( width(),  1 );
    const int H = std::max( height(), 1 );
    mPixelsPerUnit = std::min( W / ( ext.width()  * 1.05 ),
                               H / ( ext.height() * 1.05 ) );
    mCenter = { ( ext.east + ext.west  ) / 2.0,
                ( ext.north + ext.south ) / 2.0 };
}

void GrassMapCanvas::drawRegionOverlay( QPainter &p )
{
    if ( !mShowRegion || !mRegion.valid ) return;

    const QPointF nw = mapToPixel( { mRegion.west,  mRegion.north } );
    const QPointF se = mapToPixel( { mRegion.east,  mRegion.south } );

    const QRectF rect( nw, se );
    if ( rect.isEmpty() ) return;

    QPen pen( QColor( 255, 200, 0, 200 ), 2.0, Qt::DashLine );
    p.setPen( pen );
    p.setBrush( QColor( 255, 200, 0, 20 ) );
    p.drawRect( rect );

    p.setPen( QColor( 255, 200, 0, 220 ) );
    p.setFont( QFont( "sans-serif", 8 ) );
    p.drawText( nw + QPointF( 4, 14 ), "GRASS region" );
}

// ---------------------------------------------------------------------------
// Render mode + display driver
// ---------------------------------------------------------------------------
void GrassMapCanvas::setRenderMode( RenderMode m )
{
    mRenderMode = m;
    scheduleRender();
}

void GrassMapCanvas::setDecorations( const QList<GrassDisplayCmd> &cmds )
{
    mDecorations = cmds;
    if ( mRenderMode == RenderMode::DisplayDriver )
        triggerDisplayDriverRender();
    else
        scheduleRender();
}

QList<GrassDisplayCmd> GrassMapCanvas::buildDisplayCmds() const
{
    QList<GrassDisplayCmd> cmds;
    for ( GrassMapLayer *layer : mLayers )
    {
        if ( !layer->visible() ) continue;

        if ( auto *rl = dynamic_cast<GrassRasterMapLayer *>( layer ) )
        {
            const QString ref = rl->mapset().isEmpty()
                                ? rl->name()
                                : rl->name() + "@" + rl->mapset();
            cmds.append( GrassDisplayCmd::raster( ref ) );
        }
        else if ( auto *vl = dynamic_cast<GrassVectorMapLayer *>( layer ) )
        {
            const QString ref = vl->mapset().isEmpty()
                                ? vl->name()
                                : vl->name() + "@" + vl->mapset();
            cmds.append( GrassDisplayCmd::vector( ref,
                vl->style().lineColor, vl->style().fillColor,
                static_cast<int>( vl->style().lineWidth ) ) );
        }
    }
    cmds << mDecorations;
    return cmds;
}

void GrassMapCanvas::triggerDisplayDriverRender()
{
    if ( mLayers.isEmpty() ) { update(); return; }

    const QList<GrassDisplayCmd> cmds = buildDisplayCmds();
    if ( cmds.isEmpty() ) { update(); return; }

    mDriverBusy = true;
    mBusyMsg    = "Rendering via GRASS display driver…";
    emit renderingStarted();

    mDriver->startRender( cmds, width(), height() );
    update();
}

void GrassMapCanvas::onDriverRenderComplete( const QImage &img )
{
    mDriverBusy = false;
    if ( !img.isNull() )
        mPixmap = QPixmap::fromImage( img );
    update();
    emit renderingDone();
}

// ---------------------------------------------------------------------------
// Native decoration drawing for Direct render mode
// ---------------------------------------------------------------------------
static void drawDecorationsNative( QPainter &p, int W, int H,
                                    const GrassExtent &vp,
                                    const QList<GrassDisplayCmd> &decs )
{
    p.save();
    p.setRenderHint( QPainter::Antialiasing );

    const double vpEwRes = W > 0 ? vp.width()  / W : 1.0;

    for ( const GrassDisplayCmd &dc : decs )
    {
        if ( !dc.enabled ) continue;

        auto argVal = [&]( const QString &prefix, const QString &def = {} ) -> QString {
            for ( const QString &a : dc.args )
                if ( a.startsWith( prefix ) ) return a.mid( prefix.length() );
            return def;
        };

        // ── Scale bar ────────────────────────────────────────────────────────
        if ( dc.prog == "d.barscale" )
        {
            const QStringList at = argVal( "at=", "80,2" ).split( ',' );
            const int px = static_cast<int>( at.value(1,"2").toDouble()  / 100.0 * W );
            const int py = static_cast<int>( (1.0 - at.value(0,"80").toDouble()/100.0) * H );
            const QString style = argVal( "style=", "simple" );

            QColor fg( argVal("fgcolor=","#ffffff") ); if(!fg.isValid()) fg=Qt::white;
            QColor bg( argVal("bgcolor=","#000000") ); if(!bg.isValid()) bg=Qt::black;

            static const double niceD[] = { 1,2,5,10,20,50,100,200,500,
                1000,2000,5000,10000,20000,50000,100000,200000,500000,1000000 };
            double barDist = niceD[0];
            const double target = vpEwRes * W * 0.15;
            for ( double n : niceD ) if ( n <= target ) barDist = n;
            const int barPx = std::max( 20, static_cast<int>( barDist / vpEwRes ) );
            const QString lbl = barDist >= 1000
                ? QString::number( barDist/1000,'g',3 ) + " km"
                : QString::number( barDist,'g',3 ) + " m";

            p.setFont( QFont("sans-serif",8) );

            if ( style.contains("checker") )
            {
                // ── Checker: alternating bg/fg segments ───────────────────────
                const int nSeg = style.startsWith("full") ? 4 : 2;
                const int sw = barPx / nSeg;
                p.setPen( Qt::NoPen );
                for ( int i = 0; i < nSeg; ++i )
                    p.fillRect( px + i*sw, py, sw, 8, (i%2==0) ? bg : fg );
                p.setPen( QPen(fg,1) ); p.setBrush(Qt::NoBrush);
                p.drawRect( px, py, barPx, 8 );
                p.setPen( fg );
                p.drawText( px, py - 3, lbl );
            }
            else if ( style.contains("tick") )
            {
                // ── Tick styles ───────────────────────────────────────────────
                const int nDiv = 4;
                const int sw   = barPx / nDiv;
                const int tLen = style.contains("down") ? 6 : -6;
                p.setPen( QPen(bg,3) );
                p.drawLine( px, py, px+barPx, py );
                p.setPen( QPen(fg,2) );
                p.drawLine( px, py, px+barPx, py );
                for ( int i = 0; i <= nDiv; ++i )
                    p.drawLine( px+i*sw, py, px+i*sw, py + tLen );
                p.drawText( px, py - 3, lbl );
            }
            else  // "simple" (and remaining styles fall back to simple look)
            {
                // ── Simple: two-tone bar (bg left, fg right) ─────────────────
                p.setPen( Qt::NoPen );
                p.fillRect( px,         py, barPx/2, 6, bg );
                p.fillRect( px+barPx/2, py, barPx/2, 6, fg );
                p.setPen( QPen(fg,1) ); p.setBrush(Qt::NoBrush);
                p.drawRect( px, py, barPx, 6 );
                p.setPen( fg );
                p.drawText( px, py - 3, lbl );
            }
        }
        // ── North arrow ──────────────────────────────────────────────────────
        else if ( dc.prog == "d.northarrow" )
        {
            const QStringList at = argVal( "at=", "90,5" ).split( ',' );
            const int cx = static_cast<int>( at.value(1,"5").toDouble()  / 100.0 * W );
            const int cy = static_cast<int>( (1.0 - at.value(0,"90").toDouble()/100.0) * H );
            const int r  = 18;
            const QString style = argVal( "style=", "default (arrow + N)" );

            QColor fg( argVal("fgcolor=","#ffffff") ); if(!fg.isValid()) fg=Qt::white;
            QColor bg( argVal("bgcolor=","#000000") ); if(!bg.isValid()) bg=Qt::black;

            p.save();
            p.translate( cx, cy );

            if ( style.startsWith("compass") )
            {
                // ── 4-point compass rose with N/S/E/W labels ─────────────────
                const QPointF dirs[4] = { {0,-r},{r,0},{0,r},{-r,0} };
                const char *lbls[4] = { "N","E","S","W" };
                for ( int i = 0; i < 4; ++i )
                {
                    const QPointF d = dirs[i];
                    p.setPen( QPen(bg,3) ); p.drawLine(QPointF(0,0), d*0.7);
                    p.setPen( QPen(fg,1) ); p.drawLine(QPointF(0,0), d*0.7);
                    p.setPen( fg );
                    p.setFont( QFont("sans-serif",7,QFont::Bold) );
                    p.drawText( QPointF(d.x()*1.15-4, d.y()*1.15+4), lbls[i] );
                }
            }
            else if ( style.startsWith("pointed") )
            {
                // ── Solid filled arrowhead ────────────────────────────────────
                const QPointF pts[3] = { {0,-r}, {-7,4}, {7,4} };
                p.setPen( QPen(bg,1.5) );
                p.setBrush( fg );
                p.drawPolygon( pts, 3 );
                p.setFont( QFont("sans-serif",9,QFont::Bold) );
                p.setPen( fg );
                p.drawText( QPoint(-5, r+12), "N" );
            }
            else if ( style.startsWith("line") )
            {
                // ── Simple line + arrowhead, no N label ──────────────────────
                p.setPen( QPen(bg,4) ); p.drawLine( 0, r, 0, -r );
                p.setPen( QPen(fg,2) ); p.drawLine( 0, r, 0, -r );
                p.drawLine( 0, -r, -5, -r+9 );
                p.drawLine( 0, -r,  5, -r+9 );
            }
            else  // "default (arrow + N)"
            {
                // ── Arrow shaft + arrowhead + N label ────────────────────────
                p.setPen( QPen(bg,4) ); p.drawLine( 0, r, 0, -r );
                p.setPen( QPen(fg,2) );
                p.drawLine( 0, r, 0, -r );
                p.drawLine( 0, -r, -6, -r+10 );
                p.drawLine( 0, -r,  6, -r+10 );
                p.setFont( QFont("sans-serif",9,QFont::Bold) );
                p.setPen( bg ); p.drawText( QPoint(-4, -r-5), "N" );
                p.setPen( fg ); p.drawText( QPoint(-5, -r-6), "N" );
            }
            p.restore();
        }
        // ── Grid ─────────────────────────────────────────────────────────────
        else if ( dc.prog == "d.grid" )
        {
            // "auto" → nice 1/2/5×10ⁿ spacing giving ≤10 lines per axis
            double spacing = 0.0;
            const QString sizeStr = argVal( "size=", "auto" );
            if ( sizeStr == "auto" ) {
                const double span = std::max( vp.width(), vp.height() );
                if ( span > 0.0 ) {
                    const double raw  = span / 10.0;
                    const double exp  = std::floor( std::log10( raw ) );
                    const double base = std::pow( 10.0, exp );
                    for ( double m : { 1.0, 2.0, 5.0, 10.0 } )
                        if ( base * m >= raw ) { spacing = base * m; break; }
                    if ( spacing <= 0.0 ) spacing = base * 10.0;
                }
            } else {
                spacing = sizeStr.toDouble();
            }
            if ( spacing <= 0.0 ) spacing = 1000.0;
            QColor gc( argVal("color=","grey") );
            if ( !gc.isValid() ) gc = Qt::gray;
            gc.setAlpha( 160 );

            const bool labels = !dc.args.contains("-n");
            p.setPen( QPen(gc, 1, Qt::DashLine) );
            p.setFont( QFont("sans-serif",7) );
            p.setPen( gc );

            const double fNs = std::ceil( vp.south / spacing ) * spacing;
            for ( double ns = fNs; ns <= vp.north; ns += spacing )
            {
                const int y = static_cast<int>( (vp.north-ns) / vp.height() * H );
                p.drawLine( 0, y, W, y );
                if ( labels ) p.drawText(2, y-2, QString::number(ns,'f',0));
            }
            const double fEw = std::ceil( vp.west / spacing ) * spacing;
            for ( double ew = fEw; ew <= vp.east; ew += spacing )
            {
                const int x = static_cast<int>( (ew-vp.west) / vp.width() * W );
                p.drawLine( x, 0, x, H );
                if ( labels ) p.drawText(x+2, H-4, QString::number(ew,'f',0));
            }
        }
        // ── Legend ───────────────────────────────────────────────────────────
        else if ( dc.prog == "d.legend" )
        {
            const QString rasterRef = argVal( "raster=", "" );
            if ( rasterRef.isEmpty() ) continue;

            const QStringList at = argVal( "at=", "5,50,2,9" ).split( ',' );
            const int x0 = qRound( at.value(2,"2").toDouble()  / 100.0 * W );
            const int x1 = qRound( at.value(3,"9").toDouble()  / 100.0 * W );
            const int y0 = qRound( (1.0 - at.value(1,"50").toDouble()/100.0) * H );
            const int y1 = qRound( (1.0 - at.value(0,"5" ).toDouble()/100.0) * H );
            if ( x1 <= x0 + 2 || y1 <= y0 + 4 ) continue;

            // Font settings
            const QString fontFamily = argVal( "fontname=", "sans-serif" );
            const int     fontSize   = argVal( "fontsize=", "8" ).toInt();
            const bool    isBold     = dc.args.contains("bold");
            const bool    isItalic   = dc.args.contains("italic");
            QColor        fontColor( argVal("fontcolor=","#ffffff") );
            if ( !fontColor.isValid() ) fontColor = Qt::white;

            QFont legendFont( fontFamily, fontSize );
            legendFont.setBold( isBold );
            legendFont.setItalic( isItalic );

            const QStringList parts = rasterRef.split( '@' );
            const QByteArray  nameB   = parts.value(0).toLocal8Bit();
            const QByteArray  mapsetB = parts.value(1,"").toLocal8Bit();
            const char *cName   = nameB.constData();
            const char *cMapset = mapsetB.isEmpty() ? "" : mapsetB.constData();

            struct Colors colors;
            if ( Rast_read_colors( cName, cMapset, &colors ) < 0 ) continue;

            struct FPRange fpRange;
            Rast_init_fp_range( &fpRange );
            Rast_read_fp_range( cName, cMapset, &fpRange );
            DCELL dmin = 0.0, dmax = 1.0;
            Rast_get_fp_range_min_max( &fpRange, &dmin, &dmax );
            if ( dmin == dmax ) { dmax = dmin + 1; }

            const int bh = y1 - y0;
            const int bw = x1 - x0;
            for ( int row = 0; row < bh; ++row )
            {
                const double t = 1.0 - static_cast<double>(row) / bh;
                const DCELL  v = dmin + t * (dmax - dmin);
                int r, g, b;
                Rast_get_d_color( &v, &r, &g, &b, &colors );
                p.fillRect( x0, y0 + row, bw, 1, QColor(r, g, b) );
            }

            p.setPen( QPen(fontColor, 1) );
            p.setBrush( Qt::NoBrush );
            p.drawRect( x0, y0, bw, bh );

            p.setFont( legendFont );
            const int lx = x1 + 3;
            p.drawText( lx, y0 + fontSize + 2, QString::number(dmax,'g',4) );
            p.drawText( lx, y0 + bh/2 + fontSize/2, QString::number((dmin+dmax)/2,'g',4) );
            p.drawText( lx, y0 + bh,  QString::number(dmin,'g',4) );

            // Optional title above the bar
            const QString title = argVal("title=", "");
            if ( !title.isEmpty() )
            {
                QFont tf = legendFont; tf.setBold(true);
                p.setFont(tf);
                p.drawText( x0, y0 - 4, title );
            }

            Rast_free_colors( &colors );
        }
        // ── Text ─────────────────────────────────────────────────────────────
        else if ( dc.prog == "d.text" )
        {
            const QString txt = argVal( "text=", "" );
            if ( txt.isEmpty() ) continue;

            // Position: at=x%,y% — (0,0) = lower-left, (100,100) = upper-right
            const QStringList atPts = argVal( "at=", "50,97" ).split( ',' );
            const int px = qRound( atPts.value(0,"50").toDouble() / 100.0 * W );
            const int py = qRound( (1.0 - atPts.value(1,"97").toDouble() / 100.0) * H );

            const QString alignStr = argVal( "align=", "uc" );
            const double  sizePct  = argVal( "size=", "5" ).toDouble();
            const int     fontPx   = qMax( 6, qRound( sizePct / 100.0 * H ) );
            const QString fontFam  = argVal( "font=", "sans-serif" );
            const bool    bold     = dc.args.contains( "-b" );
            const double  rot      = argVal( "rotation=", "0" ).toDouble();

            QColor fg( argVal( "color=", "black" ) );
            if ( !fg.isValid() ) fg = Qt::black;
            const QString bgArg = argVal( "bgcolor=", "none" );
            const bool hasBg = ( bgArg != "none" );
            QColor bg( hasBg ? bgArg : "#000000" );
            if ( !bg.isValid() ) { bg = Qt::black; }

            QFont font( fontFam );
            font.setPixelSize( fontPx );
            font.setBold( bold );
            p.setFont( font );

            const QFontMetrics fm( font );
            const int tw = fm.horizontalAdvance( txt );
            const int th = fm.height();

            // Horizontal offset relative to anchor: l=0, c=-tw/2, r=-tw
            int dx = 0;
            if ( alignStr.size() > 1 ) {
                switch ( alignStr[1].toLatin1() ) {
                case 'c': dx = -tw / 2; break;
                case 'r': dx = -tw;     break;
                default:  break;
                }
            }
            // Vertical: Qt drawText(x,y) puts baseline at y.
            // 'l' lower → baseline = anchor         → dy = 0
            // 'c' center → centre of glyph = anchor → dy = fm.ascent() - th/2
            // 'u' upper  → top of glyph = anchor    → dy = fm.ascent()
            int dy = 0;
            switch ( alignStr.isEmpty() ? 'l' : alignStr[0].toLatin1() ) {
            case 'u': dy = fm.ascent();             break;
            case 'c': dy = fm.ascent() - th / 2;   break;
            default:  dy = 0;                       break;
            }

            p.save();
            p.translate( px, py );
            if ( rot != 0.0 ) p.rotate( -rot ); // GRASS CCW → Qt CW

            if ( hasBg && bg.isValid() ) {
                p.setPen( Qt::NoPen );
                p.setBrush( bg );
                p.drawRect( dx - 2, dy - fm.ascent() - 2, tw + 4, th + 4 );
            }
            p.setPen( fg );
            p.drawText( dx, dy, txt );
            p.restore();
        }
    }
    p.restore();
}

// ---------------------------------------------------------------------------
// Async render — Direct mode
// ---------------------------------------------------------------------------
void GrassMapCanvas::startAsyncRender()
{
    if ( mRenderMode != RenderMode::Direct ) return;

    // If a render is already running, just flag that another one is needed.
    if ( mFutureWatcher->isRunning() )
    {
        mRenderPending = true;
        return;
    }

    const int W = width();
    const int H = height();
    if ( W <= 0 || H <= 0 ) return;

    const GrassExtent vp = viewportExtent();

    // Capture everything the render lambda needs by value.
    // Raw layer pointers are safe: layers outlive the render (clearLayers waits).
    QList<GrassMapLayer *>   layers      = mLayers;
    const bool               isEmpty     = mLayers.isEmpty();
    const RegionCache        region      = mRegion;
    const bool               showRegion  = mShowRegion;
    const QList<GrassDisplayCmd> decos   = mDecorations;
    // mRenderCenter / mRenderPpu are updated in onRenderFinished.

    emit renderingStarted();

    QFuture<QImage> future = QtConcurrent::run( [=]() -> QImage
    {
        QImage canvas( W, H, QImage::Format_ARGB32_Premultiplied );
        canvas.fill( QColor( 26, 26, 26 ) );   // opaque dark background
        QPainter p( &canvas );

        // "No layers" placeholder and attribution text are drawn by paintEvent()
        // on the main thread — QFont/drawText are not safe in worker threads.
        if ( !isEmpty )
        {
            for ( GrassMapLayer *layer : layers )
            {
                if ( !layer->visible() ) continue;
                const QImage img = layer->render( W, H, vp );
                if ( img.isNull() ) continue;
                p.setOpacity( layer->opacity() );
                p.drawImage( 0, 0, img );
                p.setOpacity( 1.0 );
            }
        }

        // Region overlay — geometry only; label drawn by paintEvent (QFont unsafe here)
        if ( showRegion && region.valid )
        {
            auto vp2px = [&]( double mx, double my ) -> QPointF {
                return { ( mx - vp.west  ) / vp.width()  * W,
                         ( vp.north - my ) / vp.height() * H };
            };
            const QPointF nw = vp2px( region.west,  region.north );
            const QPointF se = vp2px( region.east,  region.south );
            const QRectF rect( nw, se );
            if ( !rect.isEmpty() )
            {
                p.setPen( QPen( QColor( 255, 200, 0, 200 ), 2.0, Qt::DashLine ) );
                p.setBrush( QColor( 255, 200, 0, 20 ) );
                p.drawRect( rect );
            }
        }

        // ── Decoration overlays — drawn while painter is still open ───────
        if ( !decos.isEmpty() )
            drawDecorationsNative( p, W, H, vp, decos );

        p.end();

        return canvas;   // QImage is thread-safe; QPixmap::fromImage() on main thread
    } );

    mFutureWatcher->setFuture( future );
}

void GrassMapCanvas::onRenderFinished()
{
    // QPixmap::fromImage() must run on the GUI thread — do it here, not in the lambda.
    mPixmap       = QPixmap::fromImage( mFutureWatcher->result() );
    mRenderCenter = mCenter;
    mRenderPpu    = mPixelsPerUnit;
    update();
    emit renderingDone();

    if ( mRenderPending )
    {
        mRenderPending = false;
        scheduleRender();  // fires another debounce → render cycle
    }
}

void GrassMapCanvas::drawBusyOverlay( QPainter &p )
{
    // Dim overlay + spinner text shown while driver is working
    p.fillRect( rect(), QColor( 0, 0, 0, 80 ) );
    p.setPen( QColor( 200, 200, 200 ) );
    p.setFont( QFont( "sans-serif", 11 ) );
    p.drawText( rect(), Qt::AlignCenter, mBusyMsg );
}

// ---------------------------------------------------------------------------
// Qt event handlers
// ---------------------------------------------------------------------------
void GrassMapCanvas::paintEvent( QPaintEvent * )
{
    QPainter p( this );

    if ( mPixmap.isNull() )
    {
        p.fillRect( rect(), QColor( 26, 26, 26 ) );
    }
    else
    {
        // During a pan-only drag (no zoom change) shift the last-rendered pixmap
        // so the UI stays live.  When zoom changed, just paint the old pixmap at
        // origin — the async render will fix it momentarily.
        if ( mRenderPpu > 0 && mPixelsPerUnit == mRenderPpu )
        {
            const double dx = -( mCenter.x() - mRenderCenter.x() ) * mRenderPpu;
            const double dy =  ( mCenter.y() - mRenderCenter.y() ) * mRenderPpu;
            p.drawPixmap( QPointF( dx, dy ), mPixmap );
            // Fill any strip exposed at the edges with the background colour.
            if ( dx > 0 ) p.fillRect( QRectF( 0, 0, dx, height() ), QColor( 26, 26, 26 ) );
            if ( dx < 0 ) p.fillRect( QRectF( width() + dx, 0, -dx, height() ), QColor( 26, 26, 26 ) );
            if ( dy > 0 ) p.fillRect( QRectF( 0, 0, width(), dy ), QColor( 26, 26, 26 ) );
            if ( dy < 0 ) p.fillRect( QRectF( 0, height() + dy, width(), -dy ), QColor( 26, 26, 26 ) );
        }
        else
        {
            p.drawPixmap( 0, 0, mPixmap );
        }
    }

    // "No layers" placeholder — drawn here (main thread) to avoid QFont in worker
    if ( mLayers.isEmpty() )
    {
        p.setPen( QColor( 80, 80, 80 ) );
        p.setFont( QFont( "sans-serif", 14 ) );
        p.drawText( rect(), Qt::AlignCenter,
            "No layers loaded.\n\nRun a module from the toolbox\n"
            "or add output layers after a run." );
    }

    // Busy spinner while an async render is running
    if ( mFutureWatcher->isRunning() )
    {
        p.fillRect( QRectF( width() - 110, 6, 104, 20 ), QColor( 0, 0, 0, 120 ) );
        p.setPen( QColor( 200, 200, 200 ) );
        p.setFont( QFont( "sans-serif", 9 ) );
        p.drawText( QRectF( width() - 110, 6, 104, 20 ),
                    Qt::AlignCenter, "Rendering…" );
    }

    // Region overlay label (text part — QFont not safe in worker thread)
    if ( mShowRegion && mRegion.valid && !mPixmap.isNull() )
    {
        const QPointF nw = mapToPixel( { mRegion.west, mRegion.north } );
        p.setPen( QColor( 255, 200, 0, 220 ) );
        p.setFont( QFont( "sans-serif", 8 ) );
        p.drawText( nw + QPointF( 4, 14 ), "GRASS region" );
    }

    // Overlay for display-driver busy state
    if ( mDriverBusy )
        drawBusyOverlay( p );

    // Digitize rubber-band overlay
    if ( mTool == MapTool::Digitize && !mDigitGeo.isEmpty() )
    {
        p.setRenderHint( QPainter::Antialiasing );

        // Convert geo → pixel for all committed vertices
        QVector<QPointF> pxPts;
        for ( const QPointF &g : mDigitGeo )
            pxPts.append( mapToPixel( g ) );

        // Add current cursor position
        const QPointF cursorPx = mapToPixel( mDigitCursorGeo );

        // Draw committed segments
        p.setPen( QPen( QColor( 50, 220, 50 ), 2 ) );
        for ( int i = 1; i < pxPts.size(); ++i )
            p.drawLine( pxPts[i-1], pxPts[i] );

        // Rubber band from last committed vertex to cursor
        if ( !pxPts.isEmpty() )
        {
            p.setPen( QPen( QColor( 50, 220, 50 ), 1.5, Qt::DashLine ) );
            p.drawLine( pxPts.last(), cursorPx );
        }

        // Vertex dots
        p.setBrush( QColor( 50, 220, 50 ) );
        p.setPen( Qt::NoPen );
        for ( const QPointF &px : pxPts )
            p.drawEllipse( px, 4.0, 4.0 );

        // Snap indicator circle at first/last vertex
        if ( mDigitHasSnap && !pxPts.isEmpty() )
        {
            p.setPen( QPen( QColor( 255, 255, 0 ), 2 ) );
            p.setBrush( Qt::NoBrush );
            p.drawEllipse( pxPts.last(), 8.0, 8.0 );
        }

        // Hint text
        p.setPen( QColor( 50, 220, 50 ) );
        p.setFont( QFont( "sans", 9 ) );
        p.drawText( 8, height() - 8,
                    QString( "%1 vertices — right-click to finish" )
                    .arg( pxPts.size() ) );
    }

    // Profile rubber-band overlay (drawn on top of cached pixmap)
    if ( mTool == MapTool::Profile && mProfilePxPts.size() >= 1 )
    {
        p.setRenderHint( QPainter::Antialiasing );
        QPen pen( QColor( 255, 80, 80 ), 2, Qt::DashLine );
        p.setPen( pen );
        for ( int i = 1; i < mProfilePxPts.size(); ++i )
            p.drawLine( mProfilePxPts[i-1], mProfilePxPts[i] );

        // Waypoint dots
        p.setBrush( QColor( 255, 80, 80 ) );
        p.setPen( Qt::NoPen );
        for ( const QPointF &pt : mProfilePxPts )
            p.drawEllipse( pt, 4.0, 4.0 );

        // Hint
        p.setPen( QColor( 255, 80, 80 ) );
        p.setFont( QFont( "sans", 9 ) );
        p.drawText( 8, height() - 8,
                    QString( "%1 point(s) — right-click to extract profile" )
                    .arg( mProfilePxPts.size() ) );
    }
}

void GrassMapCanvas::resizeEvent( QResizeEvent * )
{
    scheduleRender();
}

void GrassMapCanvas::mousePressEvent( QMouseEvent *e )
{
    const QPointF map = pixelToMap( e->position() );

    if ( e->button() == Qt::LeftButton )
    {
        switch ( mTool )
        {
            case MapTool::Pan:
                mPanning = true;
                mLastMouse = e->position();
                mCenterAtPanStart = mCenter;
                setCursor( Qt::ClosedHandCursor );
                break;

            case MapTool::ZoomIn:
                zoomToward( e->position(), 2.0 );
                break;

            case MapTool::ZoomOut:
                zoomToward( e->position(), 0.5 );
                break;

            case MapTool::Identify:
            {
                // Prefer the actively selected layer; fall back to topmost visible raster.
                GrassRasterMapLayer *target = nullptr;
                if ( !mActiveLayerName.isEmpty() ) {
                    for ( GrassMapLayer *l : mLayers ) {
                        if ( l->name() == mActiveLayerName && l->visible() ) {
                            target = dynamic_cast<GrassRasterMapLayer *>( l );
                            break;
                        }
                    }
                }
                if ( !target ) {
                    for ( int i = mLayers.size() - 1; i >= 0; --i ) {
                        GrassMapLayer *l = mLayers[i];
                        if ( !l->visible() ) continue;
                        target = dynamic_cast<GrassRasterMapLayer *>( l );
                        if ( target ) break;
                    }
                }
                if ( target ) {
                    const QString val = identifyRaster( target, map.x(), map.y() );
                    emit identifyResult( target->name(), map.x(), map.y(), val );
                    showIdentifyPopup( e->pos(), target->name() + " = " + val );
                }
                break;
            }

            case MapTool::Profile:
                mProfilePts.append( map );
                mProfilePxPts.append( e->position() );
                update();
                break;

            case MapTool::Digitize:
                emit digitizeClick( map.x(), map.y() );
                break;
        }
    }
}

void GrassMapCanvas::mouseMoveEvent( QMouseEvent *e )
{
    if ( mPanning )
    {
        const QPointF delta = e->position() - mLastMouse;
        mCenter = { mCenterAtPanStart.x() - delta.x() / mPixelsPerUnit,
                    mCenterAtPanStart.y() + delta.y() / mPixelsPerUnit };
        update();  // smooth shift of mPixmap — no re-render during drag
    }
    const QPointF map = pixelToMap( e->position() );
    emit coordsChanged( map.x(), map.y() );

    // Update digitize rubber-band cursor tracking
    if ( mTool == MapTool::Digitize && !mDigitGeo.isEmpty() )
    {
        mDigitCursorGeo = map;
        update();
    }
}

void GrassMapCanvas::mouseReleaseEvent( QMouseEvent *e )
{
    if ( e->button() == Qt::LeftButton && mPanning )
    {
        mPanning = false;
        setCursor( mTool == MapTool::Pan ? Qt::OpenHandCursor : cursor() );
        scheduleRender();  // re-render at final pan position
    }

    // Right-click in Digitize mode → finish line/area
    if ( e->button() == Qt::RightButton && mTool == MapTool::Digitize )
    {
        emit digitizeRightClick();
    }

    // Right-click in Profile mode fires the profile with collected waypoints
    if ( e->button() == Qt::RightButton && mTool == MapTool::Profile )
    {
        if ( mProfilePts.size() >= 2 )
        {
            // Find top visible raster
            for ( GrassMapLayer *l : mLayers )
            {
                if ( !l->visible() ) continue;
                auto *rl = dynamic_cast<GrassRasterMapLayer *>( l );
                if ( !rl ) continue;
                emit profileLineReady( rl->name(), rl->mapset(), mProfilePts );
                break;
            }
        }
        mProfilePts.clear();
        mProfilePxPts.clear();
        update();
    }
}

void GrassMapCanvas::wheelEvent( QWheelEvent *e )
{
    const double factor = ( e->angleDelta().y() > 0 ) ? 1.25 : 0.8;
    zoomToward( e->position(), factor );
    e->accept();
}

void GrassMapCanvas::showIdentifyPopup( const QPoint &pixelPos, const QString &text )
{
    if ( !mIdentifyPopup ) {
        mIdentifyPopup = new QLabel( this );
        mIdentifyPopup->setFrameShape( QFrame::StyledPanel );
        mIdentifyPopup->setStyleSheet(
            "QLabel { background:#222e; color:#fff; padding:4px 8px; "
            "border:1px solid #666; border-radius:4px; font-size:11px; }" );
        mIdentifyPopup->setAttribute( Qt::WA_TransparentForMouseEvents );
        mIdentifyTimer = new QTimer( this );
        mIdentifyTimer->setSingleShot( true );
        connect( mIdentifyTimer, &QTimer::timeout,
                 this, [this] { if(mIdentifyPopup) mIdentifyPopup->hide(); } );
    }

    mIdentifyPopup->setText( text );
    mIdentifyPopup->adjustSize();

    // Position popup just to the right-and-below the click, clamped to canvas
    QPoint pos = pixelPos + QPoint( 12, 12 );
    if ( pos.x() + mIdentifyPopup->width()  > width()  ) pos.rx() -= mIdentifyPopup->width()  + 24;
    if ( pos.y() + mIdentifyPopup->height() > height() ) pos.ry() -= mIdentifyPopup->height() + 24;
    mIdentifyPopup->move( pos );
    mIdentifyPopup->show();
    mIdentifyPopup->raise();

    mIdentifyTimer->start( 5000 );
}
