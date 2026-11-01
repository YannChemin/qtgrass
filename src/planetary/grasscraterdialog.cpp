#include "grasscraterdialog.h"
#include "../grassenv.h"
#include "../grassinterface.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcCrater, "qtgrass.crater")

/* ------------------------------------------------------------------ */
/* Built-in planetary body database, mirroring planet_db.c in          */
/* p.crater. Kept here as static data so the dialog can populate the   */
/* combo without spawning a subprocess. Tuples: (name, g, surf_rho,    */
/* target_type, description).                                          */
/* ------------------------------------------------------------------ */
struct BodyEntry {
    const char *name; double g; double surf_rho; int ttype;
    const char *desc;
};

static const BodyEntry kBodies[] = {
    /* Terrestrial */
    { "mercury",  3.701, 1800.0, 3, "Mercury (silicate regolith)" },
    { "venus",    8.870, 2900.0, 3, "Venus (basaltic surface)" },
    { "earth",    9.807, 2700.0, 3, "Earth (continental crust)" },
    { "mars",     3.711, 2900.0, 3, "Mars (basaltic default)" },
    { "moon",     1.622, 1500.0, 2, "Earth's Moon (Luna)" },
    /* Galilean */
    { "io",       1.796, 2900.0, 3, "Io (silicate/sulfur)" },
    { "europa",   1.315,  920.0, 1, "Europa (water ice shell)" },
    { "ganymede", 1.428,  920.0, 1, "Ganymede" },
    { "callisto", 1.235,  920.0, 1, "Callisto" },
    /* Saturnian */
    { "titan",    1.352,  920.0, 1, "Titan (ice + organics)" },
    { "mimas",    0.0648, 920.0, 1, "Mimas" },
    { "enceladus",0.113,  920.0, 1, "Enceladus" },
    { "tethys",   0.146,  920.0, 1, "Tethys" },
    { "dione",    0.232,  920.0, 1, "Dione" },
    { "rhea",     0.264,  920.0, 1, "Rhea" },
    { "iapetus",  0.223,  920.0, 1, "Iapetus" },
    { "hyperion", 0.0179, 700.0, 1, "Hyperion (porous)" },
    { "phoebe",   0.0394, 920.0, 1, "Phoebe" },
    /* Uranian */
    { "miranda",  0.079,  920.0, 1, "Miranda" },
    { "ariel",    0.249,  920.0, 1, "Ariel" },
    { "umbriel",  0.230,  920.0, 1, "Umbriel" },
    { "titania",  0.371,  920.0, 1, "Titania" },
    { "oberon",   0.346,  920.0, 1, "Oberon" },
    /* Neptunian */
    { "triton",   0.779,  920.0, 1, "Triton (N2/CH4 ice)" },
    /* Martian moons */
    { "phobos",   0.0057, 1500.0,2, "Phobos (rubble pile)" },
    { "deimos",   0.0030, 1500.0,2, "Deimos" },
    /* Dwarf planets */
    { "pluto",    0.620,  920.0, 1, "Pluto" },
    { "charon",   0.288,  920.0, 1, "Charon" },
    { "ceres",    0.284, 1600.0, 2, "Ceres" },
    { "eris",     0.824,  920.0, 1, "Eris" },
    { "haumea",   0.401,  920.0, 1, "Haumea" },
    { "makemake", 0.500,  920.0, 1, "Makemake" },
    /* TNOs */
    { "gonggong", 0.220,  920.0, 1, "Gonggong" },
    { "quaoar",   0.276,  920.0, 1, "Quaoar" },
    { "sedna",    0.330,  920.0, 1, "Sedna" },
    { "orcus",    0.250,  920.0, 1, "Orcus" },
    { "salacia",  0.250,  920.0, 1, "Salacia" },
    /* Asteroids */
    { "vesta",    0.220, 2900.0, 3, "Vesta (HED parent)" },
    { "pallas",   0.205, 2700.0, 3, "Pallas (B-type)" },
    { "hygiea",   0.143, 2700.0, 3, "Hygiea (C-type)" },
    { "psyche",   0.144, 3500.0, 3, "Psyche (M-type)" },
    { "lutetia",  0.044, 2700.0, 3, "Lutetia" },
    { "mathilde", 0.010, 1300.0, 2, "Mathilde (porous)" },
    { "eros",     0.006, 2700.0, 3, "Eros (433)" },
    { "itokawa",  0.00009,1900.0,2, "Itokawa (25143)" },
    { "bennu",    0.00006,1190.0,2, "Bennu (101955)" },
    { "ryugu",    0.00012,1190.0,2, "Ryugu (162173)" },
    { "67p",      0.0001, 533.0, 2, "67P/C-G (comet)" },
    /* Custom slot */
    { "custom",   0.0,    0.0,   0, "User-defined (set gravity/density/type)" },
};
static const int kBodyCount = sizeof(kBodies) / sizeof(kBodies[0]);

