#include "grassvectoreditor.h"

#include <QUndoCommand>
#include <QLoggingCategory>

extern "C" {
#include <grass/gis.h>
#include <grass/vector.h>
#include <grass/vedit.h>

Q_LOGGING_CATEGORY(lcVectorEd, "qtgrass.vdigit.editor")
}

// -----------------------------------------------------------------------
// QUndoCommand subclasses for Add / Delete operations
// -----------------------------------------------------------------------
class AddLineCmd : public QUndoCommand
{
  public:
    AddLineCmd( Map_info *map, long long lineId, const QString &desc )
        : QUndoCommand( desc ), mMap( map ), mLineId( lineId )
    {}

    void undo() override
    {
        Vect_delete_line( mMap, static_cast<off_t>( mLineId ) );
    }

    void redo() override
    {
        // Redo is the initial forward action — already done; nothing here.
        // If needed this could re-apply the write but Vect_delete_line +
        // re-write would assign a new ID. For now undo only.
    }

  private:
    Map_info  *mMap    = nullptr;
    long long  mLineId = 0;
};

class DeleteLineCmd : public QUndoCommand
{
  public:
    DeleteLineCmd( Map_info *map, long long lineId,
                   line_pnts *pnts, line_cats *cats )
        : QUndoCommand( "Delete feature" )
        , mMap( map ), mLineId( lineId )
    {
        // Capture the geometry for undo
        mPntsCopy = Vect_new_line_struct();
        mCatsCopy = Vect_new_cats_struct();
        // (The caller already read the geometry before deleting)
        mType = Vect_read_line( mMap, mPntsCopy, mCatsCopy,
                                static_cast<int>( lineId ) );
        Q_UNUSED( pnts ); Q_UNUSED( cats );
    }

    ~DeleteLineCmd() override
    {
        if ( mPntsCopy ) Vect_destroy_line_struct( mPntsCopy );
        if ( mCatsCopy ) Vect_destroy_cats_struct( mCatsCopy );
    }

    void undo() override
    {
        // Re-add the deleted feature (gets a new internal ID)
        if ( mType > 0 && mPntsCopy )
            Vect_write_line( mMap, mType, mPntsCopy, mCatsCopy );
    }

    void redo() override
    {
        // Re-delete it — but after undo the line has a new ID, so just skip
        // (full redo tracking requires tracking updated offsets)
    }

  private:
    Map_info  *mMap      = nullptr;
    long long  mLineId   = 0;
    int        mType     = -1;
    line_pnts *mPntsCopy = nullptr;
    line_cats *mCatsCopy = nullptr;
};

// -----------------------------------------------------------------------
// GrassVectorEditor
// -----------------------------------------------------------------------
GrassVectorEditor::GrassVectorEditor( QObject *parent )
    : QObject( parent )
{
    mUndoStack = new QUndoStack( this );
}

GrassVectorEditor::~GrassVectorEditor()
{
    if ( mOpen ) close( false );
}

// -----------------------------------------------------------------------
// open / close
// -----------------------------------------------------------------------
bool GrassVectorEditor::open( const QString &name, const QString &mapset )
{
    if ( mOpen ) close();

    mMap = new Map_info();
    const QByteArray nb = name.toLocal8Bit();
    const QByteArray mb = mapset.toLocal8Bit();

    // Vect_open_update() calls G_fatal_error (→ abort) when the map does not
    // exist.  Check first with G_find_vector2(), and create the map via
    // Vect_open_new() if it is missing so the user can start a fresh digitise
    // session without pre-existing data.
    const char *msArg = mb.isEmpty() ? "" : mb.constData();
    const char *foundMs = G_find_vector2( nb.constData(), msArg );

    int openRet = -1;
    if ( foundMs )
    {
        // Existing map → open for update in the mapset where it was found
        openRet = Vect_open_update( mMap, nb.constData(), foundMs );
        if ( openRet < 0 )
        {
            mError = QString( "Cannot open '%1@%2' for editing." )
                     .arg( name ).arg( QString::fromUtf8( foundMs ) );
            qCWarning(lcVectorEd) << "GrassVectorEditor:" << mError;
            delete mMap; mMap = nullptr;
            return false;
        }
    }
    else
    {
        // Map does not exist → create it in the current (or requested) mapset.
        // Vect_open_new() opens the new map for writing; topology is built
        // in close() via Vect_build().
        openRet = Vect_open_new( mMap, nb.constData(), WITHOUT_Z );
        if ( openRet < 0 )
        {
            mError = QString( "Cannot create new vector map '%1'." ).arg( name );
            qCWarning(lcVectorEd) << "GrassVectorEditor:" << mError;
            delete mMap; mMap = nullptr;
            return false;
        }
        qCInfo(lcVectorEd) << "GrassVectorEditor: created new vector map" << name;
    }

    mPnts = Vect_new_line_struct();
    mCats = Vect_new_cats_struct();
    mName   = name;
    mMapset = mapset.isEmpty() ? QString::fromUtf8( G_mapset() ) : mapset;
    mOpen   = true;
    mUndoStack->clear();
    mNextCat = static_cast<int>( Vect_get_num_lines( mMap ) ) + 1;

    emit sessionOpened( name );
    qCInfo(lcVectorEd) << "GrassVectorEditor: opened" << name << "for editing";
    return true;
}

