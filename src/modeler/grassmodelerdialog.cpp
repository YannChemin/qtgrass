#include "grassmodelerdialog.h"
#include "grassmodeleritems.h"
#include "../grassinterface.h"

#include <QAction>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QGraphicsLineItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>

// ── GrassModelerScene ────────────────────────────────────────────────────────

GrassModelerScene::GrassModelerScene(QObject *parent)
    : QGraphicsScene(parent)
{
    setSceneRect(-2000, -2000, 4000, 4000);
}

void GrassModelerScene::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
    {
        // Check if click is near any node's output port
        for (QGraphicsItem *item : items(e->scenePos()))
        {
            auto *node = dynamic_cast<ModelerNodeItem *>(item);
            if (node && node->nearOutputPort(e->scenePos()))
            {
                mConnSrc  = node;
                mTempLine = addLine(QLineF(e->scenePos(), e->scenePos()),
                                    QPen(QColor(80, 80, 200), 1.5, Qt::DashLine));
                e->accept();
                return;
            }
        }
    }
    QGraphicsScene::mousePressEvent(e);
}

void GrassModelerScene::mouseMoveEvent(QGraphicsSceneMouseEvent *e)
{
    if (mTempLine)
    {
        QLineF ln = mTempLine->line();
        ln.setP2(e->scenePos());
        mTempLine->setLine(ln);
        e->accept();
        return;
    }
    QGraphicsScene::mouseMoveEvent(e);
}

void GrassModelerScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *e)
{
    if (mTempLine && mConnSrc && e->button() == Qt::LeftButton)
    {
        removeItem(mTempLine);
        delete mTempLine;
        mTempLine = nullptr;

        // Find an action item under the release point
        for (QGraphicsItem *item : items(e->scenePos()))
        {
            auto *dst = dynamic_cast<ModelerActionItem *>(item);
            if (dst && dst != mConnSrc)
            {
                emit connectionRequested(mConnSrc->nodeId(), dst->nodeId());
                break;
            }
        }
        mConnSrc = nullptr;
        e->accept();
        return;
    }
    mTempLine = nullptr;
    mConnSrc  = nullptr;
    QGraphicsScene::mouseReleaseEvent(e);
}

void GrassModelerScene::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace)
    {
        // Trigger delete on all selected nodes
        for (QGraphicsItem *item : selectedItems())
        {
            auto *node = dynamic_cast<ModelerNodeItem *>(item);
            if (node) emit node->deleteRequested(node->nodeId());
        }
        return;
    }
    QGraphicsScene::keyPressEvent(e);
}

// ── GrassModelerDialog ───────────────────────────────────────────────────────