// ===================================================================
GrassCraterDialog::GrassCraterDialog( GrassInterface *iface,
                                       QWidget *parent )
    : QDialog( parent )
    , mIface( iface )
{
    setWindowTitle( "p.crater - Vector Crater Scaling" );
    setMinimumSize( 720, 640 );
    buildUi();
    populateBodies();
    refreshVectorLists();
    /* Apply Moon defaults to the labelled info line. */
    onBodyChanged( "moon" );
}

GrassCraterDialog::~GrassCraterDialog()
{
    if ( mProcess && mProcess->state() != QProcess::NotRunning ) {
        mProcess->kill();
        mProcess->waitForFinished( 1000 );
    }
}

// ── UI construction ─────────────────────────────────────────────────
void GrassCraterDialog::buildUi()
{
    auto *root = new QVBoxLayout( this );

    auto *tabs = new QTabWidget( this );
    root->addWidget( tabs );

    // ── Tab 1: Geometry & body ──
    auto *geomTab = new QWidget( tabs );
    auto *geomLay = new QFormLayout( geomTab );

    mInputVector  = new QComboBox( geomTab );
    mInputVector->setEditable( true );
    mInputVector->setMinimumWidth( 280 );
    geomLay->addRow( "Input crater rim vector:",   mInputVector );

    mOutputVector = new QLineEdit( geomTab );
    geomLay->addRow( "Output vector name:",        mOutputVector );

    mBody = new QComboBox( geomTab );
    geomLay->addRow( "Planetary body:",            mBody );

    mBodyInfo = new QLabel( "(select a body)", geomTab );
    mBodyInfo->setStyleSheet( "color:#666; font-style:italic;" );
    mBodyInfo->setWordWrap( true );
    geomLay->addRow( "",                            mBodyInfo );

    mBackward = new QCheckBox( "Backward mode (estimate impactor from rim)",
                                geomTab );
    mBackward->setChecked( true );
    geomLay->addRow( "",                            mBackward );

    tabs->addTab( geomTab, "Geometry && Body" );

    // ── Tab 2: Impactor ──
    auto *impTab = new QWidget( tabs );
    auto *impLay = new QFormLayout( impTab );

    mVelocity = new QDoubleSpinBox( impTab );
    mVelocity->setRange( 100.0, 100000.0 );
    mVelocity->setValue( 20000.0 );
    mVelocity->setSuffix( " m/s" );
    impLay->addRow( "Impactor velocity:",          mVelocity );

    mAngle = new QDoubleSpinBox( impTab );
    mAngle->setRange( 1.0, 90.0 );
    mAngle->setValue( 45.0 );
    mAngle->setSuffix( " deg" );
    impLay->addRow( "Impact angle (from horiz.):", mAngle );

    mProjDens = new QDoubleSpinBox( impTab );
    mProjDens->setRange( 100.0, 30000.0 );
    mProjDens->setValue( 3000.0 );
    mProjDens->setSuffix( " kg/m^3" );
    impLay->addRow( "Impactor density:",           mProjDens );

    mProjDiam = new QDoubleSpinBox( impTab );
    mProjDiam->setRange( 0.0, 1.0e6 );
    mProjDiam->setValue( 0.0 );
    mProjDiam->setSuffix( " m (0 = backward mode)" );
    mProjDiam->setSpecialValueText( "(backward mode)" );
    impLay->addRow( "Impactor diameter:",          mProjDiam );

    tabs->addTab( impTab, "Impactor" );

    // ── Tab 3: Geology (two-layer + DEM) ──
    auto *geolTab = new QWidget( tabs );
    auto *geolLay = new QVBoxLayout( geolTab );

    auto *surfBox = new QGroupBox( "Surface layer", geolTab );
    auto *surfForm = new QFormLayout( surfBox );
    mSurfDensMap = new QComboBox( surfBox );
    mSurfDensMap->setEditable( true );
    mSurfDensMap->addItem( "" );  // empty = unused
    mSurfThick = new QDoubleSpinBox( surfBox );
    mSurfThick->setRange( 0.0, 1.0e6 );
    mSurfThick->setValue( 0.0 );
    mSurfThick->setSuffix( " m (0 = unused)" );
    mSurfThick->setSpecialValueText( "(unused)" );
    surfForm->addRow( "Surface density raster:",  mSurfDensMap );
    surfForm->addRow( "Surface layer thickness:", mSurfThick );
    geolLay->addWidget( surfBox );

    auto *subBox = new QGroupBox( "Subsurface layer", geolTab );
    auto *subForm = new QFormLayout( subBox );
    mSubDensMap = new QComboBox( subBox );
    mSubDensMap->setEditable( true );
    mSubDensMap->addItem( "" );
    mSubThick = new QDoubleSpinBox( subBox );
    mSubThick->setRange( 0.0, 1.0e6 );
    mSubThick->setValue( 0.0 );
    mSubThick->setSuffix( " m (0 = infinite)" );
    mSubThick->setSpecialValueText( "(infinite)" );
    subForm->addRow( "Subsurface density raster:",mSubDensMap );
    subForm->addRow( "Subsurface thickness:",     mSubThick );
    geolLay->addWidget( subBox );

    auto *miscBox = new QGroupBox( "Optional raster inputs", geolTab );
    auto *miscForm = new QFormLayout( miscBox );
    mGeolType = new QComboBox( miscBox );
    mGeolType->setEditable( true );
    mGeolType->addItem( "" );
    mDem = new QComboBox( miscBox );
    mDem->setEditable( true );
    mDem->addItem( "" );
    mDdMap = new QComboBox( miscBox );
    mDdMap->setEditable( true );
    mDdMap->addItem( "" );
    miscForm->addRow( "Surface type raster (1/2/3):",      mGeolType );
    miscForm->addRow( "DEM raster (for depth check):",     mDem );
    miscForm->addRow( "d/D raster (spatial d/D variation):", mDdMap );
    geolLay->addWidget( miscBox );

    geolLay->addStretch();
    tabs->addTab( geolTab, "Geology && DEM" );

    // ── Tab 4: Impactor points (synthesis) ──
    auto *impTabSyn = new QWidget( tabs );
    auto *impSynLay = new QVBoxLayout( impTabSyn );

    mUseImpactors = new QCheckBox(
        "Use a POINT vector of impactors with per-point attributes "
        "(forward synthesis mode - DEM required, ignores the scalar "
        "Impactor tab and any rim vector on Tab 1)", impTabSyn );
    impSynLay->addWidget( mUseImpactors );

    auto *vecBox = new QGroupBox( "Impactor point vector", impTabSyn );
    auto *vecForm = new QFormLayout( vecBox );
    mImpactorVec = new QComboBox( vecBox );
    mImpactorVec->setEditable( true );
    vecForm->addRow( "Vector map:", mImpactorVec );
    impSynLay->addWidget( vecBox );

    auto *colBox = new QGroupBox(
        "Attribute column names (set defaults; edit if your schema "
        "uses different names)", impTabSyn );
    auto *colForm = new QFormLayout( colBox );

    auto mkColCombo = [&]( const QString &dflt ) {
        auto *c = new QComboBox( colBox );
        c->setEditable( true );
        c->addItem( dflt );
        c->setCurrentText( dflt );
        return c;
    };
    mColVelocity = mkColCombo( "velocity"     );
    mColAngle    = mkColCombo( "impact_angle" );
    mColAzimuth  = mkColCombo( "azimuth"      );
    mColDensity  = mkColCombo( "density"      );
    mColDiameter = mkColCombo( "diameter"     );
    colForm->addRow( "Velocity column [m/s]:",          mColVelocity );
    colForm->addRow( "Impact angle column [deg from surface]:",
                                                          mColAngle );
    colForm->addRow( "Azimuth column [deg, 0 = N CW]:",   mColAzimuth );
    colForm->addRow( "Density column:",                   mColDensity );
    colForm->addRow( "Diameter column [m]:",              mColDiameter );

    mDensityUnit = new QComboBox( colBox );
    mDensityUnit->addItem( "kg/m^3",  "kg_m3" );
    mDensityUnit->addItem( "g/cm^3",  "g_cm3" );
    colForm->addRow( "Density unit:",  mDensityUnit );
    impSynLay->addWidget( colBox );

    /* Helpful reminder: DEM is required - on tab 3. */
    auto *demHint = new QLabel(
        "<i>Note:</i> set the DEM raster on the 'Geology &amp; DEM' tab. "
        "It is required to sample elevation and local slope/aspect at each "
        "impact site, and to orient ellipse polygons.", impTabSyn );
    demHint->setWordWrap( true );
    impSynLay->addWidget( demHint );

    impSynLay->addStretch();
    tabs->addTab( impTabSyn, "Impactor Points" );

    // ── Tab 5: Custom body overrides ──
    auto *customTab = new QWidget( tabs );
    auto *customLay = new QVBoxLayout( customTab );
    mCustomBox = new QGroupBox(
        "Required only when body=custom is selected", customTab );
    auto *cLay = new QFormLayout( mCustomBox );
    mGravity = new QDoubleSpinBox( mCustomBox );
    mGravity->setRange( 0.0, 100.0 );
    mGravity->setDecimals( 4 );
    mGravity->setSuffix( " m/s^2" );
    cLay->addRow( "Surface gravity:",           mGravity );
    mTargetDens = new QDoubleSpinBox( mCustomBox );
    mTargetDens->setRange( 0.0, 30000.0 );
    mTargetDens->setSuffix( " kg/m^3" );
    cLay->addRow( "Target surface density:",    mTargetDens );
    mTargetType = new QComboBox( mCustomBox );
    mTargetType->addItem( "(use body default)", 0 );
    mTargetType->addItem( "1 - water / ice",    1 );
    mTargetType->addItem( "2 - loose sand / regolith", 2 );
    mTargetType->addItem( "3 - competent rock / saturated soil", 3 );
    cLay->addRow( "Target type:",               mTargetType );

    mDdSimple = new QDoubleSpinBox( mCustomBox );
    mDdSimple->setRange( 0.0, 0.5 );
    mDdSimple->setDecimals( 3 );
    mDdSimple->setSingleStep( 0.01 );
    mDdSimple->setValue( 0.0 );
    mDdSimple->setSpecialValueText( "(use body default)" );
    cLay->addRow( "Simple-crater d/D (override):", mDdSimple );
    customLay->addWidget( mCustomBox );
    customLay->addStretch();
    tabs->addTab( customTab, "Custom body" );

    // ── Tab 5: Run log ──
    auto *logTab = new QWidget( tabs );
    auto *logLay = new QVBoxLayout( logTab );
    mLog = new QPlainTextEdit( logTab );
    mLog->setReadOnly( true );
    mLog->setStyleSheet( "font-family: monospace; font-size: 10pt;" );
    logLay->addWidget( mLog );
    tabs->addTab( logTab, "Run log" );

    // ── Buttons ──
    auto *btnBox = new QDialogButtonBox( this );
    mRunBtn    = btnBox->addButton( "&Run p.crater",
                                      QDialogButtonBox::AcceptRole );
    mCancelBtn = btnBox->addButton( "Cancel run",
                                      QDialogButtonBox::DestructiveRole );
    btnBox->addButton( QDialogButtonBox::Close );
    mCancelBtn->setEnabled( false );
    root->addWidget( btnBox );

    connect( mRunBtn,     &QPushButton::clicked, this,
             &GrassCraterDialog::runModule );
    connect( mCancelBtn,  &QPushButton::clicked, this,
             &GrassCraterDialog::cancelRun );
    connect( btnBox, &QDialogButtonBox::rejected, this, &QDialog::accept );
    connect( mBody, &QComboBox::currentTextChanged, this,
             &GrassCraterDialog::onBodyChanged );
    connect( mBackward, &QCheckBox::toggled, this,
             &GrassCraterDialog::onModeChanged );
}

