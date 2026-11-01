#include "grasspreferencesdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRegularExpression>
#include <QSettings>
#include <QVBoxLayout>

#include <proj.h>

// ───────────────────────────────────────────────────────────────────────────────
// Provider catalogue — grouped by planetary body.
// Add new providers here, no other code changes needed.
// ───────────────────────────────────────────────────────────────────────────────

static const QList<TileProvider> kProviders = {
    // ── Earth ────────────────────────────────────────────────────────────────
    { "google_satellite",
      "Google Satellite",
      "https://mt1.google.com/vt/lyrs=s&x={x}&y={y}&z={z}",
      "© Google", PlanetaryBody::Earth },
    { "google_maps",
      "Google Maps",
      "https://mt1.google.com/vt/lyrs=m&x={x}&y={y}&z={z}",
      "© Google", PlanetaryBody::Earth },
    { "google_terrain",
      "Google Terrain",
      "https://mt1.google.com/vt/lyrs=p&x={x}&y={y}&z={z}",
      "© Google", PlanetaryBody::Earth },
    { "osm",
      "OpenStreetMap",
      "https://tile.openstreetmap.org/{z}/{x}/{y}.png",
      "© OpenStreetMap contributors", PlanetaryBody::Earth },
    { "esri_imagery",
      "Esri World Imagery",
      "https://server.arcgisonline.com/arcgis/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}",
      "© Esri, Maxar, GeoEye", PlanetaryBody::Earth },
    { "esri_topo",
      "Esri World Topo",
      "https://server.arcgisonline.com/arcgis/rest/services/World_Topo_Map/MapServer/tile/{z}/{y}/{x}",
      "© Esri", PlanetaryBody::Earth },
    { "carto_light",
      "Carto Light",
      "https://a.basemaps.cartocdn.com/light_all/{z}/{x}/{y}.png",
      "© Carto, © OpenStreetMap contributors", PlanetaryBody::Earth },
    { "carto_dark",
      "Carto Dark",
      "https://a.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}.png",
      "© Carto, © OpenStreetMap contributors", PlanetaryBody::Earth },

    // ── Moon (OpenPlanetaryMap) ──────────────────────────────────────────────
    { "opm-moon-basemap-v0-1",
      "OPM Moon basemap v0.1",
      "https://cartocdn-gusc.global.ssl.fastly.net/opmbuilder/api/v1/map/named/opm-moon-basemap-v0-1/all/{z}/{x}/{y}.png",
      "© OpenPlanetary / NASA / LRO", PlanetaryBody::Moon },
    { "opm-moon-basemap-v0-2",
      "OPM Moon basemap v0.2",
      "https://cartocdn-gusc.global.ssl.fastly.net/opmbuilder/api/v1/map/named/opm-mars-basemap-v0-2/all/{z}/{x}/{y}.png",
      "© OpenPlanetary / NASA / LRO", PlanetaryBody::Moon },
    { "opm-moon-hillshaded-albedo",
      "OPM Moon hillshaded albedo",
      "https://s3.amazonaws.com/opmbuilder/301_moon/tiles/w/hillshaded-albedo/{z}/{x}/{y}.png",
      "© OpenPlanetary / NASA / LRO", PlanetaryBody::Moon },

    // ── Mars (OpenPlanetaryMap) ──────────────────────────────────────────────
    { "opm-mars-viking-mdim21-global",
      "OPM Mars Viking MDIM 2.1 global",
      "http://s3-eu-west-1.amazonaws.com/whereonmars.cartodb.net/viking_mdim21_global/{z}/{x}/{y}.png",
      "© NASA Viking MDIM 2.1", PlanetaryBody::Mars },
    { "opm-mars-celestia-shaded",
      "OPM Mars Celestia shaded (16k)",
      "http://s3-eu-west-1.amazonaws.com/whereonmars.cartodb.net/celestia_mars-shaded-16k_global/{z}/{x}/{y}.png",
      "© Celestia / OpenPlanetary", PlanetaryBody::Mars },
    { "opm-mars-mola-grey",
      "OPM Mars MOLA (grey)",
      "http://s3-eu-west-1.amazonaws.com/whereonmars.cartodb.net/mola-gray/{z}/{x}/{y}.png",
      "© NASA MOLA / OpenPlanetary", PlanetaryBody::Mars },
    { "opm-mars-mola-colour",
      "OPM Mars MOLA (colour)",
      "http://s3-eu-west-1.amazonaws.com/whereonmars.cartodb.net/mola-color/{z}/{x}/{y}.png",
      "© NASA MOLA / OpenPlanetary", PlanetaryBody::Mars },
    { "opm-mars-mola-hillshade",
      "OPM Mars MOLA hillshade",
      "https://s3.us-east-2.amazonaws.com/opmmarstiles/hillshade-tiles/{z}/{x}/{y}.png",
      "© NASA MOLA / OpenPlanetary", PlanetaryBody::Mars },
    { "opm-mars-mola-color-noshade",
      "OPM Mars MOLA colour (no shade)",
      "http://s3-eu-west-1.amazonaws.com/whereonmars.cartodb.net/mola_color-noshade_global/{z}/{x}/{y}.png",
      "© NASA MOLA / OpenPlanetary", PlanetaryBody::Mars },
};

