#include "grassmodeleritems.h"

#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>
#include <QtMath>

// ── ModelerNodeItem ──────────────────────────────────────────────────────────

ModelerNodeItem::ModelerNodeItem(int nodeId, QGraphicsItem *parent)
    : QGraphicsObject(parent), mId(nodeId)
{
    setFlags( ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges );
    setCacheMode( DeviceCoordinateCache );
}

bool ModelerNodeItem::nearOutputPort(const QPointF &scenePos) const
{
    return QLineF(scenePos, outputPortScene()).length() < kPortR * 2.0;
}

bool ModelerNodeItem::nearInputPort(const QPointF &scenePos) const
{
    return QLineF(scenePos, inputPortScene()).length() < kPortR * 2.0;
}

void ModelerNodeItem::addArrow(ModelerArrowItem *arrow)
{
    mArrows.append(arrow);
}

void ModelerNodeItem::removeArrow(ModelerArrowItem *arrow)
{
    mArrows.removeAll(arrow);
}

void ModelerNodeItem::updateArrows()
{
    for (ModelerArrowItem *a : mArrows) a->updatePosition();
}

QVariant ModelerNodeItem::itemChange(GraphicsItemChange change,
                                      const QVariant &value)
{
    if (change == ItemPositionHasChanged)
        updateArrows();
    return QGraphicsObject::itemChange(change, value);
}

void ModelerNodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e)
{
    QGraphicsObject::mouseDoubleClickEvent(e);
    emit itemDoubleClicked(mId, mIsAction);
}

void ModelerNodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *e)
{
    QMenu menu;
    QAction *delAct = menu.addAction("Delete node");
    if (menu.exec(e->screenPos()) == delAct)
        emit deleteRequested(mId);
}

// ── ModelerActionItem ────────────────────────────────────────────────────────

ModelerActionItem::ModelerActionItem(int id, const QString &module,
                                     const QString &label, bool enabled,
                                     QGraphicsItem *parent)
    : ModelerNodeItem(id, parent)
    , mModule(module), mLabel(label), mEnabled(enabled)
{
    mIsAction = true;
    setToolTip(module);
}

QRectF ModelerActionItem::boundingRect() const
{
    return QRectF(-kPortR - 1, -1, W + 2 * kPortR + 2, H + 2);
}

QPainterPath ModelerActionItem::shape() const
{
    QPainterPath p;
    p.addRoundedRect(QRectF(0, 0, W, H), 8, 8);
    return p;
}

void ModelerActionItem::paint(QPainter *p,
                               const QStyleOptionGraphicsItem *opt, QWidget *)
{
    p->setRenderHint(QPainter::Antialiasing);

    const bool sel  = opt->state & QStyle::State_Selected;
    const QColor bg = mEnabled ? QColor(55, 120, 170) : QColor(110, 110, 130);
    const QColor bdr = sel ? QColor(255, 200, 50) : bg.darker(140);

    // Body
    p->setBrush(bg);
    p->setPen(QPen(bdr, sel ? 2.5 : 1.5));
    p->drawRoundedRect(QRectF(0, 0, W, H), 8, 8);

    // Module name (bold)
    QFont f = p->font();
    f.setBold(true);
    f.setPointSize(9);
    p->setFont(f);
    p->setPen(Qt::white);
    const QRectF titleRect(10, 5, W - 20, 26);
    p->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                p->fontMetrics().elidedText(mModule, Qt::ElideRight,
                                            static_cast<int>(titleRect.width())));

    // Label if different from module name
    if (mLabel != mModule)
    {
        f.setBold(false);
        f.setPointSize(8);
        p->setFont(f);
        p->setPen(QColor(210, 225, 240));
        p->drawText(QRectF(10, 32, W - 20, 20),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    p->fontMetrics().elidedText(mLabel, Qt::ElideRight, W - 20));
    }

    // Port circles
    const QColor portC = bdr;
    p->setBrush(QColor(220, 235, 250));
    p->setPen(QPen(portC, 1));
    // Input port (left edge)
    p->drawEllipse(QRectF(-kPortR, H / 2.0 - kPortR, 2 * kPortR, 2 * kPortR));
    // Output port (right edge)
    p->drawEllipse(QRectF(W - kPortR, H / 2.0 - kPortR, 2 * kPortR, 2 * kPortR));
}

QPointF ModelerActionItem::outputPortScene() const
{
    return mapToScene(QPointF(W, H / 2.0));
}

QPointF ModelerActionItem::inputPortScene() const
{
    return mapToScene(QPointF(0, H / 2.0));
}

// ── ModelerDataItem ──────────────────────────────────────────────────────────

ModelerDataItem::ModelerDataItem(int id, const QString &type,
                                  const QString &value, const QString &label,
                                  bool enabled, QGraphicsItem *parent)
    : ModelerNodeItem(id, parent)
    , mType(type), mValue(value), mLabel(label), mEnabled(enabled)
{
    mIsAction = false;
    setToolTip(type + ": " + value);
}

QRectF ModelerDataItem::boundingRect() const
{
    return QRectF(-kPortR - 1, -1, W + 2 * kPortR + 2, H + 2);
}

