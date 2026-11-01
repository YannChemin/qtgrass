#pragma once

/*!
    \file grassaddlayerdialog.h
    \brief Unified dialog for adding any composite or simple raster/vector layer.
*/

#include "grasscompositelayers.h"

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class GrassMapCanvas;
class GrassMapLayer;

/*!
    \class GrassAddLayerDialog
    \brief Dialog for choosing and configuring a composite display layer.

    Presents a type selector (RGB, IHS, Shade, Cell Values, Flow Arrows) at the
    top, and a QStackedWidget that shows the relevant configuration controls for
    each type.

    All raster selectors are pre-populated from \c g.list so the user can pick
    from existing maps rather than typing names.

    Calling createLayer() builds and returns the configured GrassMapLayer object.
    The caller owns the returned pointer.

    Usage in "edit" mode: pass an existing layer pointer to the static \c edit()
    factory to pre-fill all controls from the layer's current Config.

    \sa GrassRgbLayer, GrassIhsLayer, GrassShadeLayer,
        GrassRastNumLayer, GrassRastArrowLayer
*/
class GrassAddLayerDialog : public QDialog
{
    Q_OBJECT

  public:
    enum class LayerType {
        Rgb, Ihs, Shade, RastNum, RastArrow,
        VectThematic, VectChart, Label, Geodesic, Rhumbline
    };

    /*!
        \brief Constructs the dialog in "add" mode, optionally pre-selecting a type.
        \param canvas   Map canvas (used to get the current mapset context).
        \param type     Pre-selected layer type.
        \param parent   Parent widget.
    */
    explicit GrassAddLayerDialog( GrassMapCanvas *canvas,
                                   LayerType type  = LayerType::Rgb,
                                   QWidget *parent = nullptr );

    /*!
        \brief Build and return the configured layer on dialog acceptance.
        \return Heap-allocated layer owned by the caller; \c nullptr on cancel.
    */
    GrassMapLayer *createLayer() const;

    /*!
        \brief Returns the user-visible display name entered for the layer.
    */
    QString layerDisplayName() const;

  private slots:
    void onTypeChanged( int idx );
    void pickColor( QPushButton *btn, QColor &target );

  private:
    void buildRgbPage      ( QWidget *page );
    void buildIhsPage      ( QWidget *page );
    void buildShadePage    ( QWidget *page );
    void buildNumPage      ( QWidget *page );
    void buildArrowPage    ( QWidget *page );
    void buildThematicPage ( QWidget *page );
    void buildChartPage    ( QWidget *page );
    void buildLabelPage    ( QWidget *page );
    void buildGeodesicPage ( QWidget *page );
    void buildRhumbPage    ( QWidget *page );

    void populateRasterCombo( QComboBox *cb ) const;
    void populateVectorCombo( QComboBox *cb ) const;

    static QString splitMapRef( const QString &ref, QString &mapset );

    GrassMapCanvas *mCanvas = nullptr;

    QComboBox      *mTypeCombo   = nullptr;
    QStackedWidget *mStack       = nullptr;

    // RGB page controls
    QComboBox *mRgbR = nullptr, *mRgbG = nullptr, *mRgbB = nullptr;

    // IHS page controls
    QComboBox *mIhsI = nullptr, *mIhsH = nullptr, *mIhsS = nullptr;

    // Shade page controls
    QComboBox *mShColor = nullptr, *mShShade = nullptr;
    QSpinBox  *mShBright = nullptr;

    // RastNum page controls
    QComboBox *mNumMap     = nullptr;
    QSpinBox  *mNumDec     = nullptr;
    QSpinBox  *mNumFont    = nullptr;
    QSpinBox  *mNumMinCell = nullptr;
    QCheckBox *mNumShowBg  = nullptr;
    QCheckBox *mNumShowNull= nullptr;
    QPushButton *mNumTextColor = nullptr;
    QPushButton *mNumBgColor   = nullptr;
    QColor mNumTextClr  = Qt::black;
    QColor mNumBgClr    = QColor( 255, 255, 255, 160 );

    // RastArrow page controls
    QComboBox *mArrDir   = nullptr;
    QComboBox *mArrMag   = nullptr;
    QSpinBox  *mArrSize  = nullptr;
    QSpinBox  *mArrMin   = nullptr;
    QComboBox *mArrStyle = nullptr;
    QCheckBox *mArrGrass = nullptr;
    QPushButton *mArrColor = nullptr;
    QColor mArrClr = QColor( 220, 40, 40 );

    // Thematic page controls
    QComboBox *mThMap    = nullptr;
    QLineEdit *mThCol    = nullptr;
    QSpinBox  *mThN      = nullptr;
    QComboBox *mThMethod = nullptr;
    QComboBox *mThRamp   = nullptr;
    QCheckBox *mThRev    = nullptr;

    // Chart page controls
    QComboBox  *mChMap    = nullptr;
    QLineEdit  *mChCols   = nullptr;
    QComboBox  *mChType   = nullptr;
    QSpinBox   *mChSize   = nullptr;
    QCheckBox  *mChScale  = nullptr;

    // Label page controls
    QComboBox *mLbMap    = nullptr;
    QLineEdit *mLbCol    = nullptr;
    QSpinBox  *mLbFont   = nullptr;
    QSpinBox  *mLbHalo   = nullptr;
    QCheckBox *mLbBold   = nullptr;
    QCheckBox *mLbItalic = nullptr;
    QPushButton *mLbTextColor = nullptr;
    QColor mLbTextClr = Qt::black;

    // Geodesic / Rhumbline page controls (shared layout)
    QDoubleSpinBox *mGeoLon1 = nullptr, *mGeoLat1 = nullptr;
    QDoubleSpinBox *mGeoLon2 = nullptr, *mGeoLat2 = nullptr;
    QPushButton *mGeoColor = nullptr;
    QColor mGeoClr = QColor( 220, 80, 20 );

    QDoubleSpinBox *mRhmLon1 = nullptr, *mRhmLat1 = nullptr;
    QDoubleSpinBox *mRhmLon2 = nullptr, *mRhmLat2 = nullptr;
    QPushButton *mRhmColor = nullptr;
    QColor mRhmClr = QColor( 20, 120, 220 );
};
