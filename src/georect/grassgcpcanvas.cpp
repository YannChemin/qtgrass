#include "../grassenv.h"
#include "grassgcpcanvas.h"

#include <QDir>
#include <QMouseEvent>
#include <QPainter>
#include <QProcess>
#include <QTemporaryFile>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <cmath>

// ─── PolyTransform ────────────────────────────────────────────────────────────

QVector<double> PolyTransform::basis(double x, double y, int order)
{
    QVector<double> t;
    for (int i = 0; i <= order; ++i)
        for (int j = 0; j <= i; ++j)
            t.append(std::pow(x, i - j) * std::pow(y, j));
    return t;
    // order=1: [1, x, y]
    // order=2: [1, x, y, x², xy, y²]
    // order=3: [1, x, y, x², xy, y², x³, x²y, xy², y³]
}

bool PolyTransform::gaussElim(QVector<QVector<double>> A, QVector<double> &b)
{
    const int n = A.size();
    for (int col = 0; col < n; ++col)
    {
        // Partial pivot
        int piv = col;
        for (int r = col + 1; r < n; ++r)
            if (std::abs(A[r][col]) > std::abs(A[piv][col])) piv = r;
        std::swap(A[col], A[piv]);
        std::swap(b[col], b[piv]);

        if (std::abs(A[col][col]) < 1e-14) return false;

        for (int r = col + 1; r < n; ++r)
        {
            const double f = A[r][col] / A[col][col];
            for (int k = col; k < n; ++k) A[r][k] -= f * A[col][k];
            b[r] -= f * b[col];
        }
    }
    for (int r = n - 1; r >= 0; --r)
    {
        for (int c = r + 1; c < n; ++c) b[r] -= A[r][c] * b[c];
        b[r] /= A[r][r];
    }
    return true;
}

bool PolyTransform::fit(const QVector<GcpPoint> &gcps, int order)
{
    mValid = false;
    mOrder = order;
    const int nT = (order + 1) * (order + 2) / 2;

    // Count active GCPs
    int nA = 0;
    for (const auto &g : gcps) if (g.enabled) ++nA;
    if (nA < nT) return false;

    // Build normal equations  A^T A x = A^T b  (once for E, once for N)
    QVector<QVector<double>> ATA(nT, QVector<double>(nT, 0.0));
    QVector<double>          ATbE(nT, 0.0), ATbN(nT, 0.0);

    for (const auto &g : gcps)
    {
        if (!g.enabled) continue;
        const auto bs = basis(g.srcX, g.srcY, order);
        for (int i = 0; i < nT; ++i)
        {
            for (int j = 0; j < nT; ++j) ATA[i][j] += bs[i] * bs[j];
            ATbE[i] += bs[i] * g.refX;
            ATbN[i] += bs[i] * g.refY;
        }
    }

    auto ATA2 = ATA;
    mCoeffE   = ATbE;
    mCoeffN   = ATbN;

    if (!gaussElim(ATA, mCoeffE) || !gaussElim(ATA2, mCoeffN))
        return false;

    mValid = true;
    return true;
}

void PolyTransform::transform(double x, double y, double &e, double &n) const
{
    e = n = 0;
    if (!mValid) return;
    const auto bs = basis(x, y, mOrder);
    for (int i = 0; i < mCoeffE.size(); ++i) e += mCoeffE[i] * bs[i];
    for (int i = 0; i < mCoeffN.size(); ++i) n += mCoeffN[i] * bs[i];
}

void PolyTransform::computeResiduals(QVector<GcpPoint> &gcps) const
{
    for (auto &g : gcps)
    {
        if (!mValid || !g.enabled) { g.resX = g.resY = g.rms = 0; continue; }
        double predE, predN;
        transform(g.srcX, g.srcY, predE, predN);
        g.resX = predE - g.refX;
        g.resY = predN - g.refY;
        g.rms  = std::sqrt(g.resX * g.resX + g.resY * g.resY);
    }
}

