#include "grasscrsmodel.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QLoggingCategory>
#include <QUuid>

Q_LOGGING_CATEGORY(lcCrsModel, "qtgrass.startup.crs")

static const QString kProjDb = "/usr/share/proj/proj.db";

GrassCrsModel::GrassCrsModel( Authority auth, QObject *parent )
    : QAbstractTableModel( parent )
    , mAuth( auth )
    , mDbId( QUuid::createUuid().toString() )
{}

GrassCrsModel::~GrassCrsModel()
{
    if ( QSqlDatabase::contains( mDbId ) )
    {
        mDb.close();
        QSqlDatabase::removeDatabase( mDbId );
    }
}

bool GrassCrsModel::open()
{
    mDb = QSqlDatabase::addDatabase( "QSQLITE", mDbId );
    mDb.setDatabaseName( kProjDb );
    mDb.setConnectOptions( "QSQLITE_OPEN_READONLY" );

    if ( !mDb.open() )
    {
        qCWarning(lcCrsModel) << "GrassCrsModel: cannot open proj.db:"
                   << mDb.lastError().text();
        return false;
    }

    loadAll();
    return true;
}

void GrassCrsModel::loadAll()
{
    mAll.clear();

    const QString authFilter = ( mAuth == EPSG ) ? "EPSG" : "IAU_2015";

    // For IAU (planetary bodies) include both geodetic (geographic) and projected
    // CRS so all IAU-defined variants (Equirectangular, Sinusoidal, Polar, etc.)
    // are available for location creation.
    // For EPSG we include both geographic and projected (e.g. UTM zones).
    // Wrapped in a subquery so ORDER BY CAST(code AS INTEGER) works in SQLite
    // (UNION does not allow expressions in ORDER BY, only column names/positions).
    const QString sql = ( mAuth == IAU )
        ? R"(
        SELECT * FROM (
            SELECT auth_name, code, name, 'geographic' AS type
              FROM geodetic_crs
             WHERE auth_name = ?
               AND (deprecated IS NULL OR deprecated = 0)
            UNION ALL
            SELECT auth_name, code, name, 'projected' AS type
              FROM projected_crs
             WHERE auth_name = ?
               AND (deprecated IS NULL OR deprecated = 0)
        ) ORDER BY CAST(code AS INTEGER)
        )"
        : R"(
        SELECT * FROM (
            SELECT auth_name, code, name, 'geographic' AS type
              FROM geodetic_crs
             WHERE auth_name = ?
               AND (deprecated IS NULL OR deprecated = 0)
            UNION ALL
            SELECT auth_name, code, name, 'projected' AS type
              FROM projected_crs
             WHERE auth_name = ?
               AND (deprecated IS NULL OR deprecated = 0)
        ) ORDER BY CAST(code AS INTEGER)
        )";

    QSqlQuery q( mDb );
    q.prepare( sql );
    q.addBindValue( authFilter );
    q.addBindValue( authFilter );   // second placeholder (projected_crs sub-select)

    if ( !q.exec() )
    {
        qCWarning(lcCrsModel) << "GrassCrsModel query failed:" << q.lastError().text();
        return;
    }

    while ( q.next() )
    {
        CrsEntry e;
        e.authName = q.value( 0 ).toString();
        e.code     = q.value( 1 ).toString();
        e.name     = q.value( 2 ).toString();
        e.type     = q.value( 3 ).toString();
        mAll.append( e );
    }

    // Build initial visible set (all)
    mVisible.resize( mAll.size() );
    std::iota( mVisible.begin(), mVisible.end(), 0 );
}

void GrassCrsModel::setFilter( const QString &query )
{
    beginResetModel();
    mVisible.clear();

    if ( query.isEmpty() )
    {
        mVisible.resize( mAll.size() );
        std::iota( mVisible.begin(), mVisible.end(), 0 );
    }
    else
    {
        const QString lq = query.toLower();
        for ( int i = 0; i < mAll.size(); ++i )
        {
            const CrsEntry &e = mAll[i];
            if ( e.code.contains( lq, Qt::CaseInsensitive ) ||
                 e.name.contains( lq, Qt::CaseInsensitive ) )
                mVisible.append( i );
        }
    }

    endResetModel();
}

CrsEntry GrassCrsModel::entryAt( int row ) const
{
    if ( row < 0 || row >= mVisible.size() ) return {};
    return mAll[ mVisible[row] ];
}

QString GrassCrsModel::sridAt( int row ) const
{
    const CrsEntry e = entryAt( row );
    if ( e.code.isEmpty() ) return {};
    return e.authName + ":" + e.code;
}

int GrassCrsModel::rowCount( const QModelIndex &parent ) const
{
    return parent.isValid() ? 0 : mVisible.size();
}

int GrassCrsModel::columnCount( const QModelIndex &parent ) const
{
    return parent.isValid() ? 0 : 3;
}

QVariant GrassCrsModel::data( const QModelIndex &idx, int role ) const
{
    if ( !idx.isValid() || idx.row() >= mVisible.size() )
        return {};

    const CrsEntry &e = mAll[ mVisible[idx.row()] ];

    if ( role == Qt::DisplayRole )
    {
        switch ( idx.column() )
        {
            case 0: return e.authName + ":" + e.code;
            case 1: return e.name;
            case 2: return e.type;
        }
    }
    if ( role == Qt::ToolTipRole )
        return e.authName + ":" + e.code + " — " + e.name;

    return {};
}

QVariant GrassCrsModel::headerData( int sec, Qt::Orientation orientation,
                                     int role ) const
{
    if ( role != Qt::DisplayRole || orientation != Qt::Horizontal )
        return {};

    switch ( sec )
    {
        case 0: return "Code";
        case 1: return "Name";
        case 2: return "Type";
    }
    return {};
}
