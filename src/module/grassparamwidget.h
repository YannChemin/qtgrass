#pragma once

/*!
    \file grassparamwidget.h
    \brief Abstract parameter widget and concrete subclasses for GRASS module dialogs.
*/

#include "grassmoduledesc.h"

#include <QStringList>
#include <QWidget>

// ── GrassParamWidget ─────────────────────────────────────────────────────────

/*!
    \class GrassParamWidget
    \brief Abstract base for all parameter input widgets in GrassModuleDialog.

    Each concrete subclass handles one GRASS parameter \c type (string, integer,
    float, file, enum).  The factory function create() selects the appropriate
    concrete class from the GrassParamDesc descriptor.

    \sa GrassLineEditParam, GrassFileParam, GrassEnumParam,
        GrassSpinParam, GrassDoubleSpinParam, GrassModuleDialog
*/
class GrassParamWidget : public QWidget
{
    Q_OBJECT

  public:
    /*!
        \brief Constructs the widget for a single parameter descriptor.
        \param desc   Parameter metadata (name, type, required, values list, …).
        \param parent Parent widget.
    */
    explicit GrassParamWidget( const GrassParamDesc &desc,
                               QWidget *parent = nullptr );

    /*! \brief The parameter descriptor this widget was built from. */
    const GrassParamDesc &desc() const { return mDesc; }

    /*! \brief Parameter key name (used when assembling the command line). */
    QString key()      const { return mDesc.name; }

    /*! \brief \c true if the parameter must have a value before the module can run. */
    bool    required() const { return mDesc.required; }

    /*!
        \brief Current user-supplied value(s).

        Returns an empty list when the field has not been touched and the
        parameter has a default, or when the user explicitly left it blank.
    */
    virtual QStringList values() const = 0;

    /*!
        \brief Pre-fill the widget from a command-line value string.

        For multi-value parameters, \a val is comma-separated.
        The default implementation is a no-op.

        \param val Value string to parse and display.
    */
    virtual void setValue( const QString &val ) { Q_UNUSED( val ) }

    /*!
        \brief Contribute zero or more \c key=value strings to the command line.

        For flag widgets this produces \c "-name" instead.  The default
        implementation returns an empty list when values() is empty, or
        \c "key=v1,v2,…" otherwise.
    */
    virtual QStringList commandArgs() const;

    /*!
        \brief Returns \c true when the widget holds a valid, acceptable value.

        For optional parameters this is always \c true.  For required ones it
        checks that the field is non-empty.  Subclasses override as needed.
    */
    virtual bool isValid() const;

    /*!
        \brief Highlight the widget border to signal a validation failure.
        \param invalid \c true = red border; \c false = normal.
    */
    void markInvalid( bool invalid );

    /*!
        \brief Factory: instantiate the right concrete widget for \a desc.
        \param desc   Parameter descriptor from the module XML.
        \param parent Parent widget.
        \return Newly allocated widget; caller / Qt parent takes ownership.
    */
    static GrassParamWidget *create( const GrassParamDesc &desc,
                                     QWidget *parent = nullptr );

  signals:
    /*! \brief Emitted whenever the user changes the parameter value. */
    void valueChanged();

  protected:
    GrassParamDesc mDesc;
};

// ── GrassFlagWidget ──────────────────────────────────────────────────────────

class QCheckBox;

/*!
    \class GrassFlagWidget
    \brief Checkbox for a boolean GRASS module flag.

    Contributes \c "-name" to the command line when checked.

    \sa GrassParamWidget
*/
class GrassFlagWidget : public QWidget
{
    Q_OBJECT

  public:
    /*!
        \brief Constructs a labelled checkbox from a flag descriptor.
        \param desc   Flag metadata (name, description).
        \param parent Parent widget.
    */
    explicit GrassFlagWidget( const GrassFlagDesc &desc,
                              QWidget *parent = nullptr );

    /*! \brief Flag key name (without the leading \c '-'). */
    QString flag() const { return mDesc.name; }

    /*! \brief \c true when the checkbox is checked. */
    bool    checked() const;