double PolyTransform::totalRms(const QVector<GcpPoint> &gcps) const
{
    double sum = 0; int n = 0;
    for (const auto &g : gcps)
        if (g.enabled && g.rms > 0) { sum += g.rms * g.rms; ++n; }
    return n > 0 ? std::sqrt(sum / n) : 0.0;
}

// ─── GrassGcpCanvas ───────────────────────────────────────────────────────────

GrassGcpCanvas::GrassGcpCanvas(const QString &label, QWidget *parent)
    : QWidget(parent), mLabel(label)
{
    setMinimumSize(300, 250);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    updateCursor();
}

void GrassGcpCanvas::setMap(const QString &mapName)
{
    mMapName = mapName;
    mPixmap  = QPixmap();
    mMarkers.clear();
    update();
}

void GrassGcpCanvas::renderMap()
{
    if (mMapName.isEmpty()) return;

    // Get current region extent
    {
        QProcess p;
        p.setProcessChannelMode(QProcess::MergedChannels);
        p.start("g.region", {"-g"});
        if (p.waitForFinished(8000))
        {
            for (const QString &line :
                 QString::fromLocal8Bit(p.readAll()).split('\n', Qt::SkipEmptyParts))
            {
                if (line.startsWith("n="))  mExtN = line.mid(2).toDouble();
                if (line.startsWith("s="))  mExtS = line.mid(2).toDouble();
                if (line.startsWith("e="))  mExtE = line.mid(2).toDouble();
                if (line.startsWith("w="))  mExtW = line.mid(2).toDouble();
            }
        }
    }

    // Render to temp PNG
    QTemporaryFile tmp;
    tmp.setFileTemplate(QDir::tempPath() + "/qtgrass_gcpcanv_XXXXXX.png");
    tmp.setAutoRemove(false);
    if (!tmp.open()) return;
    const QString path = tmp.fileName();
    tmp.close();

    const int rw = qMax(100, width());
    const int rh = qMax(100, height());

    QProcessEnvironment env = GrassEnv::instance()->buildSubprocessEnv();
    env.insert("GRASS_RENDER_IMMEDIATE",       "cairo");
    env.insert("GRASS_RENDER_FILE",            path);
    env.insert("GRASS_RENDER_FILE_READ",       "FALSE");
    env.insert("GRASS_RENDER_WIDTH",           QString::number(rw));
    env.insert("GRASS_RENDER_HEIGHT",          QString::number(rh));
    env.insert("GRASS_RENDER_BACKGROUNDCOLOR", "white");

    QProcess p;
    p.setProcessEnvironment(env);
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("d.rast", {"map=" + mMapName});
    p.waitForFinished(20000);

    mPixmap.load(path);
    QFile::remove(path);

    zoomFit();
    update();
}

void GrassGcpCanvas::zoomFit()
{
    if (mPixmap.isNull()) return;
    const double sw = static_cast<double>(width());
    const double sh = static_cast<double>(height());
    const double pw = mPixmap.width();
    const double ph = mPixmap.height();

    mScale = qMin(sw / pw, sh / ph);
    mOffset = QPointF((sw - pw * mScale) / 2.0,
                      (sh - ph * mScale) / 2.0);
    update();
}

void GrassGcpCanvas::setGcps(const QVector<GcpPoint> &gcps, bool showSrc)
{
    mShowSrc = showSrc;
    mMarkers.clear();
    for (const auto &g : gcps)
    {
        Marker m;
        m.id     = g.id;
        m.active = g.enabled;
        m.east   = showSrc ? g.srcX : g.refX;
        m.north  = showSrc ? g.srcY : g.refY;
        mMarkers.append(m);
    }
    update();
}

void GrassGcpCanvas::updateCursor()
{
    setCursor(mPickMode ? Qt::CrossCursor : Qt::OpenHandCursor);
}

// ── Coordinate conversion ─────────────────────────────────────────────────────

QPointF GrassGcpCanvas::mapToWidget(double east, double north) const
{
    if (mPixmap.isNull()) return {};
    const double pw = mPixmap.width();
    const double ph = mPixmap.height();
    const double ex = (east  - mExtW) / (mExtE - mExtW);
    const double ey = (mExtN - north) / (mExtN - mExtS);
    return { mOffset.x() + ex * pw * mScale,
             mOffset.y() + ey * ph * mScale };
}

