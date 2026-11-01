#pragma once

/*!
    \file grassmoduledialog.h
    \brief Module run dialog: parameter widgets, command preview, and output log.
*/

#include "grassmoduledesc.h"

#include <QDialog>
#include <QProcessEnvironment>
#include <QStringList>

class QTabWidget;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QProcess;
class GrassParamWidget;
class GrassFlagWidget;
class GrassInterface;

/*!
    \class GrassModuleDialog
    \brief Dialog for running any GRASS module via its \c --interface-description XML.

    Opened when a module is invoked through the module toolbox or via
    \c --qtui on the command line.

    \section tabs Tab layout

    \li One tab per \c \<guisection\> defined by the module XML.
    \li A catch-all \b Required tab for mandatory parameters with no section.
    \li A \b Command tab showing a live preview of the constructed command line.
    \li An \b Output tab streaming \c QProcess stdout / stderr.
    \li A \b Manual tab displaying the module's HTML help page.

    \section buttons Buttons

    \b Run — validates required fields, then starts the module as a \c QProcess.

    \b Copy Command — copies the current command line to the clipboard.

    \b Close — available at all times; kills a running process if needed.

    \sa GrassParamWidget, GrassFlagWidget, GrassXmlParser
*/
class GrassModuleDialog : public QDialog
{
    Q_OBJECT

  public:
    /*!
        \brief Constructs and populates the dialog from the module's XML description.

        \param moduleName Bare module name, e.g. \c "r.slope.aspect".
        \param preArgs    Optional \c key=value strings to pre-fill parameter widgets.
        \param iface      Optional interface used to add output layers to the canvas
                          after a successful run.
        \param parent     Parent widget.
    */
    explicit GrassModuleDialog( const QString &moduleName,
                                 const QStringList &preArgs = {},
                                 GrassInterface *iface = nullptr,
                                 QWidget *parent = nullptr );
    ~GrassModuleDialog() override;

  private slots:
    void run();
    void cancel();
    void onProcessStarted();
    void onProcessOutput();
    void onProcessFinished( int exitCode, int exitStatus );
    void updateCommandPreview();

  private:
    void buildOptionTabs( QTabWidget *tabs );
    void buildOptionalTab( QTabWidget *tabs );
    void buildCommandTab( QTabWidget *tabs );
    void buildOutputTab( QTabWidget *tabs );
    void buildManualTab(  QTabWidget *tabs );

    void applyPreArgs( const QStringList &preArgs );
    QStringList collectArgs() const;
    bool validateAll();
    QString moduleBin() const;
    QProcessEnvironment buildEnv() const;

    QString          mModuleName;
    GrassModuleDesc  mDesc;
    GrassInterface  *mIface = nullptr;

    QList<GrassParamWidget *> mParamWidgets;
    QList<GrassFlagWidget *>  mFlagWidgets;

    QTabWidget     *mTabs          = nullptr;
    QPlainTextEdit *mOutputView    = nullptr;
    QPlainTextEdit *mCommandView   = nullptr;
    QLineEdit      *mCommandFooter = nullptr;
    QProgressBar   *mProgress      = nullptr;
    QPushButton    *mRunBtn        = nullptr;
    QPushButton    *mCancelBtn     = nullptr;
    QPushButton    *mCopyBtn       = nullptr;

    QProcess *mProcess = nullptr;
};
