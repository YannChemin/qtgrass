#include "grasslocationwizard.h"
#include "grasscrsmodel.h"
#include "../grassenv.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpressionValidator>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QTextEdit>
#include <QTextStream>
#include <QLoggingCategory>
#include <QTimer>
#include <QVBoxLayout>
#include <QWizardPage>

#include <proj.h>
#include <cmath>

Q_LOGGING_CATEGORY(lcStartup, "qtgrass.startup")

// ---------------------------------------------------------------------------
// fixupProjInfo — ensure GRASS-required fields are present in PROJ_INFO.
//
// g.proj writes what PROJ gives it.  For spherical bodies PROJ describes the
// ellipsoid with only the "R" (uniform sphere) parameter; it omits the "a"
// (semi-major) and "es" (eccentricity squared) keys that GRASS's internal
// G__read_ellipsoid_name() reads.  Without them G_gisinit() emits
// "No ellipsoid info given in file" and calls G_fatal_error().
//
// This function uses the PROJ 9 C API to query the authoritative ellipsoid
// parameters for the given SRID (e.g. "IAU_2015:49920") and then rewrites
// the PROJ_INFO with explicit "a:" and "es:" lines if they are absent.
// ---------------------------------------------------------------------------
static void fixupProjInfo( const QString &projInfoPath, const QString &srid )
{
    if ( srid.isEmpty() || !QFile::exists( projInfoPath ) )
        return;

    // --- Read the existing file ---
    QFile f( projInfoPath );
    if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) )
        return;
    QStringList lines;
    bool hasA = false, hasEs = false;
    double existingR = 0.0;
    while ( !f.atEnd() )
    {
        const QString line = f.readLine().trimmed();
        if ( line.startsWith( "a:" ) )          hasA  = true;
        if ( line.startsWith( "es:" ) )         hasEs = true;
        if ( line.startsWith( "R:" ) )
            existingR = line.mid(2).trimmed().toDouble();
        lines << line;
    }
    f.close();

    if ( hasA && hasEs )
        return;   // already complete — nothing to do

    // --- Query PROJ for the actual ellipsoid ---
    double semiMajor = existingR;  // fallback: use R from file
    double es        = 0.0;        // spherical by default

    PJ_CONTEXT *ctx = proj_context_create();
    // Accept both "AUTH:CODE" and bare PROJ strings
    PJ *crs = proj_create( ctx, srid.toUtf8().constData() );
    if ( !crs )
    {
        // Try splitting AUTH:CODE and looking up the database directly
        const int colon = srid.indexOf(':');
        if ( colon > 0 )
        {
            crs = proj_create_from_database(
                ctx,
                srid.left(colon).toUtf8().constData(),
                srid.mid(colon + 1).toUtf8().constData(),
                PJ_CATEGORY_CRS, 0, nullptr );
        }
    }

    if ( crs )
    {
        PJ *ell = proj_get_ellipsoid( ctx, crs );
        if ( ell )
        {
            double a = 0.0, b = 0.0, invF = 0.0;
            int semiMinorComputed = 0;
            if ( proj_ellipsoid_get_parameters( ctx, ell, &a, &b,
                                                &semiMinorComputed, &invF ) )
            {
                semiMajor = a;
                es = ( a > 0.0 && b > 0.0 ) ? 1.0 - (b / a) * (b / a) : 0.0;
            }
            proj_destroy( ell );
        }
        proj_destroy( crs );
    }
    proj_context_destroy( ctx );

    if ( semiMajor <= 0.0 )
    {
        qCWarning(lcStartup) << "fixupProjInfo: could not determine semi-major axis for"
                             << srid << "— PROJ_INFO unchanged";
        return;
    }

    // --- Rewrite the file with a: and es: inserted after the first line ---
    QFile out( projInfoPath );
    if ( !out.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate ) )
        return;
    QTextStream ts( &out );
    bool injected = false;
    for ( const QString &line : std::as_const(lines) )
    {
        if ( line.startsWith( "a:" ) || line.startsWith( "es:" ) )
            continue;   // remove stale/wrong values — we'll inject correct ones
        ts << line << "\n";
        if ( !injected && !line.startsWith( "name:" ) && !line.startsWith( "proj:" ) )
        {
            // Insert after the CRS type line (proj:) so GRASS reads them early
        }
        if ( !injected && line.startsWith( "proj:" ) )
        {
            ts << "a: "  << QString::number( semiMajor, 'f', 0 ) << "\n";
            ts << "es: " << QString::number( es, 'g', 15 )       << "\n";
            injected = true;
        }
    }
    if ( !injected )
    {
        // Fallback: append at end
        ts << "a: "  << QString::number( semiMajor, 'f', 0 ) << "\n";
        ts << "es: " << QString::number( es, 'g', 15 )       << "\n";
    }
    qCInfo(lcStartup) << "fixupProjInfo: wrote a=" << semiMajor
                      << "es=" << es << "to" << projInfoPath;
}

