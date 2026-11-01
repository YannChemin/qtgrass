#include "grassdigittoolbar.h"

#include "grassvectoreditor.h"
#include "../canvas/grasslayerpanel.h"
#include "../canvas/grassmapcanvas.h"
#include "../layers/grasslayertree.h"

#include <QActionGroup>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLoggingCategory>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QUndoStack>
#include <QWidget>

extern "C" {
#include <grass/vector.h>
}

Q_LOGGING_CATEGORY(lcVdigit, "qtgrass.vdigit")

GrassDigitToolbar::GrassDigitToolbar( GrassMapCanvas *canvas,
                                       GrassLayerPanel *panel,
                                       QWidget *parent )
    : QToolBar( "Digitize", parent )
    , mCanvas( canvas )
    , mPanel( panel )
{
    setMovable( true );
    mEditor = new GrassVectorEditor( this );

    // --- Start / Stop editing ---
    mStartStopAct = addAction( "✏ Start Editing" );
    mStartStopAct->setToolTip( "Start / stop editing the selected vector layer" );
    addSeparator();

    // --- Tool group (exclusive) ---
    mToolGroup = new QActionGroup( this );
    mToolGroup->setExclusive( true );

    auto addTool = [&]( const QString &label, const QString &tip,
                         const QString &toolName ) -> QAction * {
        QAction *a = addAction( label );
        a->setToolTip( tip );
        a->setCheckable( true );
        a->setData( toolName );
        mToolGroup->addAction( a );
        return a;
    };

    mAddPointAct   = addTool( "• Point",     "Add point (left-click to place)",       "point"  );
    mAddLineAct    = addTool( "∕ Line",      "Add line (left-click vertices, right-click to finish)", "line"  );
    mAddAreaAct    = addTool( "□ Area",      "Add area (left-click vertices, right-click to close)",  "area"  );
    addSeparator();
    mDelAct        = addTool( "✕ Delete",    "Delete feature at click",               "delete"      );
    mSplitAct      = addTool( "✂ Split",     "Split line at click point",             "split"       );
    mMoveVertexAct = addTool( "↔ Vertex",    "Move nearest vertex (drag)",            "movevertex"  );

    mAddPointAct->setChecked( true );
    mActiveToolName = "point";

    addSeparator();

    // --- Undo / Redo ---
    mUndoAct = addAction( "↩ Undo" );
    mUndoAct->setToolTip( "Undo last action (Ctrl+Z)" );
    mUndoAct->setShortcut( QKeySequence::Undo );
    mRedoAct = addAction( "↪ Redo" );
    mRedoAct->setToolTip( "Redo (Ctrl+Y)" );
    mRedoAct->setShortcut( QKeySequence::Redo );
    addSeparator();

    // --- Snap settings ---
    addWidget( new QLabel( " Snap:", this ) );

    mSnapModeCombo = new QComboBox( this );
    mSnapModeCombo->addItem( "Off",    0 ); // NO_SNAP
    mSnapModeCombo->addItem( "Nodes",  1 ); // SNAP
    mSnapModeCombo->addItem( "+Verts", 2 ); // SNAPVERTEX
    mSnapModeCombo->setCurrentIndex( 1 );
    addWidget( mSnapModeCombo );

    mSnapThreshSpin = new QDoubleSpinBox( this );
    mSnapThreshSpin->setRange( 0.1, 100000 );
    mSnapThreshSpin->setValue( 10.0 );
    mSnapThreshSpin->setSuffix( " m" );
    mSnapThreshSpin->setToolTip( "Snap threshold in map units" );
    addWidget( mSnapThreshSpin );

    // Disable editing tools until a session starts
    mToolGroup->setEnabled( false );
    mUndoAct->setEnabled( false );
    mRedoAct->setEnabled( false );

    // --- Connections ---

    connect( mStartStopAct, &QAction::triggered, this, [this] {
        if ( mEditor->isOpen() ) stopEditing();
        else                     startEditing();
    } );

    connect( mToolGroup, &QActionGroup::triggered,
             this, [this]( QAction *a ) {
                 mActiveToolName = a->data().toString();
                 clearCurrentLine();
             } );

    connect( mUndoAct, &QAction::triggered,
             mEditor->undoStack(), &QUndoStack::undo );
    connect( mRedoAct, &QAction::triggered,
             mEditor->undoStack(), &QUndoStack::redo );

    connect( mEditor->undoStack(), &QUndoStack::canUndoChanged,
             mUndoAct, &QAction::setEnabled );
    connect( mEditor->undoStack(), &QUndoStack::canRedoChanged,
             mRedoAct, &QAction::setEnabled );

    // Snap settings → editor
    connect( mSnapThreshSpin, &QDoubleSpinBox::valueChanged,
             this, [this]( double v ) { mEditor->setSnapThreshold( v ); } );
    connect( mSnapModeCombo, &QComboBox::currentIndexChanged,
             this, [this]( int i ) {
                 mEditor->setSnapMode( mSnapModeCombo->itemData( i ).toInt() );
             } );
    mEditor->setSnapMode( 1 );

    // Canvas click signals for digitizing
    connect( mCanvas, &GrassMapCanvas::digitizeClick,
             this, &GrassDigitToolbar::onCanvasClick );
    connect( mCanvas, &GrassMapCanvas::digitizeRightClick,
             this, &GrassDigitToolbar::onCanvasRightClick );

    // Editor geometry changes → canvas re-render
    connect( mEditor, &GrassVectorEditor::geometryChanged,
             this, &GrassDigitToolbar::onMapChanged );
}

