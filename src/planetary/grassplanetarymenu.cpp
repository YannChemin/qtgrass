/****************************************************************************
 * grassplanetarymenu.cpp -- auto-construct Planetary menu from module
 *                            keywords (data-driven, not hardcoded).
 *
 * Each p.* module's --interface-description XML is parsed once at
 * startup. The 2nd keyword is the submenu category; the <label> is
 * the user-facing action text. Bespoke Qt6 dialogs override the
 * generic openModule() path for specific modules.
 *
 * Public domain (Unlicense). https://unlicense.org
 ***************************************************************************/

#include "grassplanetarymenu.h"
#include "grasscraterdialog.h"
#include "grasscraterdrawdialog.h"
#include "grasscraterfreqdialog.h"
#include "grassspicekerneldialog.h"
#include "../grassenv.h"
#include "../grassinterface.h"
#include "../grassicons.h"

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMenu>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStringList>
#include <QWidget>
#include <QXmlStreamReader>

Q_LOGGING_CATEGORY(lcPlanetary, "qtgrass.planetary")

// =====================================================================
// Module discovery
// =====================================================================

/* Run a single `<fullPath> --interface-description` and parse out
 * <label> and <keywords>.  \a env must provide GISRC so the module
 * can call G_gisinit() before the parser returns the XML. */
static bool fetchInterfaceXml(const QString &fullPath,
                                const QProcessEnvironment &env,
                                GrassPlanetaryModuleInfo *out)
{
    QProcess p;
    p.setProcessEnvironment( env );
    p.setProcessChannelMode( QProcess::SeparateChannels );
    p.start( fullPath, QStringList() << "--interface-description" );
    if ( !p.waitForFinished( 8000 ) ) {
        p.kill();
        qCDebug(lcPlanetary) << "interface-description timed out for" << fullPath;
        return false;
    }
    if ( p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0 ) {
        qCDebug(lcPlanetary) << "interface-description failed for" << fullPath
                              << "(exit" << p.exitCode() << "):"
                              << QString::fromLocal8Bit( p.readAllStandardError() )
                                    .trimmed().left(120);
        return false;
    }
    QByteArray xml = p.readAllStandardOutput();

    QXmlStreamReader r( xml );
    QString descriptionFallback;
    while ( !r.atEnd() ) {
        if ( r.readNextStartElement() ) {
            if ( r.name() == QLatin1String("label") ) {
                out->label = r.readElementText().trimmed();
            }
            else if ( r.name() == QLatin1String("description") && descriptionFallback.isEmpty() ) {
                /* Python modules (g.parser) emit <description> instead of <label>.
                 * Capture the first (task-level) description as a fallback. */
                descriptionFallback = r.readElementText().trimmed();
            }
            else if ( r.name() == QLatin1String("keywords") ) {
                /* GRASS emits keywords as one comma-separated list. */
                QString txt = r.readElementText().trimmed();
                out->keywords.clear();
                for ( const QString &k : txt.split(',', Qt::SkipEmptyParts) )
                    out->keywords << k.trimmed();
            }
            else if ( r.name() != QLatin1String("task") ) {
                /* Skip past nested elements once we have keywords and a label. */
                if ( !out->keywords.isEmpty() && !out->label.isEmpty() )
                    return true;
            }
        }
    }
    /* Use the task-level <description> when no <label> was emitted (Python modules). */
    if ( out->label.isEmpty() && !descriptionFallback.isEmpty() )
        out->label = descriptionFallback;
    return !out->keywords.isEmpty();
}

QMap<QString, GrassPlanetaryModuleInfo>
GrassPlanetaryMenu::scanAvailableModules( const QProcessEnvironment &env )
{
    QMap<QString, GrassPlanetaryModuleInfo> result;

    /* 1. Build the search-path list. */
    QStringList searchDirs;
    const QString gisBase = env.value( "GISBASE",
                                QString::fromLocal8Bit( qgetenv("GISBASE") ) );
    if ( !gisBase.isEmpty() )
        searchDirs << gisBase + "/bin" << gisBase + "/scripts";
    const QString addonPath = env.value( "GRASS_ADDON_PATH",
                                  QString::fromLocal8Bit( qgetenv("GRASS_ADDON_PATH") ) );
    if ( !addonPath.isEmpty() ) {
        for ( const QString &p : addonPath.split(':', Qt::SkipEmptyParts) )
            searchDirs << p + "/bin" << p + "/scripts" << p;
    }
    const QString home = QDir::homePath();
    for ( int v = 6; v <= 9; v++ )
        searchDirs << home + QString("/.grass%1/addons/bin").arg(v);
    searchDirs << "/usr/lib/grass/addons/bin";

    /* 2. Discover p.* binaries — store full paths, deduplicate by basename. */
    QMap<QString, QString> nameToPath;  // basename → full path (first found wins)
    for ( const QString &dir : std::as_const(searchDirs) ) {
        QDir d( dir );
        if ( !d.exists() ) continue;
        const QStringList entries = d.entryList(
            QStringList() << "p.*", QDir::Files | QDir::Executable );
        for ( QString e : entries ) {
#ifdef Q_OS_WIN
            if ( e.endsWith(".exe", Qt::CaseInsensitive) ) e.chop(4);
            if ( e.endsWith(".bat", Qt::CaseInsensitive) ) e.chop(4);
#endif
            if ( e.startsWith("p.") && !nameToPath.contains(e) )
                nameToPath.insert( e, d.filePath(e) );
        }
    }

    /* 3. For each module, run --interface-description via its full path. */
    for ( auto it = nameToPath.constBegin(); it != nameToPath.constEnd(); ++it ) {
        GrassPlanetaryModuleInfo info;
        if ( fetchInterfaceXml( it.value(), env, &info ) ) {
            result.insert( it.key(), info );
        } else {
            /* Stub: keyword filter in populate() requires kw[0]=="Planetary". */
            info.label    = QString();
            info.keywords = QStringList{ "Planetary", "Utilities" };
            result.insert( it.key(), info );
        }
    }
    qCDebug(lcPlanetary) << "Planetary scan:" << result.size()
                          << "modules in" << searchDirs;
    return result;
}