bool GrassVectorEditor::close( bool rebuildTopo )
{
    if ( !mOpen ) return true;

    if ( rebuildTopo )
        Vect_build( mMap );

    Vect_close( mMap );
    delete mMap; mMap = nullptr;

    if ( mPnts ) { Vect_destroy_line_struct( mPnts ); mPnts = nullptr; }
    if ( mCats ) { Vect_destroy_cats_struct( mCats ); mCats = nullptr; }

    mOpen = false;
    emit sessionClosed( mName );
    return true;
}

// -----------------------------------------------------------------------
// snapPoint
// -----------------------------------------------------------------------
bool GrassVectorEditor::snapPoint( double &x, double &y ) const
{
    if ( !mOpen || mSnapMode == 0 /* NO_SNAP */ ) return false;

    double z = 0.0;
    const int ret = Vedit_snap_point( mMap, -1, &x, &y, &z,
                                      mSnapThreshold, mSnapMode );
    return ret > 0;
}

// -----------------------------------------------------------------------
// writeAndRecord — common helper for all add operations
// -----------------------------------------------------------------------
long long GrassVectorEditor::writeAndRecord( int vtype,
                                              line_pnts *pnts,
                                              line_cats *cats )
{
    const off_t id = Vect_write_line( mMap, vtype, pnts, cats );
    if ( id < 0 ) return -1;

    // Track for undo
    const QString desc = ( vtype == GV_POINT    ) ? "Add point" :
                         ( vtype == GV_LINE      ) ? "Add line"  :
                         ( vtype == GV_BOUNDARY  ) ? "Add boundary" : "Add feature";
    mUndoStack->push( new AddLineCmd( mMap, static_cast<long long>( id ), desc ) );

    emit geometryChanged();
    return static_cast<long long>( id );
}

// -----------------------------------------------------------------------
// addPoint
// -----------------------------------------------------------------------
long long GrassVectorEditor::addPoint( double x, double y )
{
    if ( !mOpen ) return -1;

    // Snap if enabled
    snapPoint( x, y );

    Vect_reset_line( mPnts );
    Vect_append_point( mPnts, x, y, 0.0 );
    Vect_reset_cats( mCats );
    Vect_cat_set( mCats, mLayer, mNextCat++ );

    return writeAndRecord( GV_POINT, mPnts, mCats );
}

// -----------------------------------------------------------------------
// addLine
// -----------------------------------------------------------------------
long long GrassVectorEditor::addLine( const QVector<QPointF> &points )
{
    if ( !mOpen || points.size() < 2 ) return -1;

    Vect_reset_line( mPnts );
    for ( const QPointF &p : points )
    {
        double x = p.x(), y = p.y();
        // Snap only the first and last vertex
        if ( &p == &points.first() || &p == &points.last() )
            snapPoint( x, y );
        Vect_append_point( mPnts, x, y, 0.0 );
    }
    Vect_reset_cats( mCats );
    Vect_cat_set( mCats, mLayer, mNextCat++ );

    return writeAndRecord( GV_LINE, mPnts, mCats );
}