// ───────────────────────────────────────────────────────────────────────────────
const QList<TileProvider> &GrassPreferencesDialog::providers()
{
    return kProviders;
}

QString GrassPreferencesDialog::bodyName( PlanetaryBody b )
{
    switch ( b ) {
        case PlanetaryBody::Earth:   return "Earth";
        case PlanetaryBody::Moon:    return "Moon";
        case PlanetaryBody::Mars:    return "Mars";
        case PlanetaryBody::Other:   return "Other planetary body";
        case PlanetaryBody::Unknown: return "Unknown";
    }
    return "Unknown";
}

// ───────────────────────────────────────────────────────────────────────────────
// detectBody — identify the planetary body for the active GRASS session.
//
// Detection priority:
//   1. PROJ C API: read PERMANENT/PROJ_SRID (or PROJ_WKT), query the PROJ
//      library for the authoritative CRS name and ellipsoid, check keywords.
//   2. Keyword scan of PERMANENT/PROJ_INFO + PROJ_WKT text (catches legacy
//      locations created without a PROJ_SRID file).
//   3. Ellipsoid semi-major axis fingerprint from the "a:" key in PROJ_INFO.
// ───────────────────────────────────────────────────────────────────────────────
PlanetaryBody GrassPreferencesDialog::detectBody()
{
    const QString gisdb = QString::fromLocal8Bit( qgetenv( "GISDBASE" ) );
    const QString loc   = QString::fromLocal8Bit( qgetenv( "LOCATION_NAME" ) );
    if ( gisdb.isEmpty() || loc.isEmpty() )
        return PlanetaryBody::Earth;

    const QString permDir   = gisdb + "/" + loc + "/PERMANENT";
    const QString sridPath  = permDir + "/PROJ_SRID";
    const QString wktPath   = permDir + "/PROJ_WKT";
    const QString infoPath  = permDir + "/PROJ_INFO";

    // ── Helper: keyword → body ──────────────────────────────────────────────
    auto bodyFromKeywords = []( const QString &text ) -> PlanetaryBody {
        const QString t = text.toLower();
        if ( t.contains("moon")    ) return PlanetaryBody::Moon;
        if ( t.contains("mars")    ) return PlanetaryBody::Mars;
        if ( t.contains("earth")   ) return PlanetaryBody::Earth;
        return PlanetaryBody::Unknown;   // not matched — keep trying
    };

    // ── 1. PROJ C API via PROJ_SRID or PROJ_WKT ────────────────────────────
    {
        PJ_CONTEXT *ctx = proj_context_create();
        PJ *crs = nullptr;

        // Try PROJ_SRID first (authoritative, written by g.proj and fixupProjInfo)
        {
            QFile sf( sridPath );
            if ( sf.open( QIODevice::ReadOnly | QIODevice::Text ) )
            {
                const QString srid = sf.readLine().trimmed();
                if ( !srid.isEmpty() )
                {
                    crs = proj_create( ctx, srid.toUtf8().constData() );
                    if ( !crs )
                    {
                        const int colon = srid.indexOf(':');
                        if ( colon > 0 )
                            crs = proj_create_from_database(
                                ctx,
                                srid.left(colon).toUtf8().constData(),
                                srid.mid(colon + 1).toUtf8().constData(),
                                PJ_CATEGORY_CRS, 0, nullptr );
                    }
                }
            }
        }

        // Fall back to PROJ_WKT
        if ( !crs )
        {
            QFile wf( wktPath );
            if ( wf.open( QIODevice::ReadOnly | QIODevice::Text ) )
            {
                const QByteArray wkt = wf.readAll();
                crs = proj_create_from_wkt( ctx, wkt.constData(),
                                            nullptr, nullptr, nullptr );
            }
        }

        if ( crs )
        {
            // Get CRS name and ellipsoid name from PROJ
            const char *crsName = proj_get_name( crs );
            const QString crsStr = crsName ? QString::fromUtf8(crsName) : QString();

            PlanetaryBody b = bodyFromKeywords( crsStr );

            if ( b == PlanetaryBody::Unknown )
            {
                // Try ellipsoid name too
                PJ *ell = proj_get_ellipsoid( ctx, crs );
                if ( ell )
                {
                    const char *ellName = proj_get_name( ell );
                    if ( ellName )
                        b = bodyFromKeywords( QString::fromUtf8(ellName) );

                    // Last resort: semi-major axis fingerprint via PROJ
                    if ( b == PlanetaryBody::Unknown )
                    {
                        double a = 0.0, bv = 0.0, invF = 0.0;
                        int smc = 0;
                        if ( proj_ellipsoid_get_parameters( ctx, ell, &a, &bv, &smc, &invF )
                             && a > 0.0 )
                        {
                            if ( a > 6.3e6 && a < 6.5e6 ) b = PlanetaryBody::Earth;
                            else if ( a > 1.7e6 && a < 1.8e6 ) b = PlanetaryBody::Moon;
                            else if ( a > 3.3e6 && a < 3.5e6 ) b = PlanetaryBody::Mars;
                            else b = PlanetaryBody::Other;
                        }
                    }
                    proj_destroy( ell );
                }
            }

            proj_destroy( crs );
            proj_context_destroy( ctx );

            if ( b != PlanetaryBody::Unknown )
                return b;
        }
        else
        {
            proj_context_destroy( ctx );
        }
    }

    // ── 2. Keyword scan of PROJ_INFO + PROJ_WKT text ───────────────────────
    for ( const QString &path : { infoPath, wktPath } )
    {
        QFile f( path );
        if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) ) continue;
        const PlanetaryBody b = bodyFromKeywords( QString::fromUtf8( f.readAll() ) );
        if ( b != PlanetaryBody::Unknown ) return b;
    }

    // ── 3. Ellipsoid semi-major axis from PROJ_INFO "a:" key ───────────────
    {
        QFile f( infoPath );
        if ( f.open( QIODevice::ReadOnly | QIODevice::Text ) )
        {
            const QString content = QString::fromUtf8( f.readAll() );
            static const QRegularExpression rx( "^\\s*a:\\s*([0-9.]+)",
                                                QRegularExpression::MultilineOption );
            const auto m = rx.match( content );
            if ( m.hasMatch() )
            {
                const double a = m.captured(1).toDouble();
                if ( a > 6.3e6 && a < 6.5e6 ) return PlanetaryBody::Earth;
                if ( a > 1.7e6 && a < 1.8e6 ) return PlanetaryBody::Moon;
                if ( a > 3.3e6 && a < 3.5e6 ) return PlanetaryBody::Mars;
                return PlanetaryBody::Other;
            }
        }
    }

    return PlanetaryBody::Earth;  // safe fallback when no CRS info at all
}

