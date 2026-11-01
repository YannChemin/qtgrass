#pragma once

#include <QDialog>

class QTableView;
class QStandardItemModel;
class QLineEdit;
class QLabel;

// Attribute table viewer for a GRASS vector map.
// Reads data by running v.db.select via QProcess.
// Supports filtering by typing in the search box.
class GrassAttribTable : public QDialog
{
    Q_OBJECT

  public:
    // name   — vector map name
    // mapset — mapset (empty = current)
    // layer  — database layer (default 1)
    explicit GrassAttribTable( const QString &name,
                                const QString &mapset = {},
                                int layer = 1,
                                QWidget *parent = nullptr );

  private:
    bool loadData( const QString &name, const QString &mapset, int layer );

    QTableView         *mTable  = nullptr;
    QStandardItemModel *mModel  = nullptr;
    QLabel             *mStatus = nullptr;
};