// ============================================================
// Page 0: GisdbPage
// ============================================================
class GisdbPage : public QWizardPage
{
    Q_OBJECT

  public:
    explicit GisdbPage( QWidget *parent = nullptr ) : QWizardPage( parent )
    {
        setTitle( "GRASS Database" );
        setSubTitle( "Choose or create the directory that will hold your GRASS locations." );

        QVBoxLayout *lay = new QVBoxLayout( this );

        QHBoxLayout *row = new QHBoxLayout();
        mEdit = new QLineEdit( this );
        mEdit->setPlaceholderText( QDir::homePath() + "/grassdata" );
        QPushButton *btn = new QPushButton( "Browse…", this );
        row->addWidget( mEdit, 1 );
        row->addWidget( btn );
        lay->addLayout( row );

        mInfo = new QLabel( this );
        mInfo->setWordWrap( true );
        lay->addWidget( mInfo );
        lay->addStretch();

        registerField( "gisdbase*", mEdit );

        connect( btn, &QPushButton::clicked, this, [this] {
            const QString d = QFileDialog::getExistingDirectory(
                this, "Select GRASS Database Directory",
                mEdit->text().isEmpty() ? QDir::homePath() : mEdit->text() );
            if ( !d.isEmpty() ) mEdit->setText( d );
        } );
        connect( mEdit, &QLineEdit::textChanged, this, [this]( const QString &p ) {
            QDir dir( p );
            if ( dir.exists() )
                mInfo->setText( QString( "Directory exists — %1 location(s) found." )
                                .arg( countLocations( p ) ) );
            else
                mInfo->setText( "Directory will be created." );
            emit completeChanged();
        } );
    }

    void initializePage() override
    {
        QWizardPage::initializePage();
        // If the field is pre-filled (e.g. from a previous wizard run or QSettings),
        // Qt does not automatically emit completeChanged(), so Next stays disabled.
        // Force a re-check so Next is enabled immediately when text is already present.
        if ( mEdit->text().trimmed().isEmpty() )
        {
            // Default to ~/grassdata if it exists
            const QString def = QDir::homePath() + "/grassdata";
            if ( QDir( def ).exists() )
                mEdit->setText( def );
        }
        emit completeChanged();
    }

    bool isComplete() const override
    {
        return !mEdit->text().trimmed().isEmpty();
    }

  private:
    static int countLocations( const QString &gdb )
    {
        int n = 0;
        for ( const QString &e : QDir( gdb ).entryList( QDir::Dirs | QDir::NoDotAndDotDot ) )
            if ( QFile::exists( gdb + "/" + e + "/PERMANENT/WIND" ) )
                ++n;
        return n;
    }

    QLineEdit *mEdit = nullptr;
    QLabel    *mInfo = nullptr;
};

// ============================================================
// Page 1: CrsChoicePage
// ============================================================
class CrsChoicePage : public QWizardPage
{
    Q_OBJECT