// ── Populate the body combo from the static table ──
void GrassCraterDialog::populateBodies()
{
    for ( int i = 0; i < kBodyCount; i++ )
        mBody->addItem( kBodies[i].name );
    mBody->setCurrentText( "moon" );
}

// ── List vectors / rasters in the current mapset for the combos ──
void GrassCraterDialog::refreshVectorLists()
{
    /* Simple file-system scan: $GISDBASE/$LOCATION/$MAPSET/vector/<name>/
     * Conservative implementation; if env isn't set, leave combos empty. */
    QString gisdb   = qgetenv( "GISDBASE" );
    QString loc     = qgetenv( "LOCATION_NAME" );
    QString mapset  = qgetenv( "MAPSET" );
    if ( gisdb.isEmpty() || loc.isEmpty() || mapset.isEmpty() )
        return;

    QString vecDir  = QString( "%1/%2/%3/vector"  ).arg(gisdb, loc, mapset);
    QString rastDir = QString( "%1/%2/%3/cell"    ).arg(gisdb, loc, mapset);

    QStringList vecs  = QDir(vecDir ).entryList( QDir::Dirs |
                          QDir::NoDotAndDotDot, QDir::Name );
    QStringList rasts = QDir(rastDir).entryList( QDir::Files,    QDir::Name );

    mInputVector->addItems( vecs );
    if ( mImpactorVec ) mImpactorVec->addItems( vecs );
    for ( QComboBox *c : { mSurfDensMap, mSubDensMap, mGeolType, mDem, mDdMap } ) {
        c->addItems( rasts );
    }
}

