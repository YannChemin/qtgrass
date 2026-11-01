#pragma once
// GrassAddonMenu::populate()
//
// Reads the compile-time list generated from ADDONS_MENU.md (the human-edited
// authoritative placement table) and inserts each addon into the correct QMenu.
//
// Menu path format (from ADDONS_MENU.md second column):
//   "imagery/evapotranspiration"   → Imagery → Evapotranspiration  (existing submenu)
//   "raster/multi-criteria-decision" → Raster → new submenu created if not yet present
//   "tools/addons"                 → Tools   → Addon Extensions
//
// Submenus that match an existing QMenu (via PrefixMenuMap) are used directly.
// Submenus with no existing match are created at the bottom of the parent menu.
//
// Each action is labelled:
//   "Human readable description  (module.name)"
// or just the module name when no description was found.

#include <QMap>
#include <QMenu>
#include <QAction>
#include <QString>
#include <QStringList>
#include <functional>

#include "grassmenubuilder.h"

#ifdef HAVE_GRASS_ADDONS
#  include "grassaddonslist.h"
#endif

namespace GrassAddonMenu
{

using ModuleCallback = std::function<void(const QString &)>;

// ── Helpers ───────────────────────────────────────────────────────────

// Slug → human-readable submenu title used when creating a NEW submenu.
// E.g.  "multi-criteria-decision" → "Multi-criteria decision"
static inline QString slugToTitle( const QString &slug )
{
    QString s = slug;
    s.replace('-', ' ');
    if ( !s.isEmpty() ) s[0] = s[0].toUpper();
    return s;
}

// Add one module action  "Label  (module.name)"
static inline void addAction( QMenu *menu, const QString &name,
                               const QString &label,
                               const ModuleCallback &cb )
{
    const QString text = label.isEmpty()
        ? name
        : QString( "%1  (%2)" ).arg( label ).arg( name );
    QAction *a = menu->addAction( text );
    a->setToolTip( name );
    const QString m = name;
    QObject::connect( a, &QAction::triggered, [cb, m] { cb(m); } );
}

// ── Main entry ────────────────────────────────────────────────────────

static inline void populate(
    const GrassMenuBuilder::PrefixMenuMap    &prefixMap,
    const GrassMenuBuilder::CommandActionMap &cmdMap,
    QMenu                                    *toolsMenu,
    const ModuleCallback                     &callback )
{
#ifndef HAVE_GRASS_ADDONS
    Q_UNUSED(prefixMap) Q_UNUSED(toolsMenu) Q_UNUSED(callback)
    return;
#else
    // ── Pass 0: "after:base_module" — numbered variants ──────────────
    // Insert each addon with menuPath "after:X" immediately below the
    // core module X's QAction, in the same QMenu that hosts X.
    for ( const GrassAddons::AddonInfo &info : GrassAddons::allAddons() )
    {
        const QString fullPath = info.menuPath;
        if ( !fullPath.startsWith( "after:", Qt::CaseInsensitive ) )
            continue;

        const QString baseCmd = fullPath.mid( 6 ).trimmed();  // "after:i.atcorr" → "i.atcorr"
        auto loc = cmdMap.value( baseCmd );
        if ( !loc.menu || !loc.action ) continue;   // base module not in menu

        QAction *newAct = new QAction(
            QString( "%1  (%2)" ).arg( info.label[0] ? info.label : info.name )
                                 .arg( info.name ),
            loc.menu );
        newAct->setToolTip( info.name );
        const QString m = info.name;
        QObject::connect( newAct, &QAction::triggered, [callback, m] { callback( m ); } );

        // Insert immediately after the base module's action
        const QList<QAction*> acts = loc.menu->actions();
        const int idx = acts.indexOf( loc.action );
        if ( idx >= 0 && idx + 1 < acts.size() )
            loc.menu->insertAction( acts[ idx + 1 ], newAct );
        else
            loc.menu->addAction( newAct );
    }

    // ── Build the top-level menu map from prefixMap ───────────────────
    // prefixMap contains  "i" → Imagery menu,  "r" → Raster menu, etc.
    // Build a separate slug→QMenu map for top-level lookups:
    //   "imagery" → Imagery QMenu,  "raster" → Raster, …
    static const QMap<QString,QString> catToSlug = {
        { "r",  "raster"    }, { "r3", "raster-3d" }, { "v",  "vector"   },
        { "i",  "imagery"   }, { "t",  "temporal"  }, { "db", "database" },
        { "d",  "settings"  }, { "g",  "settings"  }, { "m",  "tools"    },
    };
    QMap<QString, QMenu*> topMenus;   // slug → QMenu*
    for ( auto it = catToSlug.cbegin(); it != catToSlug.cend(); ++it )
    {
        QMenu *m = prefixMap.value( it.key() );
        if ( m && !topMenus.contains( it.value() ) )
            topMenus.insert( it.value(), m );
    }
    if ( toolsMenu ) topMenus.insert( "tools", toolsMenu );

    // ── Per-top-menu: track created submenus & separator state ────────
    // submenus created this session: slug → QMenu*
    QMap<QString, QMenu*> createdSubs;

    // Track which menus already got a separator+header for addon additions
    // to EXISTING submenus (ones found via prefixMap two-level match).
    QSet<QMenu*> headerAdded;

    // ── Process each addon from the generated header ──────────────────
    for ( const GrassAddons::AddonInfo &info : GrassAddons::allAddons() )
    {
        const QString name     = info.name;
        const QString label    = info.label;
        const QString fullPath = info.menuPath;  // e.g. "imagery/evapotranspiration"

        // Already handled in Pass 0
        if ( fullPath.startsWith( "after:", Qt::CaseInsensitive ) ) continue;

        // Split into top-level slug and optional submenu slug
        const QStringList parts = fullPath.split('/');
        if ( parts.isEmpty() ) continue;

        const QString topSlug = parts[0];                          // "imagery"
        const QString subSlug = parts.size() > 1 ? parts[1] : ""; // "evapotranspiration"

        QMenu *topMenu = topMenus.value( topSlug, toolsMenu );
        if ( !topMenu ) topMenu = toolsMenu;
        if ( !topMenu ) continue;

        if ( subSlug.isEmpty() )
        {
            // No submenu — add directly to top-level menu
            addAction( topMenu, name, label, callback );
            continue;
        }

        // ── Try to find an existing submenu via prefixMap ─────────────
        // The first token of the module name gives us the two-level prefix:
        // "i.evapo.potrad" → "i.evapo".  If that's in prefixMap, use it.
        QMenu *targetSub = nullptr;

        const QStringList modParts = name.split('.');
        if ( modParts.size() >= 2 )
        {
            const QString twoLevel = modParts[0] + "." + modParts[1];
            targetSub = prefixMap.value( twoLevel, nullptr );
        }

        if ( targetSub )
        {
            // Existing submenu — add with "— Addon modules —" separator once
            if ( !headerAdded.contains( targetSub ) )
            {
                targetSub->addSeparator();
                QAction *hdr = targetSub->addAction( "— Addon modules —" );
                hdr->setEnabled( false );
                headerAdded.insert( targetSub );
            }
            addAction( targetSub, name, label, callback );
        }
        else
        {
            // No existing submenu — find or create one from the slug
            const QString subKey = topSlug + "/" + subSlug;
            if ( !createdSubs.contains( subKey ) )
            {
                // Check whether the top menu already has a submenu with this
                // title (added in a previous loop iteration)
                const QString title = slugToTitle( subSlug );
                QMenu *existing = nullptr;
                for ( QAction *a : topMenu->actions() )
                    if ( a->menu() && a->menu()->title() == title )
                        { existing = a->menu(); break; }

                if ( existing )
                {
                    createdSubs.insert( subKey, existing );
                }
                else
                {
                    // Create the new submenu, preceded by a separator if this
                    // is the first new submenu in this top-level menu.
                    if ( !createdSubs.keys().filter( topSlug + "/" ).isEmpty()
                         || !headerAdded.contains( topMenu ) )
                    {
                        // Add separator before the very first new submenu block
                        if ( !headerAdded.contains( topMenu ) )
                        {
                            topMenu->addSeparator();
                            headerAdded.insert( topMenu );
                        }
                    }
                    QMenu *sub = topMenu->addMenu( title );
                    createdSubs.insert( subKey, sub );
                }
            }

            addAction( createdSubs[subKey], name, label, callback );
        }
    }
#endif
}

} // namespace GrassAddonMenu
