#include "grassparamwidget.h"
#include "../grassenv.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

// ============================================================
// GrassParamWidget (base)
// ============================================================
GrassParamWidget::GrassParamWidget( const GrassParamDesc &desc,
                                     QWidget *parent )
    : QWidget( parent ), mDesc( desc )
{}

QStringList GrassParamWidget::commandArgs() const
{
    const QStringList vals = values();
    if ( vals.isEmpty() || ( vals.size() == 1 && vals.first().isEmpty() ) )
        return {};
    // Multiple values are joined with comma for GRASS
    return { mDesc.name + "=" + vals.join( "," ) };
}

bool GrassParamWidget::isValid() const
{
    if ( !mDesc.required )
        return true;
    const QStringList vals = values();
    return !vals.isEmpty() && !vals.first().isEmpty();
}

void GrassParamWidget::markInvalid( bool invalid )
{
    setStyleSheet( invalid
                   ? "border: 1px solid #dc3545; border-radius: 3px;"
                   : "" );
}

// Factory
GrassParamWidget *GrassParamWidget::create( const GrassParamDesc &desc,
                                             QWidget *parent )
{
    // Enum (discrete values list)
    if ( !desc.values.isEmpty() )
        return new GrassEnumParam( desc, parent );

    // Gisprompt-based routing
    if ( desc.hasGisprompt )
    {
        const QString &pr = desc.gisprompt.prompt;
        // OS-accessible files → native file dialog
        if ( pr == "file" || pr == "datasource" || pr == "datasource_layer" )
            return new GrassFileParam( desc, parent );
        // GRASS map types → combo populated via g.list
        static const QStringList grassMapTypes = {
            "raster", "vector", "raster3d", "group",
            "strds", "stvds", "str3ds", "stds"
        };
        if ( grassMapTypes.contains( pr ) )
            return new GrassMapParam( desc, parent );
        // All other gisprompt types (region, mapset, etc.) → plain line edit
        return new GrassLineEditParam( desc, parent );
    }

    // Numeric types
    if ( desc.type == "integer" )
        return new GrassSpinParam( desc, parent );
    if ( desc.type == "float" || desc.type == "double" )
        return new GrassDoubleSpinParam( desc, parent );

    return new GrassLineEditParam( desc, parent );
}

// ============================================================
// GrassFlagWidget
// ============================================================
GrassFlagWidget::GrassFlagWidget( const GrassFlagDesc &desc,
                                   QWidget *parent )
    : QWidget( parent ), mDesc( desc )
{
    QHBoxLayout *lay = new QHBoxLayout( this );
    lay->setContentsMargins( 0, 0, 0, 0 );

    const QString text = desc.label.isEmpty() ? desc.description : desc.label;
    mCheck = new QCheckBox( text, this );
    mCheck->setToolTip( desc.description );
    lay->addWidget( mCheck );
    lay->addStretch();

    connect( mCheck, &QCheckBox::toggled, this, &GrassFlagWidget::valueChanged );
}

bool GrassFlagWidget::checked() const { return mCheck->isChecked(); }

QString GrassFlagWidget::commandArg() const
{
    return mCheck->isChecked() ? ( "-" + mDesc.name ) : QString{};
}

// ============================================================
// helpers shared by concrete widgets
// ============================================================
namespace {

QWidget *buildLabelRow( const QString &label,
                        const QString &description,
                        bool required,
                        QWidget *parent )
{
    QWidget *w   = new QWidget( parent );
    QHBoxLayout *h = new QHBoxLayout( w );
    h->setContentsMargins( 0, 0, 0, 0 );

    QString text = label.isEmpty() ? description : label;
    if ( required )
        text = "<b>" + text + "</b>";
    QLabel *lbl = new QLabel( text, w );
    lbl->setWordWrap( true );
    lbl->setToolTip( description );
    h->addWidget( lbl, 1 );
    return w;
}

} // anon

