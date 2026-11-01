#pragma once

/*!
    \file grassvectoreditor.h
    \brief GRASS vector editing session manager with undo/redo support.
*/

#include <QObject>
#include <QPointF>
#include <QUndoStack>
#include <QString>
#include <QVector>

// Forward-declare GRASS C types without pulling in vector.h for all TUs
// (vector.h transitively requires ogr_api.h which may not be on the path).
struct Map_info;
struct line_pnts;
struct line_cats;

/*!
    \class GrassVectorEditor
    \brief Manages a GRASS vector editing session with undo/redo.

    Wraps the GRASS C vector edit API (\c Vect_open_update(), \c Vect_write_line(),
    \c Vect_delete_line(), \c Vedit_snap_point(), \c Vedit_split_lines()) and
    records each mutating operation as a QUndoCommand so edits can be undone.

    All coordinates are in GRASS map units (geographic coordinates).  The
    caller is responsible for converting viewport pixels to map coordinates
    before calling any geometry method.

    \section lifecycle Session lifecycle

    \code
    GrassVectorEditor ed;
    ed.open( "roads", "PERMANENT" );   // opens for update or creates new
    ed.addLine( points );              // write + push undo command
    ed.undoStack()->undo();            // revert last operation
    ed.close( true );                  // rebuild topology + close
    \endcode

    \sa GrassDigitToolbar
*/
class GrassVectorEditor : public QObject
{
    Q_OBJECT

  public:
    /*! \brief Constructs the editor (no map is open yet). */
    explicit GrassVectorEditor( QObject *parent = nullptr );
    ~GrassVectorEditor() override;

    // ── Session management ───────────────────────────────────────────────────

    /*!
        \brief Open \a name for editing.

        If the map exists, it is opened via \c Vect_open_update().  If it does
        not exist, a new empty map is created via \c Vect_open_new() so the
        user can start a fresh digitise session without pre-existing data.

        \param name   GRASS vector map name.
        \param mapset Owning mapset (empty = current mapset).
        \return \c true on success; check lastError() on failure.
    */
    bool open(  const QString &name, const QString &mapset = {} );

    /*!
        \brief Close the editing session.
        \param rebuildTopo If \c true, run \c Vect_build() before closing to
                           refresh the spatial index and topology.
        \return \c true on success.
    */
    bool close( bool rebuildTopo = true );

    /*! \brief \c true when a map is open for editing. */
    bool isOpen() const { return mOpen; }

    /*! \brief Name of the map currently open for editing. */
    QString mapName()   const { return mName; }

    /*! \brief Mapset of the open map. */
    QString mapset()    const { return mMapset; }

    /*! \brief Human-readable error from the last failed operation. */
    QString lastError() const { return mError; }

    // ── Snap settings ────────────────────────────────────────────────────────

    /*!
        \brief Set the snapping distance threshold in map units.
        \param thresh Maximum distance for a snap to occur.
    */
    void   setSnapThreshold( double thresh ) { mSnapThreshold = thresh; }

    /*! \brief Current snap threshold in map units. */
    double snapThreshold() const { return mSnapThreshold; }

    /*!
        \brief Set the snapping mode.
        \param mode \c 0 = no snap; \c 1 = snap to vertex; \c 2 = snap to node.
    */
    void setSnapMode( int mode ) { mSnapMode = mode; }

    /*! \brief Current snapping mode. */
    int  snapMode()    const { return mSnapMode; }

    /*!
        \brief Snap a point to the nearest vertex or node within the threshold.

        Modifies \a x and \a y in place.

        \param x Easting to snap (modified in place).
        \param y Northing to snap (modified in place).
        \return \c true if a snap occurred.
    */
    bool snapPoint( double &x, double &y ) const;

    // ── Feature operations ───────────────────────────────────────────────────

    /*!
        \brief Add a single point feature.
        \param x Easting in map units.
        \param y Northing in map units.
        \return GRASS line id of the new feature, or \c -1 on error.
    */
    long long addPoint( double x, double y );

