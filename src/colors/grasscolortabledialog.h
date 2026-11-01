#pragma once

#include <QDialog>
#include <QVector>
#include <QColor>

class QComboBox;
class QLabel;
class QCheckBox;
class GrassMapCanvas;

// Renders a 1-D color gradient from GRASS color table rules.
// Built from the output of `r.colors map=X -p` (per-value RGB lines).
class GrassColorBar : public QWidget
{
    Q_OBJECT
  public:
    explicit GrassColorBar( QWidget *parent = nullptr );
    void setGradient( const QVector<QPair<double, QColor>> &stops );
  protected:
    void paintEvent( QPaintEvent * ) override;
  private:
    QVector<QPair<double, QColor>> mStops; // (value 0-100%, color)
};

// Dialog for applying color tables to a raster map via r.colors.
// Shows a live gradient preview bar built from the current color table.
class GrassColorTableDialog : public QDialog
{
    Q_OBJECT

  public:
    // mapName + mapset — raster to colorize; canvas — for re-render after apply.
    explicit GrassColorTableDialog( const QString &mapName,
                                     const QString &mapset,
                                     GrassMapCanvas *canvas,
                                     QWidget *parent = nullptr );

  private slots:
    void onTableSelected( const QString &name );
    void applyColors();
    void previewColors();

  private:
    void loadGradient( const QString &colorTable );
    // Show the colour table currently stored in the GRASS database
    void showStoredColorTable();
    // Run r.colors map=X color=Y [flags] — returns true on success
    bool runRColors( const QString &colorTable );

    QString         mMapName;
    QString         mMapset;
    GrassMapCanvas *mCanvas = nullptr;

    QComboBox  *mTableCombo   = nullptr;
    QCheckBox  *mInvertCheck  = nullptr;
    QCheckBox  *mEqualizeCheck= nullptr;
    GrassColorBar *mBar       = nullptr;
    QLabel     *mStatus       = nullptr;
};