  public:
    explicit CrsChoicePage( QWidget *parent = nullptr ) : QWizardPage( parent )
    {
        setTitle( "Coordinate Reference System" );
        setSubTitle( "Choose the CRS type for your new location." );

        // Two-column grid: radio button | description
        auto *grid = new QGridLayout( this );
        grid->setColumnStretch( 1, 1 );
        grid->setHorizontalSpacing( 16 );
        grid->setVerticalSpacing( 12 );

        // Helper to create a description label
        auto mkDesc = [this]( const QString &text ) -> QLabel * {
            auto *l = new QLabel( text, this );
            l->setWordWrap( true );
            l->setStyleSheet( "color: #555; font-size: 11px;" );
            return l;
        };

        // Row 0 — Earth
        mEarth = new QRadioButton( "Earth", this );
        mEarth->setChecked( true );
        mEarth->setStyleSheet( "font-weight: bold;" );
        grid->addWidget( mEarth, 0, 0, Qt::AlignTop );
        grid->addWidget( mkDesc(
            "Search by EPSG code or name — covers all geographic and projected "
            "coordinate systems for Earth (WGS 84, UTM, national grids, etc.).\n"
            "e.g. EPSG:4326 (WGS 84), EPSG:32632 (UTM zone 32N)…" ), 0, 1 );

        // Row 1 — Planetary body
        mPlanetary = new QRadioButton( "Planetary body", this );
        mPlanetary->setStyleSheet( "font-weight: bold;" );
        grid->addWidget( mPlanetary, 1, 0, Qt::AlignTop );
        grid->addWidget( mkDesc(
            "IAU-2015 definitions via GDAL/PROJ — covers the Moon, Mars, Venus, "
            "Jupiter, Saturn and its moons, Pluto, comets, asteroids and other "
            "solar-system bodies (2 201 CRS total)." ), 1, 1 );

        // Row 2 — XY Cartesian
        mXY = new QRadioButton( "Non-projected (XY)", this );
        mXY->setStyleSheet( "font-weight: bold;" );
        grid->addWidget( mXY, 2, 0, Qt::AlignTop );
        grid->addWidget( mkDesc(
            "No coordinate reference system — arbitrary Cartesian units (meters, "
            "feet, pixels, …). Useful for image analysis, engineering drawings, "
            "or any data without a geographic reference." ), 2, 1 );

        grid->setRowStretch( 3, 1 );  // push rows to top

        registerField( "crsEarth",     mEarth );
        registerField( "crsPlanetary", mPlanetary );
        registerField( "crsXY",        mXY );
    }

    int nextId() const override
    {
        if ( mPlanetary->isChecked() ) return PageIau;
        if ( mXY->isChecked() )        return PageLocationName;
        return PageEpsg;
    }

  private:
    QRadioButton *mEarth     = nullptr;
    QRadioButton *mPlanetary = nullptr;
    QRadioButton *mXY        = nullptr;
};

// ============================================================
// Page 2: EpsgPage — search EPSG via proj.db
// ============================================================
class EpsgPage : public QWizardPage
{
    Q_OBJECT

  public:
    explicit EpsgPage( QWidget *parent = nullptr ) : QWizardPage( parent )
    {
        setTitle( "Earth CRS — EPSG" );
        setSubTitle( "Search by code or name. Select one CRS." );

        QVBoxLayout *lay = new QVBoxLayout( this );

        mSearch = new QLineEdit( this );
        mSearch->setPlaceholderText( "Search by EPSG code or name (e.g. '4326', 'UTM 32N')" );
        mSearch->setClearButtonEnabled( true );
        lay->addWidget( mSearch );

        mTable = new QTableView( this );
        mTable->setSelectionBehavior( QAbstractItemView::SelectRows );
        mTable->setSelectionMode( QAbstractItemView::SingleSelection );
        mTable->setEditTriggers( QAbstractItemView::NoEditTriggers );
        mTable->setSortingEnabled( true );
        mTable->horizontalHeader()->setStretchLastSection( true );
        mTable->verticalHeader()->setVisible( false );
        mTable->setAlternatingRowColors( true );
        lay->addWidget( mTable, 1 );

        mSelectedLabel = new QLabel( "No CRS selected", this );
        mSelectedLabel->setStyleSheet( "font-style: italic; color: #555;" );
        lay->addWidget( mSelectedLabel );

        // Hidden QLineEdit fields store the selected SRID/label strings.
        // registerField works natively with QLineEdit::text.
        mSridField  = new QLineEdit( this );
        mLabelField = new QLineEdit( this );
        mSridField->hide();
        mLabelField->hide();
        registerField( "epsgSrid*", mSridField );
        registerField( "epsgLabel", mLabelField );

        QTimer::singleShot( 0, this, &EpsgPage::loadModel );
    }

