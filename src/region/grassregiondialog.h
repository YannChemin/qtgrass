#pragma once

#include <QDialog>

class QDoubleSpinBox;
class QSpinBox;
class QLabel;
class QComboBox;
class GrassMapCanvas;

// GUI for g.region: read and set the GRASS computational region.
// The canvas region overlay redraws automatically after setting.
class GrassRegionDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit GrassRegionDialog( GrassMapCanvas *canvas,
                                 QWidget *parent = nullptr );

  private slots:
    void loadRegion();          // read current region via g.region -p
    void setRegion();           // apply via g.region n= s= e= w= ...
    void alignToRaster();       // g.region raster=X
    void alignToVector();       // g.region vector=X
    void nsresChanged();        // rows/cols update when nsres/ewres change
    void rowsChanged();         // nsres/ewres update when rows/cols change

  private:
    void runGRegion( const QStringList &args );
    QStringList listMaps( const QString &type ) const;

    GrassMapCanvas *mCanvas = nullptr;

    QDoubleSpinBox *mNorth  = nullptr;
    QDoubleSpinBox *mSouth  = nullptr;
    QDoubleSpinBox *mEast   = nullptr;
    QDoubleSpinBox *mWest   = nullptr;
    QDoubleSpinBox *mNSRes  = nullptr;
    QDoubleSpinBox *mEWRes  = nullptr;
    QSpinBox       *mRows   = nullptr;
    QSpinBox       *mCols   = nullptr;

    QComboBox *mRasterCombo = nullptr;
    QComboBox *mVectorCombo = nullptr;

    QLabel *mStatus = nullptr;
    bool    mUpdating = false;  // suppress feedback loops
};
