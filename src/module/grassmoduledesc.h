#pragma once

/*!
    \file grassmoduledesc.h
    \brief Plain-data structs carrying the parsed GRASS module XML interface description.

    These structs are populated by GrassXmlParser and consumed by
    GrassModuleDialog to build the parameter UI.  They contain no Qt widgets
    and no business logic.
*/

#include <QList>
#include <QPair>
#include <QString>

/*!
    \struct GrassGisPrompt
    \brief Parsed \c \<gisprompt\> element from a module's interface description.

    Describes whether a map parameter expects an existing map (input) or will
    create a new one (output), and what type of GRASS element it references.
*/
struct GrassGisPrompt
{
    QString age;     /*!< \c "old" = input map; \c "new" = output map. */
    QString element; /*!< GRASS element type: \c "cell", \c "vector", \c "file", … */
    QString prompt;  /*!< Human-readable type: \c "raster", \c "vector", \c "file", … */
};

/*!
    \struct GrassParamDesc
    \brief Parsed \c \<parameter\> element from a module's interface description.

    Carries enough metadata to construct an appropriate GrassParamWidget and
    to assemble the final command-line argument.
*/
struct GrassParamDesc
{
    QString name;          /*!< Parameter key (used in \c key=value on the command line). */
    QString type;          /*!< Data type: \c "string", \c "integer", \c "float". */
    bool    required  = false; /*!< \c true if the parameter must have a value before running. */
    bool    multiple  = false; /*!< \c true if comma-separated multiple values are accepted. */
    QString description;   /*!< Long description shown as a tooltip. */
    QString label;         /*!< Short label shown next to the input field. */
    QString guiSection;    /*!< Tab name in the module dialog (maps to \c guisection attribute). */
    QString defaultValue;  /*!< Default value string (pre-fills the widget). */

    bool           hasGisprompt = false; /*!< \c true when a \c \<gisprompt\> child was found. */
    GrassGisPrompt gisprompt;            /*!< Parsed gisprompt (valid only when hasGisprompt). */

    /*!
        List of (name, description) pairs for enumerated parameters.
        Non-empty → GrassEnumParam (QComboBox) is used.
    */
    QList<QPair<QString, QString>> values;
};

/*!
    \struct GrassFlagDesc
    \brief Parsed \c \<flag\> element from a module's interface description.
*/
struct GrassFlagDesc
{
    QString name;           /*!< Flag letter or word (without the leading \c '-'). */
    QString description;    /*!< Long description shown as a tooltip. */
    QString label;          /*!< Short label shown next to the checkbox. */
    QString guiSection;     /*!< Tab name (maps to \c guisection attribute). */
    bool    hidden = false; /*!< \c true for internal flags (\c help, \c verbose, \c quiet) that are hidden in the GUI. */
};

/*!
    \struct GrassModuleDesc
    \brief Complete parsed interface description for one GRASS module.

    Produced by GrassXmlParser::parse() from the output of
    \c \<module\> --interface-description.
*/
struct GrassModuleDesc
{
    QString name;          /*!< Module name, e.g. \c "r.slope.aspect". */
    QString description;   /*!< One-line description. */
    QString label;         /*!< Short label (may be empty). */
    QString keywords;      /*!< Comma-separated keyword list. */
    QList<GrassParamDesc> params; /*!< Ordered list of parameters. */
    QList<GrassFlagDesc>  flags;  /*!< Ordered list of flags. */

    /*!
        \brief Returns the ordered list of unique GUI section names.

        Sections appear in the order they are first encountered while
        iterating parameters then flags.
    */
    QStringList sections() const
    {
        QStringList seen;
        for ( const auto &p : params )
            if ( !p.guiSection.isEmpty() && !seen.contains( p.guiSection ) )
                seen << p.guiSection;
        for ( const auto &f : flags )
            if ( !f.guiSection.isEmpty() && !seen.contains( f.guiSection ) )
                seen << f.guiSection;
        return seen;
    }
};
