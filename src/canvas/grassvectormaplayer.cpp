#include "grassvectormaplayer.h"

#include <QPainter>
#include <QLoggingCategory>
#include <algorithm>
#include <cmath>
#include <csignal>
#include <csetjmp>

extern "C" {
#include <grass/gis.h>
#include <grass/vector.h>

Q_LOGGING_CATEGORY(lcVector, "qtgrass.canvas.vector")

// The crash-guard sigsetjmp blocks cause GCC to emit -Wclobbered for local
// const char* variables that span a setjmp boundary.  These are false positives:
// the QByteArray objects are fully constructed before sigsetjmp is called and
// longjmp cannot modify stack-allocated objects.
#pragma GCC diagnostic ignored "-Wclobbered"
}

// ── Crash guard for Vect_open_old ──────────────────────────────────────────
// Corrupt topology binary data can cause fread() to dereference a bad pointer
// deep inside dig_load_plus → dig_Rd_P_isle.  We wrap Vect_open_old in a
// sigsetjmp region so the process survives bad topo files and just skips the
// map with a warning (user should run: v.build <map>).
static sigjmp_buf          sVectJmpBuf;
static volatile sig_atomic_t sVectJmpActive = 0;

static void vectCrashGuard( int )
{
    if ( sVectJmpActive )
    {
        sVectJmpActive = 0;
        siglongjmp( sVectJmpBuf, 1 );
    }
    // Outside protected section — restore default and re-raise so we still
    // get a core dump for any unrelated crash.
    struct sigaction sa {};
    sa.sa_handler = SIG_DFL;
    sigaction( SIGSEGV, &sa, nullptr );
    raise( SIGSEGV );
}

GrassVectorMapLayer::GrassVectorMapLayer( const QString &name,
                                           const QString &mapset )
    : GrassMapLayer( name )
    , mMapset( mapset )
{}

// ---------------------------------------------------------------------------
// load — read all geometries into QPolygonF caches
// ---------------------------------------------------------------------------
bool GrassVectorMapLayer::load()
{
    mLoaded = true;

    const QByteArray nameBytes   = mName.toLocal8Bit();
    const QByteArray mapsetBytes = mMapset.toLocal8Bit();
    const char *ms = mapsetBytes.isEmpty() ? "" : mapsetBytes.constData();

    // Pre-check: G_find_vector2() returns NULL without calling G_fatal_error
    // when the map is absent, so we can handle missing maps gracefully before
    // Vect_open_old() has a chance to abort the process.
    QByteArray mutableName = nameBytes;
    if ( !G_find_vector2( mutableName.data(), ms ) )
    {
        mError = QString( "Vector map '%1' not found in mapset '%2'" )
                 .arg( mName, ms[0] ? ms : "(current)" );
        qCWarning(lcVector) << "GrassVectorMapLayer:" << mError;
        return false;
    }

    // Vect_open_old can SIGSEGV when the topo binary is internally corrupt
    // (e.g. dig_Rd_P_isle reads a bad pointer from corrupted isle data).
    // Catch it with sigsetjmp so we can skip the map and warn the user.
    struct Map_info map;
    {
        struct sigaction newSa {}, oldSa {};
        newSa.sa_handler = vectCrashGuard;
        sigemptyset( &newSa.sa_mask );
        newSa.sa_flags = 0;
        sigaction( SIGSEGV, &newSa, &oldSa );
        sVectJmpActive = 1;

        volatile bool openOk = false;
        if ( sigsetjmp( sVectJmpBuf, 1 ) == 0 )
        {
            openOk = ( Vect_open_old( &map, nameBytes.constData(), ms ) >= 0 );
        }
        else
        {
            // SIGSEGV inside Vect_open_old — topo file is corrupt
            mError = QString( "Corrupt topology for '%1' — run: v.build map=%1" )
                     .arg( mName );
            qCWarning(lcVector) << "GrassVectorMapLayer:" << mError;
        }

        sVectJmpActive = 0;
        sigaction( SIGSEGV, &oldSa, nullptr );

        if ( !openOk )
        {
            if ( mError.isEmpty() )
            {
                mError = QString( "Cannot open vector '%1'" ).arg( mName );
                qCWarning(lcVector) << "GrassVectorMapLayer:" << mError;
            }
            return false;
        }
    }

    // Bounding box
    struct bound_box bbox;
    Vect_get_map_box( &map, &bbox );
    mExtent = { bbox.N, bbox.S, bbox.E, bbox.W };

    struct line_pnts *pnts = Vect_new_line_struct();
    struct line_cats *cats = Vect_new_cats_struct();

    const int nlines = static_cast<int>( Vect_get_num_lines( &map ) );

    for ( int i = 1; i <= nlines; ++i )
    {
        const int type = Vect_read_line( &map, pnts, cats, i );
        if ( type < 0 ) continue;

        if ( type & GV_POINTS )    // GV_POINT | GV_CENTROID
        {
            for ( int k = 0; k < pnts->n_points; ++k )
                mPoints.append( { pnts->x[k], pnts->y[k] } );
        }
        else if ( type & GV_LINES )  // GV_LINE | GV_BOUNDARY
        {
            QPolygonF poly;
            poly.reserve( pnts->n_points );
            for ( int k = 0; k < pnts->n_points; ++k )
                poly.append( { pnts->x[k], pnts->y[k] } );
            mLines.append( poly );
        }
    }

    // Areas — iterate via area IDs
    const int nareas = static_cast<int>( Vect_get_num_areas( &map ) );
    for ( int a = 1; a <= nareas; ++a )
    {
        if ( Vect_get_area_points( &map, a, pnts ) < 0 ) continue;
        QPolygonF poly;
        poly.reserve( pnts->n_points );
        for ( int k = 0; k < pnts->n_points; ++k )
            poly.append( { pnts->x[k], pnts->y[k] } );
        if ( !poly.isEmpty() )
            mAreas.append( poly );
    }

    Vect_destroy_line_struct( pnts );
    Vect_destroy_cats_struct( cats );
    Vect_close( &map );

    qCInfo(lcVector) << "GrassVectorMapLayer:" << mName
            << "lines:" << mLines.size()
            << "areas:" << mAreas.size()
            << "points:" << mPoints.size();
    return true;
}

