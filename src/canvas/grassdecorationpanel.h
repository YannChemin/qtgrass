#pragma once

/*!
    \file grassdecorationpanel.h
    \brief Panel widget for enabling and configuring map decorations.
*/

#include "grassdisplaycmd.h"

#include <QList>
#include <QWidget>

class GrassMapCanvas;
class QCheckBox;
class QLineEdit;
class QComboBox;
class QDoubleSpinBox;

/*!
    \class GrassDecorationPanel
    \brief Panel for map decorations rendered via GRASS \c d.* display modules.

    Lives at the bottom of the layer panel.  Each decoration (legend, scale bar,
    north arrow, grid, text) has an enable/disable checkbox and a "…" settings
    button that opens a small configuration dialog.

    Connect decorationsChanged() to GrassMapCanvas::setDecorations() so the
    canvas re-renders whenever a setting changes.

    \sa GrassDisplayCmd, GrassMapCanvas::setDecorations()
*/
class GrassDecorationPanel : public QWidget
{
    Q_OBJECT

  public:
    /*!
        \brief Constructs the decoration panel with all decorations disabled.
        \param canvas Canvas pointer — used by the Grid dialog to read the
                      current viewport extent for auto-spacing computation.
    */
    explicit GrassDecorationPanel( GrassMapCanvas *canvas,
                                    QWidget *parent = nullptr );

    /*!
        \brief Returns the list of currently enabled decoration commands.

        The caller should pass this list to GrassMapCanvas::setDecorations()
        to trigger a re-render.
    */
    QList<GrassDisplayCmd> activeDecorations() const;

    /*!
        \brief Set the raster map reference used by the legend decoration.

        Call when the topmost visible raster layer changes so the legend always
        displays the current map's colour table.

        \param mapRef Map reference in \c name\@mapset form.
    */
    void setLegendRaster( const QString &mapRef );

    /*! \brief Snapshot of one decoration's state for workspace save/restore. */
    struct DecoState {
        QString     prog;
        QStringList args;
        bool        enabled = false;
    };

    /*!
        \brief Returns the state of all decorations (enabled or not) for serialisation.
        Order: legend, barscale, northarrow, grid, text.
    */
    QList<DecoState> savedState() const;

    /*!
        \brief Restores decoration state from a previously saved snapshot.
        Matches entries by \c prog name; unknown prog names are ignored.
        Emits decorationsChanged() once after all states are applied.
    */
    void restoreState( const QList<DecoState> &states );

  signals:
    /*! \brief Emitted whenever any decoration is toggled or reconfigured. */
    void decorationsChanged();

  private slots:
    void editLegend();
    void editScalebar();
    void editNorthArrow();
    void editGrid();
    void editText();

  private:
    GrassMapCanvas *mCanvas    = nullptr;

    QCheckBox *mLegendCheck   = nullptr;
    QCheckBox *mBarscaleCheck = nullptr;
    QCheckBox *mArrowCheck    = nullptr;
    QCheckBox *mGridCheck     = nullptr;
    QCheckBox *mTextCheck     = nullptr;

    GrassDisplayCmd mLegendCmd;
    GrassDisplayCmd mBarscaleCmd;
    GrassDisplayCmd mArrowCmd;
    GrassDisplayCmd mGridCmd;
    GrassDisplayCmd mTextCmd;
};
