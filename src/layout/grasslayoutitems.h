#pragma once

#include <QGraphicsObject>
#include <QFont>
#include <QColor>
#include <QString>
#include <QPixmap>
#include <QDomDocument>

class GrassMapCanvas;

// Item type IDs
static constexpr int kLayoutMap        = QGraphicsItem::UserType + 20;
static constexpr int kLayoutLegend     = QGraphicsItem::UserType + 21;
static constexpr int kLayoutScaleBar   = QGraphicsItem::UserType + 22;
static constexpr int kLayoutNorthArrow = QGraphicsItem::UserType + 23;
static constexpr int kLayoutText       = QGraphicsItem::UserType + 24;
static constexpr int kLayoutImage      = QGraphicsItem::UserType + 25;

// ─── Abstract base ────────────────────────────────────────────────────────────
// All positions/sizes are in mm (1 scene unit = 1 mm).
// Items are movable and selectable; resize handles appear when selected.
class GrassLayoutItem : public QGraphicsObject
{
    Q_OBJECT
public:
    explicit GrassLayoutItem(double w, double h, QGraphicsItem *parent = nullptr);

    QRectF       boundingRect() const override;
    QPainterPath shape()        const override;

    double itemW() const { return mW; }
    double itemH() const { return mH; }
    void   setItemSize(double w, double h);

    virtual void    saveToXml(QDomElement &el) const;
    virtual void    loadFromXml(const QDomElement &el);
    virtual QString itemTypeStr() const = 0;

signals:
    void geometryChanged();

protected:
    void paint(QPainter *p,
               const QStyleOptionGraphicsItem *o, QWidget *w) override;
    virtual void paintContent(QPainter *p, const QRectF &r) = 0;

    QVariant itemChange(GraphicsItemChange change,
                        const QVariant &value) override;
    void mousePressEvent  (QGraphicsSceneMouseEvent *e) override;
    void mouseMoveEvent   (QGraphicsSceneMouseEvent *e) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override;
    void hoverMoveEvent   (QGraphicsSceneHoverEvent *e) override;

    double mW = 0, mH = 0;

private:
    enum Handle { None, TL, TM, TR, ML, MR, BL, BM, BR };
    Handle    handleAt(const QPointF &p) const;
    QRectF    handleRect(Handle h) const;
    void      applyResize(Handle h, const QPointF &delta);

    static constexpr double kHS = 5.0; // handle size mm

    Handle  mDragHandle  = None;
    QPointF mDragStart;
    double  mWAtDrag = 0, mHAtDrag = 0;
    QPointF mPosAtDrag;
};

// ─── Map frame ────────────────────────────────────────────────────────────────
class GrassLayoutMapItem : public GrassLayoutItem
{
    Q_OBJECT
public:
    GrassLayoutMapItem(GrassMapCanvas *canvas, double w, double h,
                       QGraphicsItem *parent = nullptr);

    int     type()        const override { return kLayoutMap;    }
    QString itemTypeStr() const override { return "map";         }
    void saveToXml  (QDomElement &el) const override;

    GrassMapCanvas *canvas() const { return mCanvas; }

protected:
    void paintContent(QPainter *p, const QRectF &r) override;

private:
    GrassMapCanvas *mCanvas = nullptr;
};

// ─── Legend ───────────────────────────────────────────────────────────────────
class GrassLayoutLegendItem : public GrassLayoutItem
{
    Q_OBJECT
public:
    GrassLayoutLegendItem(GrassMapCanvas *canvas, double w, double h,
                           QGraphicsItem *parent = nullptr);

    int     type()        const override { return kLayoutLegend; }
    QString itemTypeStr() const override { return "legend";      }
    void saveToXml  (QDomElement &el) const override;
    void loadFromXml(const QDomElement &el) override;

