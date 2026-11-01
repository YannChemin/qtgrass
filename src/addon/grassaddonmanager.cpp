#include "../grassenv.h"
#include "grassaddonmanager.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QDebug>

GrassAddonManager::GrassAddonManager( QWidget *parent )
    : QDialog( parent )
{
    setWindowTitle( "GRASS Addon Manager — g.extension" );
    setMinimumSize( 700, 540 );

    QVBoxLayout *root = new QVBoxLayout( this );

    mTabs = new QTabWidget( this );

    // ---- Installed tab ----
    QWidget *instTab = new QWidget();
    QVBoxLayout *instLay = new QVBoxLayout( instTab );

    mInstTable = new QTableWidget( 0, 2, instTab );
    mInstTable->setHorizontalHeaderLabels( { "Extension", "Type" } );
    mInstTable->horizontalHeader()->setStretchLastSection( true );
    mInstTable->setEditTriggers( QAbstractItemView::NoEditTriggers );
    mInstTable->setSelectionBehavior( QAbstractItemView::SelectRows );
    mInstTable->setSelectionMode( QAbstractItemView::SingleSelection );
    mInstTable->verticalHeader()->setVisible( false );
    mInstTable->setAlternatingRowColors( true );
    instLay->addWidget( mInstTable, 1 );

    QHBoxLayout *instBtnRow = new QHBoxLayout();
    QPushButton *instRefreshBtn = new QPushButton( "Refresh", instTab );
    mRemoveBtn = new QPushButton( "Remove selected", instTab );
    mRemoveBtn->setEnabled( false );
    instBtnRow->addWidget( instRefreshBtn );
    instBtnRow->addStretch();
    instBtnRow->addWidget( mRemoveBtn );
    instLay->addLayout( instBtnRow );
    mTabs->addTab( instTab, "Installed" );

    // ---- Available tab ----
    QWidget *availTab = new QWidget();
    QVBoxLayout *availLay = new QVBoxLayout( availTab );

    QHBoxLayout *searchRow = new QHBoxLayout();
    searchRow->addWidget( new QLabel( "Search:", availTab ) );
    mSearch = new QLineEdit( availTab );
    mSearch->setPlaceholderText( "Filter by name or description…" );
    mSearch->setClearButtonEnabled( true );
    searchRow->addWidget( mSearch, 1 );
    mRefreshBtn = new QPushButton( "Load list", availTab );
    searchRow->addWidget( mRefreshBtn );
    availLay->addLayout( searchRow );

    mAvailTable = new QTableWidget( 0, 2, availTab );
    mAvailTable->setHorizontalHeaderLabels( { "Extension", "Description" } );
    mAvailTable->horizontalHeader()->setStretchLastSection( true );
    mAvailTable->setEditTriggers( QAbstractItemView::NoEditTriggers );
    mAvailTable->setSelectionBehavior( QAbstractItemView::SelectRows );
    mAvailTable->setSelectionMode( QAbstractItemView::SingleSelection );
    mAvailTable->verticalHeader()->setVisible( false );
    mAvailTable->setAlternatingRowColors( true );
    availLay->addWidget( mAvailTable, 1 );

    QHBoxLayout *availBtnRow = new QHBoxLayout();
    availBtnRow->addStretch();
    mInstallBtn = new QPushButton( "Install selected", availTab );
    mInstallBtn->setEnabled( false );
    availBtnRow->addWidget( mInstallBtn );
    availLay->addLayout( availBtnRow );
    mTabs->addTab( availTab, "Available (online)" );

    root->addWidget( mTabs, 2 );

    // ---- Log ----
    QLabel *logLbl = new QLabel( "Installation log:", this );
    root->addWidget( logLbl );
    mLog = new QPlainTextEdit( this );
    mLog->setReadOnly( true );
    mLog->setFont( QFont( "Monospace", 9 ) );
    mLog->setStyleSheet( "background:#1e1e1e; color:#ccc;" );
    mLog->setFixedHeight( 100 );
    root->addWidget( mLog );

    mStatus = new QLabel( this );
    root->addWidget( mStatus );

    QDialogButtonBox *btns = new QDialogButtonBox( QDialogButtonBox::Close, this );
    connect( btns, &QDialogButtonBox::rejected, this, &QDialog::accept );
    root->addWidget( btns );

    // Connections
    connect( instRefreshBtn, &QPushButton::clicked,
             this, &GrassAddonManager::loadInstalled );
    connect( mRefreshBtn,    &QPushButton::clicked,
             this, &GrassAddonManager::loadAvailable );
    connect( mInstallBtn,    &QPushButton::clicked,
             this, &GrassAddonManager::installSelected );
    connect( mRemoveBtn,     &QPushButton::clicked,
             this, &GrassAddonManager::removeSelected );

    connect( mInstTable, &QTableWidget::itemSelectionChanged,
             this, [this] {
                 mRemoveBtn->setEnabled( !mInstTable->selectedItems().isEmpty() );
             } );
    connect( mAvailTable, &QTableWidget::itemSelectionChanged,
             this, [this] {
                 mInstallBtn->setEnabled( !mAvailTable->selectedItems().isEmpty() );
             } );

    // Filter the available table when search changes
    connect( mSearch, &QLineEdit::textChanged,
             this, [this]( const QString &text ) {
                 const QString lower = text.toLower();
                 for ( int r = 0; r < mAvailTable->rowCount(); ++r )
                 {
                     const bool match =
                         lower.isEmpty() ||
                         mAvailTable->item( r, 0 )->text().contains( lower, Qt::CaseInsensitive ) ||
                         mAvailTable->item( r, 1 )->text().contains( lower, Qt::CaseInsensitive );
                     mAvailTable->setRowHidden( r, !match );
                 }
             } );

    // Load installed addons immediately
    loadInstalled();
}