// ── Update info label & toggle custom-body tab availability ─────────
void GrassCraterDialog::onBodyChanged( const QString &name )
{
    for ( int i = 0; i < kBodyCount; i++ ) {
        if ( name == kBodies[i].name ) {
            const auto &b = kBodies[i];
            if ( QString::fromLatin1(b.name) == "custom" ) {
                mBodyInfo->setText(
                    "<b>Custom body</b> - fill in gravity, target density "
                    "and target type on the 'Custom body' tab." );
                mCustomBox->setEnabled( true );
            } else {
                mBodyInfo->setText( QString(
                    "%1<br><i>g=%2 m/s^2, surface density=%3 kg/m^3, "
                    "target type=%4</i>" )
                    .arg( b.desc )
                    .arg( b.g, 0, 'f', 4 )
                    .arg( b.surf_rho, 0, 'f', 0 )
                    .arg( b.ttype ) );
                mCustomBox->setEnabled( false );
            }
            return;
        }
    }
}

void GrassCraterDialog::onModeChanged()
{
    /* When backward mode is on, impactor diameter has no meaning. */
    mProjDiam->setEnabled( !mBackward->isChecked() );
}

// ── Build the QProcess argv ─────────────────────────────────────────
QStringList GrassCraterDialog::buildArgs() const
{
    QStringList args;
    bool synth = mUseImpactors && mUseImpactors->isChecked();

    if ( synth )
        args << "impactors=" + mImpactorVec->currentText().trimmed();
    else
        args << "input=" + mInputVector->currentText().trimmed();
    args << "output=" + mOutputVector->text().trimmed();
    args << "body="   + mBody->currentText().trimmed();

    if ( !synth ) {
        /* Scalar impactor (Tab 2) only used in the polygon-scaling mode. */
        args << QString( "impactor_velocity=%1" ).arg(mVelocity->value());
        args << QString( "impactor_angle=%1"   ).arg(mAngle->value());
        args << QString( "impactor_density=%1" ).arg(mProjDens->value());
        if ( !mBackward->isChecked() && mProjDiam->value() > 0.0 )
            args << QString( "impactor_diameter=%1" ).arg(mProjDiam->value());
    } else {
        /* Synthesis-mode: per-point columns. */
        args << "col_velocity=" + mColVelocity->currentText().trimmed();
        args << "col_angle="    + mColAngle   ->currentText().trimmed();
        args << "col_azimuth="  + mColAzimuth ->currentText().trimmed();
        args << "col_density="  + mColDensity ->currentText().trimmed();
        args << "col_diameter=" + mColDiameter->currentText().trimmed();
        args << "density_unit=" + mDensityUnit->currentData().toString();
    }

    /* Custom body overrides */
    if ( mBody->currentText() == "custom" ) {
        args << QString( "gravity=%1"        ).arg(mGravity->value());
        args << QString( "target_density=%1" ).arg(mTargetDens->value());
        int tt = mTargetType->currentData().toInt();
        if ( tt > 0 )
            args << QString( "target_type=%1" ).arg(tt);
    }
    /* Optional d/D override - applies to any body, not just custom. */
    if ( mDdSimple && mDdSimple->value() > 0.0 )
        args << QString( "dd_simple=%1" ).arg(mDdSimple->value());

    /* Optional rasters (DEM required in synthesis mode; the others
     * are only meaningful in scaling mode but harmless either way). */
    auto pushIfFilled = [&]( const QString &key, QComboBox *c ) {
        QString v = c->currentText().trimmed();
        if ( !v.isEmpty() ) args << key + "=" + v;
    };
    if ( !synth ) {
        pushIfFilled( "surface_density_map",    mSurfDensMap );
        pushIfFilled( "subsurface_density_map", mSubDensMap );
        pushIfFilled( "geology_type",           mGeolType );
        if ( mSurfThick->value() > 0.0 )
            args << QString( "surface_thickness=%1"    ).arg(mSurfThick->value());
        if ( mSubThick->value()  > 0.0 )
            args << QString( "subsurface_thickness=%1" ).arg(mSubThick->value());
    }
    pushIfFilled( "dem",           mDem );
    pushIfFilled( "dd_simple_map", mDdMap );

    /* Backward flag is invalid in synthesis mode. */
    if ( !synth && mBackward->isChecked() )
        args << "-b";
    args << "--overwrite";
    return args;
}

