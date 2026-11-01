#include "grasslayoutitems.h"
#include "../canvas/grassmapcanvas.h"
#include "../canvas/grassmaplayer.h"

#include <QApplication>
#include <QCursor>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>
#include <QtMath>

// ─── GrassLayoutItem ──────────────────────────────────────────────────────────

GrassLayoutItem::GrassLayoutItem(double w, double h, QGraphicsItem *parent)
    : QGraphicsObject(parent), mW(w), mH(h)
{
    setFlags(ItemIsSelectable | ItemIsMovable |
             ItemSendsGeometryChanges | ItemAcceptsInputMethod);
    setAcceptHoverEvents(true);
    setCacheMode(NoCache);
}

QRectF GrassLayoutItem::boundingRect() const
{
    return QRectF(-kHS / 2, -kHS / 2, mW + kHS, mH + kHS);
}

QPainterPath GrassLayoutItem::shape() const
{
    QPainterPath p;
    p.addRect(QRectF(0, 0, mW, mH));
    return p;
}

void GrassLayoutItem::setItemSize(double w, double h)
{
    prepareGeometryChange();
    mW = qMax(5.0, w);
    mH = qMax(5.0, h);
    update();
    emit geometryChanged();
}

void GrassLayoutItem::saveToXml(QDomElement &el) const
{
    el.setAttribute("type", itemTypeStr());
    el.setAttribute("x",    pos().x());
    el.setAttribute("y",    pos().y());
    el.setAttribute("w",    mW);
    el.setAttribute("h",    mH);
    el.setAttribute("z",    zValue());
}

void GrassLayoutItem::loadFromXml(const QDomElement &el)
{
    setPos(el.attribute("x").toDouble(), el.attribute("y").toDouble());
    setItemSize(el.attribute("w", "60").toDouble(),
                el.attribute("h", "40").toDouble());
    setZValue(el.attribute("z", "0").toDouble());
}

void GrassLayoutItem::paint(QPainter *p,
                              const QStyleOptionGraphicsItem *opt, QWidget *)
{
    p->setRenderHint(QPainter::Antialiasing, false);
    const QRectF r(0, 0, mW, mH);

    // Content painted by subclass
    paintContent(p, r);

    // Selection / border overlay
    const bool sel = opt->state & QStyle::State_Selected;
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(sel ? QColor(30, 130, 255) : QColor(160, 160, 160), 0.4));
    p->drawRect(r);

    if (sel)
    {
        // Draw 8 resize handles
        for (int h = TL; h <= BR; ++h)
        {
            const QRectF hr = handleRect(static_cast<Handle>(h));
            p->fillRect(hr, QColor(30, 130, 255));
            p->setPen(QPen(Qt::white, 0.3));
            p->drawRect(hr);
        }
    }
}

QVariant GrassLayoutItem::itemChange(GraphicsItemChange change,
                                      const QVariant &value)
{
    if (change == ItemPositionHasChanged) emit geometryChanged();
    return QGraphicsObject::itemChange(change, value);
}

// ─── Resize handle helpers ────────────────────────────────────────────────────

QRectF GrassLayoutItem::handleRect(Handle h) const
{
    const double s = kHS;
    const double cx = mW / 2, cy = mH / 2;
    switch (h)
    {
    case TL: return QRectF(    0 - s/2,     0 - s/2, s, s);
    case TM: return QRectF(cx     - s/2,     0 - s/2, s, s);
    case TR: return QRectF(mW     - s/2,     0 - s/2, s, s);
    case ML: return QRectF(    0 - s/2, cy     - s/2, s, s);
    case MR: return QRectF(mW     - s/2, cy     - s/2, s, s);
    case BL: return QRectF(    0 - s/2, mH     - s/2, s, s);
    case BM: return QRectF(cx     - s/2, mH     - s/2, s, s);
    case BR: return QRectF(mW     - s/2, mH     - s/2, s, s);
    default: return {};
    }
}

