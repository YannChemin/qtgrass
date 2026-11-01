#include "../grassenv.h"
#include "grassmapswipe.h"

#include <QComboBox>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryFile>
#include <QVBoxLayout>

// ─── GrassMapSwipeWidget ──────────────────────────────────────────────────────

GrassMapSwipeWidget::GrassMapSwipeWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(400, 300);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    setCursor(Qt::SplitHCursor);
}

QPixmap GrassMapSwipeWidget::renderMap(const QString &map, int w, int h)
{
    if (map.isEmpty() || w < 1 || h < 1) return {};

    QTemporaryFile tmp;
    tmp.setFileTemplate(QDir::tempPath() + "/qtgrass_swipe_XXXXXX.png");
    tmp.setAutoRemove(false);
    if (!tmp.open()) return {};
    const QString path = tmp.fileName();
    tmp.close();

    QProcessEnvironment env = GrassEnv::instance()->buildSubprocessEnv();
    env.insert("GRASS_RENDER_IMMEDIATE",       "cairo");
    env.insert("GRASS_RENDER_FILE",            path);
    env.insert("GRASS_RENDER_FILE_READ",       "FALSE");
    env.insert("GRASS_RENDER_WIDTH",           QString::number(w));
    env.insert("GRASS_RENDER_HEIGHT",          QString::number(h));
    env.insert("GRASS_RENDER_BACKGROUNDCOLOR", "white");

    QProcess p;
    p.setProcessEnvironment(env);
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("d.rast", {"map=" + map});
    p.waitForFinished(30000);

    QPixmap pix(path);
    QFile::remove(path);
    return pix;
}

void GrassMapSwipeWidget::setLeft(const QString &mapName)
{
    mLeftMap = mapName;
}

void GrassMapSwipeWidget::setRight(const QString &mapName)
{
    mRightMap = mapName;
}

void GrassMapSwipeWidget::renderBoth()
{
    if (mBusy) return;
    mBusy = true;
    setCursor(Qt::WaitCursor);

    const int w = qMax(100, width());
    const int h = qMax(100, height());

    mLeftPix  = renderMap(mLeftMap,  w, h);
    mRightPix = renderMap(mRightMap, w, h);

    if (mDivX < 0) mDivX = w / 2;
    mBusy = false;
    setCursor(Qt::SplitHCursor);
    update();
}

void GrassMapSwipeWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const int W = width(), H = height();

    if (mLeftPix.isNull() && mRightPix.isNull())
    {
        p.fillRect(rect(), QColor(60, 65, 75));
        p.setPen(Qt::lightGray);
        p.drawText(rect(), Qt::AlignCenter,
                   "Select two rasters and press Render.");
        return;
    }

    const int divX = (mDivX < 0) ? W / 2 : mDivX;

    // Left half
    if (!mLeftPix.isNull())
    {
        p.setClipRect(0, 0, divX, H);
        p.drawPixmap(0, 0, W, H, mLeftPix);
        p.setClipping(false);
    }

    // Right half
    if (!mRightPix.isNull())
    {
        p.setClipRect(divX, 0, W - divX, H);
        p.drawPixmap(0, 0, W, H, mRightPix);
        p.setClipping(false);
    }

    // Divider line + handle
    p.setPen(QPen(Qt::white, 2));
    p.drawLine(divX, 0, divX, H);
    p.setPen(QPen(QColor(50, 50, 50), 1));
    p.drawLine(divX, 0, divX, H);

    // Handle circle at midpoint
    const QPoint mid(divX, H / 2);
    p.setBrush(Qt::white);
    p.setPen(QPen(Qt::darkGray, 1.5));
    p.drawEllipse(mid, 12, 12);
    p.setPen(Qt::darkGray);
    const int al = 5;
    // Left arrow
    QPolygon la; la << mid + QPoint(-al-4, 0) << mid + QPoint(-al+2, -al)
                    << mid + QPoint(-al+2,  al);
    p.drawPolyline(la);
    // Right arrow
    QPolygon ra; ra << mid + QPoint(al+4, 0) << mid + QPoint(al-2, -al)
                    << mid + QPoint(al-2,  al);
    p.drawPolyline(ra);

    // Layer name labels
    QFont f; f.setPointSize(8); f.setBold(true); p.setFont(f);
    if (!mLeftMap.isEmpty())
    {
        p.fillRect(4, 4, 160, 18, QColor(0, 0, 0, 120));
        p.setPen(Qt::white);
        p.drawText(QRect(6, 4, 156, 18), Qt::AlignVCenter,
                   p.fontMetrics().elidedText(mLeftMap, Qt::ElideRight, 156));
    }
    if (!mRightMap.isEmpty())
    {
        p.fillRect(W - 164, 4, 160, 18, QColor(0, 0, 0, 120));
        p.setPen(Qt::white);
        p.drawText(QRect(W - 162, 4, 156, 18), Qt::AlignVCenter | Qt::AlignRight,
                   p.fontMetrics().elidedText(mRightMap, Qt::ElideLeft, 156));
    }
}

void GrassMapSwipeWidget::resizeEvent(QResizeEvent *)
{
    if (mDivX > 0)
        mDivX = qBound(0, mDivX, width());
}

void GrassMapSwipeWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
    {
        const int divX = (mDivX < 0) ? width() / 2 : mDivX;
        if (qAbs(e->position().x() - divX) < 20)
        {
            mDragging = true;
            e->accept();
        }
    }
}

void GrassMapSwipeWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (mDragging)
    {
        mDivX = qBound(0, static_cast<int>(e->position().x()), width());
        update();
        e->accept();
    }
}

void GrassMapSwipeWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) mDragging = false;
}

// ─── GrassMapSwipeDialog ──────────────────────────────────────────────────────

GrassMapSwipeDialog::GrassMapSwipeDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Map Swipe Comparison");
    resize(860, 580);

    auto *vbox = new QVBoxLayout(this);

    // Controls row
    auto *row = new QHBoxLayout;
    row->addWidget(new QLabel("Left:", this));
    mLeftCombo = new QComboBox(this);
    mLeftCombo->setEditable(true);
    mLeftCombo->setMinimumWidth(160);
    row->addWidget(mLeftCombo, 1);

    row->addWidget(new QLabel("Right:", this));
    mRightCombo = new QComboBox(this);
    mRightCombo->setEditable(true);
    mRightCombo->setMinimumWidth(160);
    row->addWidget(mRightCombo, 1);

    auto *renderBtn = new QPushButton("Render", this);
    connect(renderBtn, &QPushButton::clicked, this, [this] {
        mSwipe->setLeft (mLeftCombo->currentText().trimmed());
        mSwipe->setRight(mRightCombo->currentText().trimmed());
        mSwipe->renderBoth();
    });
    row->addWidget(renderBtn);

    auto *closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    row->addWidget(closeBtn);

    vbox->addLayout(row);

    mSwipe = new GrassMapSwipeWidget(this);
    vbox->addWidget(mSwipe, 1);

    mHint = new QLabel("Drag the centre divider to compare.", this);
    mHint->setStyleSheet("color:#666; font-size:8pt;");
    vbox->addWidget(mHint);

    // Populate raster combos
    QProcess p;
    p.start("g.list", {"type=raster", "mapset=*"});
    if (p.waitForFinished(8000))
        for (const QString &l : QString::fromLocal8Bit(p.readAllStandardOutput())
                                .split('\n', Qt::SkipEmptyParts))
        {
            mLeftCombo->addItem(l.trimmed());
            mRightCombo->addItem(l.trimmed());
        }
    if (mRightCombo->count() > 1) mRightCombo->setCurrentIndex(1);
}