GrassModelerDialog::GrassModelerDialog(GrassInterface *iface, QWidget *parent)
    : QDialog(parent)
    , mIface(iface)
    , mModel(new GrassModel(this))
    , mScene(new GrassModelerScene(this))
{
    setWindowTitle("Graphical Modeler");
    resize(1100, 700);

    QVBoxLayout *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(4);

    buildToolBar();

    // Scene view + properties panel (horizontal)
    QSplitter *hSplit = new QSplitter(Qt::Horizontal, this);

    buildScene();
    hSplit->addWidget(mView);

    QWidget *propsWidget = new QWidget(this);
    buildPropsPanel();
    QVBoxLayout *pv = new QVBoxLayout(propsWidget);
    pv->setContentsMargins(0, 0, 0, 0);
    pv->addWidget(mPropsStack);
    hSplit->addWidget(propsWidget);
    hSplit->setStretchFactor(0, 3);
    hSplit->setStretchFactor(1, 1);
    hSplit->setSizes({ 780, 280 });

    // Log panel
    mLog = new QTextEdit(this);
    mLog->setReadOnly(true);
    mLog->setMaximumHeight(140);
    mLog->setFont(QFont("Monospace", 8));
    mLog->setPlaceholderText("Model execution log…");

    QSplitter *vSplit = new QSplitter(Qt::Vertical, this);
    vSplit->addWidget(hSplit);
    vSplit->addWidget(mLog);
    vSplit->setStretchFactor(0, 4);
    vSplit->setStretchFactor(1, 1);
    vSplit->setSizes({ 540, 140 });

    vbox->addWidget(vSplit, 1);

    // Scene signals
    connect(mScene, &GrassModelerScene::connectionRequested,
            this, [this](int fromData, int toAction) {
                bool ok;
                const QString param = QInputDialog::getText(
                    this, "Connect", "Parameter name in module:", QLineEdit::Normal, {}, &ok);
                if (!ok || param.trimmed().isEmpty()) return;

                // Validate: fromData must be a data node
                if (!mModel->dataById(fromData)) return;
                if (!mModel->actionById(toAction)) return;

                const int relId = mModel->addRelation(fromData, toAction, param.trimmed());
                const ModelRelation *rel = mModel->relationById(relId);
                if (rel) addArrowToScene(*rel);
            });

    connect(mScene, &GrassModelerScene::nodeDoubleClicked,
            this, [this](int nodeId, bool isAction) {
                if (isAction) showActionProps(nodeId);
                else          showDataProps(nodeId);
            });

    connect(mScene, &QGraphicsScene::selectionChanged,
            this, [this] {
                const auto sel = mScene->selectedItems();
                if (sel.size() == 1)
                {
                    auto *node = dynamic_cast<ModelerNodeItem *>(sel.first());
                    if (node)
                    {
                        mSelId = node->nodeId();
                        if (dynamic_cast<ModelerActionItem *>(node))
                            showActionProps(node->nodeId());
                        else
                            showDataProps(node->nodeId());
                        return;
                    }
                }
                mSelId = -1;
                mPropsStack->setCurrentIndex(0);
            });
}

GrassModelerDialog::~GrassModelerDialog()
{
    stopModel();
}

// ── Toolbar ──────────────────────────────────────────────────────────────────