    /*!
        \brief Returns \c "-name" if checked, or an empty string otherwise.
    */
    QString commandArg() const;

  signals:
    /*! \brief Emitted when the checkbox state changes. */
    void valueChanged();

  private:
    GrassFlagDesc mDesc;
    QCheckBox    *mCheck = nullptr;
};

// ── Concrete parameter widgets ───────────────────────────────────────────────

class QLineEdit;

/*!
    \class GrassLineEditParam
    \brief Plain text / GRASS map name parameter widget (QLineEdit).

    Used for string parameters and any map-name parameter without an enum.
*/
class GrassLineEditParam : public GrassParamWidget
{
    Q_OBJECT
  public:
    explicit GrassLineEditParam( const GrassParamDesc &desc,
                                 QWidget *parent = nullptr );
    QStringList values() const override;
    bool        isValid() const override;
    void        setValue( const QString &val ) override;

  private:
    QLineEdit *mEdit = nullptr;
};

/*!
    \class GrassFileParam
    \brief File or directory picker parameter widget.

    Shows a QLineEdit and a "Browse…" button.  The button opens a
    QFileDialog configured for save or open depending on whether the
    parameter \c age is \c "new" (output) or \c "existing" (input).
*/
class GrassFileParam : public GrassParamWidget
{
    Q_OBJECT
  public:
    explicit GrassFileParam( const GrassParamDesc &desc,
                             QWidget *parent = nullptr );
    QStringList values() const override;
    bool        isValid() const override;
    void        setValue( const QString &val ) override;

  private slots:
    void browse();

  private:
    QLineEdit *mEdit = nullptr;
    bool       mSave = false;
};

class QComboBox;

/*!
    \class GrassEnumParam
    \brief Enumerated string parameter widget (QComboBox).

    Populated from the \c \<values\> list in the module XML.
*/
class GrassEnumParam : public GrassParamWidget
{
    Q_OBJECT
  public:
    explicit GrassEnumParam( const GrassParamDesc &desc,
                             QWidget *parent = nullptr );
    QStringList values() const override;
    void        setValue( const QString &val ) override;

  private:
    QComboBox *mCombo = nullptr;
};

/*!
    \class GrassMapParam
    \brief GRASS map selection widget: editable combo populated via g.list.

    Runs g.list on construction for the map type derived from the
    \c \<gisprompt\> element.  Lists maps from both the active mapset and
    PERMANENT (when different).  The user can also type a name directly.

    \sa GrassParamWidget, GrassEnumParam
*/
class GrassMapParam : public GrassParamWidget
{
    Q_OBJECT
  public:
    explicit GrassMapParam( const GrassParamDesc &desc,
                            QWidget *parent = nullptr );
    QStringList values() const override;
    bool        isValid() const override;
    void        setValue( const QString &val ) override;

  private slots:
    void populate();

  private:
    QComboBox *mCombo    = nullptr;
    QString    mGisType;  // g.list type= value
};

class QSpinBox;

/*!
    \class GrassSpinParam
    \brief Integer parameter widget (QSpinBox).

    The \c mIsSet flag tracks whether the user has touched the field, so that
    a parameter at its default value contributes nothing to the command line.
*/
class GrassSpinParam : public GrassParamWidget
{
    Q_OBJECT
  public:
    explicit GrassSpinParam( const GrassParamDesc &desc,
                             QWidget *parent = nullptr );
    QStringList values() const override;
    bool        isValid() const override;

  private:
    QSpinBox *mSpin  = nullptr;
    bool      mIsSet = false;
};

class QDoubleSpinBox;

/*!
    \class GrassDoubleSpinParam
    \brief Floating-point parameter widget (QDoubleSpinBox).

    The \c mIsSet flag tracks whether the user has touched the field.
*/
class GrassDoubleSpinParam : public GrassParamWidget
{
    Q_OBJECT
  public:
    explicit GrassDoubleSpinParam( const GrassParamDesc &desc,
                                   QWidget *parent = nullptr );
    QStringList values() const override;
    bool        isValid() const override;

  private:
    QDoubleSpinBox *mSpin  = nullptr;
    bool            mIsSet = false;
};
