#include "grassmapsetbrowser.h"

#include <QDir>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

GrassMapsetBrowser::GrassMapsetBrowser( QWidget *parent )
    : QDialog( parent )
{
    setWindowTitle( "Open GRASS Mapset" );
    setMinimumSize( 700, 400 );

    QVBoxLayout *root = new QVBoxLayout( this );

    // — GISDBASE row —
    QGroupBox *gbDb = new QGroupBox( "GRASS Database (GISDBASE)", this );
    QHBoxLayout *dbRow = new QHBoxLayout( gbDb );
    mGisdbEdit = new QLineEdit( this );
    mGisdbEdit->setPlaceholderText( "Path to GRASS database directory…" );
    QPushButton *browseBtn = new QPushButton( "Browse…", this );
    dbRow->addWidget( mGisdbEdit, 1 );
    dbRow->addWidget( browseBtn );
    root->addWidget( gbDb );

    // — Location + Mapset lists —
    QHBoxLayout *listsRow = new QHBoxLayout();

    QGroupBox *gbLoc = new QGroupBox( "Location (Project)", this );
    QVBoxLayout *locLay = new QVBoxLayout( gbLoc );
    mLocList = new QListWidget( gbLoc );
    locLay->addWidget( mLocList );

    QGroupBox *gbMs = new QGroupBox( "Mapset", this );
    QVBoxLayout *msLay = new QVBoxLayout( gbMs );
    mMslist = new QListWidget( gbMs );
    msLay->addWidget( mMslist );

    listsRow->addWidget( gbLoc );
    listsRow->addWidget( gbMs );
    root->addLayout( listsRow, 1 );

    // — Workspace files found in selected location —
    mGxwBox = new QGroupBox( "Workspace files (.gxw) found in location:", this );
    mGxwBox->setVisible( false );
    QVBoxLayout *gxwLay = new QVBoxLayout( mGxwBox );
    mGxwList = new QListWidget( mGxwBox );
    mGxwList->setMaximumHeight( 80 );
    gxwLay->addWidget( mGxwList );
    mOpenGxwBtn = new QPushButton( "Open selected workspace", mGxwBox );
    mOpenGxwBtn->setEnabled( false );
    gxwLay->addWidget( mOpenGxwBtn );
    root->addWidget( mGxwBox );

    // — Status + buttons —
    mStatus = new QLabel( " ", this );
    mStatus->setStyleSheet( "color: #c00;" );
    root->addWidget( mStatus );

    QDialogButtonBox *btns = new QDialogButtonBox( this );
    mOpenBtn = btns->addButton( "Open Mapset", QDialogButtonBox::AcceptRole );
    btns->addButton( QDialogButtonBox::Cancel );
    mOpenBtn->setEnabled( false );
    root->addWidget( btns );

    // Connections
    connect( browseBtn,  &QPushButton::clicked,
             this, &GrassMapsetBrowser::browseGisdbase );
    connect( mGisdbEdit, &QLineEdit::textChanged,
             this, [this]( const QString &p ) {
                 mGisdbase = p.trimmed();
                 populateLocations( mGisdbase );
             } );
    connect( mLocList,  &QListWidget::currentTextChanged,
             this, [this]( const QString &loc ) {
                 mLocation = loc;
                 populateMapsets( loc );
             } );
    connect( mMslist,   &QListWidget::currentTextChanged,
             this, [this]( const QString &ms ) {
                 mMapset = ms;
                 validate();
             } );
    connect( btns, &QDialogButtonBox::accepted, this, &QDialog::accept );
    connect( btns, &QDialogButtonBox::rejected, this, &QDialog::reject );
    connect( mLocList,  &QListWidget::itemDoubleClicked,
             this, [this]{ if ( mMslist->count() == 1 ) accept(); } );
    connect( mMslist,   &QListWidget::itemDoubleClicked,
             this, [this]{ if ( mOpenBtn->isEnabled() ) accept(); } );

    // Workspace list interactions
    connect( mGxwList, &QListWidget::currentItemChanged, this,
             [this]( QListWidgetItem *item ) {
                 mOpenGxwBtn->setEnabled( item != nullptr );
             } );
    connect( mGxwList, &QListWidget::itemDoubleClicked,
             this, [this]( QListWidgetItem * ) {
                 if ( mOpenGxwBtn->isEnabled() ) mOpenGxwBtn->click();
             } );
    connect( mOpenGxwBtn, &QPushButton::clicked, this, [this] {
        auto *item = mGxwList->currentItem();
        if ( !item ) return;
        mSelectedWorkspace = item->data( Qt::UserRole ).toString();
        accept();
    } );
}