// ───────────────────────────────────────────────────────────────────────────────
void GrassPreferencesDialog::loadSettings( bool &enabled, int &providerIndex )
{
    QSettings s;
    enabled       = s.value( "tiles/enabled",  false ).toBool();
    const QString id = s.value( "tiles/provider", "google_satellite" ).toString();
    providerIndex = 0;
    for ( int i = 0; i < kProviders.size(); ++i )
        if ( kProviders[i].id == id ) { providerIndex = i; break; }
}

// ───────────────────────────────────────────────────────────────────────────────
GrassPreferencesDialog::GrassPreferencesDialog( QWidget *parent )
    : QDialog( parent )
{
    setWindowTitle( "qtgrass — Preferences" );
    setMinimumWidth( 460 );

    mBody = detectBody();

    QVBoxLayout *lay = new QVBoxLayout( this );

    // ── Basemap group ────────────────────────────────────────────────────
    auto *grp    = new QGroupBox( "Basemap tile background", this );
    auto *grpLay = new QFormLayout( grp );

    // Detected planetary body label
    mBodyLabel = new QLabel( grp );
    QString icon = ( mBody == PlanetaryBody::Earth ) ? "🌍"
                 : ( mBody == PlanetaryBody::Moon  ) ? "🌕"
                 : ( mBody == PlanetaryBody::Mars  ) ? "🔴"
                                                     : "❓";
    mBodyLabel->setText( QString( "<b>%1 Planetary body:</b> %2 "
                                   "(auto-detected from CRS)" )
                          .arg( icon, bodyName( mBody ) ) );
    grpLay->addRow( mBodyLabel );

    mEnableCheck = new QCheckBox( "Enable basemap background layer", grp );
    grpLay->addRow( mEnableCheck );

    mProviderCombo = new QComboBox( grp );
    grpLay->addRow( "Provider:", mProviderCombo );

    QString noteText;
    switch ( mBody ) {
        case PlanetaryBody::Earth:
            noteText =
                "Basemap tiles are fetched live from the provider's servers "
                "(Google, OpenStreetMap, Esri, Carto…).<br>"
                "Cached in <code>~/.cache/qtgrass/tiles/</code>.";
            break;
        case PlanetaryBody::Moon:
            noteText =
                "Lunar tiles served by the OpenPlanetaryMap project "
                "(OPM, NASA LRO data).<br>"
                "Cached in <code>~/.cache/qtgrass/tiles/</code>.";
            break;
        case PlanetaryBody::Mars:
            noteText =
                "Martian tiles served by the OpenPlanetaryMap project "
                "(OPM, NASA MOLA / Viking MDIM data).<br>"
                "Cached in <code>~/.cache/qtgrass/tiles/</code>.";
            break;
        case PlanetaryBody::Other:
            noteText =
                "<b>No tile provider available for this planetary body.</b><br>"
                "The CRS does not match Earth, Moon or Mars.  The basemap "
                "layer will remain disabled.";
            break;
    }
    auto *note = new QLabel( "<small>" + noteText + "</small>", grp );
    note->setWordWrap( true );
    grpLay->addRow( note );

    lay->addWidget( grp );

    // ── Buttons ──────────────────────────────────────────────────────────
    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this );
    lay->addWidget( btns );

    connect( btns, &QDialogButtonBox::accepted, this, &GrassPreferencesDialog::apply );
    connect( btns, &QDialogButtonBox::rejected, this, &QDialog::reject );

    // Populate combo with only the providers matching the detected body
    rebuildProviderCombo();

    // Restore saved state
    bool enabled; int idx;
    loadSettings( enabled, idx );

    // Disable enable-checkbox entirely when no provider is available for this body.
    if ( mProviderCombo->count() == 0 ) {
        mEnableCheck->setChecked( false );
        mEnableCheck->setEnabled( false );
        mProviderCombo->setEnabled( false );
    } else {
        mEnableCheck->setChecked( enabled );
        // Try to restore the saved provider; if it doesn't belong to the current
        // body, fall back to the first one in the filtered list.
        const QString savedId = ( idx >= 0 && idx < kProviders.size() )
                                ? kProviders[idx].id : QString();
        int matchIdx = -1;
        for ( int i = 0; i < mProviderCombo->count(); ++i )
            if ( mProviderCombo->itemData( i ).toString() == savedId )
                { matchIdx = i; break; }
        mProviderCombo->setCurrentIndex( matchIdx >= 0 ? matchIdx : 0 );
    }
}

// ───────────────────────────────────────────────────────────────────────────────
// rebuildProviderCombo — filter the provider list by the detected body.
// ───────────────────────────────────────────────────────────────────────────────
void GrassPreferencesDialog::rebuildProviderCombo()
{
    mProviderCombo->clear();
    for ( const TileProvider &p : kProviders ) {
        if ( p.body != mBody ) continue;
        mProviderCombo->addItem( p.name, p.id );
    }
}

// ───────────────────────────────────────────────────────────────────────────────
void GrassPreferencesDialog::apply()
{
    const bool enabled = mEnableCheck->isChecked() && mProviderCombo->count() > 0;
    const QString id   = mProviderCombo->currentData().toString();

    // Find the provider by id from the master catalogue.
    int catalogueIdx = -1;
    for ( int i = 0; i < kProviders.size(); ++i )
        if ( kProviders[i].id == id ) { catalogueIdx = i; break; }
    if ( catalogueIdx < 0 ) catalogueIdx = 0;

    QSettings s;
    s.setValue( "tiles/enabled",  enabled );
    s.setValue( "tiles/provider", kProviders[catalogueIdx].id );

    emit tileSettingsChanged( enabled, kProviders[catalogueIdx] );
    accept();
}