// ============================================================
// GrassLineEditParam
// ============================================================
GrassLineEditParam::GrassLineEditParam( const GrassParamDesc &desc,
                                         QWidget *parent )
    : GrassParamWidget( desc, parent )
{
    QVBoxLayout *lay = new QVBoxLayout( this );
    lay->setContentsMargins( 0, 2, 0, 2 );
    lay->setSpacing( 2 );

    lay->addWidget( buildLabelRow( desc.label, desc.description,
                                   desc.required, this ) );

    mEdit = new QLineEdit( this );
    mEdit->setPlaceholderText( desc.required ? "required" : desc.defaultValue );
    if ( !desc.defaultValue.isEmpty() )
        mEdit->setText( desc.defaultValue );
    lay->addWidget( mEdit );

    connect( mEdit, &QLineEdit::textChanged, this, &GrassLineEditParam::valueChanged );
}

QStringList GrassLineEditParam::values() const
{
    const QString t = mEdit->text().trimmed();
    if ( t.isEmpty() ) return {};
    if ( mDesc.multiple )
        return t.split( ',', Qt::SkipEmptyParts );
    return { t };
}

bool GrassLineEditParam::isValid() const
{
    if ( !mDesc.required ) return true;
    return !mEdit->text().trimmed().isEmpty();
}

void GrassLineEditParam::setValue( const QString &val )
{
    mEdit->setText( val );
}

// ============================================================
// GrassFileParam
// ============================================================
GrassFileParam::GrassFileParam( const GrassParamDesc &desc,
                                 QWidget *parent )
    : GrassParamWidget( desc, parent )
    , mSave( desc.hasGisprompt && desc.gisprompt.age == "new" )
{
    QVBoxLayout *lay = new QVBoxLayout( this );
    lay->setContentsMargins( 0, 2, 0, 2 );
    lay->setSpacing( 2 );

    lay->addWidget( buildLabelRow( desc.label, desc.description,
                                   desc.required, this ) );

    QHBoxLayout *row = new QHBoxLayout();
    mEdit = new QLineEdit( this );
    mEdit->setPlaceholderText( mSave ? "output file path" : "file path" );
    if ( !desc.defaultValue.isEmpty() )
        mEdit->setText( desc.defaultValue );
    row->addWidget( mEdit, 1 );

    QPushButton *btn = new QPushButton( "…", this );
    btn->setFixedWidth( 28 );
    btn->setToolTip( "Browse…" );
    row->addWidget( btn );
    lay->addLayout( row );

    connect( mEdit, &QLineEdit::textChanged, this, &GrassFileParam::valueChanged );
    connect( btn,   &QPushButton::clicked,   this, &GrassFileParam::browse );
}

void GrassFileParam::browse()
{
    const QString cur = mEdit->text();
    QString path;
    if ( mSave )
        path = QFileDialog::getSaveFileName( this, "Select output file", cur );
    else
        path = QFileDialog::getOpenFileName( this, "Select file", cur );
    if ( !path.isEmpty() )
        mEdit->setText( path );
}

QStringList GrassFileParam::values() const
{
    const QString t = mEdit->text().trimmed();
    return t.isEmpty() ? QStringList{} : QStringList{ t };
}

bool GrassFileParam::isValid() const
{
    if ( !mDesc.required ) return true;
    return !mEdit->text().trimmed().isEmpty();
}

void GrassFileParam::setValue( const QString &val )
{
    mEdit->setText( val );
}

// ============================================================
// GrassEnumParam
// ============================================================
GrassEnumParam::GrassEnumParam( const GrassParamDesc &desc,
                                 QWidget *parent )
    : GrassParamWidget( desc, parent )
{
    QVBoxLayout *lay = new QVBoxLayout( this );
    lay->setContentsMargins( 0, 2, 0, 2 );
    lay->setSpacing( 2 );

    lay->addWidget( buildLabelRow( desc.label, desc.description,
                                   desc.required, this ) );

    mCombo = new QComboBox( this );
    if ( !desc.required )
        mCombo->addItem( "" );   // empty = "not set"

    for ( const auto &v : desc.values )
    {
        const QString tip = v.second.isEmpty() ? v.first
                                                : v.first + " — " + v.second;
        mCombo->addItem( v.first );
        mCombo->setItemData( mCombo->count() - 1, tip, Qt::ToolTipRole );
    }

    // Select default
    if ( !desc.defaultValue.isEmpty() )
    {
        int idx = mCombo->findText( desc.defaultValue );
        if ( idx >= 0 ) mCombo->setCurrentIndex( idx );
    }

    lay->addWidget( mCombo );

    connect( mCombo, &QComboBox::currentTextChanged,
             this,   &GrassEnumParam::valueChanged );
}

