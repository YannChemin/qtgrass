#include "grassnvizwidget.h"

#include <QApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <limits>

extern "C" {
#include <grass/gis.h>
#include <grass/raster.h>
}

// ── GLSL shaders ─────────────────────────────────────────────────────────────

static const char *kVert = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec3 aNormal;

uniform mat4  uMVP;
uniform float uZExag;
uniform vec3  uLightDir;

out vec3 vColor;
out float vLight;

void main() {
    vec3 p = vec3(aPos.x, aPos.y, aPos.z * uZExag);
    gl_Position = uMVP * vec4(p, 1.0);

    // Re-scale normal Z by exaggeration factor then normalise
    vec3 n = normalize(vec3(aNormal.x, aNormal.y, aNormal.z / max(uZExag, 0.01)));
    vLight = 0.25 + 0.75 * max(dot(n, normalize(uLightDir)), 0.0);
    vColor = aColor;
}
)glsl";

static const char *kFrag = R"glsl(
#version 330 core
in vec3  vColor;
in float vLight;
out vec4 fragColor;
void main() {
    fragColor = vec4(vColor * vLight, 1.0);
}
)glsl";

// ─────────────────────────────────────────────────────────────────────────────

GrassNvizWidget::GrassNvizWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setMinimumSize(400, 300);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setSamples(4);
    setFormat(fmt);
}

GrassNvizWidget::~GrassNvizWidget()
{
    makeCurrent();
    mVao.destroy();
    mVbo.destroy();
    mEbo.destroy();
    delete mProg;
    doneCurrent();
}

// ── OpenGL lifecycle ──────────────────────────────────────────────────────────

void GrassNvizWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glClearColor(0.18f, 0.20f, 0.25f, 1.0f);

    mProg = new QOpenGLShaderProgram(this);
    if (!mProg->addShaderFromSourceCode(QOpenGLShader::Vertex,   kVert) ||
        !mProg->addShaderFromSourceCode(QOpenGLShader::Fragment, kFrag) ||
        !mProg->link())
    {
        mError = "Shader error: " + mProg->log();
        return;
    }

    mVao.create();
    mVbo.create();
    mEbo.create();
}

void GrassNvizWidget::resizeGL(int w, int h)
{
    mProjection.setToIdentity();
    mProjection.perspective(45.0f, float(w) / float(h ? h : 1), 0.01f, 100.0f);
}

void GrassNvizWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!mProg || mIndexCount == 0) return;

    // Build view matrix (orbit camera)
    const float azR  = qDegreesToRadians(mAzim);
    const float elR  = qDegreesToRadians(mElev);
    const QVector3D eye = mCenter + mDist * QVector3D(
        qSin(azR) * qCos(elR),
        qCos(azR) * qCos(elR),
        qSin(elR));

    QMatrix4x4 view;
    view.lookAt(eye, mCenter, QVector3D(0, 0, 1));

    const QMatrix4x4 mvp = mProjection * view;

    mProg->bind();
    mProg->setUniformValue("uMVP",      mvp);
    mProg->setUniformValue("uZExag",    mZExag);
    mProg->setUniformValue("uLightDir", mLightDir);

    mVao.bind();
    glPolygonMode(GL_FRONT_AND_BACK, mWireframe ? GL_LINE : GL_FILL);
    glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, nullptr);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    mVao.release();
    mProg->release();
}

// ── Mouse / wheel input ───────────────────────────────────────────────────────

void GrassNvizWidget::mousePressEvent(QMouseEvent *e)
{
    mLastMouse = e->pos();
    mOrbiting  = (e->button() == Qt::LeftButton);
    mPanning   = (e->button() == Qt::RightButton);
}

void GrassNvizWidget::mouseMoveEvent(QMouseEvent *e)
{
    const QPoint delta = e->pos() - mLastMouse;
    mLastMouse = e->pos();

    if (mOrbiting)
    {
        mAzim -= delta.x() * 0.5f;
        mElev  = qBound(-89.0f, mElev + delta.y() * 0.5f, 89.0f);
        update();
    }
    else if (mPanning)
    {
        // Move centre in the horizontal plane
        const float speed = mDist * 0.002f;
        const float azR   = qDegreesToRadians(mAzim);
        const QVector3D right(qCos(azR),  -qSin(azR), 0);
        const QVector3D fwd  (-qSin(azR), -qCos(azR), 0);
        mCenter += -right * (delta.x() * speed) + fwd * (delta.y() * speed);
        update();
    }
}

void GrassNvizWidget::mouseReleaseEvent(QMouseEvent *)
{
    mOrbiting = mPanning = false;
}

void GrassNvizWidget::wheelEvent(QWheelEvent *e)
{
    const float factor = e->angleDelta().y() > 0 ? 0.85f : 1.18f;
    mDist = qBound(0.05f, mDist * factor, 50.0f);
    update();
}

