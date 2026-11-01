#include "grassdatacat.h"
#include "../canvas/grassmapcanvas.h"
#include "../canvas/grasslayerpanel.h"
#include "../grassenv.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QTextStream>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcCatalog, "qtgrass.catalog")

// ── Constants ─────────────────────────────────────────────────────────────────

static constexpr int kMinGroupSize = 3;   // sequences shorter than this stay flat

// ── Map type section descriptors ──────────────────────────────────────────────

struct MapSection { QString type; QString label; };
static const QList<MapSection> kSections = {
    { "raster",    "Rasters"    },
    { "vector",    "Vectors"    },
    { "raster_3d", "3D Rasters" },
    { "region",    "Regions"    },
};

// ═══════════════════════════════════════════════════════════════════════════════
// Group detection helpers
// ═══════════════════════════════════════════════════════════════════════════════

// Sanitize a detected base name into a valid GRASS element name.
// GRASS names must be alphanumeric + underscore; dots/dashes become underscores,
// and trailing/leading underscores are stripped.
static QString sanitizeGroupName( const QString &raw )
{
    QString s = raw;
    s.replace( QRegularExpression( "[^a-zA-Z0-9_]+" ), "_" );
    s.remove(  QRegularExpression( "^_+|_+$" ) );
    return s;
}

// Detect sequential-number patterns in a flat map list.
// Recognises both separator forms and bare numeric suffixes:
//   • "band_001"   → sep '_',  base "band"
//   • "Hd2f4.100"  → sep '.',  base "Hd2f4"
//   • "scene-05"   → sep '-',  base "scene"
//   • "image3"     → no sep,   base "image"
//
// Returns a map: sanitizedBase → member list ("name@mapset").
// Only groups with ≥ kMinGroupSize members are returned.
static QMap<QString, QStringList> detectSequences( const QStringList &maps )
{
    // Capture: (any chars, minimal) + optional single separator [._-] + trailing digits
    static const QRegularExpression rx( "^(.*?)[._\\-]?(\\d+)$" );

    QMap<QString, QStringList> byBase;

    for ( const QString &ref : maps )
    {
        const int at = ref.indexOf( '@' );
        const QString name = ( at < 0 ) ? ref : ref.left( at );

        const auto m = rx.match( name );
        if ( !m.hasMatch() ) continue;

        const QString raw = m.captured( 1 );   // base before the number
        if ( raw.isEmpty() ) continue;

        const QString base = sanitizeGroupName( raw );
        if ( base.isEmpty() ) continue;

        byBase[base] << ref;
    }

    // Keep only groups large enough
    QMap<QString, QStringList> result;
    for ( auto it = byBase.cbegin(); it != byBase.cend(); ++it )
        if ( it.value().size() >= kMinGroupSize )
            result.insert( it.key(), it.value() );

    return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// GRASS group persistence (direct filesystem write — no subprocess)
// ═══════════════════════════════════════════════════════════════════════════════

// Path: $GISDBASE/$LOCATION/$MAPSET/group/<name>/subgroups/default/REF
static QString refFilePath( const QString &gisdb, const QString &loc,
                             const QString &mapset, const QString &grpName )
{
    return gisdb + "/" + loc + "/" + mapset +
           "/group/" + grpName + "/subgroups/default/REF";
}

// Write (or overwrite) the REF file for a GRASS imagery group.
// Creates all intermediate directories.  Does NOT call i.group — avoids the
// OS argument-length limit for large groups (2400+ maps).
static bool writeGroupRef( const QString &gisdb, const QString &loc,
                            const QString &mapset, const QString &grpName,
                            const QStringList &members )
{
    const QString path = refFilePath( gisdb, loc, mapset, grpName );
    QDir().mkpath( QFileInfo( path ).absolutePath() );

    QFile f( path );
    if ( !f.open( QIODevice::WriteOnly | QIODevice::Text ) )
    {
        qCWarning(lcCatalog) << "Cannot write group REF:" << path;
        return false;
    }
    QTextStream ts( &f );
    for ( const QString &m : members )
        ts << m << "\n";

    qCInfo(lcCatalog) << "Registered group" << grpName
                      << "with" << members.size() << "members";
    return true;
}

// Read members of an existing GRASS imagery group from its REF file.
static QStringList readGroupRef( const QString &gisdb, const QString &loc,
                                  const QString &mapset, const QString &grpName )
{
    QStringList members;
    QFile f( refFilePath( gisdb, loc, mapset, grpName ) );
    if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) ) return members;
    QTextStream ts( &f );
    while ( !ts.atEnd() ) {
        const QString line = ts.readLine().trimmed();
        if ( !line.isEmpty() ) members << line;
    }
    return members;
}

