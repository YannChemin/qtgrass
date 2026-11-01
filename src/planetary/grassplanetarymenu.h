#pragma once

#include <QMap>
#include <QMenu>
#include <QProcessEnvironment>
#include <QSet>
#include <QString>
#include <QStringList>
#include <functional>

class GrassInterface;
class QWidget;

/*!
    \struct GrassPlanetaryModuleInfo
    \brief Cached metadata for one p.* planetary module.

    Filled by GrassPlanetaryMenu::scanAvailableModules() via the
    `<module> --interface-description` XML output. Used to drive
    the **auto-constructed** Planetary menu, where submenus are
    derived from each module's 2nd keyword (the menu-category tag,
    e.g. "Camera & Geometry", "Crater Analysis"). New addons that
    follow the keyword convention appear in the menu without any
    qtgrass code change.
*/
struct GrassPlanetaryModuleInfo {
    QString     label;     /*!< short user-facing description */
    QStringList keywords;  /*!< Planetary, <category>, <topic>, ... */
};

/*!
    \class GrassPlanetaryMenu
    \brief Auto-constructs the top-level Planetary menu in qtgrass.

    The menu hierarchy is **data-driven** from the installed modules'
    keyword metadata, not hardcoded:
      keyword[0] = "Planetary"      (top-level menu, fixed)
      keyword[1] = <category>       (submenu name)
      keyword[2..] = topic tags     (ignored for menu structure)

    Modules with a bespoke Qt6 dialog (e.g. p.crater, p.crater.draw,
    p.crater.freq, p.spiceinit) are overridden via a small static
    factory table; everything else launches the generic
    GrassModuleDialog through the openModule callback.
*/
class GrassPlanetaryMenu
{
  public:
    using ModuleCallback = std::function<void(const QString &)>;

    /*!
        Scan $GISBASE/bin, $GISBASE/scripts, $GRASS_ADDON_PATH/bin for
        installed p.* executables, run their --interface-description using
        \a env (which must supply GISRC so the modules can initialise GRASS),
        and return their label + keywords.
    */
    static QMap<QString, GrassPlanetaryModuleInfo>
        scanAvailableModules( const QProcessEnvironment &env );

    /*!
        Build the menu hierarchy under \a menu from \a modules,
        grouping by keyword[1].
    */
    static void populate( QMenu *menu,
                          const ModuleCallback &openModule,
                          const QMap<QString, GrassPlanetaryModuleInfo>
                                &modules,
                          GrassInterface *iface,
                          QWidget *parent );

    /*!
        Convenience for legacy call sites that just want the name list.
    */
    static QSet<QString> availableNames(
        const QMap<QString, GrassPlanetaryModuleInfo> &modules);
};
