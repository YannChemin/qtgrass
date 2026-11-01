#pragma once

#include <QToolBar>
#include <QVector>
#include <QPointF>

class QAction;
class QActionGroup;
class QDoubleSpinBox;
class QComboBox;
class GrassMapCanvas;
class GrassVectorEditor;
class GrassLayerPanel;

// -----------------------------------------------------------------------
// GrassDigitToolbar
//
// Floating toolbar for vector digitizing.
// Manages the edit session (open/close) and dispatches tool modes.
//
// Canvas integration:
//   • The toolbar sets MapTool::Digitize on the canvas when any tool is active
//   • Mouse events are received from the canvas via signals:
//       canvasClicked(east, north)  — left-click: add vertex / select feature
//       canvasRightClicked()        — right-click: finish current line
//   • The rubber-band is drawn by the canvas in its paint event overlay
//     (GrassMapCanvas stores mDigitPts and mDigitPxPts)
// -----------------------------------------------------------------------
class GrassDigitToolbar : public QToolBar
{
    Q_OBJECT

  public:
    explicit GrassDigitToolbar( GrassMapCanvas *canvas,
                                 GrassLayerPanel *panel,
                                 QWidget *parent = nullptr );
    ~GrassDigitToolbar() override;

    GrassVectorEditor *editor() const { return mEditor; }

  signals:
    void editingStarted( const QString &mapName );
    void editingStopped();

  private slots:
    void startEditing();
    void stopEditing();
    void onCanvasClick(  double east, double north );
    void onCanvasRightClick();
    void onMapChanged();  // re-render when geometry changes

  private:
    void commitCurrentLine();
    void clearCurrentLine();
    void updateStatusText();

    GrassMapCanvas  *mCanvas = nullptr;
    GrassLayerPanel *mPanel  = nullptr;
    GrassVectorEditor *mEditor = nullptr;

    QAction *mStartStopAct = nullptr;
    QAction *mUndoAct      = nullptr;
    QAction *mRedoAct      = nullptr;
    QActionGroup *mToolGroup = nullptr;

    QAction *mAddPointAct    = nullptr;
    QAction *mAddLineAct     = nullptr;
    QAction *mAddAreaAct     = nullptr;
    QAction *mDelAct         = nullptr;
    QAction *mSplitAct       = nullptr;
    QAction *mMoveVertexAct  = nullptr;

    QDoubleSpinBox *mSnapThreshSpin = nullptr;
    QComboBox      *mSnapModeCombo  = nullptr;

    // Current line being drawn (geographic coords)
    QVector<QPointF> mCurrentLine;
    QString          mActiveToolName;
};
