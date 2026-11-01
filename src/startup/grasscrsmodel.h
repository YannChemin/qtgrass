#pragma once

#include <QAbstractTableModel>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

// A CRS entry from the PROJ database (either EPSG or IAU_2015).
struct CrsEntry {
    QString authName;  // "EPSG" or "IAU_2015"
    QString code;
    QString name;
    QString type;      // "geographic 2D", "projected", etc.
};

// Read-only model backed by /usr/share/proj/proj.db.
// Supports two modes: EPSG search and IAU_2015 body listing.
class GrassCrsModel : public QAbstractTableModel
{
    Q_OBJECT

  public:
    enum Authority { EPSG, IAU };

    explicit GrassCrsModel( Authority auth, QObject *parent = nullptr );
    ~GrassCrsModel() override;

    // Load rows from the database; call once after construction.
    bool open();

    // Filter visible rows to those matching the query (case-insensitive
    // search across code + name). Empty query shows all rows.
    void setFilter( const QString &query );

    // Returns the CrsEntry for the given proxy row index.
    CrsEntry entryAt( int row ) const;

    // Returns SRID string e.g. "EPSG:4326" or "IAU_2015:49900".
    QString sridAt( int row ) const;

    int rowCount( const QModelIndex &parent = {} ) const override;
    int columnCount( const QModelIndex &parent = {} ) const override;
    QVariant data( const QModelIndex &idx, int role = Qt::DisplayRole ) const override;
    QVariant headerData( int sec, Qt::Orientation, int role = Qt::DisplayRole ) const override;

  private:
    void loadAll();

    Authority        mAuth;
    QSqlDatabase     mDb;
    QString          mDbId;       // unique connection name

    QVector<CrsEntry> mAll;       // all rows from DB
    QVector<int>      mVisible;   // indices into mAll currently shown
};