// ---------------------------------------------------------------------------
// render — project geo coords to viewport pixels and draw
// ---------------------------------------------------------------------------
QImage GrassVectorMapLayer::render( int viewW, int viewH,
                                     const GrassExtent &vp ) const
{
    if ( cacheHit( viewW, viewH, vp ) )
        return cachedImage();

    if ( !mLoaded )
        const_cast<GrassVectorMapLayer *>( this )->load();

    if ( mLines.isEmpty() && mAreas.isEmpty() && mPoints.isEmpty() )
        return {};

    if ( viewW <= 0 || viewH <= 0 || !vp.isValid() ) return {};

    // Helper: geographic (east, north) → pixel
    auto geo2px = [&]( double e, double n ) -> QPointF {
        return {
            ( e - vp.west  ) / vp.width()  * viewW,
            ( vp.north - n ) / vp.height() * viewH
        };
    };

    auto projectPoly = [&]( const QPolygonF &geo ) -> QPolygonF {
        QPolygonF px;
        px.reserve( geo.size() );
        for ( const QPointF &p : geo )
            px.append( geo2px( p.x(), p.y() ) );
        return px;
    };

    QImage img( viewW, viewH, QImage::Format_ARGB32_Premultiplied );
    img.fill( Qt::transparent );

    QPainter p( &img );
    p.setRenderHint( QPainter::Antialiasing );

    // ── Areas (filled polygons) ───────────────────────────────────────────────
    // drawFill  → fill interior;  drawLines → draw border
    if ( !mAreas.isEmpty() && ( mStyle.drawFill || mStyle.drawLines ) )
    {
        const QPen   border = mStyle.drawLines
            ? QPen( mStyle.lineColor, mStyle.lineWidth, mStyle.penStyle )
            : Qt::NoPen;
        const QBrush fill   = mStyle.drawFill
            ? QBrush( mStyle.fillColor, mStyle.brushStyle )
            : Qt::NoBrush;
        p.setPen( border );
        p.setBrush( fill );
        for ( const QPolygonF &geo : mAreas )
            p.drawPolygon( projectPoly( geo ) );
    }

    // ── Lines ────────────────────────────────────────────────────────────────
    if ( mStyle.drawLines && !mLines.isEmpty() )
    {
        QPen pen( mStyle.lineColor, mStyle.lineWidth, mStyle.penStyle );
        pen.setCapStyle( Qt::RoundCap );
        pen.setJoinStyle( Qt::RoundJoin );
        p.setPen( pen );
        p.setBrush( Qt::NoBrush );
        for ( const QPolygonF &geo : mLines )
            p.drawPolyline( projectPoly( geo ) );
    }

    // ── Points ───────────────────────────────────────────────────────────────
    if ( mStyle.drawPoints && !mPoints.isEmpty() )
    {
        const QColor &pc = mStyle.pointColor;
        const float   r  = mStyle.pointSize / 2.0f;

        auto drawShape = [&]( const QPointF &px )
        {
            p.save();
            p.translate( px );

            switch ( mStyle.pointShape )
            {
                case OsmPointShape::Circle:
                    p.setPen( Qt::NoPen );
                    p.setBrush( pc );
                    p.drawEllipse( QPointF(0,0), r, r );
                    break;

                case OsmPointShape::Square:
                    p.setPen( Qt::NoPen );
                    p.setBrush( pc );
                    p.drawRect( QRectF( -r, -r, 2*r, 2*r ) );
                    break;

                case OsmPointShape::Triangle:
                {
                    const QPointF pts[3] = { {0,-r}, {-r,r}, {r,r} };
                    p.setPen( Qt::NoPen );
                    p.setBrush( pc );
                    p.drawPolygon( pts, 3 );
                    break;
                }
                case OsmPointShape::Diamond:
                {
                    const QPointF pts[4] = { {0,-r}, {r,0}, {0,r}, {-r,0} };
                    p.setPen( Qt::NoPen );
                    p.setBrush( pc );
                    p.drawPolygon( pts, 4 );
                    break;
                }
                case OsmPointShape::Cross:
                {
                    const float w = r * 0.35f;
                    p.setPen( QPen( pc, w*2, Qt::SolidLine, Qt::RoundCap ) );
                    p.drawLine( QPointF(-r,0), QPointF(r,0) );
                    p.drawLine( QPointF(0,-r), QPointF(0,r) );
                    break;
                }
                case OsmPointShape::Star:
                {
                    const float ri = r * 0.45f;
                    QPolygonF star;
                    for ( int i = 0; i < 10; ++i )
                    {
                        const float angle = i * M_PI / 5 - M_PI/2;
                        const float rad   = ( i%2==0 ) ? r : ri;
                        star << QPointF( rad*std::cos(angle), rad*std::sin(angle) );
                    }
                    p.setPen( Qt::NoPen );
                    p.setBrush( pc );
                    p.drawPolygon( star );
                    break;
                }
                case OsmPointShape::MedicalCross:
                {
                    const float arm = r * 0.4f;
                    // White background circle
                    p.setPen( Qt::NoPen );
                    p.setBrush( Qt::white );
                    p.drawEllipse( QPointF(0,0), r, r );
                    // Coloured cross arms
                    p.setBrush( pc );
                    p.drawRect( QRectF(-arm, -r,  2*arm, 2*r) );
                    p.drawRect( QRectF(-r,  -arm, 2*r,  2*arm) );
                    break;
                }
                case OsmPointShape::ArrowUp:
                {
                    const QPointF pts[3] = { {0,-r}, {-r,r*0.5f}, {r,r*0.5f} };
                    p.setPen( Qt::NoPen );
                    p.setBrush( pc );
                    p.drawPolygon( pts, 3 );
                    p.fillRect( QRectF(-r*0.25f, r*0.2f, r*0.5f, r*0.6f), pc );
                    break;
                }
                case OsmPointShape::Pentagon:
                {
                    QPolygonF pent;
                    for ( int i = 0; i < 5; ++i )
                    {
                        const float angle = i * 2*M_PI/5 - M_PI/2;
                        pent << QPointF( r*std::cos(angle), r*std::sin(angle) );
                    }
                    p.setPen( Qt::NoPen );
                    p.setBrush( pc );
                    p.drawPolygon( pent );
                    break;
                }
                case OsmPointShape::Hexagon:
                {
                    QPolygonF hex;
                    for ( int i = 0; i < 6; ++i )
                    {
                        const float angle = i * M_PI/3;
                        hex << QPointF( r*std::cos(angle), r*std::sin(angle) );
                    }
                    p.setPen( Qt::NoPen );
                    p.setBrush( pc );
                    p.drawPolygon( hex );
                    break;
                }
            }

            // Thin contrasting outline on all solid shapes
            if ( mStyle.pointShape != OsmPointShape::Cross &&
                 mStyle.pointShape != OsmPointShape::MedicalCross )
            {
                p.setBrush( Qt::NoBrush );
                p.setPen( QPen( pc.darker(160), 0.8f ) );
                switch ( mStyle.pointShape )
                {
                    case OsmPointShape::Circle:
                        p.drawEllipse( QPointF(0,0), r, r ); break;
                    case OsmPointShape::Square:
                        p.drawRect( QRectF(-r,-r,2*r,2*r) ); break;
                    default: break;
                }
            }

            p.restore();
        };

        for ( const QPointF &gpt : mPoints )
            drawShape( geo2px( gpt.x(), gpt.y() ) );
    }

    p.end();
    storeCache( viewW, viewH, vp, img );
    return img;
}
