#include "grassxmlparser.h"
#include "../grassenv.h"

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcModule, "qtgrass.module")

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static QString elemText( const QDomElement &parent, const QString &tag )
{
    return parent.firstChildElement( tag ).text().trimmed();
}

bool GrassXmlParser::isInternalFlag( const QString &name )
{
    // These are shown as a global "Verbose/Quiet" option, not per-param widgets
    static const QStringList hidden = { "help", "verbose", "quiet", "overwrite", "qq" };
    return hidden.contains( name );
}

// ---------------------------------------------------------------------------
// findModuleBin
// ---------------------------------------------------------------------------
QString GrassXmlParser::findModuleBin( const QString &moduleName )
{
    const QString gisBase = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
    if ( gisBase.isEmpty() )
        return {};

    // 1. Core module locations inside GISBASE
    for ( const QString &sub : { QString("/bin/"), QString("/scripts/") } ) {
        const QString c = gisBase + sub + moduleName;
        if ( QFile::exists( c ) ) return c;
    }

    // 2. Addon paths — mirror the same search order as scanAvailableModules()
    QStringList addonDirs;
    const QString addonPath = QString::fromLocal8Bit( qgetenv( "GRASS_ADDON_PATH" ) );
    if ( !addonPath.isEmpty() ) {
        for ( const QString &p : addonPath.split( ':', Qt::SkipEmptyParts ) )
            addonDirs << p + "/bin" << p + "/scripts" << p;
    }
    const QString home = QDir::homePath();
    for ( int v = 6; v <= 9; v++ )
        addonDirs << home + QString( "/.grass%1/addons/bin" ).arg( v );
    addonDirs << "/usr/lib/grass/addons/bin";

    for ( const QString &dir : std::as_const( addonDirs ) ) {
        const QString c = dir + "/" + moduleName;
        if ( QFile::exists( c ) ) return c;
    }

    return {};
}

// ---------------------------------------------------------------------------
// fetchXml
// ---------------------------------------------------------------------------
QByteArray GrassXmlParser::fetchXml( const QString &moduleName,
                                      QString *errorOut )
{
    const QString bin = findModuleBin( moduleName );
    if ( bin.isEmpty() )
    {
        if ( errorOut )
            *errorOut = QString( "Module '%1' not found in GISBASE/bin or GISBASE/scripts." )
                        .arg( moduleName );
        return {};
    }

    // Build environment with GISRC file so the subprocess can call G_gisinit().
    // qtgrass uses G_GISRC_MODE_MEMORY; child processes need a real GISRC file.
    QProcessEnvironment env = GrassEnv::instance()->buildSubprocessEnv();

    QProcess proc;
    proc.setProcessEnvironment( env );
    proc.start( bin, { "--interface-description" } );

    if ( !proc.waitForStarted( 5000 ) )
    {
        if ( errorOut )
            *errorOut = "Failed to start: " + proc.errorString();
        return {};
    }
    if ( !proc.waitForFinished( 30000 ) )
    {
        proc.kill();
        if ( errorOut )
            *errorOut = "Timed out waiting for --interface-description";
        return {};
    }

    const QByteArray out = proc.readAllStandardOutput();
    if ( out.isEmpty() )
    {
        if ( errorOut )
            *errorOut = "No output from --interface-description: "
                        + proc.readAllStandardError();
    }
    return out;
}

// ---------------------------------------------------------------------------
// parse
// ---------------------------------------------------------------------------
GrassModuleDesc GrassXmlParser::parse( const QByteArray &xml )
{
    GrassModuleDesc desc;
    if ( xml.isEmpty() )
        return desc;

    QDomDocument doc;
    auto parseResult = doc.setContent( xml );
    if ( !parseResult )
    {
        qCWarning(lcModule) << "GrassXmlParser: XML parse error:" << parseResult.errorMessage;
        return desc;
    }

    const QDomElement task = doc.documentElement(); // <task name="...">
    if ( task.isNull() )
        return desc;

    desc.name        = task.attribute( "name" );
    desc.label       = elemText( task, "label" );
    desc.description = elemText( task, "description" );
    desc.keywords    = elemText( task, "keywords" );

    // --- Parameters ---
    QDomNodeList paramNodes = task.elementsByTagName( "parameter" );
    for ( int i = 0; i < paramNodes.count(); ++i )
    {
        QDomElement pe = paramNodes.at( i ).toElement();
        GrassParamDesc p;
        p.name     = pe.attribute( "name" );
        p.type     = pe.attribute( "type", "string" );
        p.required = ( pe.attribute( "required" ) == "yes" );
        p.multiple = ( pe.attribute( "multiple" ) == "yes" );

        p.label       = elemText( pe, "label" );
        p.description = elemText( pe, "description" );
        p.guiSection  = elemText( pe, "guisection" );
        p.defaultValue= elemText( pe, "default" );

        // gisprompt
        QDomElement gp = pe.firstChildElement( "gisprompt" );
        if ( !gp.isNull() )
        {
            p.hasGisprompt    = true;
            p.gisprompt.age     = gp.attribute( "age" );
            p.gisprompt.element = gp.attribute( "element" );
            p.gisprompt.prompt  = gp.attribute( "prompt" );
        }

        // enum values
        QDomElement valuesElem = pe.firstChildElement( "values" );
        if ( !valuesElem.isNull() )
        {
            QDomNodeList vals = valuesElem.elementsByTagName( "value" );
            for ( int j = 0; j < vals.count(); ++j )
            {
                QDomElement ve = vals.at( j ).toElement();
                const QString vname = elemText( ve, "name" );
                const QString vdesc = elemText( ve, "description" );
                p.values.append( { vname, vdesc } );
            }
        }

        desc.params.append( p );
    }

    // --- Flags ---
    QDomNodeList flagNodes = task.elementsByTagName( "flag" );
    for ( int i = 0; i < flagNodes.count(); ++i )
    {
        QDomElement fe = flagNodes.at( i ).toElement();
        GrassFlagDesc f;
        f.name        = fe.attribute( "name" );
        f.label       = elemText( fe, "label" );
        f.description = elemText( fe, "description" );
        f.guiSection  = elemText( fe, "guisection" );
        f.hidden      = isInternalFlag( f.name );
        desc.flags.append( f );
    }

    return desc;
}

// ---------------------------------------------------------------------------
// load
// ---------------------------------------------------------------------------
GrassModuleDesc GrassXmlParser::load( const QString &moduleName,
                                       QString *errorOut )
{
    QString err;
    const QByteArray xml = fetchXml( moduleName, &err );
    if ( xml.isEmpty() )
    {
        if ( errorOut ) *errorOut = err;
        return {};
    }
    GrassModuleDesc desc = parse( xml );
    if ( desc.name.isEmpty() )
    {
        if ( errorOut ) *errorOut = "XML parsed but task element was empty";
    }
    return desc;
}