void GrassGcpCanvas::widgetToMap(QPointF px, double &east, double &north) const
{
    if (mPixmap.isNull()) { east = north = 0; return; }
    const double pw = mPixmap.width();
    const double ph = mPixmap.height();
    const double fx = (px.x() - mOffset.x()) / (pw * mScale);
    const double fy = (px.y() - mOffset.y()) / (ph * mScale);
    east  = mExtW + fx * (mExtE - mExtW);
    north = mExtN - fy * (mExtN - mExtS);
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void GrassGcpCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(50, 55, 65));

    if (mPixmap.isNull())
    {
        p.setPen(QColor(180, 180, 180));
        p.drawText(rect(), Qt::AlignCenter,
                   mLabel + "\n" + (mMapName.isEmpty() ? "(no map loaded)" : "Rendering…"));
        return;
    }

    // Draw pixmap
    p.drawPixmap(QRectF(mOffset.x(), mOffset.y(),
                        mPixmap.width()  * mScale,
                        mPixmap.height() * mScale),
                 mPixmap, QRectF(mPixmap.rect()));

    // Draw GCP markers
    p.setRenderHint(QPainter::Antialiasing);
    for (const Marker &m : mMarkers)
    {
        const QPointF wp = mapToWidget(m.east, m.north);
        const QColor col = m.active ? QColor(255, 50, 50) : QColor(150, 150, 150);
        const int arm = 7;
        p.setPen(QPen(Qt::black, 3));
        p.drawLine(wp + QPointF(-arm, 0), wp + QPointF(arm, 0));
        p.drawLine(wp + QPointF(0, -arm), wp + QPointF(0, arm));
        p.setPen(QPen(col, 1.5));
        p.drawLine(wp + QPointF(-arm, 0), wp + QPointF(arm, 0));
        p.drawLine(wp + QPointF(0, -arm), wp + QPointF(0, arm));

        // ID label
        p.setPen(col);
        QFont f; f.setPointSize(7); f.setBold(true); p.setFont(f);
        p.drawText(wp + QPointF(arm + 2, -arm), QString::number(m.id));
    }

    // Label overlay (top-left)
    p.setPen(Qt::white);
    QFont lf; lf.setPointSize(8); p.setFont(lf);
    p.fillRect(QRectF(2, 2, 120, 16), QColor(0, 0, 0, 120));
    p.drawText(QRectF(4, 2, 120, 16), Qt::AlignVCenter, mLabel);

    // Pick-mode indicator
    if (mPickMode)
    {
        p.setPen(QPen(QColor(255, 200, 0), 1.5));
        p.drawRect(rect().adjusted(1, 1, -2, -2));
    }
}

void GrassGcpCanvas::resizeEvent(QResizeEvent *)
{
    // Keep the view centered when the widget resizes
    if (!mPixmap.isNull()) zoomFit();
}

// ── Mouse events ──────────────────────────────────────────────────────────────

void GrassGcpCanvas::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
    {
        if (mPickMode)
        {
            double east, north;
            widgetToMap(e->position(), east, north);
            emit coordClicked(east, north);
            return;
        }
        mPanning   = true;
        mLastMouse = e->position();
        setCursor(Qt::ClosedHandCursor);
    }
}

void GrassGcpCanvas::mouseMoveEvent(QMouseEvent *e)
{
    if (mPanning)
    {
        mOffset += e->position() - mLastMouse;
        mLastMouse = e->position();
        update();
    }
}

void GrassGcpCanvas::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
    {
        mPanning = false;
        updateCursor();
    }
}

void GrassGcpCanvas::wheelEvent(QWheelEvent *e)
{
    const double factor = e->angleDelta().y() > 0 ? 1.2 : 1.0 / 1.2;
    const QPointF mp = e->position();

    // Zoom around mouse position
    mOffset = mp + (mOffset - mp) * factor;
    mScale *= factor;
    update();
}