GrassLayoutItem::Handle GrassLayoutItem::handleAt(const QPointF &p) const
{
    for (int h = TL; h <= BR; ++h)
        if (handleRect(static_cast<Handle>(h)).contains(p))
            return static_cast<Handle>(h);
    return None;
}

void GrassLayoutItem::applyResize(Handle h, const QPointF &delta)
{
    double newW = mWAtDrag, newH = mHAtDrag;
    QPointF newPos = mPosAtDrag;

    switch (h)
    {
    case TL: newPos.rx() += delta.x(); newPos.ry() += delta.y();
             newW -= delta.x(); newH -= delta.y(); break;
    case TM: newPos.ry() += delta.y(); newH -= delta.y(); break;
    case TR: newW += delta.x(); newPos.ry() += delta.y(); newH -= delta.y(); break;
    case ML: newPos.rx() += delta.x(); newW -= delta.x(); break;
    case MR: newW += delta.x(); break;
    case BL: newPos.rx() += delta.x(); newW -= delta.x(); newH += delta.y(); break;
    case BM: newH += delta.y(); break;
    case BR: newW += delta.x(); newH += delta.y(); break;
    default: break;
    }

    newW = qMax(10.0, newW);
    newH = qMax(5.0,  newH);

    prepareGeometryChange();
    setPos(newPos);
    mW = newW;
    mH = newH;
    update();
    emit geometryChanged();
}

void GrassLayoutItem::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && isSelected())
    {
        mDragHandle = handleAt(e->pos());
        if (mDragHandle != None)
        {
            mDragStart  = e->scenePos();
            mWAtDrag    = mW;
            mHAtDrag    = mH;
            mPosAtDrag  = pos();
            e->accept();
            return;
        }
    }
    QGraphicsObject::mousePressEvent(e);
}

void GrassLayoutItem::mouseMoveEvent(QGraphicsSceneMouseEvent *e)
{
    if (mDragHandle != None)
    {
        applyResize(mDragHandle, e->scenePos() - mDragStart);
        e->accept();
        return;
    }
    QGraphicsObject::mouseMoveEvent(e);
}

void GrassLayoutItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *e)
{
    mDragHandle = None;
    QGraphicsObject::mouseReleaseEvent(e);
}

void GrassLayoutItem::hoverMoveEvent(QGraphicsSceneHoverEvent *e)
{
    if (!isSelected()) { unsetCursor(); return; }

    const Handle h = handleAt(e->pos());
    switch (h)
    {
    case TL: case BR: setCursor(Qt::SizeFDiagCursor);  break;
    case TR: case BL: setCursor(Qt::SizeBDiagCursor);  break;
    case TM: case BM: setCursor(Qt::SizeVerCursor);    break;
    case ML: case MR: setCursor(Qt::SizeHorCursor);    break;
    default:          unsetCursor();                   break;
    }
}

// ─── GrassLayoutMapItem ───────────────────────────────────────────────────────

GrassLayoutMapItem::GrassLayoutMapItem(GrassMapCanvas *canvas,
                                        double w, double h,
                                        QGraphicsItem *parent)
    : GrassLayoutItem(w, h, parent), mCanvas(canvas)
{
    setToolTip("Map Frame — shows current canvas view");
}

void GrassLayoutMapItem::saveToXml(QDomElement &el) const
{
    GrassLayoutItem::saveToXml(el);
}

void GrassLayoutMapItem::paintContent(QPainter *p, const QRectF &r)
{
    if (mCanvas)
    {
        QPixmap pix = mCanvas->grab();
        if (!pix.isNull())
        {
            p->drawPixmap(r, pix, QRectF(pix.rect()));
            return;
        }
    }
    p->fillRect(r, QColor(210, 215, 225));
    p->setPen(QColor(100, 110, 130));
    QFont f; f.setPointSize(10);
    p->setFont(f);
    p->drawText(r, Qt::AlignCenter, "Map Frame");
}

