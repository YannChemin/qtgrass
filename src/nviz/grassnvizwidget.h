#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QVector3D>
#include <QPoint>
#include <QString>
#include <QVector>

// ── 3-D terrain view via OpenGL 3.3 ──────────────────────────────────────────
// Reads a GRASS raster as a height field, builds a triangulated mesh with
// per-vertex colours from the GRASS colour table, and renders it with
// orbit-camera navigation and diffuse shading.
class GrassNvizWidget : public QOpenGLWidget,
                         protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT
public:
    explicit GrassNvizWidget(QWidget *parent = nullptr);
    ~GrassNvizWidget() override;

    // Load an elevation map (name@mapset); also sets the colour map.
    // Pass a separate colorMap to drape another raster over the surface.
    bool loadElevation(const QString &elevMap,
                       const QString &colorMap = {});

    void setExaggeration(double z);
    void setWireframe(bool on);
    void resetView();

    QString lastError() const { return mError; }

signals:
    void loadingDone(bool ok, const QString &msg);

protected:
    void initializeGL()              override;
    void resizeGL(int w, int h)      override;
    void paintGL()                   override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *)  override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *)   override;

private:
    void buildMesh(const QVector<float> &raw,    // NR*NC z-values (NaN=null)
                   const QVector<uint8_t> &rgba, // NR*NC*3 rgb bytes
                   int NR, int NC,
                   double west, double south, double ewRes, double nsRes);
    void uploadMesh();

    QOpenGLShaderProgram *mProg  = nullptr;
    QOpenGLVertexArrayObject mVao;
    QOpenGLBuffer mVbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer mEbo{QOpenGLBuffer::IndexBuffer};

    int      mIndexCount = 0;
    bool     mWireframe  = false;

    // Camera (orbit)
    float    mAzim    = 225.0f;   // degrees around Z
    float    mElev    =  30.0f;   // degrees above horizontal
    float    mDist    =   2.5f;   // distance from centre
    QVector3D mCenter { 0, 0, 0 };
    QPoint   mLastMouse;
    bool     mOrbiting = false;
    bool     mPanning  = false;

    // Uniforms
    QMatrix4x4 mProjection;
    float      mZExag    = 1.0f;
    QVector3D  mLightDir { 1, 1, 2 };

    // Interleaved VBO layout: pos(3) + color(3) + normal(3) = 9 floats
    QVector<float>    mVertexData;
    QVector<uint32_t> mIndexData;

    // Mesh metadata
    float mMeshScale  = 1.0f;   // geographic → normalised units
    float mZScale     = 1.0f;   // z → normalised units (same as mMeshScale)

    QString mError;
};
