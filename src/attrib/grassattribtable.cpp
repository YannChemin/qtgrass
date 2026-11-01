#include "grassattribtable.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSortFilterProxyModel>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>
#include <QDebug>

GrassAttribTable::GrassAttribTable( const QString &name,
                                     const QString &mapset,
                                     int layer,
                                     QWidget *parent )
    : QDialog( parent )
{
    setWindowTitle( "Attribute Table — " + name );
    setMinimumSize( 700, 450 );

    QVBoxLayout *lay = new QVBoxLayout( this );

    // Filter box
    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->addWidget( new QLabel( "Filter:", this ) );
    QLineEdit *filter = new QLineEdit( this );
    filter->setClearButtonEnabled( true );
    filter->setPlaceholderText( "Search across all columns…" );
    topRow->addWidget( filter, 1 );
    lay->addLayout( topRow );

    // Table
    mModel = new QStandardItemModel( this );
    mModel->setHorizontalHeaderItem( 0, new QStandardItem( "Loading…" ) );

    auto *proxy = new QSortFilterProxyModel( this );
    proxy->setSourceModel( mModel );
    proxy->setFilterCaseSensitivity( Qt::CaseInsensitive );
    proxy->setFilterKeyColumn( -1 );  // search all columns

    mTable = new QTableView( this );
    mTable->setModel( proxy );
    mTable->setAlternatingRowColors( true );
    mTable->setEditTriggers( QAbstractItemView::NoEditTriggers );
    mTable->setSelectionBehavior( QAbstractItemView::SelectRows );
    mTable->setSortingEnabled( true );
    mTable->horizontalHeader()->setStretchLastSection( true );
    mTable->verticalHeader()->setVisible( false );
    lay->addWidget( mTable, 1 );

    mStatus = new QLabel( this );
    lay->addWidget( mStatus );

    QDialogButtonBox *btns = new QDialogButtonBox( QDialogButtonBox::Close, this );
    connect( btns, &QDialogButtonBox::rejected, this, &QDialog::accept );
    lay->addWidget( btns );

    connect( filter, &QLineEdit::textChanged,
             proxy, &QSortFilterProxyModel::setFilterFixedString );

    loadData( name, mapset, layer );
}

bool GrassAttribTable::loadData( const QString &name,
                                  const QString &mapset,
                                  int layer )
{
    mModel->clear();

    const QString gisBase = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
    if ( gisBase.isEmpty() )
    {
        mStatus->setText( "GISBASE not set." );
        return false;
    }

    // Build map reference string "name" or "name@mapset"
    const QString mapRef = mapset.isEmpty() ? name : name + "@" + mapset;

    QProcess proc;
    proc.setProcessEnvironment( QProcessEnvironment::systemEnvironment() );
    proc.start( gisBase + "/bin/v.db.select",
                { "map=" + mapRef,
                  "layer=" + QString::number( layer ),
                  "separator=pipe" } );

    if ( !proc.waitForFinished( 15000 ) )
    {
        mStatus->setText( "v.db.select timed out." );
        return false;
    }

    const QString stderr_ = proc.readAllStandardError();
    const QString stdout_ = proc.readAllStandardOutput();

    if ( proc.exitCode() != 0 )
    {
        mStatus->setText( "v.db.select failed: " + stderr_.simplified() );
        return false;
    }

    const QStringList lines = stdout_.split( '\n', Qt::SkipEmptyParts );
    if ( lines.isEmpty() )
    {
        mStatus->setText( "No attribute data (vector may have no table)." );
        return false;
    }

    // First line is the header
    const QStringList headers = lines.first().split( '|' );
    mModel->setHorizontalHeaderLabels( headers );

    for ( int r = 1; r < lines.size(); ++r )
    {
        const QStringList fields = lines[r].split( '|' );
        QList<QStandardItem *> row;
        for ( int c = 0; c < headers.size(); ++c )
        {
            const QString val = ( c < fields.size() ) ? fields[c] : "";
            auto *item = new QStandardItem( val );
            item->setEditable( false );
            row.append( item );
        }
        mModel->appendRow( row );
    }

    const int nRows = mModel->rowCount();
    mStatus->setText( QString( " %1 feature(s), %2 column(s)" )
                      .arg( nRows ).arg( headers.size() ) );

    mTable->resizeColumnsToContents();
    return true;
}
