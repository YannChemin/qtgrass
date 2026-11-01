#include "grassspicekerneldialog.h"
#include "../grassinterface.h"
#include "../grassmessagebar.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLoggingCategory>
#include <QPushButton>
#include <QSettings>
#include <QTreeWidget>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcSpice, "qtgrass.spice")

const QStringList GrassSpiceKernelDialog::kKernelTypes = {
    "LSK", "SCLK", "CK", "SPK", "IK", "FK", "PCK", "DSK"
};

/* File filter string for each kernel type. */
static QString filterForType( const QString &type )
{
    if ( type == "LSK"  ) return "Leapsecond kernels (*.tls);;All files (*)";
    if ( type == "SCLK" ) return "Spacecraft clock kernels (*.tsc);;All files (*)";
    if ( type == "CK"   ) return "Pointing kernels (*.bc);;All files (*)";
    if ( type == "SPK"  ) return "Ephemeris kernels (*.bsp);;All files (*)";
    if ( type == "IK"   ) return "Instrument kernels (*.ti);;All files (*)";
    if ( type == "FK"   ) return "Frame kernels (*.tf);;All files (*)";
    if ( type == "PCK"  ) return "PCK kernels (*.tpc *.bpc);;All files (*)";
    if ( type == "DSK"  ) return "Shape kernels (*.bds);;All files (*)";
    return "All files (*)";
}

GrassSpiceKernelDialog::GrassSpiceKernelDialog( GrassInterface *iface,
                                                  QWidget *parent )
    : QDialog( parent )
    , mIface( iface )
{
    setWindowTitle( "SPICE Kernel Manager" );
    setMinimumSize( 620, 480 );

    QVBoxLayout *root = new QVBoxLayout( this );

    // ── Kernel tree ──────────────────────────────────────────────────
    QGroupBox *treeBox = new QGroupBox( "Kernel files by type", this );
    QVBoxLayout *treeLay = new QVBoxLayout( treeBox );

    mTree = new QTreeWidget( treeBox );
    mTree->setColumnCount( 2 );
    mTree->setHeaderLabels( { "Type / Path", "Status" } );
    mTree->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
    mTree->header()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
    mTree->setRootIsDecorated( true );
    mTree->setAlternatingRowColors( true );

    /* Create one top-level item per kernel type. */
    for ( const QString &t : kKernelTypes ) {
        auto *item = new QTreeWidgetItem( mTree );
        item->setText( 0, t );
        item->setData( 0, Qt::UserRole, t );  /* tag so we know the type */
        item->setExpanded( true );
        item->setFlags( item->flags() & ~Qt::ItemIsSelectable );
    }

    treeLay->addWidget( mTree );
    root->addWidget( treeBox );

    // ── Buttons row ──────────────────────────────────────────────────
    QHBoxLayout *btnRow = new QHBoxLayout;
    QPushButton *addBtn = new QPushButton( "Add kernel…", this );
    QPushButton *remBtn = new QPushButton( "Remove selected", this );
    QPushButton *valBtn = new QPushButton( "Validate with CSPICE", this );
    btnRow->addWidget( addBtn );
    btnRow->addWidget( remBtn );
    btnRow->addWidget( valBtn );
    btnRow->addStretch();
    root->addLayout( btnRow );

    // ── Status bar ───────────────────────────────────────────────────
    mStatus = new QLabel( "No kernels loaded.", this );
    mStatus->setWordWrap( true );
    root->addWidget( mStatus );

    // ── Dialog buttons ───────────────────────────────────────────────
    QDialogButtonBox *btns = new QDialogButtonBox( this );
    mApplyBtn = btns->addButton( "Apply to session", QDialogButtonBox::ApplyRole );
    btns->addButton( QDialogButtonBox::Close );
    root->addWidget( btns );

    connect( addBtn,     &QPushButton::clicked, this, &GrassSpiceKernelDialog::addKernel );
    connect( remBtn,     &QPushButton::clicked, this, &GrassSpiceKernelDialog::removeSelected );
    connect( valBtn,     &QPushButton::clicked, this, &GrassSpiceKernelDialog::validateKernels );
    connect( mApplyBtn,  &QPushButton::clicked, this, &GrassSpiceKernelDialog::applyKernels );
    connect( btns, &QDialogButtonBox::rejected, this, &QDialog::accept );

    loadFromSettings();
}

// ── addKernel ─────────────────────────────────────────────────────────────

void GrassSpiceKernelDialog::addKernel()
{
    /* Ask which type first via a simple combo dialog — we use the currently
     * selected top-level item's type, falling back to LSK. */
    QString selectedType = "LSK";
    QTreeWidgetItem *sel = mTree->currentItem();
    if ( sel ) {
        QTreeWidgetItem *top = sel->parent() ? sel->parent() : sel;
        selectedType = top->data( 0, Qt::UserRole ).toString();
    }

    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        QString( "Add %1 kernel(s)" ).arg( selectedType ),
        QDir::homePath(),
        filterForType( selectedType ) );

    for ( const QString &path : files )
        addKernelItem( selectedType, path );

    saveToSettings();
}