QStringList GrassEnumParam::values() const
{
    const QString t = mCombo->currentText().trimmed();
    return t.isEmpty() ? QStringList{} : QStringList{ t };
}

void GrassEnumParam::setValue( const QString &val )
{
    const int idx = mCombo->findText( val );
    if ( idx >= 0 ) mCombo->setCurrentIndex( idx );
}

// ============================================================
// GrassSpinParam (integer)
// ============================================================
GrassSpinParam::GrassSpinParam( const GrassParamDesc &desc,
                                 QWidget *parent )
    : GrassParamWidget( desc, parent )
{
    QVBoxLayout *lay = new QVBoxLayout( this );
    lay->setContentsMargins( 0, 2, 0, 2 );
    lay->setSpacing( 2 );

    lay->addWidget( buildLabelRow( desc.label, desc.description,
                                   desc.required, this ) );

    mSpin = new QSpinBox( this );
    mSpin->setRange( INT_MIN, INT_MAX );
    mSpin->setSpecialValueText( desc.required ? "" : "(not set)" );

    bool ok = false;
    if ( !desc.defaultValue.isEmpty() )
    {
        int val = desc.defaultValue.toInt( &ok );
        if ( ok )
        {
            mSpin->setValue( val );
            mIsSet = true;
        }
    }
    if ( !desc.required && !mIsSet )
        mSpin->setValue( mSpin->minimum() ); // shows "(not set)"

    lay->addWidget( mSpin );

    connect( mSpin, &QSpinBox::valueChanged, this, [this]( int ) {
        mIsSet = true;
        emit valueChanged();
    } );
}

QStringList GrassSpinParam::values() const
{
    if ( !mIsSet && !mDesc.required ) return {};
    return { QString::number( mSpin->value() ) };
}

bool GrassSpinParam::isValid() const
{
    if ( !mDesc.required ) return true;
    return mIsSet;
}

// ============================================================
// GrassDoubleSpinParam (float)
// ============================================================
GrassDoubleSpinParam::GrassDoubleSpinParam( const GrassParamDesc &desc,
                                             QWidget *parent )
    : GrassParamWidget( desc, parent )
{
    QVBoxLayout *lay = new QVBoxLayout( this );
    lay->setContentsMargins( 0, 2, 0, 2 );
    lay->setSpacing( 2 );

    lay->addWidget( buildLabelRow( desc.label, desc.description,
                                   desc.required, this ) );

    mSpin = new QDoubleSpinBox( this );
    mSpin->setRange( -1e15, 1e15 );
    mSpin->setDecimals( 6 );
    mSpin->setSpecialValueText( desc.required ? "" : "(not set)" );

    bool ok = false;
    if ( !desc.defaultValue.isEmpty() )
    {
        double val = desc.defaultValue.toDouble( &ok );
        if ( ok )
        {
            mSpin->setValue( val );
            mIsSet = true;
        }
    }
    if ( !desc.required && !mIsSet )
        mSpin->setValue( mSpin->minimum() );

    lay->addWidget( mSpin );

    connect( mSpin, &QDoubleSpinBox::valueChanged, this, [this]( double ) {
        mIsSet = true;
        emit valueChanged();
    } );
}

QStringList GrassDoubleSpinParam::values() const
{
    if ( !mIsSet && !mDesc.required ) return {};
    return { QString::number( mSpin->value(), 'g', 10 ) };
}

bool GrassDoubleSpinParam::isValid() const
{
    if ( !mDesc.required ) return true;
    return mIsSet;
}

// ============================================================
// GrassMapParam — GRASS map selector (g.list combo)
// ============================================================

// Map from gisprompt prompt string to g.list element type
static QString promptToGlistType( const QString &prompt )
{
    if ( prompt == "raster"  ) return "raster";
    if ( prompt == "vector"  ) return "vector";
    if ( prompt == "raster3d") return "raster_3d";
    if ( prompt == "group"   ) return "group";
    if ( prompt == "strds"   ) return "strds";
    if ( prompt == "stvds"   ) return "stvds";
    if ( prompt == "str3ds"  ) return "str3ds";
    if ( prompt == "stds"    ) return "stds";
    return prompt;
}