// -----------------------------------------------------------------------
// addArea — boundary + centroid
// -----------------------------------------------------------------------
long long GrassVectorEditor::addArea( const QVector<QPointF> &points )
{
    if ( !mOpen || points.size() < 3 ) return -1;

    Vect_reset_line( mPnts );
    for ( const QPointF &p : points )
        Vect_append_point( mPnts, p.x(), p.y(), 0.0 );

    // Close the boundary if not already closed
    if ( points.first() != points.last() )
        Vect_append_point( mPnts, points.first().x(), points.first().y(), 0.0 );

    Vect_reset_cats( mCats );
    Vect_cat_set( mCats, mLayer, mNextCat );

    const long long boundId = writeAndRecord( GV_BOUNDARY, mPnts, mCats );
    if ( boundId < 0 ) return -1;

    // Add centroid at the polygon centroid (simple bbox center for now)
    double cx = 0.0, cy = 0.0;
    for ( const QPointF &p : points ) { cx += p.x(); cy += p.y(); }
    cx /= points.size(); cy /= points.size();

    Vect_reset_line( mPnts );
    Vect_append_point( mPnts, cx, cy, 0.0 );
    Vect_reset_cats( mCats );
    Vect_cat_set( mCats, mLayer, mNextCat++ );

    writeAndRecord( GV_CENTROID, mPnts, mCats );
    return boundId;
}

// -----------------------------------------------------------------------
// deleteLine
// -----------------------------------------------------------------------
bool GrassVectorEditor::deleteLine( long long lineId )
{
    if ( !mOpen ) return false;

    // Read geometry first (for undo command)
    auto *cmd = new DeleteLineCmd( mMap, lineId, nullptr, nullptr );
    if ( Vect_delete_line( mMap, static_cast<off_t>( lineId ) ) < 0 )
    {
        delete cmd;
        mError = "Failed to delete line " + QString::number( lineId );
        return false;
    }
    mUndoStack->push( cmd );
    emit geometryChanged();
    return true;
}

// -----------------------------------------------------------------------
// moveVertex
// -----------------------------------------------------------------------
bool GrassVectorEditor::moveVertex( long long lineId,
                                     double x, double y,
                                     double dx, double dy,
                                     double selectRadius )
{
    if ( !mOpen ) return false;

    // Use Vedit_move_vertex
    struct ilist *list = Vect_new_list();
    Vect_list_append( list, static_cast<int>( lineId ) );

    struct Map_info *bgMaps[1] = { nullptr };
    line_pnts *movePnts = Vect_new_line_struct();
    Vect_append_point( movePnts, x, y, 0.0 );

    // Vedit_move_vertex(map, bgmaps, nbgmaps, list, pnts,
    //                   thresh, dx, dy, dz, snap_thresh, snap, move_first)
    const int ret = Vedit_move_vertex(
        mMap, bgMaps, 0, list, movePnts,
        selectRadius, dx, dy, 0.0,
        mSnapThreshold, 1 /*snap*/, 1 /*move_first*/ );

    Vect_destroy_line_struct( movePnts );
    Vect_destroy_list( list );

    if ( ret > 0 ) emit geometryChanged();
    return ret > 0;
}

// -----------------------------------------------------------------------
// splitLine
// -----------------------------------------------------------------------
bool GrassVectorEditor::splitLine( long long lineId,
                                    double x, double y )
{
    if ( !mOpen ) return false;

    struct ilist *list = Vect_new_list();
    Vect_list_append( list, static_cast<int>( lineId ) );

    Vect_reset_line( mPnts );
    Vect_append_point( mPnts, x, y, 0.0 );

    const int ret = Vedit_split_lines( mMap, list, mPnts,
                                        mSnapThreshold, nullptr );
    Vect_destroy_list( list );

    if ( ret > 0 )
    {
        mUndoStack->push( new QUndoCommand( "Split line" ) );
        emit geometryChanged();
    }
    return ret > 0;
}

// -----------------------------------------------------------------------
// nearestLine — proximity query using Vect_find_line
// -----------------------------------------------------------------------
long long GrassVectorEditor::nearestLine( double x, double y,
                                           double maxDist, int type ) const
{
    if ( !mOpen || !mMap ) return -1;

    // Vect_find_line searches the spatial index for the nearest feature.
    // type: bitmask of GV_POINT | GV_LINE | GV_BOUNDARY | GV_CENTROID etc.
    // Pass -1 (all bits set) to search every type.
    const int ftype = ( type < 0 )
        ? ( GV_POINT | GV_LINE | GV_BOUNDARY | GV_CENTROID | GV_FACE | GV_KERNEL )
        : type;

    const int id = Vect_find_line( mMap, x, y, 0.0, ftype,
                                   maxDist, 0 /*not 3D*/, 0 /*with_z*/ );
    return ( id > 0 ) ? static_cast<long long>( id ) : -1LL;
}
