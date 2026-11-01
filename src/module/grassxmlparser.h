#pragma once

#include "grassmoduledesc.h"
#include <QString>
#include <QByteArray>

// Runs "<module> --interface-description" and parses the resulting XML.
// All methods are static — no state.
class GrassXmlParser
{
  public:
    // Locate the module binary in GISBASE/bin or GISBASE/scripts.
    // Returns empty string if not found.
    static QString findModuleBin( const QString &moduleName );

    // Run the module with --interface-description and return raw XML.
    // Returns empty QByteArray on failure; sets *errorOut if provided.
    static QByteArray fetchXml( const QString &moduleName,
                                QString *errorOut = nullptr );

    // Parse raw XML bytes into a GrassModuleDesc.
    // Returns a desc with empty name on parse failure.
    static GrassModuleDesc parse( const QByteArray &xml );

    // Convenience: fetch + parse in one call.
    static GrassModuleDesc load( const QString &moduleName,
                                 QString *errorOut = nullptr );

  private:
    // Flags whose names we suppress in the GUI (handled globally)
    static bool isInternalFlag( const QString &name );
};