void GrassModelerDialog::buildToolBar()
{
    QToolBar *tb = new QToolBar(this);
    tb->setMovable(false);
    tb->setIconSize(QSize(16, 16));
    static_cast<QVBoxLayout *>(layout())->insertWidget(0, tb);

    tb->addAction("New",  this, [this] {
        if (QMessageBox::question(this, "New Model",
                                  "Clear current model?",
                                  QMessageBox::Yes | QMessageBox::No)
            != QMessageBox::Yes) return;
        mModel->clear();
        mScene->clear();
        mActionItems.clear();
        mDataItems.clear();
        mArrowItems.clear();
        mFilePath.clear();
        setWindowTitle("Graphical Modeler");
    })->setToolTip("New model");

    tb->addAction("Open…", this, [this] {
        const QString f = QFileDialog::getOpenFileName(
            this, "Open Model", {}, "GRASS Models (*.gxm);;All Files (*)");
        if (!f.isEmpty()) loadFile(f);
    })->setToolTip("Open .gxm model file");

    tb->addAction("Save", this, [this] {
        if (mFilePath.isEmpty()) {
            mFilePath = QFileDialog::getSaveFileName(
                this, "Save Model", {}, "GRASS Models (*.gxm)");
        }
        if (!mFilePath.isEmpty())
        {
            // Persist current positions from scene items
            for (auto it = mActionItems.cbegin(); it != mActionItems.cend(); ++it)
                mModel->setActionPos(it.key(), it.value()->pos());
            for (auto it = mDataItems.cbegin(); it != mDataItems.cend(); ++it)
                mModel->setDataPos(it.key(), it.value()->pos());

            if (!mModel->saveFile(mFilePath))
                QMessageBox::warning(this, "Save failed", "Could not write " + mFilePath);
        }
    })->setToolTip("Save model");

    tb->addAction("Save As…", this, [this] {
        const QString f = QFileDialog::getSaveFileName(
            this, "Save Model As", {}, "GRASS Models (*.gxm)");
        if (!f.isEmpty()) { mFilePath = f; }
        // trigger save
        if (!mFilePath.isEmpty())
        {
            for (auto it = mActionItems.cbegin(); it != mActionItems.cend(); ++it)
                mModel->setActionPos(it.key(), it.value()->pos());
            for (auto it = mDataItems.cbegin(); it != mDataItems.cend(); ++it)
                mModel->setDataPos(it.key(), it.value()->pos());
            mModel->saveFile(mFilePath);
        }
    });

    tb->addSeparator();

    tb->addAction("+ Module", this, [this] {
        bool ok;
        const QString mod = QInputDialog::getText(
            this, "Add Module Node", "GRASS module name (e.g. r.slope.aspect):",
            QLineEdit::Normal, {}, &ok).trimmed();
        if (!ok || mod.isEmpty()) return;
        // Place near centre of visible scene
        const QPointF pos = mView->mapToScene(mView->viewport()->rect().center())
                            + QPointF(-90, -31);
        const int id = mModel->addAction(mod, pos);
        if (auto *n = mModel->actionById(id)) addActionToScene(*n);
    })->setToolTip("Add a module (action) node");

    tb->addAction("+ Data", this, [this] {
        QDialog dlg(this);
        dlg.setWindowTitle("Add Data Node");
        QFormLayout *form = new QFormLayout(&dlg);
        auto *typeCombo = new QComboBox(&dlg);
        typeCombo->addItems({"raster", "vector", "raster_3d", "any"});
        auto *valueEdit = new QLineEdit(&dlg);
        auto *labelEdit = new QLineEdit(&dlg);
        form->addRow("Type:",  typeCombo);
        form->addRow("Map name:", valueEdit);
        form->addRow("Label:", labelEdit);
        auto *btns = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        form->addRow(btns);
        connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        if (dlg.exec() != QDialog::Accepted) return;
        const QString val = valueEdit->text().trimmed();
        if (val.isEmpty()) return;
        const QPointF pos = mView->mapToScene(mView->viewport()->rect().center())
                            + QPointF(-70, -22);
        const int id = mModel->addData(typeCombo->currentText(), val, pos,
                                        labelEdit->text().trimmed());
        if (auto *n = mModel->dataById(id)) addDataToScene(*n);
    })->setToolTip("Add a data node");

    tb->addSeparator();

    mRunAct = tb->addAction("▶ Run", this, [this] { runModel(); });
    mRunAct->setToolTip("Run the model");

    mStopAct = tb->addAction("■ Stop", this, [this] { stopModel(); });
    mStopAct->setToolTip("Stop model execution");
    mStopAct->setEnabled(false);

    tb->addSeparator();

    tb->addAction("Zoom In",  this, [this] { mView->scale(1.2, 1.2); });
    tb->addAction("Zoom Out", this, [this] { mView->scale(1/1.2, 1/1.2); });
    tb->addAction("Fit",      this, [this] {
        mView->fitInView(mScene->itemsBoundingRect().adjusted(-40,-40,40,40),
                         Qt::KeepAspectRatio);
    });
}

// ── Scene view ────────────────────────────────────────────────────────────────

