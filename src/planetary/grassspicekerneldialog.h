#pragma once

#include <QDialog>
#include <QStringList>

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;
class GrassInterface;

/*!
    \class GrassSpiceKernelDialog
    \brief Native dialog for managing NAIF SPICE kernel file assignments.

    Displays a QTreeWidget grouped by kernel type (LSK, SCLK, CK, SPK,
    IK, FK, PCK, DSK).  The user can browse for kernel files, add or
    remove entries, and apply the selection.  Applying writes the kernel
    paths to a QSettings key so that p.spiceinit can read them as defaults,
    and also exports a shell-style environment variable PLANETSPICE_PATH for
    the GRASS session.

    The dialog does NOT call p_spice_load() itself — kernel loading is
    delegated to p.spiceinit which runs in a subprocess and validates each
    file via CSPICE.
*/
class GrassSpiceKernelDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit GrassSpiceKernelDialog( GrassInterface *iface,
                                      QWidget *parent = nullptr );

  private slots:
    void addKernel();
    void removeSelected();
    void applyKernels();
    void validateKernels();

  private:
    void loadFromSettings();
    void saveToSettings();
    void addKernelItem( const QString &type, const QString &path );

    GrassInterface *mIface    = nullptr;
    QTreeWidget    *mTree     = nullptr;
    QLabel         *mStatus   = nullptr;
    QPushButton    *mApplyBtn = nullptr;

    /* One top-level item per kernel type; children are file paths. */
    static const QStringList kKernelTypes;
};