// ── Run p.crater ───────────────────────────────────────────────────
void GrassCraterDialog::runModule()
{
    bool synth = mUseImpactors && mUseImpactors->isChecked();
    if ( synth ) {
        if ( mImpactorVec->currentText().trimmed().isEmpty() ||
             mOutputVector->text().trimmed().isEmpty() )
        {
            mLog->appendPlainText(
                "ERROR: impactor vector and output vector name are required "
                "in synthesis mode.\n" );
            return;
        }
        if ( mDem->currentText().trimmed().isEmpty() ) {
            mLog->appendPlainText(
                "ERROR: synthesis mode requires a DEM raster on the "
                "'Geology & DEM' tab.\n" );
            return;
        }
    } else if ( mInputVector->currentText().trimmed().isEmpty() ||
                  mOutputVector->text().trimmed().isEmpty() )
    {
        mLog->appendPlainText(
            "ERROR: input vector and output vector name are required.\n" );
        return;
    }
    if ( mBody->currentText() == "custom" &&
         ( mGravity->value() <= 0.0 ||
           mTargetDens->value() <= 0.0 ||
           mTargetType->currentData().toInt() == 0 ) )
    {
        mLog->appendPlainText(
            "ERROR: body=custom requires gravity, target_density and "
            "target_type to be set on the Custom body tab.\n" );
        return;
    }

    QStringList args = buildArgs();
    mLog->appendPlainText( "$ p.crater " + args.join(" ") );

    mProcess = new QProcess( this );
    mProcess->setProcessChannelMode( QProcess::MergedChannels );
    connect( mProcess, &QProcess::readyReadStandardOutput, this,
             &GrassCraterDialog::onProcessOutput );
    connect( mProcess,
             QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
             this, &GrassCraterDialog::onProcessFinished );

    mRunBtn->setEnabled( false );
    mCancelBtn->setEnabled( true );
    mProcess->start( "p.crater", args );
}

void GrassCraterDialog::cancelRun()
{
    if ( mProcess && mProcess->state() != QProcess::NotRunning ) {
        mProcess->kill();
        mLog->appendPlainText( "[ run cancelled by user ]" );
    }
}

void GrassCraterDialog::onProcessOutput()
{
    if ( !mProcess ) return;
    QByteArray data = mProcess->readAllStandardOutput();
    mLog->appendPlainText( QString::fromLocal8Bit(data).trimmed() );
}

void GrassCraterDialog::onProcessFinished( int exitCode,
                                            QProcess::ExitStatus status )
{
    mLog->appendPlainText( QString( "[ p.crater exited with code %1 (%2) ]" )
                              .arg(exitCode)
                              .arg( status == QProcess::NormalExit
                                      ? "normal" : "crash" ) );
    mRunBtn->setEnabled( true );
    mCancelBtn->setEnabled( false );
    if ( mProcess ) { mProcess->deleteLater(); mProcess = nullptr; }
}