// ─── GrassLayoutLegendItem ────────────────────────────────────────────────────

GrassLayoutLegendItem::GrassLayoutLegendItem(GrassMapCanvas *canvas,
                                               double w, double h,
                                               QGraphicsItem *parent)
    : GrassLayoutItem(w, h, parent), mCanvas(canvas)
{
    mFont.setPointSize(8);
    setToolTip("Legend — layer names from canvas");
}

void GrassLayoutLegendItem::saveToXml(QDomElement &el) const
{
    GrassLayoutItem::saveToXml(el);
    el.setAttribute("title",    mTitle);
    el.setAttribute("fontSize", mFont.pointSize());
}

void GrassLayoutLegendItem::loadFromXml(const QDomElement &el)
{
    GrassLayoutItem::loadFromXml(el);
    mTitle = el.attribute("title", "Legend");
    mFont.setPointSize(el.attribute("fontSize", "8").toInt());
}

void GrassLayoutLegendItem::paintContent(QPainter *p, const QRectF &r)
{
    p->fillRect(r, QColor(255, 255, 255, 230));

    // Title
    QFont titleFont = mFont;
    titleFont.setBold(true);
    titleFont.setPointSize(mFont.pointSize() + 1);
    p->setFont(titleFont);
    p->setPen(Qt::black);
    p->drawText(QRectF(r.x() + 3, r.y() + 3, r.width() - 6, 12),
                Qt::AlignLeft | Qt::AlignVCenter, mTitle);

    if (!mCanvas) return;

    double y = r.y() + 18;
    QFont lf = mFont;
    p->setFont(lf);
    const double entryH = mFont.pointSize() + 6;

    for (GrassMapLayer *layer : mCanvas->layers())
    {
        if (!layer->visible())   continue;
        if (y + entryH > r.bottom() - 2) break;

        // Colour swatch
        QColor swatchCol = QColor(100, 150, 220); // default blue
        p->fillRect(QRectF(r.x() + 3, y + 2, entryH - 4, entryH - 4), swatchCol);
        p->setPen(QPen(Qt::darkGray, 0.3));
        p->drawRect(QRectF(r.x() + 3, y + 2, entryH - 4, entryH - 4));

        // Layer name
        p->setPen(Qt::black);
        p->drawText(QRectF(r.x() + entryH + 2, y, r.width() - entryH - 5, entryH),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    p->fontMetrics().elidedText(layer->name(), Qt::ElideRight,
                                                static_cast<int>(r.width() - entryH - 6)));
        y += entryH + 1;
    }
}

// ─── GrassLayoutScaleBarItem ──────────────────────────────────────────────────

GrassLayoutScaleBarItem::GrassLayoutScaleBarItem(GrassMapCanvas *canvas,
                                                   GrassLayoutMapItem *mapFrame,
                                                   double w, double h,
                                                   QGraphicsItem *parent)
    : GrassLayoutItem(w, h, parent), mCanvas(canvas), mMapFrame(mapFrame)
{
    setToolTip("Scale Bar");
}

void GrassLayoutScaleBarItem::saveToXml(QDomElement &el) const
{
    GrassLayoutItem::saveToXml(el);
    el.setAttribute("units",    mUnits);
    el.setAttribute("segments", mSegments);
}

void GrassLayoutScaleBarItem::loadFromXml(const QDomElement &el)
{
    GrassLayoutItem::loadFromXml(el);
    mUnits    = el.attribute("units", "m");
    mSegments = el.attribute("segments", "4").toInt();
}

double GrassLayoutScaleBarItem::niceNumber(double x)
{
    if (x <= 0) return 1;
    const double p = std::pow(10, std::floor(std::log10(x)));
    const double f = x / p;
    double nice = 10;
    if      (f < 1.5) nice = 1;
    else if (f < 3.0) nice = 2;
    else if (f < 7.0) nice = 5;
    return nice * p;
}