void GrassModelerDialog::buildScene()
{
    mView = new QGraphicsView(mScene, this);
    mView->setDragMode(QGraphicsView::RubberBandDrag);
    mView->setRenderHint(QPainter::Antialiasing);
    mView->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    mView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    mView->setBackgroundBrush(QColor(240, 242, 248));

    // Context menu on empty canvas
    mView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(mView, &QGraphicsView::customContextMenuRequested,
            this, [this](const QPoint &pos) {
                // Only if no item is under cursor
                if (mScene->itemAt(mView->mapToScene(pos), mView->transform()))
                    return;
                QMenu menu(mView);
                menu.addAction("Add Module Node…", this, [this, pos] {
                    bool ok;
                    const QString mod = QInputDialog::getText(
                        this, "Add Module", "GRASS module name:",
                        QLineEdit::Normal, {}, &ok).trimmed();
                    if (!ok || mod.isEmpty()) return;
                    const QPointF spos = mView->mapToScene(pos);
                    const int id = mModel->addAction(mod, spos - QPointF(90, 31));
                    if (auto *n = mModel->actionById(id)) addActionToScene(*n);
                });
                menu.addAction("Add Data Node…", this, [this, pos] {
                    QDialog dlg(this);
                    dlg.setWindowTitle("Add Data Node");
                    QFormLayout *form = new QFormLayout(&dlg);
                    auto *typeCombo = new QComboBox(&dlg);
                    typeCombo->addItems({"raster", "vector", "raster_3d", "any"});
                    auto *valueEdit = new QLineEdit(&dlg);
                    form->addRow("Type:",     typeCombo);
                    form->addRow("Map name:", valueEdit);
                    auto *btns = new QDialogButtonBox(
                        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
                    form->addRow(btns);
                    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
                    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
                    if (dlg.exec() != QDialog::Accepted) return;
                    const QString val = valueEdit->text().trimmed();
                    if (val.isEmpty()) return;
                    const QPointF spos = mView->mapToScene(pos);
                    const int id = mModel->addData(typeCombo->currentText(), val,
                                                    spos - QPointF(70, 22));
                    if (auto *n = mModel->dataById(id)) addDataToScene(*n);
                });
                menu.exec(mView->mapToGlobal(pos));
            });
}

// ── Properties panel ─────────────────────────────────────────────────────────

void GrassModelerDialog::buildPropsPanel()
{
    mPropsStack = new QStackedWidget(this);
    mPropsStack->setMinimumWidth(240);

    // Page 0: nothing selected
    auto *pg0 = new QWidget(mPropsStack);
    {
        QVBoxLayout *v = new QVBoxLayout(pg0);
        auto *l = new QLabel("Select a node\nto edit its properties.", pg0);
        l->setAlignment(Qt::AlignCenter);
        l->setWordWrap(true);
        v->addWidget(l);
    }
    mPropsStack->addWidget(pg0);

    // Page 1: Action props
    auto *pg1 = new QWidget(mPropsStack);
    {
        QVBoxLayout *v = new QVBoxLayout(pg1);
        v->setSpacing(6);
        mActModLabel  = new QLabel(pg1);
        mActModLabel->setStyleSheet("font-weight:bold;");
        mActLabelEdit = new QLineEdit(pg1);
        mParamsTable  = new QTableWidget(0, 2, pg1);
        mParamsTable->setHorizontalHeaderLabels({"Parameter", "Value"});
        mParamsTable->horizontalHeader()->setStretchLastSection(true);
        mParamsTable->setMinimumHeight(120);

        v->addWidget(new QLabel("Module:", pg1));
        v->addWidget(mActModLabel);
        v->addWidget(new QLabel("Label:", pg1));
        v->addWidget(mActLabelEdit);
        v->addWidget(new QLabel("Parameters:", pg1));
        v->addWidget(mParamsTable);

        auto *addParamBtn = new QPushButton("+ Add param", pg1);
        connect(addParamBtn, &QPushButton::clicked, this, [this] {
            const int row = mParamsTable->rowCount();
            mParamsTable->insertRow(row);
            mParamsTable->setItem(row, 0, new QTableWidgetItem("name"));
            mParamsTable->setItem(row, 1, new QTableWidgetItem("value"));
        });
        v->addWidget(addParamBtn);

        auto *applyBtn = new QPushButton("Apply", pg1);
        applyBtn->setDefault(true);
        connect(applyBtn, &QPushButton::clicked, this, [this] { applyActionProps(); });
        v->addWidget(applyBtn);
        v->addStretch();
    }
    mPropsStack->addWidget(pg1);

    // Page 2: Data props
    auto *pg2 = new QWidget(mPropsStack);
    {
        QFormLayout *form = new QFormLayout(pg2);
        mDatLabelEdit = new QLineEdit(pg2);
        mDatTypeCombo = new QComboBox(pg2);
        mDatTypeCombo->addItems({"raster", "vector", "raster_3d", "any"});
        mDatValueEdit = new QLineEdit(pg2);
        form->addRow("Label:", mDatLabelEdit);
        form->addRow("Type:",  mDatTypeCombo);
        form->addRow("Value:", mDatValueEdit);

        auto *applyBtn = new QPushButton("Apply", pg2);
        connect(applyBtn, &QPushButton::clicked, this, [this] { applyDataProps(); });
        form->addRow(applyBtn);
    }
    mPropsStack->addWidget(pg2);

    mPropsStack->setCurrentIndex(0);
}

// ── Scene ↔ Model sync ────────────────────────────────────────────────────────

void GrassModelerDialog::addActionToScene(const ModelActionNode &node)
{
    auto *item = new ModelerActionItem(node.id, node.moduleName, node.label,
                                       node.enabled);
    item->setPos(node.pos);
    mScene->addItem(item);
    mActionItems[node.id] = item;

    connect(item, &ModelerNodeItem::deleteRequested, this, [this](int id) {
        // Remove all arrows for this node first
        const auto rels = mModel->relationsForNode(id);
        for (const auto &r : rels)
        {
            if (mArrowItems.contains(r.id))
            {
                auto *arr = mArrowItems.take(r.id);
                mScene->removeItem(arr);
                delete arr;
            }
            mModel->removeRelation(r.id);
        }
        mScene->removeItem(mActionItems.take(id));
        delete mActionItems.value(id, nullptr);   // already taken above
        mModel->removeNode(id);
        mPropsStack->setCurrentIndex(0);
    });

    connect(item, &ModelerNodeItem::itemDoubleClicked,
            this, [this](int nodeId, bool isAction) {
                if (isAction) showActionProps(nodeId);
                else          showDataProps(nodeId);
            });
}

void GrassModelerDialog::addDataToScene(const ModelDataNode &node)
{
    auto *item = new ModelerDataItem(node.id, node.type, node.value,
                                     node.label, node.enabled);
    item->setPos(node.pos);
    mScene->addItem(item);
    mDataItems[node.id] = item;

    connect(item, &ModelerNodeItem::deleteRequested, this, [this](int id) {
        const auto rels = mModel->relationsForNode(id);
        for (const auto &r : rels)
        {
            if (mArrowItems.contains(r.id))
            {
                auto *arr = mArrowItems.take(r.id);
                mScene->removeItem(arr);
                delete arr;
            }
            mModel->removeRelation(r.id);
        }
        if (mDataItems.contains(id))
        {
            mScene->removeItem(mDataItems[id]);
            delete mDataItems.take(id);
        }
        mModel->removeNode(id);
        mPropsStack->setCurrentIndex(0);
    });

    connect(item, &ModelerNodeItem::itemDoubleClicked,
            this, [this](int nodeId, bool isAction) {
                if (isAction) showActionProps(nodeId);
                else          showDataProps(nodeId);
            });
}

void GrassModelerDialog::addArrowToScene(const ModelRelation &rel)
{
    auto *src = dataItemById(rel.fromId);
    auto *dst = actionItemById(rel.toId);
    if (!src || !dst) return;

    auto *arrow = new ModelerArrowItem(src, dst, rel.paramName, rel.id);
    src->addArrow(arrow);
    dst->addArrow(arrow);
    mScene->addItem(arrow);
    mArrowItems[rel.id] = arrow;
}

void GrassModelerDialog::syncSceneFromModel()
{
    mScene->clear();
    mActionItems.clear();
    mDataItems.clear();
    mArrowItems.clear();

    for (const auto &n : mModel->actions())   addActionToScene(n);
    for (const auto &n : mModel->dataNodes()) addDataToScene(n);
    for (const auto &r : mModel->relations()) addArrowToScene(r);
}

ModelerActionItem *GrassModelerDialog::actionItemById(int id) const
{
    return mActionItems.value(id, nullptr);
}

ModelerDataItem *GrassModelerDialog::dataItemById(int id) const
{
    return mDataItems.value(id, nullptr);
}

// ── Properties panel ─────────────────────────────────────────────────────────

void GrassModelerDialog::showActionProps(int nodeId)
{
    auto *n = mModel->actionById(nodeId);
    if (!n) return;

    mSelId       = nodeId;
    mSelIsAction = true;

    mActModLabel->setText(n->moduleName);
    mActLabelEdit->setText(n->label);

    mParamsTable->setRowCount(0);
    for (auto it = n->params.cbegin(); it != n->params.cend(); ++it)
    {
        const int row = mParamsTable->rowCount();
        mParamsTable->insertRow(row);
        mParamsTable->setItem(row, 0, new QTableWidgetItem(it.key()));
        mParamsTable->setItem(row, 1, new QTableWidgetItem(it.value()));
    }

    mPropsStack->setCurrentIndex(1);
}

void GrassModelerDialog::showDataProps(int nodeId)
{
    auto *n = mModel->dataById(nodeId);
    if (!n) return;

    mSelId       = nodeId;
    mSelIsAction = false;

    mDatLabelEdit->setText(n->label);
    mDatValueEdit->setText(n->value);
    const int idx = mDatTypeCombo->findText(n->type);
    mDatTypeCombo->setCurrentIndex(idx >= 0 ? idx : 0);

    mPropsStack->setCurrentIndex(2);
}

void GrassModelerDialog::applyActionProps()
{
    if (mSelId < 0 || !mSelIsAction) return;
    auto *n = mModel->actionById(mSelId);
    if (!n) return;

    mModel->setNodeLabel(mSelId, mActLabelEdit->text().trimmed());

    // Collect params from table
    QMap<QString,QString> params;
    for (int row = 0; row < mParamsTable->rowCount(); ++row)
    {
        const auto *kItem = mParamsTable->item(row, 0);
        const auto *vItem = mParamsTable->item(row, 1);
        if (kItem && vItem)
            params[kItem->text().trimmed()] = vItem->text();
    }
    n->params = params;

    // Sync scene item label
    if (auto *item = actionItemById(mSelId))
        item->setLabel(mActLabelEdit->text().trimmed());
}

void GrassModelerDialog::applyDataProps()
{
    if (mSelId < 0 || mSelIsAction) return;
    mModel->setNodeLabel(mSelId, mDatLabelEdit->text().trimmed());
    mModel->setDataType (mSelId, mDatTypeCombo->currentText());
    mModel->setDataValue(mSelId, mDatValueEdit->text().trimmed());

    if (auto *item = dataItemById(mSelId))
    {
        item->setLabel   (mDatLabelEdit->text().trimmed());
        item->setDataType(mDatTypeCombo->currentText());
        item->setValue   (mDatValueEdit->text().trimmed());
    }
}

// ── File load ────────────────────────────────────────────────────────────────

void GrassModelerDialog::loadFile(const QString &path)
{
    if (!mModel->loadFile(path))
    {
        QMessageBox::warning(this, "Load failed", "Cannot load: " + path);
        return;
    }
    mFilePath = path;
    setWindowTitle("Modeler — " + QFileInfo(path).baseName());
    syncSceneFromModel();
    if (!mActionItems.isEmpty() || !mDataItems.isEmpty())
        mView->fitInView(mScene->itemsBoundingRect().adjusted(-40,-40,40,40),
                         Qt::KeepAspectRatio);
}

// ── Model execution ───────────────────────────────────────────────────────────

void GrassModelerDialog::runModel()
{
    if (mRunning) return;

    // Build execution order: actions in ID order (insertion order)
    mExecOrder.clear();
    for (const auto &n : mModel->actions())
        if (n.enabled) mExecOrder.append(n.id);

    if (mExecOrder.isEmpty())
    {
        log("No enabled module nodes to run.");
        return;
    }

    mRunning = true;
    mExecIdx = 0;
    mRunAct->setEnabled(false);
    mStopAct->setEnabled(true);
    mLog->clear();
    log("=== Model run started ===");
    runNextAction();
}

void GrassModelerDialog::runNextAction()
{
    if (!mRunning) return;

    if (mExecIdx >= mExecOrder.size())
    {
        log("=== Model run complete ===");
        mRunning = false;
        mRunAct->setEnabled(true);
        mStopAct->setEnabled(false);
        return;
    }

    const int id      = mExecOrder[mExecIdx++];
    auto *action = mModel->actionById(id);
    if (!action) { runNextAction(); return; }

    // Build argument list
    QStringList args;
    QSet<QString> coveredParams;

    // Params from connected data nodes (via relations)
    for (const ModelRelation &rel : mModel->relations())
    {
        if (rel.toId != id) continue;
        auto *data = mModel->dataById(rel.fromId);
        if (data && !data->value.isEmpty())
        {
            args << QString("%1=%2").arg(rel.paramName, data->value);
            coveredParams.insert(rel.paramName);
        }
    }

    // Direct params (don't override relation-provided params)
    for (auto it = action->params.cbegin(); it != action->params.cend(); ++it)
    {
        if (!coveredParams.contains(it.key()) && !it.value().isEmpty())
            args << QString("%1=%2").arg(it.key(), it.value());
    }

    // Flags
    for (const QString &f : action->flags)
        args << QString("--%1").arg(f);

    log(QString(">>> %1 %2").arg(action->moduleName, args.join(' ')));

    mProcess = new QProcess(this);
    mProcess->setProcessChannelMode(QProcess::MergedChannels);

    connect(mProcess, &QProcess::readyRead, this, [this] {
        const QString out = QString::fromLocal8Bit(mProcess->readAll());
        log(out.trimmed());
    });

    connect(mProcess,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
                const QString remaining =
                    QString::fromLocal8Bit(mProcess->readAll()).trimmed();
                if (!remaining.isEmpty()) log(remaining);

                mProcess->deleteLater();
                mProcess = nullptr;

                if (code != 0)
                {
                    log(QString("ERROR: process exited with code %1 — stopping.")
                        .arg(code), true);
                    mRunning = false;
                    mRunAct->setEnabled(true);
                    mStopAct->setEnabled(false);
                    return;
                }
                runNextAction();
            });

    mProcess->start(action->moduleName, args);
    if (!mProcess->waitForStarted(3000))
    {
        log(QString("ERROR: cannot start '%1'").arg(action->moduleName), true);
        mProcess->deleteLater();
        mProcess = nullptr;
        mRunning = false;
        mRunAct->setEnabled(true);
        mStopAct->setEnabled(false);
    }
}

void GrassModelerDialog::stopModel()
{
    if (mProcess)
    {
        mProcess->kill();
        mProcess->waitForFinished(1000);
        mProcess->deleteLater();
        mProcess = nullptr;
    }
    mRunning = false;
    mRunAct->setEnabled(true);
    mStopAct->setEnabled(false);
    if (!mExecOrder.isEmpty()) log("=== Stopped by user ===");
}

void GrassModelerDialog::log(const QString &line, bool isError)
{
    if (line.isEmpty()) return;
    mLog->setTextColor(isError ? Qt::red : Qt::black);
    // Each call may be multiline
    for (const QString &l : line.split('\n', Qt::SkipEmptyParts))
        mLog->append(l);
    mLog->setTextColor(Qt::black);
}
