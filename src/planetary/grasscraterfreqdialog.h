#pragma once

#include <QDialog>
#include <QProcess>

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QLineEdit;
class QRadioButton;
class QCheckBox;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTabWidget;
class GrassInterface;

QT_BEGIN_NAMESPACE
namespace QtCharts {}  /* Qt 6.2+ keeps Charts in the global namespace */
QT_END_NAMESPACE
class QChartView;
class QChart;

/*!
    \class GrassCraterFreqDialog
    \brief Native Qt6 dialog for p.crater.freq - crater size-frequency dating.

    The input source toggles between:
      - CSV file picker (default) - one diameter [km] per line
      - GRASS vector map + diameter column - typically the Df_pi column
        produced by p.crater

    Stacks the two input forms in a QStackedWidget driven by a pair of
    radio buttons, so only the active source is visible. On Run, spawns
    p.crater.freq with the right argv.
*/
class GrassCraterFreqDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit GrassCraterFreqDialog( GrassInterface *iface,
                                     QWidget *parent = nullptr );
    ~GrassCraterFreqDialog() override;

  private slots:
    void browseCsv();
    void browseOutput();
    void browsePng();
    void onVectorChanged( const QString &name );
    void runModule();
    void cancelRun();
    void onProcessOutput();
    void onProcessFinished( int exitCode, QProcess::ExitStatus status );
    void redrawChartFromCsv();

  private:
    void buildUi();
    void refreshVectorList();
    void refreshColumns();
    QStringList buildArgs() const;
    bool        parseOutputCsv( const QString &path,
                                  QString *body, double *ageGa,
                                  double *slope, double *intercept,
                                  QVector<double> *D,
                                  QVector<int>    *nInBin,
                                  QVector<double> *Nobs,
                                  QVector<double> *Nnpf,
                                  QVector<double> *Npl,
                                  QString *errorMsg );
    void        buildChart(   const QString &body, double ageGa,
                                const QVector<double> &D,
                                const QVector<double> &Nobs,
                                const QVector<double> &Nnpf,
                                const QVector<double> &Npl );
    void        savePngA5Landscape( const QString &path );

    GrassInterface  *mIface         = nullptr;
    QProcess        *mProcess       = nullptr;

    QRadioButton    *mUseCsv        = nullptr;
    QRadioButton    *mUseVector     = nullptr;
    QStackedWidget  *mStack         = nullptr;

    // CSV pane
    QLineEdit       *mCsvPath       = nullptr;

    // Vector pane
    QComboBox       *mVector        = nullptr;
    QComboBox       *mColumn        = nullptr;

    // Analysis params
    QDoubleSpinBox  *mArea          = nullptr;
    QComboBox       *mBody          = nullptr;
    QDoubleSpinBox  *mDmin          = nullptr;
    QDoubleSpinBox  *mDmax          = nullptr;
    QSpinBox        *mNbins         = nullptr;
    QCheckBox       *mHartmann      = nullptr;

    // Output
    QLineEdit       *mOutputCsv     = nullptr;
    QLineEdit       *mOutputPng     = nullptr;
    QCheckBox       *mShowGraph     = nullptr;

    // Tabs + chart
    QTabWidget      *mTabs          = nullptr;
    QChartView      *mChartView     = nullptr;
    QString          mLastCsvPath;  /* CSV produced by the last run */

    // Controls
    QPlainTextEdit  *mLog           = nullptr;
    QPushButton     *mRunBtn        = nullptr;
    QPushButton     *mCancelBtn     = nullptr;
};