QString GrassLayoutScaleBarItem::formatDist(double mapUnits, const QString &units)
{
    if (units == "km")
        return QString("%1 km").arg(mapUnits / 1000.0, 0, 'f',
                                    mapUnits >= 10000 ? 0 : 1);
    if (units == "mi")
        return QString("%1 mi").arg(mapUnits / 1609.344, 0, 'f', 1);
    // metres
    if (mapUnits >= 1000)
        return QString("%1 km").arg(mapUnits / 1000.0, 0, 'f',
                                    mapUnits >= 10000 ? 0 : 1);
    return QString("%1 m").arg(mapUnits, 0, 'f', mapUnits >= 100 ? 0 : 1);
}

void GrassLayoutScaleBarItem::paintContent(QPainter *p, const QRectF &r)
{
    p->setRenderHint(QPainter::Antialiasing, false);

    // Compute scale ratio (map units per mm)
    double scaleUperMM = 1.0;
    if (mCanvas && mMapFrame)
    {
        const auto ext      = mCanvas->viewportExtent();
        const double geoW   = ext.width();
        const double frameW = mMapFrame->itemW(); // mm
        if (frameW > 0) scaleUperMM = geoW / frameW;
    }

    // Target bar length: 80% of item width
    const double targetBarMM  = r.width() * 0.8;
    const double targetBarGeo = targetBarMM * scaleUperMM;
    const double niceGeo      = niceNumber(targetBarGeo);
    const double barMM        = niceGeo / scaleUperMM;

    const double barH = r.height() * 0.4;
    const double barY = r.center().y() - barH / 2;
    const double barX = r.x() + (r.width() - barMM) / 2;

    const int segs = qMax(1, mSegments);
    const double segW = barMM / segs;

    for (int i = 0; i < segs; ++i)
    {
        const QRectF segR(barX + i * segW, barY, segW, barH);
        p->fillRect(segR, (i % 2 == 0) ? Qt::black : Qt::white);
        p->setPen(QPen(Qt::black, 0.4));
        p->drawRect(segR);
    }

    // Labels
    QFont f; f.setPointSize(6); p->setFont(f); p->setPen(Qt::black);
    p->drawText(QRectF(barX - 6, barY - 9, 12, 8), Qt::AlignCenter, "0");
    const QString rightLabel = formatDist(niceGeo, mUnits);
    p->drawText(QRectF(barX + barMM - 12, barY - 9, 24, 8),
                Qt::AlignCenter, rightLabel);
}

// ─── GrassLayoutNorthArrowItem ────────────────────────────────────────────────

GrassLayoutNorthArrowItem::GrassLayoutNorthArrowItem(double w, double h,
                                                       QGraphicsItem *parent)
    : GrassLayoutItem(w, h, parent)
{
    setToolTip("North Arrow");
}

void GrassLayoutNorthArrowItem::paintContent(QPainter *p, const QRectF &r)
{
    p->setRenderHint(QPainter::Antialiasing);
    p->fillRect(r, Qt::transparent);

    const QPointF ctr = r.center();
    const double ah   = r.height() * 0.65;  // arrow height
    const double aw   = r.width()  * 0.28;  // arrow half-width
    const double tip  = ctr.y() - ah / 2;
    const double base = ctr.y() + ah / 4;

    // Left (dark) half
    QPainterPath left;
    left.moveTo(ctr.x(), tip);
    left.lineTo(ctr.x(), ctr.y());
    left.lineTo(ctr.x() - aw, base);
    left.closeSubpath();
    p->fillPath(left, Qt::black);

    // Right (light) half
    QPainterPath right;
    right.moveTo(ctr.x(), tip);
    right.lineTo(ctr.x() + aw, base);
    right.lineTo(ctr.x(), ctr.y());
    right.closeSubpath();
    p->fillPath(right, QColor(220, 220, 220));

    // Outline
    p->setPen(QPen(Qt::black, 0.5));
    QPainterPath outline;
    outline.moveTo(ctr.x(), tip);
    outline.lineTo(ctr.x() + aw, base);
    outline.lineTo(ctr.x(), ctr.y());
    outline.lineTo(ctr.x() - aw, base);
    outline.closeSubpath();
    p->drawPath(outline);

    // "N" label below arrow
    QFont f; f.setBold(true); f.setPointSize(7);
    p->setFont(f); p->setPen(Qt::black);
    const double labelY = ctr.y() + ah / 2 - 2;
    p->drawText(QRectF(ctr.x() - 8, labelY, 16, 12), Qt::AlignCenter, "N");
}