    /*!
        \brief Add a line feature.
        \param points Ordered vertex list (at least 2 points required).
        \return GRASS line id of the new feature, or \c -1 on error.
    */
    long long addLine( const QVector<QPointF> &points );

    /*!
        \brief Add a closed area (boundary + centroid).

        Automatically closes the ring if the first and last points differ, and
        adds a centroid at the arithmetic mean of the vertices.

        \param points Ordered vertex list (at least 3 points required).
        \return GRASS line id of the boundary, or \c -1 on error.
    */
    long long addArea( const QVector<QPointF> &points );

    /*!
        \brief Delete a feature by its GRASS line id.
        \param lineId Feature id as returned by addLine() / nearestLine().
        \return \c true on success.
    */
    bool deleteLine( long long lineId );

    /*!
        \brief Move all vertices of \a lineId within \a selectRadius of (\a x, \a y) by (\a dx, \a dy).
        \param lineId       Target feature id.
        \param x            Easting of the selection centre.
        \param y            Northing of the selection centre.
        \param dx           Easting offset to apply.
        \param dy           Northing offset to apply.
        \param selectRadius Selection radius in map units.
        \return \c true if at least one vertex was moved.
    */
    bool moveVertex( long long lineId, double x, double y,
                     double dx, double dy, double selectRadius );

    /*!
        \brief Split \a lineId at the given point.
        \param lineId Target feature id.
        \param x      Easting of the split point.
        \param y      Northing of the split point.
        \return \c true if the split succeeded.
    */
    bool splitLine( long long lineId, double x, double y );

    /*!
        \brief Find the nearest vector feature within \a maxDist map units.

        Uses the GRASS spatial index (\c Vect_find_line()) for an efficient
        proximity query without iterating all features.

        \param x       Query easting.
        \param y       Query northing.
        \param maxDist Search radius in map units.
        \param type    Bitmask of \c GV_* constants to restrict the search
                       (e.g. \c GV_LINE|GV_BOUNDARY).  Pass \c -1 to search
                       all feature types.
        \return GRASS line id (≥ 1) of the nearest feature, or \c -1 if nothing
                was found within \a maxDist.
    */
    long long nearestLine( double x, double y,
                           double maxDist, int type = -1 ) const;

    // ── Undo / redo ──────────────────────────────────────────────────────────

    /*! \brief Returns the undo stack (connect to QAction::triggered for undo/redo buttons). */
    QUndoStack *undoStack() { return mUndoStack; }

    // ── Category management ───────────────────────────────────────────────────

    /*! \brief Next category number assigned to new features (auto-increments after each write). */
    int  nextCat() const { return mNextCat; }

    /*! \brief Override the next category number (useful when importing or merging). */
    void setNextCat( int c ) { mNextCat = c; }

    /*! \brief GRASS layer number used for new category assignments. */
    int  layer() const { return mLayer; }

    /*! \brief Set the GRASS layer number for new features. */
    void setLayer( int l ) { mLayer = l; }

  signals:
    /*! \brief Emitted after any operation that changes vector geometry. Connect to canvas scheduleRender(). */
    void geometryChanged();

    /*! \brief Emitted when an editing session is opened. \param name Map name. */
    void sessionOpened( const QString &name );

    /*! \brief Emitted when an editing session is closed. \param name Map name. */
    void sessionClosed( const QString &name );

  private:
    long long writeAndRecord( int vtype,
                               struct line_pnts *pnts,
                               struct line_cats *cats );

    bool        mOpen          = false;
    QString     mName;
    QString     mMapset;
    QString     mError;
    double      mSnapThreshold = 10.0;
    int         mSnapMode      = 0;
    int         mNextCat       = 1;
    int         mLayer         = 1;

    Map_info   *mMap       = nullptr;
    line_pnts  *mPnts      = nullptr;
    line_cats  *mCats      = nullptr;
    QUndoStack *mUndoStack = nullptr;
};