    bool isComplete() const override { return !mSridField->text().isEmpty(); }

  private slots:
    void loadModel()
    {
        mModel = new GrassCrsModel( GrassCrsModel::EPSG, this );
        if ( !mModel->open() )
        {
            mSelectedLabel->setText( "Could not open proj.db — check PROJ installation." );
            return;
        }

        mProxy = new QSortFilterProxyModel( this );
        mProxy->setSourceModel( mModel );
        mTable->setModel( mProxy );
        mTable->hideColumn( 2 );
        mTable->setColumnWidth( 0, 120 );

        connect( mSearch, &QLineEdit::textChanged,
                 mModel, &GrassCrsModel::setFilter );

        connect( mTable->selectionModel(), &QItemSelectionModel::currentRowChanged,
                 this, [this]( const QModelIndex &cur, const QModelIndex & ) {
                     const QModelIndex src = mProxy->mapToSource( cur );
                     if ( !src.isValid() ) return;
                     const CrsEntry e = mModel->entryAt( src.row() );
                     const QString srid = e.authName + ":" + e.code;
                     mSridField->setText( srid );
                     mLabelField->setText( e.name );
                     mSelectedLabel->setText( srid + " — " + e.name );
                     emit completeChanged();
                 } );

        mModel->setFilter( "WGS 84" );
    }

  private:
    GrassCrsModel         *mModel  = nullptr;
    QSortFilterProxyModel *mProxy  = nullptr;
    QLineEdit  *mSearch            = nullptr;
    QTableView *mTable             = nullptr;
    QLabel     *mSelectedLabel     = nullptr;
    QLineEdit  *mSridField         = nullptr;
    QLineEdit  *mLabelField        = nullptr;
};

// ============================================================
// Page 3: IauPage — planetary bodies (IAU_2015 from proj.db)
// ============================================================
class IauPage : public QWizardPage
{
    Q_OBJECT

  public:
    explicit IauPage( QWidget *parent = nullptr ) : QWizardPage( parent )
    {
        setTitle( "Planetary Body — IAU 2015" );
        setSubTitle( "Select a body and CRS variant (97 bodies, 2201 CRS total)." );

        QVBoxLayout *lay = new QVBoxLayout( this );

        QHBoxLayout *bodyRow = new QHBoxLayout();
        bodyRow->addWidget( new QLabel( "Filter:", this ) );
        mBodySearch = new QLineEdit( this );
        mBodySearch->setPlaceholderText( "e.g. Mars, Moon, Titan, Enceladus…" );
        mBodySearch->setClearButtonEnabled( true );
        bodyRow->addWidget( mBodySearch, 1 );
        lay->addLayout( bodyRow );

        mBodyTable = new QTableView( this );
        mBodyTable->setSelectionBehavior( QAbstractItemView::SelectRows );
        mBodyTable->setSelectionMode( QAbstractItemView::SingleSelection );
        mBodyTable->setEditTriggers( QAbstractItemView::NoEditTriggers );
        mBodyTable->horizontalHeader()->setStretchLastSection( true );
        mBodyTable->verticalHeader()->setVisible( false );
        mBodyTable->setAlternatingRowColors( true );
        lay->addWidget( mBodyTable, 2 );

        mSelectedLabel = new QLabel( "No body selected", this );
        mSelectedLabel->setStyleSheet( "font-style: italic; color: #555;" );
        lay->addWidget( mSelectedLabel );

        mSridField  = new QLineEdit( this );
        mLabelField = new QLineEdit( this );
        mSridField->hide();
        mLabelField->hide();
        registerField( "iauSrid*", mSridField );
        registerField( "iauLabel", mLabelField );

        QTimer::singleShot( 0, this, &IauPage::loadModel );
    }