// ─── GrassLayoutTextItem ──────────────────────────────────────────────────────

GrassLayoutTextItem::GrassLayoutTextItem(const QString &text,
                                           double w, double h,
                                           QGraphicsItem *parent)
    : GrassLayoutItem(w, h, parent), mText(text)
{
    mFont.setPointSize(12);
    setToolTip("Text label");
}

void GrassLayoutTextItem::saveToXml(QDomElement &el) const
{
    GrassLayoutItem::saveToXml(el);
    el.setAttribute("text",      mText);
    el.setAttribute("fontSize",  mFont.pointSize());
    el.setAttribute("bold",      mFont.bold() ? 1 : 0);
    el.setAttribute("italic",    mFont.italic() ? 1 : 0);
    el.setAttribute("color",     mColor.name());
    el.setAttribute("align",     static_cast<int>(mAlign));
}

void GrassLayoutTextItem::loadFromXml(const QDomElement &el)
{
    GrassLayoutItem::loadFromXml(el);
    mText  = el.attribute("text", "Text");
    mFont.setPointSize(el.attribute("fontSize", "12").toInt());
    mFont.setBold  (el.attribute("bold",   "0").toInt() != 0);
    mFont.setItalic(el.attribute("italic", "0").toInt() != 0);
    mColor = QColor(el.attribute("color", "#000000"));
    mAlign = static_cast<Qt::Alignment>(el.attribute("align", "0").toInt());
    if (!mAlign) mAlign = Qt::AlignLeft | Qt::AlignVCenter;
}

void GrassLayoutTextItem::paintContent(QPainter *p, const QRectF &r)
{
    p->setFont(mFont);
    p->setPen(mColor);
    p->setRenderHint(QPainter::TextAntialiasing);
    p->drawText(r.adjusted(2, 1, -2, -1), mAlign | Qt::TextWordWrap, mText);
}

// ─── GrassLayoutImageItem ─────────────────────────────────────────────────────

GrassLayoutImageItem::GrassLayoutImageItem(const QString &path,
                                             double w, double h,
                                             QGraphicsItem *parent)
    : GrassLayoutItem(w, h, parent)
{
    setFilePath(path);
    setToolTip("Image");
}

void GrassLayoutImageItem::setFilePath(const QString &path)
{
    mPath = path;
    mPixmap = path.isEmpty() ? QPixmap() : QPixmap(path);
    update();
}

void GrassLayoutImageItem::saveToXml(QDomElement &el) const
{
    GrassLayoutItem::saveToXml(el);
    el.setAttribute("path", mPath);
}

void GrassLayoutImageItem::loadFromXml(const QDomElement &el)
{
    GrassLayoutItem::loadFromXml(el);
    setFilePath(el.attribute("path"));
}

void GrassLayoutImageItem::paintContent(QPainter *p, const QRectF &r)
{
    if (mPixmap.isNull())
    {
        p->fillRect(r, QColor(230, 230, 230));
        p->setPen(Qt::gray);
        p->drawText(r, Qt::AlignCenter, "Image\n(no file)");
    }
    else
    {
        p->drawPixmap(r, mPixmap, QRectF(mPixmap.rect()));
    }
}
