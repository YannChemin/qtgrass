#pragma once

/*!
    \file grassdisplaycmd.h
    \brief Display command descriptor for the GRASS display driver pipeline.
*/

#include <QColor>
#include <QString>
#include <QStringList>

/*!
    \struct GrassDisplayCmd
    \brief Descriptor for one \c d.* GRASS display module invocation.

    GrassDisplayDriver accepts an ordered list of GrassDisplayCmd objects and
    runs them sequentially to composite layers and decorations into a single
    map image.

    Convenience factory functions (raster(), vector(), legend(), barscale(),
    northarrow(), grid()) cover the most common use cases.

    \sa GrassDisplayDriver, GrassMapCanvas::buildDisplayCmds()
*/
struct GrassDisplayCmd
{
    QString     prog;        /*!< Bare module name: \c "d.rast", \c "d.vect", \c "d.legend", … */
    QStringList args;        /*!< Module arguments: \c "map=elevation@PERMANENT", \c "color=red", … */
    bool        enabled   = true;   /*!< \c false causes the command to be skipped. */
    bool        isOverlay = false;  /*!< \c false = map layer; \c true = decoration overlay. */

    /*!
        \brief Create a raster display command.
        \param mapRef Map reference in \c name\@mapset form.
    */
    static GrassDisplayCmd raster( const QString &mapRef )
    { return { "d.rast", { "map=" + mapRef } }; }

    /*!
        \brief Create a vector display command.
        \param mapRef    Map reference in \c name\@mapset form.
        \param color     Line / border colour (default blue).
        \param fillColor Fill colour for polygon interiors (default translucent blue).
        \param width     Line width in pixels.
    */
    static GrassDisplayCmd vector( const QString &mapRef,
                                    const QColor &color     = QColor(50,80,200),
                                    const QColor &fillColor = QColor(50,80,200,80),
                                    int width = 1 )
    {
        return { "d.vect", {
            "map=" + mapRef,
            "color="      + QString("%1:%2:%3").arg(color.red()).arg(color.green()).arg(color.blue()),
            "fill_color=" + QString("%1:%2:%3").arg(fillColor.red()).arg(fillColor.green()).arg(fillColor.blue()),
            "width="      + QString::number(width)
        } };
    }

    /*!
        \brief Create a legend overlay command.
        \param rasterRef Map reference for the colour ramp legend.
        \param at        Legend placement as \c "top%,bottom%,left%,right%" percentages.
    */
    static GrassDisplayCmd legend( const QString &rasterRef,
                                    const QString &at = "5,50,2,9" )
    { return { "d.legend", { "raster=" + rasterRef, "at=" + at }, true, true }; }

    /*!
        \brief Create a scale bar overlay command.
        \param at    Placement as \c "bottom%,left%" percentages from the lower-left corner.
        \param style Bar style: \c "simple", \c "line", \c "checker", \c "tick", \c "compact".
    */
    static GrassDisplayCmd barscale( const QString &at     = "80,2",
                                      const QString &style  = "simple" )
    { return { "d.barscale", { "at=" + at, "style=" + style }, true, true }; }

    /*!
        \brief Create a north arrow overlay command.
        \param at Placement as \c "bottom%,left%" percentages from the lower-left corner.
    */
    static GrassDisplayCmd northarrow( const QString &at = "90,5" )
    { return { "d.northarrow", { "at=" + at }, true, true }; }

    /*!
        \brief Create a grid overlay command.
        \param size   Grid cell size in map units.
        \param color  Grid line colour name or \c R:G:B triplet.
        \param labels \c true to draw coordinate labels at the grid lines.
    */
    static GrassDisplayCmd grid( const QString &size   = "auto",
                                  const QString &color  = "grey",
                                  bool labels = true )
    {
        QStringList a = { "size=" + size, "color=" + color };
        if ( !labels ) a << "-n";
        return { "d.grid", a, true, true };
    }

    /*!
        \brief Create a text overlay command (d.text).
        \param txt      Text string to display.
        \param at       Position as "x%,y%" from lower-left (default: centred near top).
        \param align    Alignment code: ll/lc/lr/cl/cc/cr/ul/uc/ur.
        \param size     Text height as percentage of frame height.
        \param fgcolor  Text colour (GRASS name or R:G:B).
        \param bgcolor  Background colour ("none" for transparent).
        \param font     Font name (empty = default).
        \param bold     \c true for bold text.
        \param rotation Rotation angle in degrees counter-clockwise.
    */
    static GrassDisplayCmd text( const QString &txt      = "Map Title",
                                  const QString &at       = "50,97",
                                  const QString &align    = "uc",
                                  double         size     = 5.0,
                                  const QString &fgcolor  = "black",
                                  const QString &bgcolor  = "none",
                                  const QString &font     = "",
                                  bool           bold     = false,
                                  double         rotation = 0.0 )
    {
        QStringList a = {
            "text="    + txt,
            "at="      + at,
            "align="   + align,
            "size="    + QString::number( size, 'g', 4 ),
            "color="   + fgcolor,
            "bgcolor=" + bgcolor,
        };
        if ( !font.isEmpty() )  a << "font=" + font;
        if ( bold )             a << "-b";
        if ( rotation != 0.0 )  a << "rotation=" + QString::number( rotation );
        return { "d.text", a, true, true };
    }
};
