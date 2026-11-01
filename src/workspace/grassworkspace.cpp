#include "grassworkspace.h"

#include <QDomDocument>
#include <QFile>

bool GrassWorkspace::load(const QString &path)
{
    mPath = path;
    mError.clear();
    mDisplays.clear();
    mDatabase = mLocation = mMapset = {};

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        mError = "Cannot open: " + path;
        return false;
    }

    QDomDocument doc;
    auto result = doc.setContent(&f);
    f.close();
    if (!result)
    {
        mError = "XML parse error in " + path + ": " + result.errorMessage;
        return false;
    }

    const QDomElement root = doc.documentElement();  // <gxw>

    // --- <session> ---
    const QDomElement session = root.firstChildElement("session");
    if (!session.isNull())
    {
        mDatabase = session.firstChildElement("database").text().trimmed();
        mLocation = session.firstChildElement("location").text().trimmed();
        mMapset   = session.firstChildElement("mapset").text().trimmed();
    }

    // --- <display> blocks ---
    for (QDomElement disp = root.firstChildElement("display");
         !disp.isNull(); disp = disp.nextSiblingElement("display"))
    {
        GwxDisplay d;
        d.name = disp.attribute("name");

        // extent="west,south,east,north,bottom,top"
        const QString extStr = disp.attribute("extent");
        if (!extStr.isEmpty())
        {
            const QStringList parts = extStr.split(',');
            if (parts.size() >= 4)
            {
                d.west  = parts[0].toDouble();
                d.south = parts[1].toDouble();
                d.east  = parts[2].toDouble();
                d.north = parts[3].toDouble();
            }
        }

        // --- <layer> children ---
        for (QDomElement lay = disp.firstChildElement("layer");
             !lay.isNull(); lay = lay.nextSiblingElement("layer"))
        {
            GwxLayer l;
            l.type    = lay.attribute("type");
            l.name    = lay.attribute("name");
            l.checked = lay.attribute("checked", "1") != "0";
            l.opacity = lay.attribute("opacity",  "1").toDouble();

            // <task name="d.rast"> or <task name="d.vect"> …
            const QDomElement task = lay.firstChildElement("task");
            if (!task.isNull())
            {
                l.displayCmd = task.attribute("name");
                for (QDomElement param = task.firstChildElement("parameter");
                     !param.isNull(); param = param.nextSiblingElement("parameter"))
                {
                    const QString pname = param.attribute("name");
                    const QString pval  = param.firstChildElement("value").text().trimmed();
                    l.params[pname] = pval;
                    // Use the "map" parameter as the canonical layer name if present
                    if (pname == "map" && !pval.isEmpty())
                        l.name = pval;
                }
            }

            if (!l.name.isEmpty())
                d.layers.append(l);
        }

        // --- <qtgrass-decorations> (qtgrass extension, ignored by wxPython) ---
        const QDomElement decoBlock = disp.firstChildElement("qtgrass-decorations");
        if ( !decoBlock.isNull() )
        {
            for ( QDomElement dc = decoBlock.firstChildElement("deco");
                  !dc.isNull(); dc = dc.nextSiblingElement("deco") )
            {
                GwxDecoration gd;
                gd.prog    = dc.attribute("prog");
                gd.enabled = dc.attribute("enabled", "0") != "0";
                for ( QDomElement a = dc.firstChildElement("a");
                      !a.isNull(); a = a.nextSiblingElement("a") )
                    gd.args << a.text();
                if ( !gd.prog.isEmpty() )
                    d.decorations.append( gd );
            }
        }

        mDisplays.append(d);
    }

    return true;
}