void GrassMapsetBrowser::setInitial( const QString &gisdbase,
                                      const QString &location,
                                      const QString &mapset )
{
    // Default to $HOME/grassdata when no database has been set yet
    QString db = gisdbase;
    if ( db.isEmpty() )
    {
        const QString grassdata = QDir::homePath() + "/grassdata";
        db = QDir( grassdata ).exists() ? grassdata : QDir::homePath();
    }
    mGisdbEdit->setText( db );
    populateLocations( db );

    if ( !location.isEmpty() )
    {
        const auto items = mLocList->findItems( location, Qt::MatchExactly );
        if ( !items.isEmpty() )
        {
            mLocList->setCurrentItem( items.first() );
            populateMapsets( location );
        }
    }
    if ( !mapset.isEmpty() )
    {
        const auto items = mMslist->findItems( mapset, Qt::MatchExactly );
        if ( !items.isEmpty() )
            mMslist->setCurrentItem( items.first() );
    }
}

void GrassMapsetBrowser::browseGisdbase()
{
    const QString startDir = !mGisdbEdit->text().isEmpty()
                              ? mGisdbEdit->text()
                              : ( QDir( QDir::homePath() + "/grassdata" ).exists()
                                  ? QDir::homePath() + "/grassdata"
                                  : QDir::homePath() );
    const QString dir = QFileDialog::getExistingDirectory(
        this, "Select GRASS Database Directory", startDir );
    if ( !dir.isEmpty() )
        mGisdbEdit->setText( dir );
}

void GrassMapsetBrowser::updateWorkspaceList( const QString &locationDir )
{
    mGxwList->clear();
    mGxwBox->setVisible( false );
    mOpenGxwBtn->setEnabled( false );

    if ( locationDir.isEmpty() ) return;

    const QStringList gxwFiles = QDir( locationDir ).entryList(
        {"*.gxw"}, QDir::Files, QDir::Name );

    for ( const QString &fname : gxwFiles )
    {
        const QString fullPath = locationDir + "/" + fname;
        auto *item = new QListWidgetItem(
            QFileInfo( fname ).completeBaseName() + "  (" + fname + ")", mGxwList );
        item->setData( Qt::UserRole, fullPath );
        item->setToolTip( fullPath );
    }

    if ( !gxwFiles.isEmpty() )
        mGxwBox->setVisible( true );
}

void GrassMapsetBrowser::populateLocations( const QString &gisdbase )
{
    mLocList->clear();
    mMslist->clear();
    mLocation.clear();
    mMapset.clear();
    mSelectedWorkspace.clear();
    updateWorkspaceList( {} );
    validate();

    if ( gisdbase.isEmpty() ) return;

    QDir dir( gisdbase );
    if ( !dir.exists() ) return;

    // A location is a directory that contains a PERMANENT/WIND file
    for ( const QString &entry : dir.entryList( QDir::Dirs | QDir::NoDotAndDotDot,
                                                QDir::Name ) )
    {
        if ( QFile::exists( dir.filePath( entry + "/PERMANENT/WIND" ) ) )
            mLocList->addItem( entry );
    }
}



void GrassMapsetBrowser::populateMapsets( const QString &location )
{
    // Scan the location directory for .gxw workspace files
    if ( !mGisdbase.isEmpty() && !location.isEmpty() )
        updateWorkspaceList( mGisdbase + "/" + location );
    mMslist->clear();
    mMapset.clear();
    validate();

    if ( mGisdbase.isEmpty() || location.isEmpty() ) return;

    QDir locDir( mGisdbase + "/" + location );
    for ( const QString &entry : locDir.entryList( QDir::Dirs | QDir::NoDotAndDotDot,
                                                    QDir::Name ) )
    {
        // A valid mapset contains a WIND file
        if ( QFile::exists( locDir.filePath( entry + "/WIND" ) ) )
            mMslist->addItem( entry );
    }

    // Auto-select PERMANENT if present
    const auto perm = mMslist->findItems( "PERMANENT", Qt::MatchExactly );
    if ( !perm.isEmpty() )
        mMslist->setCurrentItem( perm.first() );
}

void GrassMapsetBrowser::validate()
{
    const bool ok = !mGisdbase.isEmpty() &&
                    !mLocation.isEmpty()  &&
                    !mMapset.isEmpty();
    mOpenBtn->setEnabled( ok );
    if ( ok )
        mStatus->setText( QString( " %1 / %2 / %3 " )
                          .arg( mGisdbase, mLocation, mMapset ) );
    else
        mStatus->clear();
}
