#pragma once

#include <QDialog>
#include <QPixmap>
#include <QString>

class QComboBox;
class QLabel;

// ── Swipe comparison widget ───────────────────────────────────────────────────
// Renders two GRASS rasters and lets the user drag a vertical divider to
// compare them side by side.  Both rasters are rendered at the current
// computational region.
class GrassMapSwipeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GrassMapSwipeWidget(QWidget *parent = nullptr);

    void setLeft (const QString &mapName);
    void setRight(const QString &mapName);
    void renderBoth();

protected:
    void paintEvent      (QPaintEvent *)    override;
    void resizeEvent     (QResizeEvent *)   override;
    void mousePressEvent (QMouseEvent *)    override;
    void mouseMoveEvent  (QMouseEvent *)    override;
    void mouseReleaseEvent(QMouseEvent *)   override;

private:
    QPixmap renderMap(const QString &map, int w, int h);

    QString mLeftMap, mRightMap;
    QPixmap mLeftPix, mRightPix;
    int     mDivX     = -1;   // -1 = centre
    bool    mDragging = false;
    bool    mBusy     = false;
};

// ── Map swipe dialog ──────────────────────────────────────────────────────────
class GrassMapSwipeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GrassMapSwipeDialog(QWidget *parent = nullptr);

private:
    QComboBox          *mLeftCombo  = nullptr;
    QComboBox          *mRightCombo = nullptr;
    GrassMapSwipeWidget *mSwipe     = nullptr;
    QLabel             *mHint       = nullptr;
};