GrassDigitToolbar::~GrassDigitToolbar()
{
    if ( mEditor->isOpen() ) mEditor->close();
}

// -----------------------------------------------------------------------
// startEditing
// -----------------------------------------------------------------------
void GrassDigitToolbar::startEditing()
{
    // Get top visible vector layer from the layer tree
    QString name, mapset;
    if ( mPanel && mPanel->tree() )
    {
        const auto layers = mPanel->tree()->layerModel()->allLayers();
        for ( const QStandardItem *item : layers )
        {
            if ( item->data( LayerRole::Type ).toString() == "vector" &&
                 item->data( LayerRole::Visible ).toBool() )
            {
                name   = item->data( LayerRole::Name   ).toString();
                mapset = item->data( LayerRole::Mapset ).toString();
                break;
            }
        }
    }

    if ( name.isEmpty() )
    {
        bool ok;
        name = QInputDialog::getText( parentWidget(),
            "Start Editing", "Vector map name (name@mapset):", QLineEdit::Normal, {}, &ok );
        if ( !ok || name.isEmpty() ) return;
        const int at = name.indexOf( '@' );
        if ( at >= 0 ) { mapset = name.mid( at + 1 ); name = name.left( at ); }
    }

    if ( !mEditor->open( name, mapset ) )
    {
        QMessageBox::critical( parentWidget(), "Cannot edit",
                               mEditor->lastError() );
        return;
    }

    mStartStopAct->setText( "■ Stop Editing" );
    mToolGroup->setEnabled( true );
    mCanvas->setTool( MapTool::Digitize );
    emit editingStarted( name );
}

// -----------------------------------------------------------------------
// stopEditing
// -----------------------------------------------------------------------
void GrassDigitToolbar::stopEditing()
{
    clearCurrentLine();
    mEditor->close();

    mStartStopAct->setText( "✏ Start Editing" );
    mToolGroup->setEnabled( false );
    mUndoAct->setEnabled( false );
    mRedoAct->setEnabled( false );
    mCanvas->setTool( MapTool::Pan );
    onMapChanged();
    emit editingStopped();
}

// -----------------------------------------------------------------------
// onCanvasClick — dispatches to the active tool
// -----------------------------------------------------------------------
void GrassDigitToolbar::onCanvasClick( double east, double north )
{
    if ( !mEditor->isOpen() ) return;

    if ( mActiveToolName == "point" )
    {
        mEditor->addPoint( east, north );
        clearCurrentLine();
    }
    else if ( mActiveToolName == "line" || mActiveToolName == "area" )
    {
        // Snap the incoming point
        double x = east, y = north;
        mEditor->snapPoint( x, y );
        mCurrentLine.append( { x, y } );
        mCanvas->setDigitPoints( mCurrentLine );
        updateStatusText();
    }
    else if ( mActiveToolName == "delete" )
    {
        // Find the nearest feature within the current snap threshold and delete it.
        const long long id = mEditor->nearestLine( east, north,
                                                    mEditor->snapThreshold() );
        if ( id >= 0 )
            mEditor->deleteLine( id );
        else
            qCInfo(lcVdigit) << "GrassDigitToolbar: no feature within snap distance to delete";
    }
    else if ( mActiveToolName == "split" )
    {
        // Find the nearest line within the snap threshold and split it at (east, north).
        const long long id = mEditor->nearestLine( east, north,
                                                    mEditor->snapThreshold(),
                                                    GV_LINE | GV_BOUNDARY );
        if ( id >= 0 )
            mEditor->splitLine( id, east, north );
        else
            qCInfo(lcVdigit) << "GrassDigitToolbar: no line within snap distance to split";
    }
}

// -----------------------------------------------------------------------
// onCanvasRightClick — finish the current line/area
// -----------------------------------------------------------------------
void GrassDigitToolbar::onCanvasRightClick()
{
    if ( !mEditor->isOpen() ) return;
    commitCurrentLine();
}

void GrassDigitToolbar::commitCurrentLine()
{
    if ( mCurrentLine.isEmpty() ) return;

    if ( mActiveToolName == "line" && mCurrentLine.size() >= 2 )
        mEditor->addLine( mCurrentLine );
    else if ( mActiveToolName == "area" && mCurrentLine.size() >= 3 )
        mEditor->addArea( mCurrentLine );

    clearCurrentLine();
}

void GrassDigitToolbar::clearCurrentLine()
{
    mCurrentLine.clear();
    if ( mCanvas ) mCanvas->setDigitPoints( {} );
}

void GrassDigitToolbar::onMapChanged()
{
    // Reload the vector layer in the canvas so new geometry shows
    if ( mEditor->isOpen() )
    {
        const QString name   = mEditor->mapName();
        const QString mapset = mEditor->mapset();
        // Remove and re-add the layer to flush the cached geometry
        mCanvas->clearLayers();  // simple approach — re-add from panel
        mCanvas->scheduleRender();
    }
}

void GrassDigitToolbar::updateStatusText()
{
    // Could emit a signal to show in status bar; keep simple for now
}
