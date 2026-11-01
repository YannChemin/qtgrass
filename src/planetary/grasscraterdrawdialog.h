#pragma once

#include <QDialog>
#include <QProcess>

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QLineEdit;
class QCheckBox;
class QPlainTextEdit;
class QPushButton;
class GrassInterface;

/*!
    \class GrassCraterDrawDialog
    \brief Native Qt6 dialog for p.crater.draw - automated crater-rim
           delineation from a DEM and/or panchromatic image.

    Three tabs:
      - Inputs:  DEM picker, image picker, sun-azimuth spinner, body
      - Detect:  min/max diameter, threshold, method chain, NMS IoU,
                 scale count, OpenCL flag, OpenMP thread count
      - Run log: live merged stdout/stderr from p.crater.draw subprocess

    On Run, validates that at least one input is supplied, that image
    mode has a sun_azimuth, and spawns p.crater.draw via QProcess.
    The output is a polygon vector consumable by GrassCraterDialog
    (input=) and GrassCraterFreqDialog (vector= column=D_eq).
*/
class GrassCraterDrawDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit GrassCraterDrawDialog( GrassInterface *iface,
                                     QWidget *parent = nullptr );
    ~GrassCraterDrawDialog() override;

  private slots:
    void runModule();
    void cancelRun();
    void onProcessOutput();
    void onProcessFinished( int exitCode, QProcess::ExitStatus status );
    void onMethodChanged( const QString &method );

  private:
    void buildUi();
    void refreshMapLists();
    QStringList buildArgs() const;

    GrassInterface  *mIface       = nullptr;
    QProcess        *mProcess     = nullptr;

    // Inputs
    QComboBox       *mDem         = nullptr;
    QComboBox       *mImage       = nullptr;
    QDoubleSpinBox  *mSunAzimuth  = nullptr;
    QComboBox       *mBody        = nullptr;
    QLineEdit       *mOutput      = nullptr;

    // Detection params
    QComboBox       *mMethod      = nullptr;
    QDoubleSpinBox  *mMinDiameter = nullptr;
    QDoubleSpinBox  *mMaxDiameter = nullptr;
    QDoubleSpinBox  *mThreshold   = nullptr;
    QDoubleSpinBox  *mNmsIou      = nullptr;
    QSpinBox        *mScales      = nullptr;
    QSpinBox        *mAzSamples   = nullptr;
    QSpinBox        *mThreads     = nullptr;
    QCheckBox       *mOpenCL      = nullptr;

    // Controls
    QPlainTextEdit  *mLog         = nullptr;
    QPushButton     *mRunBtn      = nullptr;
    QPushButton     *mCancelBtn   = nullptr;
};