// Scan all mapsets in the location for existing GRASS imagery groups.
// Returns: groupName@mapset → sorted member list.
static QMap<QString, QStringList> readAllGroups( const QString &gisdb,
                                                   const QString &loc )
{
    QMap<QString, QStringList> result;
    const QDir locDir( gisdb + "/" + loc );
    if ( !locDir.exists() ) return result;

    for ( const QString &mapset :
          locDir.entryList( QDir::Dirs | QDir::NoDotAndDotDot ) )
    {
        const QDir grpDir( gisdb + "/" + loc + "/" + mapset + "/group" );
        if ( !grpDir.exists() ) continue;
        for ( const QString &grp :
              grpDir.entryList( QDir::Dirs | QDir::NoDotAndDotDot ) )
        {
            const QStringList members = readGroupRef( gisdb, loc, mapset, grp );
            if ( !members.isEmpty() )
                result[grp + "@" + mapset] = members;
        }
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// GrassDataCatalog
// ═══════════════════════════════════════════════════════════════════════════════

GrassDataCatalog::GrassDataCatalog( GrassMapCanvas  *canvas,
                                     GrassLayerPanel *panel,
                                     QWidget *parent )
    : QWidget( parent )
    , mCanvas( canvas )
    , mPanel( panel )
{
    auto *lay = new QVBoxLayout( this );
    lay->setContentsMargins( 0, 0, 0, 0 );
    lay->setSpacing( 2 );

    auto *topRow = new QHBoxLayout();
    mFilter = new QLineEdit( this );
    mFilter->setPlaceholderText( "Filter…" );
    mFilter->setClearButtonEnabled( true );
    topRow->addWidget( mFilter, 1 );
    auto *refreshBtn = new QPushButton( "⟳", this );
    refreshBtn->setFixedWidth( 28 );
    refreshBtn->setToolTip( "Refresh catalog" );
    topRow->addWidget( refreshBtn );
    lay->addLayout( topRow );

    mTree = new QTreeWidget( this );
    mTree->setHeaderHidden( true );
    mTree->setUniformRowHeights( false );
    mTree->setAnimated( true );
    mTree->setEditTriggers( QAbstractItemView::NoEditTriggers );
    mTree->setToolTip( "Double-click a raster or vector to add it to the canvas" );
    lay->addWidget( mTree, 1 );

    connect( refreshBtn, &QPushButton::clicked, this, &GrassDataCatalog::refresh );
    connect( mFilter,    &QLineEdit::textChanged,
             this,        &GrassDataCatalog::onFilterChanged );
    connect( mTree,      &QTreeWidget::itemDoubleClicked,
             this,        &GrassDataCatalog::onItemDoubleClicked );

    refresh();
}

// ---------------------------------------------------------------------------
// listMaps — scan GRASS database directories (no subprocess)
//
// Respects the GRASS mapset access model:
//   • active mapset == PERMANENT → show PERMANENT only
//   • active mapset != PERMANENT → show <active mapset> + PERMANENT (read-only)
// ---------------------------------------------------------------------------
QStringList GrassDataCatalog::listMaps( const QString &type ) const
{
    const QString gisdb  = QString::fromLocal8Bit( qgetenv( "GISDBASE" ) );
    const QString loc    = QString::fromLocal8Bit( qgetenv( "LOCATION_NAME" ) );
    const QString active = QString::fromLocal8Bit( qgetenv( "MAPSET" ) );
    if ( gisdb.isEmpty() || loc.isEmpty() ) return {};

    QString subdir; bool isDir = false;
    if      ( type == "raster"    ) { subdir = "cellhd";  isDir = false; }
    else if ( type == "vector"    ) { subdir = "vector";   isDir = true;  }
    else if ( type == "raster_3d" ) { subdir = "grid3";    isDir = true;  }
    else if ( type == "region"    ) { subdir = "windows";  isDir = false; }
    else return {};

    // Build the list of accessible mapsets per the GRASS access model.
    QStringList accessible;
    if ( !active.isEmpty() ) accessible << active;
    if ( active != "PERMANENT" ) accessible << "PERMANENT";

    QStringList result;
    for ( const QString &mapset : accessible )
    {
        const QDir mDir( gisdb + "/" + loc + "/" + mapset + "/" + subdir );
        if ( !mDir.exists() ) continue;
        const auto f = isDir
            ? ( QDir::Dirs  | QDir::NoDotAndDotDot )
            : ( QDir::Files | QDir::NoDotAndDotDot );
        for ( const QString &name : mDir.entryList( f ) )
            result << name + "@" + mapset;
    }
    result.sort( Qt::CaseInsensitive );
    return result;
}

// ---------------------------------------------------------------------------
// Decorate a leaf map item to reflect the GRASS access model.
// If the map lives in PERMANENT AND the active mapset is not PERMANENT,
// mark it read-only: dimmed text, "🔒" prefix, and a UserRole+4 = true flag
// that downstream context menus can check.
// ---------------------------------------------------------------------------
static void applyAccessDecoration( QTreeWidgetItem *child, const QString &ref,
                                    const QString &activeMapset, const QString &type )
{
    const int at = ref.indexOf( '@' );
    const QString mapset = at < 0 ? QString() : ref.mid( at + 1 );
    const bool readOnly = ( mapset == "PERMANENT" && activeMapset != "PERMANENT" );

    child->setData( 0, Qt::UserRole + 4, readOnly );

    if ( readOnly )
    {
        child->setText( 0, "🔒 " + ref );
        child->setForeground( 0, QColor( "#888" ) );
        child->setToolTip( 0, QString( "%1 (read-only — lives in PERMANENT; "
                                        "active mapset is %2)" )
                                .arg( type, activeMapset ) );
    }
    else
    {
        child->setText( 0, ref );
        child->setToolTip( 0, type + ": " + ref + "  (read+write)" );
    }
}

// ---------------------------------------------------------------------------
// populate — fill one section, with auto-grouping for rasters
// ---------------------------------------------------------------------------
void GrassDataCatalog::populate( const QString &type, QTreeWidgetItem *section )
{
    section->takeChildren();
    const QStringList allMaps = listMaps( type );
    const QString activeMapset =
        QString::fromLocal8Bit( qgetenv( "MAPSET" ) );

    const QString baseLabel = section->data( 0, Qt::UserRole + 2 ).toString();
    section->setText( 0, QString( "%1  (%2)" ).arg( baseLabel ).arg( allMaps.size() ) );

    // Non-raster types: flat list
    if ( type != "raster" )
    {
        for ( const QString &ref : allMaps )
        {
            auto *child = new QTreeWidgetItem( section, { ref } );
            child->setData( 0, Qt::UserRole,     type );
            child->setData( 0, Qt::UserRole + 1, ref  );
            applyAccessDecoration( child, ref, activeMapset, type );
        }
        return;
    }

    // ── Raster path ───────────────────────────────────────────────────────────

    const QString gisdb  = QString::fromLocal8Bit( qgetenv( "GISDBASE"       ) );
    const QString loc    = QString::fromLocal8Bit( qgetenv( "LOCATION_NAME"  ) );
    const QString mapset = QString::fromLocal8Bit( qgetenv( "MAPSET"         ) );

    // 1. Read groups that already exist on disk
    QMap<QString, QStringList> groups = readAllGroups( gisdb, loc );

    // Build set of already-grouped maps
    QSet<QString> grouped;
    for ( const auto &members : groups )
        for ( const QString &m : members )
            grouped.insert( m );

    // 2. Detect sequences among ungrouped rasters
    QStringList ungrouped;
    for ( const QString &m : allMaps )
        if ( !grouped.contains( m ) ) ungrouped << m;

    const QMap<QString, QStringList> detected = detectSequences( ungrouped );

    // 3. Persist newly detected groups directly as REF files (no subprocess)
    for ( auto it = detected.cbegin(); it != detected.cend(); ++it )
    {
        const QString &grpName = it.key();
        const QStringList &members = it.value();

        // Skip if a group with this sanitized name already exists
        const QString grpRef = grpName + "@" + mapset;
        if ( groups.contains( grpRef ) ) continue;

        if ( writeGroupRef( gisdb, loc, mapset, grpName, members ) )
        {
            groups[grpRef] = members;
            for ( const QString &m : members ) grouped.insert( m );
        }
    }

    // 4. Also display detected-but-not-yet-persisted groups (so UI is never blank)
    //    (covers the case where writeGroupRef() fails on a read-only mapset)
    QMap<QString, QStringList> displayGroups = groups;
    for ( auto it = detected.cbegin(); it != detected.cend(); ++it )
    {
        const QString grpRef = it.key() + "@" + mapset;
        if ( !displayGroups.contains( grpRef ) )
        {
            displayGroups[grpRef] = it.value();
            for ( const QString &m : it.value() ) grouped.insert( m );
        }
    }

    // 5. Build tree: group nodes (collapsed) then standalone maps
    for ( auto it = displayGroups.cbegin(); it != displayGroups.cend(); ++it )
    {
        const QString grpRef  = it.key();
        const QString grpName = grpRef.left( grpRef.indexOf( '@' ) );
        const QStringList &members = it.value();

        // Only show groups whose members intersect this raster catalog
        bool relevant = false;
        for ( const QString &m : members )
            if ( allMaps.contains( m ) ) { relevant = true; break; }
        if ( !relevant ) continue;

        // Count only members that are in the current catalog
        int count = 0;
        for ( const QString &m : members )
            if ( allMaps.contains( m ) ) ++count;

        auto *grpItem = new QTreeWidgetItem( section,
            { QString( "📁 %1  (%2)" ).arg( grpName ).arg( count ) } );
        grpItem->setData( 0, Qt::UserRole,     "group"  );
        grpItem->setData( 0, Qt::UserRole + 1, grpRef   );
        grpItem->setData( 0, Qt::UserRole + 3, grpName  );
        grpItem->setExpanded( false );   // collapsed by default

        for ( const QString &m : members )
        {
            if ( !allMaps.contains( m ) ) continue;
            auto *child = new QTreeWidgetItem( grpItem, { m } );
            child->setData( 0, Qt::UserRole,     "raster" );
            child->setData( 0, Qt::UserRole + 1, m );
            applyAccessDecoration( child, m, activeMapset, "raster" );
        }
    }

    // Standalone (ungrouped) rasters
    for ( const QString &m : allMaps )
    {
        if ( grouped.contains( m ) ) continue;
        auto *child = new QTreeWidgetItem( section, { m } );
        child->setData( 0, Qt::UserRole,     "raster" );
        child->setData( 0, Qt::UserRole + 1, m );
        applyAccessDecoration( child, m, activeMapset, "raster" );
    }
}

// ---------------------------------------------------------------------------
// refresh
// ---------------------------------------------------------------------------
void GrassDataCatalog::refresh()
{
    mTree->clear();

    for ( const MapSection &s : kSections )
    {
        auto *item = new QTreeWidgetItem( mTree,
            { QString( "%1  (loading…)" ).arg( s.label ) } );
        QFont f = item->font( 0 );
        f.setBold( true );
        item->setFont( 0, f );
        item->setData( 0, Qt::UserRole,     QString() );
        item->setData( 0, Qt::UserRole + 2, s.label  );
        mTree->addTopLevelItem( item );

        populate( s.type, item );
        item->setExpanded( s.type == "raster" || s.type == "vector" );
    }
}

// ---------------------------------------------------------------------------
// onItemDoubleClicked
// ---------------------------------------------------------------------------
void GrassDataCatalog::onItemDoubleClicked( QTreeWidgetItem *item, int )
{
    const QString type = item->data( 0, Qt::UserRole ).toString();
    const QString ref  = item->data( 0, Qt::UserRole + 1 ).toString();
    if ( ref.isEmpty() ) return;   // section or group header

    const int at = ref.indexOf( '@' );
    const QString name   = at < 0 ? ref : ref.left( at );
    const QString mapset = at < 0 ? QString() : ref.mid( at + 1 );

    if      ( type == "raster" ) mPanel->addRasterLayer( name, mapset );
    else if ( type == "vector" ) mPanel->addVectorLayer( name, mapset );
    // group / raster_3d / region: no canvas action — user expands the node
}

// ---------------------------------------------------------------------------
// onFilterChanged — recursive hide/show
// ---------------------------------------------------------------------------
void GrassDataCatalog::onFilterChanged( const QString &text )
{
    const QString lower = text.toLower();

    std::function<bool(QTreeWidgetItem *)> visit = [&]( QTreeWidgetItem *item ) -> bool
    {
        if ( item->childCount() == 0 )
        {
            const bool match = lower.isEmpty() ||
                               item->text( 0 ).contains( lower, Qt::CaseInsensitive );
            item->setHidden( !match );
            return match;
        }
        int visible = 0;
        for ( int i = 0; i < item->childCount(); ++i )
            if ( visit( item->child( i ) ) ) ++visible;
        const bool show = lower.isEmpty() || visible > 0;
        item->setHidden( !show );
        if ( !lower.isEmpty() && visible > 0 )
            item->setExpanded( true );   // auto-expand groups that have matches
        return show;
    };

    for ( int i = 0; i < mTree->topLevelItemCount(); ++i )
        visit( mTree->topLevelItem( i ) );
}