    bool isComplete() const override { return !mSridField->text().isEmpty(); }
    int  nextId()     const override { return PageLocationName; }

  private slots:
    void loadModel()
    {
        mModel = new GrassCrsModel( GrassCrsModel::IAU, this );
        if ( !mModel->open() )
        {
            mSelectedLabel->setText( "Could not open proj.db." );
            return;
        }

        mProxy = new QSortFilterProxyModel( this );
        mProxy->setSourceModel( mModel );
        mBodyTable->setModel( mProxy );
        mBodyTable->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
        mBodyTable->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );

        connect( mBodySearch, &QLineEdit::textChanged,
                 mModel, &GrassCrsModel::setFilter );

        connect( mBodyTable->selectionModel(), &QItemSelectionModel::currentRowChanged,
                 this, [this]( const QModelIndex &cur, const QModelIndex & ) {
                     const QModelIndex src = mProxy->mapToSource( cur );
                     if ( !src.isValid() ) return;
                     const CrsEntry e = mModel->entryAt( src.row() );
                     const QString srid = e.authName + ":" + e.code;
                     mSridField->setText( srid );
                     mLabelField->setText( e.name );
                     mSelectedLabel->setText( srid + " — " + e.name );
                     emit completeChanged();
                 } );

        // Show all bodies on open; user can filter with the search box
        mModel->setFilter( "" );
    }

  private:
    GrassCrsModel         *mModel      = nullptr;
    QSortFilterProxyModel *mProxy      = nullptr;
    QLineEdit  *mBodySearch            = nullptr;
    QTableView *mBodyTable             = nullptr;
    QLabel     *mSelectedLabel         = nullptr;
    QLineEdit  *mSridField             = nullptr;
    QLineEdit  *mLabelField            = nullptr;
};

// ============================================================
// Page 4: LocationNamePage
// ============================================================
class LocationNamePage : public QWizardPage
{
    Q_OBJECT

  public:
    explicit LocationNamePage( QWidget *parent = nullptr ) : QWizardPage( parent )
    {
        setTitle( "Location and Mapset Names" );
        setSubTitle( "Choose names for the new location and its initial mapset." );

        QFormLayout *form = new QFormLayout( this );

        mLocEdit = new QLineEdit( this );
        mLocEdit->setPlaceholderText( "e.g. MyProject, Mars_InSight, NorthAfrica" );
        // GRASS legal filenames: letters, digits, underscore, hyphen; no spaces
        mLocEdit->setValidator(
            new QRegularExpressionValidator(
                QRegularExpression( "[A-Za-z0-9_\\-]+" ), this ) );
        form->addRow( "Location name:", mLocEdit );

        mMsEdit = new QLineEdit( "PERMANENT", this );
        mMsEdit->setValidator(
            new QRegularExpressionValidator(
                QRegularExpression( "[A-Za-z0-9_\\-]+" ), this ) );
        form->addRow( "Initial mapset:", mMsEdit );

        mNote = new QLabel(
            "Names may contain letters, digits, underscore, and hyphen. No spaces.", this );
        mNote->setStyleSheet( "color: #555; font-size: 10px;" );
        form->addRow( mNote );

        registerField( "locationName*", mLocEdit );
        registerField( "mapsetName",    mMsEdit );

        connect( mLocEdit, &QLineEdit::textChanged,
                 this, &QWizardPage::completeChanged );
    }

    bool isComplete() const override
    {
        return !mLocEdit->text().trimmed().isEmpty();
    }

    int nextId() const override { return PageSummary; }

  private:
    QLineEdit *mLocEdit = nullptr;
    QLineEdit *mMsEdit  = nullptr;
    QLabel    *mNote    = nullptr;
};

// ============================================================
// Page 5: SummaryPage
// ============================================================
class SummaryPage : public QWizardPage
{
    Q_OBJECT

