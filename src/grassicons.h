#pragma once

#include <QIcon>
#include <QPixmap>
#include <QString>

// Load a GRASS icon by base name (without .png) from $GISBASE/gui/icons/grass/.
// Falls back to a null QIcon if GISBASE is not set or the file is missing.
inline QIcon grassIcon( const QString &name )
{
    const QByteArray gb = qgetenv( "GISBASE" );
    if ( !gb.isEmpty() )
    {
        const QString path = QString::fromLocal8Bit( gb )
                             + "/gui/icons/grass/" + name + ".png";
        QIcon icon( path );
        if ( !icon.isNull() ) return icon;
    }
    return {};
}

// Toolbar-sized icon (24×24).  Scales the GRASS PNG if needed.
inline QIcon grassToolIcon( const QString &name, int size = 24 )
{
    QIcon icon = grassIcon( name );
    if ( icon.isNull() ) return {};
    QPixmap pm = icon.pixmap( size, size );
    return QIcon( pm );
}