QSet<QString> GrassPlanetaryMenu::availableNames(
    const QMap<QString, GrassPlanetaryModuleInfo> &modules)
{
    QSet<QString> s;
    for ( auto it = modules.constBegin(); it != modules.constEnd(); ++it )
        s.insert( it.key() );
    return s;
}

// =====================================================================
// Menu population
// =====================================================================

/* Preferred submenu order — categories listed here appear first in the menu.
 * Any category found in module keywords but not listed here is appended
 * alphabetically.  Add new entries here only when a stable canonical order
 * matters; otherwise the dynamic alphabetical tail suffices. */
static const QStringList kPreferredFirst = {
    "Import & Export"
};

/* Bespoke-dialog override registry. When a module name matches one of
 * these keys, the generic openModule() is bypassed in favour of the
 * factory closure. */
using BespokeFactory = std::function<void(GrassInterface *, QWidget *)>;
static QMap<QString, BespokeFactory> bespokeFactories(
    GrassInterface * /*iface*/, QWidget * /*parent*/)
{
    QMap<QString, BespokeFactory> m;
    m["p.crater"] = [](GrassInterface *i, QWidget *p) {
        auto *d = new GrassCraterDialog(i, p);
        d->setAttribute(Qt::WA_DeleteOnClose); d->show();
    };
    m["p.crater.draw"] = [](GrassInterface *i, QWidget *p) {
        auto *d = new GrassCraterDrawDialog(i, p);
        d->setAttribute(Qt::WA_DeleteOnClose); d->show();
    };
    m["p.crater.freq"] = [](GrassInterface *i, QWidget *p) {
        auto *d = new GrassCraterFreqDialog(i, p);
        d->setAttribute(Qt::WA_DeleteOnClose); d->show();
    };
    return m;
}

void GrassPlanetaryMenu::populate(
    QMenu *menu,
    const ModuleCallback &openModule,
    const QMap<QString, GrassPlanetaryModuleInfo> &modules,
    GrassInterface *iface,
    QWidget *parent )
{
    qCDebug(lcPlanetary) << "[populate] entry menu=" << (void*)menu
                         << "iface=" << (void*)iface << "parent=" << (void*)parent;
    /* Group modules by the 2nd keyword (category).
     * Only include modules whose 1st keyword is "Planetary" — this enforces
     * the keyword convention and excludes any p.* module that belongs to a
     * different menu. */
    QMap<QString, QStringList> byCategory;
    for ( auto it = modules.constBegin(); it != modules.constEnd(); ++it ) {
        const QStringList &kw = it.value().keywords;
        if ( kw.isEmpty() || kw[0].compare("Planetary", Qt::CaseInsensitive) != 0 )
            continue;
        QString cat = (kw.size() >= 2) ? kw[1] : QString("Utilities");
        byCategory[cat] << it.key();
    }
    qCDebug(lcPlanetary) << "[populate] byCategory=" << byCategory.keys();

    /* Build the final ordered category list: preferred categories first (in
     * kPreferredFirst order), then all remaining categories alphabetically.
     * The category names come entirely from module keywords — nothing is
     * hardcoded beyond the two pinned entries. */
    QStringList categories;
    for ( const QString &pref : kPreferredFirst )
        if ( byCategory.contains(pref) ) categories << pref;
    QStringList rest = byCategory.keys();
    std::sort( rest.begin(), rest.end() );
    for ( const QString &cat : rest )
        if ( !categories.contains(cat) ) categories << cat;
    qCDebug(lcPlanetary) << "[populate] categories=" << categories;

    qCDebug(lcPlanetary) << "[populate] calling bespokeFactories";
    auto factories = bespokeFactories(iface, parent);
    qCDebug(lcPlanetary) << "[populate] bespokeFactories done, count=" << factories.size();

    for ( const QString &cat : categories ) {
        if ( !byCategory.contains(cat) ) continue;
        QStringList modList = byCategory[cat];
        std::sort( modList.begin(), modList.end() );

        qCDebug(lcPlanetary) << "[populate] adding submenu" << cat << "with" << modList.size() << "items";
        QMenu *sub = menu->addMenu( cat );
        for ( const QString &name : modList ) {
            qCDebug(lcPlanetary) << "[populate]   item:" << name;
            const GrassPlanetaryModuleInfo &info = modules[name];
            QString label = info.label.isEmpty()
                              ? name
                              : info.label + QString("  [%1]").arg(name);

            QAction *act = sub->addAction( label );
            act->setToolTip( name );

            if ( factories.contains(name) ) {
                BespokeFactory fac = factories[name];
                QObject::connect( act, &QAction::triggered, parent,
                                  [fac, iface, parent] { fac(iface, parent); } );
            } else {
                QObject::connect( act, &QAction::triggered, parent,
                                  [openModule, name] { openModule(name); } );
            }
        }

        /* Special-case items injected per category. Currently only one:
         * the SPICE Kernel Manager belongs to Import & Export.        */
        if ( cat == QLatin1String("Import & Export") ) {
            sub->addSeparator();
            sub->addAction( "&SPICE Kernel Manager...", parent,
                              [iface, parent] {
                auto *d = new GrassSpiceKernelDialog(iface, parent);
                d->setAttribute(Qt::WA_DeleteOnClose);
                d->show();
            });
        }
    }
}
