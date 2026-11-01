#pragma once

#include <QGraphicsObject>
#include <QList>
#include <QString>

class QPainter;
class QStyleOptionGraphicsItem;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneContextMenuEvent;
class ModelerArrowItem;

static constexpr int kActionItemType = QGraphicsItem::UserType + 1;
static constexpr int kDataItemType   = QGraphicsItem::UserType + 2;
static constexpr int kArrowItemType  = QGraphicsItem::UserType + 3;
static constexpr double kPortR       = 6.0;   // port circle radius

// ── Abstract base for draggable, port-bearing nodes ──────────────────────────
class ModelerNodeItem : public QGraphicsObject
{
    Q_OBJECT
public:
    explicit ModelerNodeItem(int nodeId, QGraphicsItem *parent = nullptr);

    int nodeId() const { return mId; }

    virtual QPointF outputPortScene() const = 0;
    virtual QPointF inputPortScene()  const = 0;

    bool nearOutputPort(const QPointF &scenePos) const;
    bool nearInputPort (const QPointF &scenePos) const;

    void addArrow   (ModelerArrowItem *arrow);
    void removeArrow(ModelerArrowItem *arrow);
    void updateArrows();

signals:
    void itemDoubleClicked(int nodeId, bool isAction);
    void deleteRequested  (int nodeId);

protected:
    QVariant itemChange(GraphicsItemChange change,
                        const QVariant &value) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *e) override;

    int   mId = 0;
    bool  mIsAction = false;   // set by subclass
    QList<ModelerArrowItem *> mArrows;
};

// ── Module/action node  (blue rounded rectangle) ─────────────────────────────
class ModelerActionItem : public ModelerNodeItem
{
    Q_OBJECT
public:
    ModelerActionItem(int id, const QString &module, const QString &label,
                      bool enabled, QGraphicsItem *parent = nullptr);

    int  type()         const override { return kActionItemType; }
    QRectF boundingRect() const override;
    void   paint(QPainter *p,
                 const QStyleOptionGraphicsItem *o, QWidget *w) override;
    QPainterPath shape() const override;

    QPointF outputPortScene() const override;
    QPointF inputPortScene()  const override;

    void setLabel   (const QString &l) { mLabel   = l;  update(); }
    void setEnabled2(bool on)          { mEnabled = on; update(); }

    QString moduleName() const { return mModule;  }
    QString label()      const { return mLabel;   }
    bool    isEnabled2() const { return mEnabled; }

    static constexpr int W = 180;
    static constexpr int H = 62;

private:
    QString mModule;
    QString mLabel;
    bool    mEnabled = true;
};

// ── Data node  (green ellipse) ───────────────────────────────────────────────
class ModelerDataItem : public ModelerNodeItem
{
    Q_OBJECT
public:
    ModelerDataItem(int id, const QString &type, const QString &value,
                    const QString &label, bool enabled,
                    QGraphicsItem *parent = nullptr);

    int  type()         const override { return kDataItemType; }
    QRectF boundingRect() const override;
    void   paint(QPainter *p,
                 const QStyleOptionGraphicsItem *o, QWidget *w) override;
    QPainterPath shape() const override;

    QPointF outputPortScene() const override;
    QPointF inputPortScene()  const override;

    void setLabel   (const QString &l) { mLabel   = l;  update(); }
    void setValue   (const QString &v) { mValue   = v;  update(); }
    void setDataType(const QString &t) { mType    = t;  update(); }
    void setEnabled2(bool on)          { mEnabled = on; update(); }

    QString dataType()   const { return mType;    }
    QString value()      const { return mValue;   }
    QString label()      const { return mLabel;   }
    bool    isEnabled2() const { return mEnabled; }

    static constexpr int W = 140;
    static constexpr int H = 44;

private:
    QString mType;
    QString mValue;
    QString mLabel;
    bool    mEnabled = true;
};

// ── Arrow connecting data → action ───────────────────────────────────────────
class ModelerArrowItem : public QGraphicsItem
{
public:
    ModelerArrowItem(ModelerNodeItem *src, ModelerNodeItem *dst,
                     const QString &paramName, int relId,
                     QGraphicsItem *parent = nullptr);

    int          type()         const override { return kArrowItemType; }
    QRectF       boundingRect() const override;
    QPainterPath shape()        const override;
    void         paint(QPainter *p,
                       const QStyleOptionGraphicsItem *o, QWidget *w) override;

    void updatePosition();

    int     relId()     const { return mRelId; }
    int     srcId()     const { return mSrc ? mSrc->nodeId() : -1; }
    int     dstId()     const { return mDst ? mDst->nodeId() : -1; }
    QString paramName() const { return mParam; }

private:
    void buildPath();

    ModelerNodeItem *mSrc   = nullptr;
    ModelerNodeItem *mDst   = nullptr;
    QString          mParam;
    int              mRelId = 0;

    QPointF      mP1, mP2;
    QPainterPath mPath;    // cached bezier
};