  public:
    explicit SummaryPage( QWidget *parent = nullptr ) : QWizardPage( parent )
    {
        setTitle( "Summary" );
        setSubTitle( "Review the settings below, then click 'Create'." );
        setFinalPage( true );
        setButtonText( QWizard::FinishButton, "Create Location" );

        mText = new QTextEdit( this );
        mText->setReadOnly( true );
        mText->setFrameShape( QFrame::StyledPanel );

        QVBoxLayout *lay = new QVBoxLayout( this );
        lay->addWidget( mText );

        mStatus = new QLabel( this );
        mStatus->setWordWrap( true );
        lay->addWidget( mStatus );
    }

    void initializePage() override
    {
        // Determine which SRID was chosen
        const bool earthEpsg     = field( "crsEarth" ).toBool();
        const bool planetary     = field( "crsPlanetary" ).toBool();
        const bool xy            = field( "crsXY" ).toBool();

        QString srid, sridLabel;
        if ( earthEpsg )
        {
            srid      = field( "epsgSrid" ).toString();
            sridLabel = field( "epsgLabel" ).toString();
        }
        else if ( planetary )
        {
            srid      = field( "iauSrid" ).toString();
            sridLabel = field( "iauLabel" ).toString();
        }
        else if ( xy )
        {
            srid      = "XY";
            sridLabel = "XY Cartesian / no CRS";
        }

        const QString gisdbase = field( "gisdbase" ).toString();
        const QString locName  = field( "locationName" ).toString();
        const QString msName   = field( "mapsetName" ).toString();

        mText->setHtml( QString(
            "<table cellpadding='4'>"
            "<tr><td><b>GISDBASE:</b></td><td>%1</td></tr>"
            "<tr><td><b>Location:</b></td><td>%2</td></tr>"
            "<tr><td><b>Mapset:</b></td><td>%3</td></tr>"
            "<tr><td><b>CRS:</b></td><td>%4</td></tr>"
            "<tr><td><b>SRID:</b></td><td>%5</td></tr>"
            "</table>" )
            .arg( gisdbase, locName, msName, sridLabel, srid ) );

        mStatus->clear();
    }

    void setStatus( const QString &msg, bool ok )
    {
        mStatus->setText( msg );
        mStatus->setStyleSheet( ok ? "color: #1a8a1a; font-weight: bold;"
                                   : "color: #c00000; font-weight: bold;" );
    }

    int nextId() const override { return -1; }

  private:
    QTextEdit *mText   = nullptr;
    QLabel    *mStatus = nullptr;
};

// ============================================================
// GrassLocationWizard
// ============================================================
GrassLocationWizard::GrassLocationWizard( GrassEnv *env, QWidget *parent )
    : QWizard( parent )
    , mEnv( env )
{
    setWindowTitle( "Create new GRASS Location" );
    setWizardStyle( QWizard::ModernStyle );
    setMinimumSize( 700, 520 );

    setPage( PageGisdb,        new GisdbPage( this ) );
    setPage( PageCrsChoice,    new CrsChoicePage( this ) );
    setPage( PageEpsg,         new EpsgPage( this ) );
    setPage( PageIau,          new IauPage( this ) );
    setPage( PageLocationName, new LocationNamePage( this ) );
    setPage( PageSummary,      new SummaryPage( this ) );

    setStartId( PageGisdb );
}

QString GrassLocationWizard::selectedSrid() const
{
    if ( field( "crsPlanetary" ).toBool() )
        return field( "iauSrid" ).toString();
    if ( field( "crsXY" ).toBool() )
        return {};
    return field( "epsgSrid" ).toString();
}

QString GrassLocationWizard::selectedSridLabel() const
{
    if ( field( "crsPlanetary" ).toBool() )
        return field( "iauLabel" ).toString();
    if ( field( "crsXY" ).toBool() )
        return "XY Cartesian";
    return field( "epsgLabel" ).toString();
}