// ---------------------------------------------------------------------------
// loadInstalled — g.extension -a
// ---------------------------------------------------------------------------
void GrassAddonManager::loadInstalled()
{
    mInstTable->setRowCount( 0 );
    mStatus->setText( "Loading installed extensions…" );

    const QString out = [&]() -> QString {
        QProcess p;
        p.setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );
        const QString gb = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
        if ( gb.isEmpty() ) return {};
        p.start( gb + "/bin/g.extension", { "-a" } );
        p.waitForFinished( 10000 );
        return QString::fromLocal8Bit( p.readAllStandardOutput() +
                                        p.readAllStandardError() );
    }();

    if ( out.trimmed().startsWith( "No extension" ) || out.trimmed().isEmpty() )
    {
        mStatus->setText( "No addons installed." );
        return;
    }

    for ( const QString &line : out.split( '\n', Qt::SkipEmptyParts ) )
    {
        const QString name = line.trimmed();
        if ( name.isEmpty() || name.startsWith( "List" ) ) continue;
        const int r = mInstTable->rowCount();
        mInstTable->insertRow( r );
        mInstTable->setItem( r, 0, new QTableWidgetItem( name ) );
        mInstTable->setItem( r, 1, new QTableWidgetItem( "module" ) );
    }

    mStatus->setText( QString( "%1 addon(s) installed." )
                      .arg( mInstTable->rowCount() ) );
    mInstTable->resizeColumnsToContents();
}

