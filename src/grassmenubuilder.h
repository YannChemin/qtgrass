#pragma once

/*!
    \file grassmenubuilder.h
    \brief Builds a QMenuBar from the GRASS \c menudata.xml definition file.
*/

#include <QMap>
#include <QMenu>
#include <QMenuBar>
#include <QString>
#include <functional>
#include <map>

class QDomElement;

/*!
    \class GrassMenuBuilder
    \brief Static builder that populates a QMenuBar from GRASS's \c menudata.xml.

    Reads \c \$GISBASE/gui/wxpython/xml/menudata.xml and creates a menu tree
    matching the wxGUI layout.  Each \c \<menuitem\> with a \c \<command\> tag
    triggers \a moduleCallback with the command string when activated.
    Items with a known \c \<handler\> name call the corresponding entry in
    \a handlerMap.

    Two index maps are filled during build():
    \li \b prefixMap — maps the module prefix (e.g. \c "r.", \c "v.") to the
        QMenu that hosts commands in that family.
    \li \b commandActions — maps a full command name to the menu and action that
        represent it, enabling addon menus to insert numbered variants
        (e.g. \c "i.atcorr2") immediately after the base module's action.
*/
class GrassMenuBuilder
{
public:
    /*! \brief Callback invoked when a module menu item is triggered. */
    using ModuleCallback = std::function<void(const QString &command)>;

    /*! \brief Map from handler name to a zero-argument callable (for built-in actions). */
    using HandlerMap = std::map<QString, std::function<void()>>;

    /*! \brief Maps a module prefix (e.g. \c "r.") to the QMenu that hosts it. */
    using PrefixMenuMap = QMap<QString, QMenu *>;

    /*!
        \struct ActionLocation
        \brief Locates a QAction within its parent QMenu.
    */
    struct ActionLocation {
        QMenu   *menu   = nullptr; /*!< Parent menu. */
        QAction *action = nullptr; /*!< The action item. */
    };

    /*! \brief Maps a command name (e.g. \c "r.slope.aspect") to its menu action. */
    using CommandActionMap = QMap<QString, ActionLocation>;

    /*!
        \brief Populate \a mb with menus built from \c menudata.xml.

        \param mb             Target menu bar (menus are appended).
        \param moduleCallback Called when the user activates a module menu item.
        \param handlerMap     Maps handler names to callables for built-in actions.
        \param prefixMap      Filled with prefix → QMenu* mappings.
        \param commandActions Filled with command → ActionLocation mappings.
        \param skipLabels     Top-level menu labels to skip (case-insensitive).
                              Defaults to \c {"File", "Help"} which qtgrass builds separately.
        \return \c true on success; \c false if the XML could not be loaded or parsed.
    */
    static bool build( QMenuBar             *mb,
                        const ModuleCallback &moduleCallback,
                        const HandlerMap     &handlerMap,
                        PrefixMenuMap        &prefixMap,
                        CommandActionMap     &commandActions,
                        const QStringList    &skipLabels = { "File", "Help" } );
};