QString GrassLocationWizard::gisdbase()    const { return field( "gisdbase" ).toString(); }
QString GrassLocationWizard::locationName() const { return field( "locationName" ).toString(); }
QString GrassLocationWizard::mapsetName()  const { return field( "mapsetName" ).toString(); }

bool GrassLocationWizard::createLocation( QString *errorOut )
{
    const QString gdb    = gisdbase();
    const QString loc    = locationName();
    const QString srid   = selectedSrid();

    // Ensure the GISDBASE directory exists
    QDir().mkpath( gdb );

    // Build the g.proj command
    // g.proj -c project=<name> [srid=<srid>]
    // Needs GISDBASE set in the environment; no running session required.
    const QString gisBase = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
    if ( gisBase.isEmpty() )
    {
        if ( errorOut ) *errorOut = "GISBASE not set";
        return false;
    }

    QStringList args;
    args << "-c" << ( "project=" + loc );
    if ( !srid.isEmpty() )
        args << ( "srid=" + srid );

    // Write a minimal GISRC file so G_no_gisinit() finds the GISDBASE.
    // G_no_gisinit() reads GISRC; without it g.proj refuses to start.
    const QString gisrcPath = QDir::temp().filePath(
        "qtgrass_gisrc_" + QString::number( QCoreApplication::applicationPid() ) );
    {
        QFile gisrc( gisrcPath );
        if ( gisrc.open( QIODevice::WriteOnly | QIODevice::Text ) )
        {
            QTextStream s( &gisrc );
            s << "GISDBASE: " << gdb << "\n"
              << "LOCATION_NAME: DUMMY\n"
              << "MAPSET: PERMANENT\n";
        }
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert( "GISBASE", gisBase );
    env.insert( "GISRC",   gisrcPath );
    env.remove( "GISDBASE" );
    env.remove( "LOCATION_NAME" );
    env.remove( "MAPSET" );

    const QString gproj = gisBase + "/bin/g.proj";

    QProcess proc;
    proc.setProcessEnvironment( env );
    proc.start( gproj, args );

    if ( !proc.waitForStarted( 5000 ) )
    {
        if ( errorOut ) *errorOut = "Failed to start g.proj: " + proc.errorString();
        return false;
    }
    proc.waitForFinished( 30000 );

    const QString out  = proc.readAllStandardOutput();
    const QString err2 = proc.readAllStandardError();
    const int     rc   = proc.exitCode();

    qCDebug(lcStartup) << "g.proj stdout:" << out;
    qCDebug(lcStartup) << "g.proj stderr:" << err2;
    qCDebug(lcStartup) << "g.proj exit:  " << rc;

    QFile::remove( gisrcPath );  // clean up temp GISRC

    if ( rc != 0 )
    {
        if ( errorOut )
            *errorOut = "g.proj failed (exit " + QString::number(rc) + "): " + err2;
        return false;
    }

    // Fix PROJ_INFO: PROJ describes spherical bodies with only "R:" (uniform
    // sphere radius); GRASS also needs "a:" (semi-major axis) and "es:"
    // (eccentricity²).  Query the PROJ 9 library directly for authoritative
    // ellipsoid parameters and rewrite the file when those keys are absent.
    if ( !srid.isEmpty() )
        fixupProjInfo( gdb + "/" + loc + "/PERMANENT/PROJ_INFO", srid );

    // Create the chosen mapset if it differs from PERMANENT
    const QString ms = mapsetName();
    if ( ms != "PERMANENT" && !ms.isEmpty() )
    {
        const QString msPath = gdb + "/" + loc + "/" + ms;
        if ( !QDir().mkpath( msPath ) ||
             !QFile::copy( gdb + "/" + loc + "/PERMANENT/WIND",
                           msPath + "/WIND" ) )
        {
            if ( errorOut ) *errorOut = "Created location but could not create mapset " + ms;
            // Non-fatal — use PERMANENT
        }
    }

    // Switch the environment to the new location
    if ( mEnv )
        mEnv->openMapset( gdb, loc, ms.isEmpty() ? "PERMANENT" : ms );

    return true;
}

#include "grasslocationwizard.moc"