GrassMapParam::GrassMapParam( const GrassParamDesc &desc,
                               QWidget *parent )
    : GrassParamWidget( desc, parent )
    , mGisType( promptToGlistType( desc.gisprompt.prompt ) )
{
    QVBoxLayout *lay = new QVBoxLayout( this );
    lay->setContentsMargins( 0, 2, 0, 2 );
    lay->setSpacing( 2 );

    lay->addWidget( buildLabelRow( desc.label, desc.description,
                                   desc.required, this ) );

    QHBoxLayout *row = new QHBoxLayout();
    mCombo = new QComboBox( this );
    mCombo->setEditable( true );
    mCombo->setInsertPolicy( QComboBox::NoInsert );
    mCombo->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
    mCombo->setToolTip( desc.description );
    if ( !desc.required )
        mCombo->addItem( "" );  // blank "not set" option at top
    row->addWidget( mCombo, 1 );

    QPushButton *refreshBtn = new QPushButton( "⟳", this );
    refreshBtn->setFixedWidth( 28 );
    refreshBtn->setToolTip( "Refresh map list" );
    row->addWidget( refreshBtn );
    lay->addLayout( row );

    populate();

    if ( !desc.defaultValue.isEmpty() )
    {
        int idx = mCombo->findText( desc.defaultValue );
        if ( idx >= 0 )
            mCombo->setCurrentIndex( idx );
        else
            mCombo->setEditText( desc.defaultValue );
    }

    connect( mCombo, &QComboBox::currentTextChanged,
             this,   &GrassMapParam::valueChanged );
    connect( refreshBtn, &QPushButton::clicked,
             this,       &GrassMapParam::populate );
}

void GrassMapParam::populate()
{
    const QString gisbase = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
    const QString mapset  = QString::fromLocal8Bit( qgetenv( "MAPSET" ) );
    const QString perm    = "PERMANENT";
    if ( gisbase.isEmpty() || mGisType.isEmpty() ) return;

    const QString glist = gisbase + "/bin/g.list";

    // Collect maps from active mapset (and PERMANENT when different)
    QStringList maps;
    auto runGlist = [&]( const QString &ms ) {
        QProcess p;
        p.setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );
        p.start( glist, { "type=" + mGisType, "mapset=" + ms, "-m", "-t" } );
        if ( !p.waitForFinished( 5000 ) ) return;
        const QString out = QString::fromLocal8Bit( p.readAllStandardOutput() );
        for ( const QString &line : out.split( '\n', Qt::SkipEmptyParts ) )
        {
            const QString trimmed = line.trimmed();
            if ( !trimmed.isEmpty() && !maps.contains( trimmed ) )
                maps << trimmed;
        }
    };

    runGlist( mapset );
    if ( !mapset.isEmpty() && mapset != perm )
        runGlist( perm );

    maps.sort( Qt::CaseInsensitive );

    const QString current = mCombo->currentText();
    mCombo->blockSignals( true );
    mCombo->clear();
    if ( !mDesc.required )
        mCombo->addItem( "" );
    mCombo->addItems( maps );

    // Restore previous selection
    const int idx = mCombo->findText( current );
    if ( idx >= 0 )
        mCombo->setCurrentIndex( idx );
    else if ( !current.isEmpty() )
        mCombo->setEditText( current );
    mCombo->blockSignals( false );
}

QStringList GrassMapParam::values() const
{
    const QString t = mCombo->currentText().trimmed();
    if ( t.isEmpty() ) return {};
    if ( mDesc.multiple )
        return t.split( ',', Qt::SkipEmptyParts );
    return { t };
}

bool GrassMapParam::isValid() const
{
    if ( !mDesc.required ) return true;
    return !mCombo->currentText().trimmed().isEmpty();
}

void GrassMapParam::setValue( const QString &val )
{
    const int idx = mCombo->findText( val );
    if ( idx >= 0 )
        mCombo->setCurrentIndex( idx );
    else
        mCombo->setEditText( val );
}