void GrassSpiceKernelDialog::addKernelItem( const QString &type, const QString &path )
{
    /* Find the parent item for this type. */
    for ( int i = 0; i < mTree->topLevelItemCount(); i++ ) {
        QTreeWidgetItem *top = mTree->topLevelItem( i );
        if ( top->data( 0, Qt::UserRole ).toString() == type ) {
            /* Check for duplicates. */
            for ( int c = 0; c < top->childCount(); c++ )
                if ( top->child(c)->text(0) == path ) return;

            auto *child = new QTreeWidgetItem( top );
            child->setText( 0, path );
            child->setData( 0, Qt::UserRole, type );

            bool exists = QFile::exists( path );
            child->setText( 1, exists ? "✓ found" : "✗ missing" );
            child->setForeground( 1, exists ? QColor("#1a8a1a") : QColor("#c00") );
            return;
        }
    }
}

// ── removeSelected ────────────────────────────────────────────────────────

void GrassSpiceKernelDialog::removeSelected()
{
    QList<QTreeWidgetItem *> sel = mTree->selectedItems();
    for ( QTreeWidgetItem *item : sel ) {
        if ( item->parent() ) {  /* only remove child (file) items */
            delete item;
        }
    }
    saveToSettings();
}

// ── validateKernels ───────────────────────────────────────────────────────

void GrassSpiceKernelDialog::validateKernels()
{
    /* Collect all kernel paths and call p.spiceinit --test via G_spawn.
     * We build a temporary p.spiceinit command string and run it so the
     * CSPICE validation happens in a subprocess (p.spiceinit links CSPICE). */
    QStringList args;
    args << "p.spiceinit" << "-t";

    /* Gather paths by type. */
    QMap<QString, QStringList> kernelsByType;
    for ( int i = 0; i < mTree->topLevelItemCount(); i++ ) {
        QTreeWidgetItem *top = mTree->topLevelItem(i);
        QString t = top->data(0, Qt::UserRole).toString().toLower();
        for ( int c = 0; c < top->childCount(); c++ )
            kernelsByType[t] << top->child(c)->text(0);
    }

    /* We need a map to attach kernels to — use a dummy map named "SPICE_TEST".
     * For now, just show a status message since full validation requires a
     * GRASS location context. */
    if ( kernelsByType.isEmpty() ) {
        mStatus->setText( "No kernels to validate." );
        return;
    }

    mStatus->setStyleSheet( "color:#888;" );
    mStatus->setText( "Validation via p.spiceinit -t requires a GRASS location. "
                       "Use the 'Apply to session' button to pass kernels to p.spiceinit." );
}

// ── applyKernels ──────────────────────────────────────────────────────────

void GrassSpiceKernelDialog::applyKernels()
{
    /* Write kernels to QSettings so they appear as defaults in the
     * p.spiceinit module dialog. */
    saveToSettings();

    /* Build a semicolon-separated PLANETSPICE_PATH env string listing
     * all kernel paths in load order: LSK first, then SCLK, FK, PCK,
     * IK, SPK, CK, DSK. */
    QStringList all;
    const QStringList loadOrder = { "LSK","SCLK","FK","PCK","IK","SPK","CK","DSK" };
    for ( const QString &t : loadOrder ) {
        for ( int i = 0; i < mTree->topLevelItemCount(); i++ ) {
            QTreeWidgetItem *top = mTree->topLevelItem(i);
            if ( top->data(0,Qt::UserRole).toString() != t ) continue;
            for ( int c = 0; c < top->childCount(); c++ )
                all << top->child(c)->text(0);
        }
    }

    if ( all.isEmpty() ) {
        mStatus->setStyleSheet( "color:#c00;" );
        mStatus->setText( "No kernels to apply." );
        return;
    }

    QString combined = all.join(";");
    qputenv( "PLANETSPICE_PATH", combined.toLocal8Bit() );

    mStatus->setStyleSheet( "color:#1a8a1a;" );
    mStatus->setText( QString( "Applied %1 kernel path(s) to session "
                                "(PLANETSPICE_PATH set)." ).arg( all.size() ) );

    qCDebug(lcSpice) << "PLANETSPICE_PATH =" << combined;
}

// ── Persistence ───────────────────────────────────────────────────────────

void GrassSpiceKernelDialog::loadFromSettings()
{
    QSettings s( "qtgrass", "qtgrass" );
    for ( const QString &t : kKernelTypes ) {
        QStringList paths = s.value( "spice/" + t ).toStringList();
        for ( const QString &p : paths )
            if ( !p.isEmpty() )
                addKernelItem( t, p );
    }
    int total = 0;
    for ( int i = 0; i < mTree->topLevelItemCount(); i++ )
        total += mTree->topLevelItem(i)->childCount();
    if ( total > 0 )
        mStatus->setText( QString( "Loaded %1 kernel path(s) from previous session." )
                          .arg(total) );
}

void GrassSpiceKernelDialog::saveToSettings()
{
    QSettings s( "qtgrass", "qtgrass" );
    for ( const QString &t : kKernelTypes ) {
        QStringList paths;
        for ( int i = 0; i < mTree->topLevelItemCount(); i++ ) {
            QTreeWidgetItem *top = mTree->topLevelItem(i);
            if ( top->data(0,Qt::UserRole).toString() != t ) continue;
            for ( int c = 0; c < top->childCount(); c++ )
                paths << top->child(c)->text(0);
        }
        s.setValue( "spice/" + t, paths );
    }
}