// ── Data loading ──────────────────────────────────────────────────────────────

bool GrassNvizWidget::loadElevation(const QString &elevMap,
                                     const QString &colorMap)
{
    // Parse "name@mapset"
    const QString effColor = colorMap.isEmpty() ? elevMap : colorMap;
    auto parseRef = [](const QString &ref) -> QPair<QByteArray, QByteArray> {
        const int at = ref.indexOf('@');
        if (at >= 0)
            return { ref.left(at).toLocal8Bit(), ref.mid(at+1).toLocal8Bit() };
        return { ref.toLocal8Bit(), {} };
    };

    const auto [elevName, elevMapset] = parseRef(elevMap);
    const auto [colName,  colMapset]  = parseRef(effColor);

    // Cell header for elevation
    struct Cell_head elevHd;
    Rast_get_cellhd(elevName.constData(), elevMapset.constData(), &elevHd);

    // Subsample to at most 256 × 256
    const int subR = qMax(1, elevHd.rows / 256);
    const int subC = qMax(1, elevHd.cols / 256);
    const int NR   = (elevHd.rows + subR - 1) / subR;
    const int NC   = (elevHd.cols + subC - 1) / subC;

    // Open elevation raster
    const int fdElev = Rast_open_old(elevName.constData(), elevMapset.constData());
    if (fdElev < 0)
    {
        mError = "Cannot open elevation raster: " + elevMap;
        return false;
    }

    // Open color raster (may be the same)
    const int fdCol  = Rast_open_old(colName.constData(), colMapset.constData());

    // Read colour table for color map
    struct Colors colors;
    Rast_read_colors(colName.constData(), colMapset.constData(), &colors);

    DCELL *elevRow = Rast_allocate_d_buf();
    DCELL *colRow  = Rast_allocate_d_buf();

    QVector<float>   raw (NR * NC, 0.0f);
    QVector<uint8_t> rgba(NR * NC * 3, 128);

    const double west   = elevHd.west;
    const double south  = elevHd.south;
    const double ewRes  = elevHd.ew_res  * subC;
    const double nsRes  = elevHd.ns_res  * subR;

    double minZ =  std::numeric_limits<double>::max();
    double maxZ = -std::numeric_limits<double>::max();

    for (int ri = 0; ri < NR; ++ri)
    {
        const int grassRow = ri * subR;
        Rast_get_d_row(fdElev, elevRow, grassRow);
        if (fdCol >= 0) Rast_get_d_row(fdCol, colRow, grassRow);

        for (int ci = 0; ci < NC; ++ci)
        {
            const int gc  = ci * subC;
            const DCELL z = elevRow[gc];

            if (Rast_is_d_null_value(&z))
            {
                raw[ri * NC + ci] = std::numeric_limits<float>::quiet_NaN();
            }
            else
            {
                raw[ri * NC + ci] = static_cast<float>(z);
                minZ = qMin(minZ, (double)z);
                maxZ = qMax(maxZ, (double)z);
            }

            // Colour
            const DCELL cv = (fdCol >= 0) ? colRow[gc] : z;
            int r = 128, g = 128, b = 128;
            if (!Rast_is_d_null_value(&cv))
                Rast_get_d_color(&cv, &r, &g, &b, &colors);
            const int base = (ri * NC + ci) * 3;
            rgba[base]     = static_cast<uint8_t>(r);
            rgba[base + 1] = static_cast<uint8_t>(g);
            rgba[base + 2] = static_cast<uint8_t>(b);
        }
    }

    Rast_close(fdElev);
    if (fdCol >= 0 && fdCol != fdElev) Rast_close(fdCol);
    G_free(elevRow);
    G_free(colRow);
    Rast_free_colors(&colors);

    if (minZ > maxZ) { minZ = 0; maxZ = 1; }

    buildMesh(raw, rgba, NR, NC, west, south, ewRes, nsRes);
    emit loadingDone(true, QString("Loaded %1 × %2 vertices").arg(NR).arg(NC));
    return true;
}

// ── Mesh building ─────────────────────────────────────────────────────────────