    QString title() const       { return mTitle; }
    void    setTitle(const QString &t) { mTitle = t; update(); }
    QFont   font()  const       { return mFont;  }
    void    setFont(const QFont &f)    { mFont  = f; update(); }

protected:
    void paintContent(QPainter *p, const QRectF &r) override;

private:
    GrassMapCanvas *mCanvas = nullptr;
    QString mTitle = "Legend";
    QFont   mFont;
};

// ─── Scale bar ────────────────────────────────────────────────────────────────
class GrassLayoutMapItem;   // forward

class GrassLayoutScaleBarItem : public GrassLayoutItem
{
    Q_OBJECT
public:
    GrassLayoutScaleBarItem(GrassMapCanvas *canvas,
                             GrassLayoutMapItem *mapFrame,
                             double w, double h,
                             QGraphicsItem *parent = nullptr);

    int     type()        const override { return kLayoutScaleBar; }
    QString itemTypeStr() const override { return "scalebar";      }
    void saveToXml  (QDomElement &el) const override;
    void loadFromXml(const QDomElement &el) override;

    QString units()    const { return mUnits;    }
    int     segments() const { return mSegments; }
    void setUnits   (const QString &u) { mUnits    = u; update(); }
    void setSegments(int n)            { mSegments = n; update(); }

protected:
    void paintContent(QPainter *p, const QRectF &r) override;

private:
    static double niceNumber(double x);
    static QString formatDist(double mapUnits, const QString &units);

    GrassMapCanvas     *mCanvas   = nullptr;
    GrassLayoutMapItem *mMapFrame = nullptr;
    QString mUnits    = "m";
    int     mSegments = 4;
};

// ─── North arrow ──────────────────────────────────────────────────────────────
class GrassLayoutNorthArrowItem : public GrassLayoutItem
{
    Q_OBJECT
public:
    GrassLayoutNorthArrowItem(double w, double h, QGraphicsItem *parent = nullptr);

    int     type()        const override { return kLayoutNorthArrow; }
    QString itemTypeStr() const override { return "northarrow";      }

protected:
    void paintContent(QPainter *p, const QRectF &r) override;
};

// ─── Text label ───────────────────────────────────────────────────────────────
class GrassLayoutTextItem : public GrassLayoutItem
{
    Q_OBJECT
public:
    GrassLayoutTextItem(const QString &text, double w, double h,
                         QGraphicsItem *parent = nullptr);

    int     type()        const override { return kLayoutText; }
    QString itemTypeStr() const override { return "text";      }
    void saveToXml  (QDomElement &el) const override;
    void loadFromXml(const QDomElement &el) override;

    QString text()  const { return mText;  }
    QFont   font()  const { return mFont;  }
    QColor  color() const { return mColor; }
    Qt::Alignment alignment() const { return mAlign; }

    void setText     (const QString &t)  { mText  = t; update(); }
    void setFont     (const QFont   &f)  { mFont  = f; update(); }
    void setColor    (const QColor  &c)  { mColor = c; update(); }
    void setAlignment(Qt::Alignment a)   { mAlign = a; update(); }

protected:
    void paintContent(QPainter *p, const QRectF &r) override;

private:
    QString       mText;
    QFont         mFont;
    QColor        mColor  = Qt::black;
    Qt::Alignment mAlign  = Qt::AlignLeft | Qt::AlignVCenter;
};

// ─── Image ────────────────────────────────────────────────────────────────────
class GrassLayoutImageItem : public GrassLayoutItem
{
    Q_OBJECT
public:
    GrassLayoutImageItem(const QString &path, double w, double h,
                          QGraphicsItem *parent = nullptr);

    int     type()        const override { return kLayoutImage; }
    QString itemTypeStr() const override { return "image";      }
    void saveToXml  (QDomElement &el) const override;
    void loadFromXml(const QDomElement &el) override;

    QString filePath() const { return mPath; }
    void    setFilePath(const QString &path);

protected:
    void paintContent(QPainter *p, const QRectF &r) override;

private:
    QString mPath;
    QPixmap mPixmap;
};
