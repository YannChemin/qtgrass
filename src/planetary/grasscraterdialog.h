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
class QLabel;
class QGroupBox;
class GrassInterface;

/*!
    \class GrassCraterDialog
    \brief Native Qt6 dialog for p.crater - vector-based impact-crater scaling.

    Provides a richer, planetary-aware UI than the auto-generated
    GrassModuleDialog: the body picker drives the layout state (custom-body
    overrides become required), and the two-layer geology section uses a
    grouped layout that mirrors the physical model.

    On Run, the dialog spawns p.crater via QProcess with all collected
    options and streams stdout/stderr to the embedded log.

    \sa GrassCraterFreqDialog (follow-on size-frequency dating)
*/
class GrassCraterDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit GrassCraterDialog( GrassInterface *iface,
                                 QWidget *parent = nullptr );
    ~GrassCraterDialog() override;

  private slots:
    void onBodyChanged( const QString &name );
    void onModeChanged();
    void runModule();
    void cancelRun();
    void onProcessOutput();
    void onProcessFinished( int exitCode, QProcess::ExitStatus status );

  private:
    void buildUi();
    void populateBodies();
    void refreshVectorLists();
    QStringList buildArgs() const;

    GrassInterface  *mIface       = nullptr;
    QProcess        *mProcess     = nullptr;

    // ── Geometry & body ──
    QComboBox       *mInputVector = nullptr;
    QLineEdit       *mOutputVector= nullptr;
    QComboBox       *mBody        = nullptr;
    QLabel          *mBodyInfo    = nullptr;

    // ── Custom body overrides ──
    QGroupBox       *mCustomBox   = nullptr;
    QDoubleSpinBox  *mGravity     = nullptr;
    QDoubleSpinBox  *mTargetDens  = nullptr;
    QComboBox       *mTargetType  = nullptr;
    QDoubleSpinBox  *mDdSimple    = nullptr;  /* 0 = use body default */

    // ── Impactor ──
    QDoubleSpinBox  *mVelocity    = nullptr;
    QDoubleSpinBox  *mAngle       = nullptr;
    QDoubleSpinBox  *mProjDens    = nullptr;
    QDoubleSpinBox  *mProjDiam    = nullptr;
    QCheckBox       *mBackward    = nullptr;

    // ── Geology two-layer ──
    QComboBox       *mSurfDensMap = nullptr;
    QDoubleSpinBox  *mSurfThick   = nullptr;
    QComboBox       *mSubDensMap  = nullptr;
    QDoubleSpinBox  *mSubThick    = nullptr;
    QComboBox       *mGeolType    = nullptr;
    QComboBox       *mDem         = nullptr;
    QComboBox       *mDdMap       = nullptr;

    // ── Impactor-points synthesis mode ──
    QCheckBox       *mUseImpactors= nullptr;
    QComboBox       *mImpactorVec = nullptr;
    QComboBox       *mColVelocity = nullptr;
    QComboBox       *mColAngle    = nullptr;
    QComboBox       *mColAzimuth  = nullptr;
    QComboBox       *mColDensity  = nullptr;
    QComboBox       *mColDiameter = nullptr;
    QComboBox       *mDensityUnit = nullptr;

    // ── Controls ──
    QPlainTextEdit  *mLog         = nullptr;
    QPushButton     *mRunBtn      = nullptr;
    QPushButton     *mCancelBtn   = nullptr;
};
