#include "grassmenubuilder.h"
#include "grassicons.h"

#include <QAction>
#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QMenu>
#include <QMenuBar>
#include <QRegularExpression>
#include <QSet>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcMenuBuilder, "qtgrass.menu")

static QString cleanLabel( QString s )
{
    s.remove( QRegularExpression( "&(?!amp;)" ) );
    return s.simplified();
}

// ---------------------------------------------------------------------------
// Record prefix and command bindings.
// ---------------------------------------------------------------------------
static void trackPrefix( const QString &command, QMenu *currentMenu,
                          QMenu *topMenu,
                          GrassMenuBuilder::PrefixMenuMap &out )
{
    const QStringList parts = command.split('.');
    if ( parts.size() < 2 ) return;

    const QString first     = parts[0];
    const QString twoLevel  = parts[0] + "." + parts[1];

    if ( !out.contains( twoLevel ) )
        out.insert( twoLevel, currentMenu );
    if ( !out.contains( first ) )
        out.insert( first, topMenu );
}

// ---------------------------------------------------------------------------
static void parseItems( const QDomElement &itemsElem,
                         QMenu *menu, QMenu *topMenu,
                         const GrassMenuBuilder::ModuleCallback &cb,
                         const GrassMenuBuilder::HandlerMap &hmap,
                         GrassMenuBuilder::PrefixMenuMap    &prefixMap,
                         GrassMenuBuilder::CommandActionMap &cmdMap )
{
    QDomElement child = itemsElem.firstChildElement();
    while ( !child.isNull() )
    {
        const QString tag = child.tagName();

        if ( tag == "separator" )
        {
            menu->addSeparator();
        }
        else if ( tag == "menu" )
        {
            const QString label = cleanLabel(
                child.firstChildElement( "label" ).text() );
            if ( label.isEmpty() ) { child = child.nextSiblingElement(); continue; }

            QMenu *sub = menu->addMenu( label );
            QDomElement subItems = child.firstChildElement( "items" );
            if ( !subItems.isNull() )
                parseItems( subItems, sub, topMenu, cb, hmap, prefixMap, cmdMap );
        }
        else if ( tag == "menuitem" )
        {
            const QString label    = cleanLabel(
                child.firstChildElement( "label" ).text() );
            const QString command  = child.firstChildElement( "command" ).text().trimmed();
            const QString handler  = child.firstChildElement( "handler" ).text().trimmed();
            const QString help     = child.firstChildElement( "help" ).text().trimmed();
            const QString shortcut = child.firstChildElement( "shortcut" ).text().trimmed();
            const QString iconName = child.firstChildElement( "icon" ).text().trimmed();

            if ( label.isEmpty() ) { child = child.nextSiblingElement(); continue; }

            QString displayLabel = label;
            if ( !command.isEmpty() )
                displayLabel = label + "  (" + command + ")";

            QAction *act = new QAction( displayLabel, menu );
            if ( !help.isEmpty() )      act->setToolTip( help );
            if ( !shortcut.isEmpty() )  act->setShortcut( QKeySequence( shortcut ) );
            if ( !iconName.isEmpty() ) {
                const QIcon ico = grassIcon( iconName );
                if ( !ico.isNull() ) act->setIcon( ico );
            }

            bool connected = false;

            if ( !handler.isEmpty() ) {
                auto it = hmap.find( handler );
                if ( it != hmap.end() ) {
                    const auto fn = it->second;
                    QObject::connect( act, &QAction::triggered, [fn] { fn(); } );
                    connected = true;
                }
            }

            if ( !connected && !command.isEmpty() ) {
                const QString cmd = command;
                QObject::connect( act, &QAction::triggered,
                                  [cb, cmd] { cb( cmd ); } );
                connected = true;

                // Track prefix for family-based addon placement
                trackPrefix( command, menu, topMenu, prefixMap );

                // Track exact command → {menu, action} for after: placement
                if ( !cmdMap.contains( command ) )
                    cmdMap.insert( command, { menu, act } );
            }

            if ( !connected )
                act->setEnabled( false );

            menu->addAction( act );
        }

        child = child.nextSiblingElement();
    }
}

// ---------------------------------------------------------------------------
bool GrassMenuBuilder::build( QMenuBar             *mb,
                               const ModuleCallback &moduleCallback,
                               const HandlerMap     &handlerMap,
                               PrefixMenuMap        &prefixMap,
                               CommandActionMap     &commandActions,
                               const QStringList    &skipLabels )
{
    const QByteArray gisbase = qgetenv( "GISBASE" );
    if ( gisbase.isEmpty() ) {
        qCWarning(lcMenuBuilder) << "GrassMenuBuilder: GISBASE not set";
        return false;
    }

    const QString xmlPath = QString::fromLocal8Bit( gisbase )
                            + "/gui/wxpython/xml/menudata.xml";
    QFile f( xmlPath );
    if ( !f.open( QIODevice::ReadOnly ) ) {
        qCWarning(lcMenuBuilder) << "GrassMenuBuilder: cannot open" << xmlPath;
        return false;
    }

    QDomDocument doc;
    const auto parseResult = doc.setContent( &f );
    if ( !parseResult ) {
        qCWarning(lcMenuBuilder) << "GrassMenuBuilder: XML parse error"
                                  << parseResult.errorLine
                                  << parseResult.errorColumn
                                  << parseResult.errorMessage;
        return false;
    }
    f.close();

    const QDomElement menubar = doc.documentElement()
                                    .firstChildElement( "menubar" );
    if ( menubar.isNull() ) {
        qCWarning(lcMenuBuilder) << "GrassMenuBuilder: no <menubar>";
        return false;
    }

    QDomElement menuElem = menubar.firstChildElement( "menu" );
    while ( !menuElem.isNull() )
    {
        const QString label = cleanLabel(
            menuElem.firstChildElement( "label" ).text() );

        if ( !label.isEmpty() )
        {
            bool skip = false;
            for ( const QString &sl : skipLabels )
                if ( label.compare( sl, Qt::CaseInsensitive ) == 0 )
                    { skip = true; break; }

            if ( !skip )
            {
                QMenu *qmenu = mb->addMenu( label );
                QDomElement items = menuElem.firstChildElement( "items" );
                if ( !items.isNull() )
                    parseItems( items, qmenu, qmenu,
                                moduleCallback, handlerMap,
                                prefixMap, commandActions );
            }
        }
        menuElem = menuElem.nextSiblingElement( "menu" );
    }

    return true;
}
