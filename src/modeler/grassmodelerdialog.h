#pragma once

#include "grassmodel.h"

#include <QDialog>
#include <QGraphicsScene>
#include <QMap>

class QAction;
class QComboBox;
class QGraphicsLineItem;
class QGraphicsView;
class QLabel;
class QLineEdit;
class QProcess;
class QSplitter;
class QStackedWidget;
class QTableWidget;
class QTextEdit;
class ModelerNodeItem;
class ModelerActionItem;
class ModelerDataItem;
class ModelerArrowItem;
class GrassInterface;

// ── Scene — handles port-drag connection UX ───────────────────────────────────
class GrassModelerScene : public QGraphicsScene
{
    Q_OBJECT
public:
    explicit GrassModelerScene(QObject *parent = nullptr);

signals:
    void connectionRequested(int fromDataId, int toActionId);
    void nodeDoubleClicked  (int nodeId, bool isAction);
    void canvasRightClicked (QPointF scenePos);

protected:
    void mousePressEvent  (QGraphicsSceneMouseEvent *e) override;
    void mouseMoveEvent   (QGraphicsSceneMouseEvent *e) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override;
    void keyPressEvent    (QKeyEvent *e) override;

private:
    ModelerNodeItem   *mConnSrc  = nullptr;
    QGraphicsLineItem *mTempLine = nullptr;
};

// ── Main Graphical Modeler dialog ─────────────────────────────────────────────
class GrassModelerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GrassModelerDialog(GrassInterface *iface, QWidget *parent = nullptr);
    ~GrassModelerDialog() override;

    void loadFile(const QString &path);

private:
    void buildToolBar();
    void buildScene();
    void buildPropsPanel();

    void syncSceneFromModel();
    void addActionToScene(const ModelActionNode &node);
    void addDataToScene  (const ModelDataNode   &node);
    void addArrowToScene (const ModelRelation   &rel);

    ModelerActionItem *actionItemById(int id) const;
    ModelerDataItem   *dataItemById  (int id) const;

    void showActionProps(int nodeId);
    void showDataProps  (int nodeId);
    void applyActionProps();
    void applyDataProps();

    void runModel();
    void stopModel();
    void runNextAction();
    void log(const QString &line, bool isError = false);

    // — Data —
    GrassInterface    *mIface   = nullptr;
    GrassModel        *mModel   = nullptr;
    GrassModelerScene *mScene   = nullptr;
    QGraphicsView     *mView    = nullptr;

    QMap<int, ModelerActionItem *> mActionItems;
    QMap<int, ModelerDataItem   *> mDataItems;
    QMap<int, ModelerArrowItem  *> mArrowItems;

    // Properties panel widgets
    QStackedWidget *mPropsStack    = nullptr;  // 0=none 1=action 2=data
    QLineEdit      *mActLabelEdit  = nullptr;
    QLabel         *mActModLabel   = nullptr;
    QTableWidget   *mParamsTable   = nullptr;
    QLineEdit      *mDatLabelEdit  = nullptr;
    QComboBox      *mDatTypeCombo  = nullptr;
    QLineEdit      *mDatValueEdit  = nullptr;
    int             mSelId         = -1;
    bool            mSelIsAction   = false;

    // Log
    QTextEdit *mLog = nullptr;

    // File path
    QString mFilePath;

    // Execution engine
    QProcess    *mProcess    = nullptr;
    int          mExecIdx    = 0;
    QVector<int> mExecOrder;
    bool         mRunning    = false;
    QAction     *mRunAct     = nullptr;
    QAction     *mStopAct    = nullptr;
};