void GrassNvizWidget::buildMesh(const QVector<float> &raw,
                                 const QVector<uint8_t> &rgba,
                                 int NR, int NC,
                                 double west, double south,
                                 double ewRes, double nsRes)
{
    // Compute bounding box in geographic coords
    double minX = west,             maxX = west + NC * ewRes;
    double minY = south,            maxY = south + NR * nsRes;
    double minZ = std::numeric_limits<double>::max();
    double maxZ = -std::numeric_limits<double>::max();
    for (float v : raw)
        if (!std::isnan(v)) { minZ = qMin(minZ, (double)v); maxZ = qMax(maxZ, (double)v); }
    if (minZ > maxZ) { minZ = 0; maxZ = 1; }

    // Scale so the XY extent fits in [-0.5, 0.5]
    const double extXY = qMax(maxX - minX, maxY - minY);
    mMeshScale = static_cast<float>(extXY > 0 ? 1.0 / extXY : 1.0);
    mZScale    = mMeshScale;  // same so terrain is isotropic initially

    const float cx = static_cast<float>((minX + maxX) / 2);
    const float cy = static_cast<float>((minY + maxY) / 2);
    const float cz = static_cast<float>(minZ);

    // Build position array (NR rows, NC cols)
    // Row 0 = south (bottom of raster), row NR-1 = north
    struct Vert { float x, y, z; };
    QVector<Vert> pos(NR * NC);
    for (int ri = 0; ri < NR; ++ri)
        for (int ci = 0; ci < NC; ++ci)
        {
            const float gx = static_cast<float>(west  + ci * ewRes + ewRes * 0.5);
            const float gy = static_cast<float>(south + ri * nsRes + nsRes * 0.5);
            const float gz = raw[ri * NC + ci];
            pos[ri * NC + ci] = {
                (gx - cx) * mMeshScale,
                (gy - cy) * mMeshScale,
                std::isnan(gz) ? 0.0f : (gz - cz) * mZScale
            };
        }

    // Compute per-vertex normals via cross products of neighbours
    QVector<QVector3D> normals(NR * NC, QVector3D(0, 0, 1));
    for (int ri = 0; ri < NR; ++ri)
        for (int ci = 0; ci < NC; ++ci)
        {
            const int idx = ri * NC + ci;
            const auto &p  = pos[idx];
            QVector3D dx, dy;
            auto toV = [&](int r, int c) {
                const auto &v = pos[r * NC + c];
                return QVector3D(v.x, v.y, v.z);
            };
            if (ci < NC-1) dx = toV(ri, ci+1) - QVector3D(p.x,p.y,p.z);
            else            dx = QVector3D(p.x,p.y,p.z) - toV(ri, ci-1);
            if (ri < NR-1) dy = toV(ri+1, ci) - QVector3D(p.x,p.y,p.z);
            else            dy = QVector3D(p.x,p.y,p.z) - toV(ri-1, ci);
            normals[idx] = QVector3D::crossProduct(dx, dy).normalized();
        }

    // Interleaved vertex data: pos(3) + color(3) + normal(3)
    mVertexData.resize(NR * NC * 9);
    for (int i = 0; i < NR * NC; ++i)
    {
        float *vp = mVertexData.data() + i * 9;
        vp[0] = pos[i].x; vp[1] = pos[i].y; vp[2] = pos[i].z;
        vp[3] = rgba[i*3]   / 255.0f;
        vp[4] = rgba[i*3+1] / 255.0f;
        vp[5] = rgba[i*3+2] / 255.0f;
        vp[6] = normals[i].x();
        vp[7] = normals[i].y();
        vp[8] = normals[i].z();
    }

    // Build triangle index list
    mIndexData.clear();
    mIndexData.reserve((NR-1) * (NC-1) * 6);
    for (int ri = 0; ri < NR-1; ++ri)
        for (int ci = 0; ci < NC-1; ++ci)
        {
            const uint32_t tl = ri * NC + ci;
            const uint32_t tr = ri * NC + ci + 1;
            const uint32_t bl = (ri+1) * NC + ci;
            const uint32_t br = (ri+1) * NC + ci + 1;
            mIndexData << tl << bl << tr
                       << tr << bl << br;
        }

    // Reset camera to view the full terrain
    mCenter = QVector3D(0, 0, 0.1f);
    mDist   = 1.8f;
    mAzim   = 225.0f;
    mElev   = 30.0f;

    makeCurrent();
    uploadMesh();
    doneCurrent();
    update();
}

void GrassNvizWidget::uploadMesh()
{
    mVao.bind();

    mVbo.bind();
    mVbo.allocate(mVertexData.constData(),
                  mVertexData.size() * sizeof(float));

    mEbo.bind();
    mEbo.allocate(mIndexData.constData(),
                  mIndexData.size() * sizeof(uint32_t));

    constexpr int stride = 9 * sizeof(float);
    // Position (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    // Colour (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Normal (location 2)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    mVao.release();
    mIndexCount = mIndexData.size();
}

// ── Public controls ───────────────────────────────────────────────────────────

void GrassNvizWidget::setExaggeration(double z)
{
    mZExag = static_cast<float>(qMax(0.01, z));
    update();
}

void GrassNvizWidget::setWireframe(bool on)
{
    mWireframe = on;
    update();
}

void GrassNvizWidget::resetView()
{
    mCenter = QVector3D(0, 0, 0.1f);
    mDist   = 1.8f;
    mAzim   = 225.0f;
    mElev   = 30.0f;
    update();
}
