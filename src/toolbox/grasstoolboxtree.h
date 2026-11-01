#pragma once

#include <QWidget>
#include <QSortFilterProxyModel>

class QLineEdit;
class QTreeView;
class QStandardItemModel;
class QStandardItem;
class QDomElement;

// ---------------------------------------------------------------------------
// GrassToolboxFilterProxy
//
// Accepts a row if:
//   • the item itself matches (label, command, keywords contain the filter text)
//   • OR it is a group and at least one descendant matches
// ---------------------------------------------------------------------------
class GrassToolboxFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT

  public:
    explicit GrassToolboxFilterProxy( QObject *parent = nullptr );

  protected:
    bool filterAcceptsRow( int sourceRow,
                           const QModelIndex &sourceParent ) const override;

  private:
    bool anyChildMatches( const QModelIndex &parent ) const;
};

// ---------------------------------------------------------------------------
// GrassToolboxTree
//
// Left-panel widget: search box + tree view of all GRASS modules.
// Loads $GISBASE/gui/wxpython/xml/module_tree_menudata.xml.
// Emits moduleActivated(name) when a leaf is double-clicked or Enter pressed.
// ---------------------------------------------------------------------------
class GrassToolboxTree : public QWidget
{
    Q_OBJECT

  public:
    explicit GrassToolboxTree( QWidget *parent = nullptr );

    // Reload core modules from disk (call after GISBASE changes)
    bool loadModules( const QString &xmlPath = {} );

    // Append grass-addons section from the generated grassaddonslist.h.
    // Called automatically after loadModules().
    void loadAddons();

    // Prepend a "Display Layers" section for the 10 native d.* composites.
    // Items emit moduleActivated("d.rgb") etc. — grassapp.cpp intercepts these.
    void addNativeDisplaySection();

  signals:
    void moduleActivated( const QString &moduleName );

  private slots:
    void onFilterChanged( const QString &text );
    void onItemActivated( const QModelIndex &index );

  private:
    // Data roles stored on each QStandardItem
    enum Role {
        Command  = Qt::UserRole,      // module command name (empty for groups)
        Search   = Qt::UserRole + 1,  // concatenated label+command+keywords
    };

    void populateModel( const QDomElement &items,
                        QStandardItem *parent );

    QLineEdit            *mSearch      = nullptr;
    QTreeView            *mTree        = nullptr;
    QStandardItemModel   *mModel       = nullptr;
    GrassToolboxFilterProxy *mProxy    = nullptr;
};