// ---------------------------------------------------------------------------
// loadAvailable — g.extension -c -l (online, may be slow)
// ---------------------------------------------------------------------------
void GrassAddonManager::loadAvailable()
{
    mAvailTable->setRowCount( 0 );
    mInstallBtn->setEnabled( false );
    mStatus->setText( "Fetching addon list from repository (may take a while)…" );
    QApplication::processEvents();

    const QString gb = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
    if ( gb.isEmpty() ) { mStatus->setText( "GISBASE not set." ); return; }

    QProcess proc;
    proc.setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );
    proc.start( gb + "/bin/g.extension", { "-c", "-l" } );

    // This can take up to 30s on slow connections
    if ( !proc.waitForFinished( 60000 ) )
    {
        mStatus->setText( "Timed out fetching addon list." );
        return;
    }

    const QString out = QString::fromLocal8Bit( proc.readAllStandardOutput() );
    const QString err = QString::fromLocal8Bit( proc.readAllStandardError() );
    if ( !err.isEmpty() ) mLog->appendPlainText( err );

    // Output format: "name|description" per line
    for ( const QString &line : out.split( '\n', Qt::SkipEmptyParts ) )
    {
        const QStringList parts = line.split( '|' );
        if ( parts.isEmpty() ) continue;
        const QString name = parts[0].trimmed();
        const QString desc = parts.size() > 1 ? parts[1].trimmed() : "";
        if ( name.isEmpty() ) continue;

        const int r = mAvailTable->rowCount();
        mAvailTable->insertRow( r );
        mAvailTable->setItem( r, 0, new QTableWidgetItem( name ) );
        mAvailTable->setItem( r, 1, new QTableWidgetItem( desc ) );
    }

    mAvailTable->resizeColumnToContents( 0 );
    mStatus->setText( QString( "%1 addon(s) available." )
                      .arg( mAvailTable->rowCount() ) );
}

// ---------------------------------------------------------------------------
// installSelected — g.extension name=X operation=add
// ---------------------------------------------------------------------------
void GrassAddonManager::installSelected()
{
    const int row = mAvailTable->currentRow();
    if ( row < 0 ) return;
    const QString name = mAvailTable->item( row, 0 )->text();

    if ( QMessageBox::question( this, "Install addon",
             QString( "Install '%1' from the official GRASS repository?" ).arg( name ),
             QMessageBox::Yes | QMessageBox::No ) != QMessageBox::Yes )
        return;

    mLog->clear();
    mStatus->setText( "Installing " + name + "…" );
    QApplication::processEvents();

    const int rc = runGExtension( { "name=" + name, "operation=add" } );
    if ( rc == 0 )
    {
        mStatus->setText( name + " installed successfully." );
        loadInstalled();
    }
    else
    {
        mStatus->setText( "Installation of " + name + " failed (see log)." );
    }
}

// ---------------------------------------------------------------------------
// removeSelected — g.extension name=X operation=remove
// ---------------------------------------------------------------------------
void GrassAddonManager::removeSelected()
{
    const int row = mInstTable->currentRow();
    if ( row < 0 ) return;
    const QString name = mInstTable->item( row, 0 )->text();

    if ( QMessageBox::question( this, "Remove addon",
             QString( "Remove '%1'?" ).arg( name ),
             QMessageBox::Yes | QMessageBox::No ) != QMessageBox::Yes )
        return;

    mLog->clear();
    mStatus->setText( "Removing " + name + "…" );
    QApplication::processEvents();

    const int rc = runGExtension( { "name=" + name, "operation=remove" } );
    if ( rc == 0 )
    {
        mStatus->setText( name + " removed." );
        loadInstalled();
    }
    else
    {
        mStatus->setText( "Removal of " + name + " failed (see log)." );
    }
}

// ---------------------------------------------------------------------------
// runGExtension — run g.extension and stream output to mLog
// ---------------------------------------------------------------------------
int GrassAddonManager::runGExtension( const QStringList &args )
{
    const QString gb = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
    if ( gb.isEmpty() ) return -1;

    QProcess proc;
    proc.setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );
    proc.setProcessChannelMode( QProcess::MergedChannels );
    proc.start( gb + "/bin/g.extension", args );

    // Stream output live
    while ( proc.waitForReadyRead( 1000 ) || proc.state() != QProcess::NotRunning )
    {
        const QByteArray data = proc.readAll();
        if ( !data.isEmpty() )
        {
            mLog->appendPlainText( QString::fromLocal8Bit( data ) );
            QApplication::processEvents();
        }
    }
    proc.waitForFinished( 10000 );
    return proc.exitCode();
}