QPainterPath ModelerDataItem::shape() const
{
    QPainterPath p;
    p.addEllipse(QRectF(0, 0, W, H));
    return p;
}

void ModelerDataItem::paint(QPainter *p,
                              const QStyleOptionGraphicsItem *opt, QWidget *)
{
    p->setRenderHint(QPainter::Antialiasing);

    const bool sel   = opt->state & QStyle::State_Selected;
    const QColor bg  = mEnabled ? QColor(50, 155, 85) : QColor(110, 130, 110);
    const QColor bdr = sel ? QColor(255, 200, 50) : bg.darker(140);

    // Body (ellipse)
    p->setBrush(bg);
    p->setPen(QPen(bdr, sel ? 2.5 : 1.5));
    p->drawEllipse(QRectF(0, 0, W, H));

    // Type icon character
    const QString icon = (mType == "vector") ? "V" :
                         (mType == "raster_3d") ? "3D" : "R";
    QFont f = p->font();
    f.setBold(true);
    f.setPointSize(10);
    p->setFont(f);
    p->setPen(QColor(200, 240, 210));
    p->drawText(QRectF(6, 0, 24, H), Qt::AlignCenter, icon);

    // Value / label
    f.setBold(false);
    f.setPointSize(8);
    p->setFont(f);
    p->setPen(Qt::white);
    const QString display = mLabel.isEmpty() ? mValue : mLabel;
    p->drawText(QRectF(32, 0, W - 38, H), Qt::AlignLeft | Qt::AlignVCenter,
                p->fontMetrics().elidedText(display, Qt::ElideRight, W - 38));

    // Port circles
    p->setBrush(QColor(210, 245, 220));
    p->setPen(QPen(bdr, 1));
    // Input port (left)
    p->drawEllipse(QRectF(-kPortR, H / 2.0 - kPortR, 2 * kPortR, 2 * kPortR));
    // Output port (right)
    p->drawEllipse(QRectF(W - kPortR, H / 2.0 - kPortR, 2 * kPortR, 2 * kPortR));
}

QPointF ModelerDataItem::outputPortScene() const
{
    return mapToScene(QPointF(W, H / 2.0));
}

QPointF ModelerDataItem::inputPortScene() const
{
    return mapToScene(QPointF(0, H / 2.0));
}

// ── ModelerArrowItem ─────────────────────────────────────────────────────────

ModelerArrowItem::ModelerArrowItem(ModelerNodeItem *src, ModelerNodeItem *dst,
                                    const QString &paramName, int relId,
                                    QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , mSrc(src), mDst(dst), mParam(paramName), mRelId(relId)
{
    setZValue(-1);
    setFlag(ItemIsSelectable);
    updatePosition();
}

void ModelerArrowItem::buildPath()
{
    const double dx = qAbs(mP2.x() - mP1.x()) * 0.55 + 30.0;
    mPath = QPainterPath(mP1);
    mPath.cubicTo(mP1 + QPointF(dx, 0),
                  mP2 - QPointF(dx, 0),
                  mP2);
}

void ModelerArrowItem::updatePosition()
{
    prepareGeometryChange();
    mP1 = mSrc->outputPortScene();
    mP2 = mDst->inputPortScene();
    buildPath();
    update();
}

QRectF ModelerArrowItem::boundingRect() const
{
    return mPath.boundingRect().adjusted(-12, -12, 12, 12);
}

QPainterPath ModelerArrowItem::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(12);
    return stroker.createStroke(mPath);
}

void ModelerArrowItem::paint(QPainter *p,
                               const QStyleOptionGraphicsItem *opt, QWidget *)
{
    p->setRenderHint(QPainter::Antialiasing);

    const bool sel = opt->state & QStyle::State_Selected;
    QColor col = sel ? QColor(200, 140, 0) : QColor(80, 80, 140);

    p->setPen(QPen(col, sel ? 2.5 : 1.8));
    p->setBrush(Qt::NoBrush);
    p->drawPath(mPath);

    // Arrowhead at mP2
    const double dx = qAbs(mP2.x() - mP1.x()) * 0.55 + 30.0;
    const QPointF ctrl2 = mP2 - QPointF(dx, 0);
    const double angle = qAtan2(mP2.y() - ctrl2.y(), mP2.x() - ctrl2.x());

    static constexpr double L = 10.0, W2 = 5.0;
    QPolygonF head;
    head << mP2
         << QPointF(mP2.x() - L * qCos(angle) + W2 * qSin(angle),
                    mP2.y() - L * qSin(angle) - W2 * qCos(angle))
         << QPointF(mP2.x() - L * qCos(angle) - W2 * qSin(angle),
                    mP2.y() - L * qSin(angle) + W2 * qCos(angle));

    p->setBrush(col);
    p->setPen(Qt::NoPen);
    p->drawPolygon(head);

    // Param label at midpoint
    if (!mParam.isEmpty())
    {
        const QPointF mid = mPath.pointAtPercent(0.5);
        QFont f = p->font();
        f.setPointSize(7);
        p->setFont(f);
        p->setPen(QColor(60, 60, 100));
        p->drawText(mid + QPointF(4, -4), mParam);
    }
}
