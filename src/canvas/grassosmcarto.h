#pragma once
// OSM Carto style presets for qtgrass vector rendering.
//
// Sources:
//   wiki.openstreetmap.org/wiki/OpenStreetMap_Carto/Lines
//   wiki.openstreetmap.org/wiki/OpenStreetMap_Carto/Areas
//   wiki.openstreetmap.org/wiki/OpenStreetMap_Carto/Symbols
//
// Each preset maps directly to fields in GrassVectorStyle so the renderer
// does not need to know about OSM Carto at all.

#include <QColor>
#include <QString>
#include <QVector>
#include <Qt>   // Qt::PenStyle, Qt::BrushStyle

// ── Point shapes ─────────────────────────────────────────────────────────────
enum class OsmPointShape {
    Circle, Square, Triangle, Diamond, Cross, Star,
    MedicalCross,   // thick + for hospital / pharmacy
    ArrowUp,        // direction marker
    Pentagon, Hexagon
};

// ── Fill patterns ─────────────────────────────────────────────────────────────
// Mapped to Qt::BrushStyle for areas.
// "Solid" = Qt::SolidPattern, "None" = Qt::NoBrush, etc.

// ── Preset record ─────────────────────────────────────────────────────────────
struct OsmCartoPreset {
    QString      name;         // human-readable description shown in combo
    QString      key;          // unique short id

    // --- Lines & borders ---
    QColor       lineColor   = {50,120,200};
    float        lineWidth   = 1.2f;
    Qt::PenStyle penStyle    = Qt::SolidLine;

    // --- Areas ---
    QColor       fillColor   = {};   // default QColor (invalid) → use lineColor with low alpha
    Qt::BrushStyle brushStyle = Qt::SolidPattern;
    bool         hasFill     = false;

    // --- Points ---
    OsmPointShape pointShape = OsmPointShape::Circle;
    QColor        pointColor = {};   // default QColor (invalid) → use lineColor
    float         pointSize  = 6.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
// LINE presets (wiki.openstreetmap.org/wiki/OpenStreetMap_Carto/Lines)
// ─────────────────────────────────────────────────────────────────────────────
inline const QVector<OsmCartoPreset>& osmCartoLines()
{
    static const QVector<OsmCartoPreset> v = {
        // Roads — casing colour / fill colour from OSM Carto style.mss
        { "Motorway",          "motorway",         {233,144,160}, 4.0f },
        { "Motorway link",     "motorway_link",     {233,144,160}, 2.0f },
        { "Trunk road",        "trunk",             {249,178,156}, 3.5f },
        { "Trunk link",        "trunk_link",        {249,178,156}, 2.0f },
        { "Primary road",      "primary",           {252,214,164}, 3.0f },
        { "Primary link",      "primary_link",      {252,214,164}, 1.8f },
        { "Secondary road",    "secondary",         {247,250,191}, 2.5f },
        { "Secondary link",    "secondary_link",    {247,250,191}, 1.5f },
        { "Tertiary road",     "tertiary",          {255,255,255}, 2.0f },
        { "Tertiary link",     "tertiary_link",     {255,255,255}, 1.2f },
        { "Residential road",  "residential",       {204,204,204}, 1.5f },
        { "Unclassified road", "unclassified",      {204,204,204}, 1.5f },
        { "Living street",     "living_street",     {225,225,225}, 1.2f },
        { "Pedestrian area",   "pedestrian",        {238,238,238}, 1.0f },
        { "Service road",      "service",           {170,170,170}, 0.8f },
        { "Track",             "track",             {153,102,  0}, 1.0f, Qt::DashLine },
        { "Path",              "path",              {122,122,122}, 0.5f, Qt::DotLine  },
        { "Footway",           "footway",           {255,153,  0}, 0.5f, Qt::DashLine },
        { "Cycleway",          "cycleway",          {  0,  0,255}, 0.7f, Qt::DashLine },
        { "Steps",             "steps",             {249,153,  0}, 1.0f, Qt::DotLine  },
        { "Bridleway",         "bridleway",         {102,153,  0}, 0.7f, Qt::DashLine },
        { "Construction",      "construction",      {255,165,  0}, 1.2f, Qt::DashDotLine },
        // Railways
        { "Rail",              "rail",              { 68, 68, 68}, 1.5f },
        { "Light rail",        "light_rail",        { 68, 68, 68}, 1.2f },
        { "Subway",            "subway",            {100, 80,180}, 1.2f },
        { "Tram",              "tram",              { 80, 80, 80}, 0.8f },
        // Waterways
        { "River",             "river",             {102,153,204}, 2.5f },
        { "Stream",            "stream",            {102,153,204}, 1.0f },
        { "Canal",             "canal",             {102,153,204}, 1.8f, Qt::DashLine },
        { "Drain",             "drain",             {102,153,204}, 0.5f },
        { "Ditch",             "ditch",             {102,153,204}, 0.5f, Qt::DotLine  },
        // Boundaries & barriers
        { "Admin boundary",    "boundary_admin",    {153,  0,238}, 1.0f, Qt::DashLine },
        { "National border",   "boundary_national", {200, 50, 50}, 1.2f, Qt::DashDotLine },
        { "Fence",             "fence",             {136,136,136}, 0.5f, Qt::DotLine  },
        { "Hedge",             "hedge",             {  0,153,  0}, 1.0f },
        { "Wall",              "wall",              {160,140,120}, 0.8f },
        // Infrastructure
        { "Power line",        "power_line",        {119,119,119}, 0.5f, Qt::DashDotLine },
        { "Pipeline",          "pipeline",          {120, 80, 40}, 0.7f, Qt::DashLine },
        { "Contour (minor)",   "contour_minor",     {184,148, 96}, 0.3f },
        { "Contour (major)",   "contour_major",     {184,148, 96}, 0.8f },
        // Coastline
        { "Coastline",         "coastline",         { 10,100,200}, 1.5f },
        // Generic custom
        { "Custom (solid)",    "custom_solid",      { 50,120,200}, 1.2f },
    };
    return v;
}

// ─────────────────────────────────────────────────────────────────────────────
// AREA presets (wiki.openstreetmap.org/wiki/OpenStreetMap_Carto/Areas)
// ─────────────────────────────────────────────────────────────────────────────
inline const QVector<OsmCartoPreset>& osmCartoAreas()
{
    // Helper lambda inline to init
    auto a = []( const char *name, const char *key,
                 QColor border, float bw, QColor fill,
                 Qt::BrushStyle bs = Qt::SolidPattern ) -> OsmCartoPreset {
        OsmCartoPreset p;
        p.name = name; p.key = key;
        p.lineColor = border; p.lineWidth = bw;
        p.fillColor = fill;  p.brushStyle = bs;
        p.hasFill   = true;
        return p;
    };

    static const QVector<OsmCartoPreset> v = {
        // Built environment
        a("Building",          "building",       {181,172,161},1.0f,{217,208,201}),
        a("Commercial bldg",   "commercial_bld", {181,172,161},0.5f,{234,218,218}),
        a("Residential area",  "residential",    {189,185,185},0.5f,{224,222,222}),
        a("Industrial area",   "industrial",     {192,176,176},0.5f,{232,224,224}),
        a("Retail area",       "retail",         {224,128,128},0.5f,{255,210,210}),
        a("Commercial area",   "commercial",     {208,160,160},0.5f,{243,200,200}),
        a("Military",          "military",       {200,  0,  0},0.7f,{255, 85, 85}, Qt::FDiagPattern),
        a("Aerodrome",         "aerodrome",      {153,153,153},0.5f,{228,228,228}),
        a("Railway yard",      "railway_yard",   {136,136,136},0.5f,{214,214,214}),
        // Land
        a("Farmland",          "farmland",       {196,198,153},0.3f,{238,240,213}),
        a("Farmyard",          "farmyard",       {208,188,148},0.5f,{245,225,193}),
        a("Allotments",        "allotments",     {145,129, 79},0.5f,{194,179,130}),
        a("Quarry",            "quarry",         {144,143,143},0.5f,{196,194,194}),
        a("Construction site", "construction",   {176,144, 89},0.5f,{200,166,113}),
        // Nature
        a("Forest / Wood",     "wood",           {130,168,126},0.5f,{173,209,158}),
        a("Natural wood",      "natural_wood",   {123,162,119},0.5f,{157,202,138}),
        a("Grass / Meadow",    "grass",          {172,205,140},0.3f,{205,235,176}),
        a("Scrub",             "scrub",          {120,180,120},0.3f,{200,215,171}),
        a("Heath",             "heath",          {214,217,159},0.3f,{214,217,159}),
        a("Wetland",           "wetland",        {128,168,112},0.5f,{209,231,176}),
        a("Marsh",             "marsh",          {120,160,112},0.5f,{204,224,176}),
        a("Bog",               "bog",            {128,160,112},0.5f,{204,220,180}),
        a("Bare rock",         "bare_rock",      {192,192,192},0.3f,{212,212,208}),
        a("Sand / Beach",      "beach",          {232,208,128},0.3f,{255,241,186}),
        a("Glacier",           "glacier",        {221,236,255},0.3f,{221,236,255}),
        a("Tundra",            "tundra",         {200,210,190},0.3f,{221,228,210}),
        // Water
        a("Water body",        "water",          {102,153,204},0.5f,{170,211,223}),
        a("Lake",              "lake",           {102,153,204},0.5f,{170,211,223}),
        a("Reservoir",         "reservoir",      {102,153,204},0.5f,{170,211,223}),
        a("Swimming pool",     "swimming_pool",  {102,153,204},0.5f,{189,234,248}),
        // Leisure & amenity
        a("Park",              "park",           {141,194,141},0.5f,{200,250,204}),
        a("Garden",            "garden",         {172,194,141},0.3f,{207,240,181}),
        a("Golf course",       "golf",           {141,194,141},0.3f,{199,231,168}),
        a("Pitch / Sports",    "pitch",          {139,195,118},0.5f,{170,224,203}),
        a("Track / Raceway",   "track",          {168,165, 80},0.5f,{218,216,136}),
        a("Playground",        "playground",     {162,170,136},0.5f,{219,227,183}),
        a("Cemetery",          "cemetery",       {169,200,173},0.5f,{170,203,175}),
        a("Zoo",               "zoo",            {136,168,136},0.5f,{192,224,192}),
        // Amenity areas
        a("Hospital / Health", "hospital",       {224,192,192},0.5f,{255,241,238}),
        a("School / Education","school",         {192,192,144},0.3f,{240,240,216}),
        a("University",        "university",     {200,192,144},0.3f,{240,240,210}),
        a("Library",           "library",        {184,184,152},0.3f,{232,232,216}),
        a("Place of worship",  "worship",        {176,176,176},0.3f,{232,232,232}),
        a("Prison",            "prison",         {144,144,160},0.5f,{192,192,208}, Qt::CrossPattern),
        // Tourism
        a("Camp site",         "campsite",       {  0,153, 76},0.3f,{221,247,196}),
        a("Caravan site",      "caravan",        {  0,153, 76},0.3f,{216,244,190}),
    };
    return v;
}

// ─────────────────────────────────────────────────────────────────────────────
// POINT presets (wiki.openstreetmap.org/wiki/OpenStreetMap_Carto/Symbols)
// ─────────────────────────────────────────────────────────────────────────────
inline const QVector<OsmCartoPreset>& osmCartoPoints()
{
    auto p = []( const char *name, const char *key,
                 QColor col, OsmPointShape shape, float sz = 7.f ) -> OsmCartoPreset {
        OsmCartoPreset r;
        r.name = name; r.key = key;
        r.lineColor = col; r.pointColor = col; r.pointShape = shape;
        r.pointSize = sz; r.hasFill = false;
        return r;
    };

    static const QVector<OsmCartoPreset> v = {
        // ── Generic shapes ──────────────────────────────────────────────
        p("Circle",                   "circle",          {100,100,200}, OsmPointShape::Circle,    6.f),
        p("Square",                   "square",          {100,100,200}, OsmPointShape::Square,    6.f),
        p("Triangle",                 "triangle",        {100,150, 80}, OsmPointShape::Triangle,  7.f),
        p("Diamond",                  "diamond",         {150,100,200}, OsmPointShape::Diamond,   7.f),
        p("Cross / Plus",             "cross",           {180, 30, 30}, OsmPointShape::Cross,     7.f),
        p("Star (5-pointed)",         "star",            {200,160,  0}, OsmPointShape::Star,      8.f),
        p("Pentagon",                 "pentagon",        {100,150,100}, OsmPointShape::Pentagon,  7.f),
        p("Hexagon",                  "hexagon",         { 80,130,180}, OsmPointShape::Hexagon,   7.f),
        // ── Amenity ─────────────────────────────────────────────────────
        p("Restaurant",               "restaurant",      {255,100,  0}, OsmPointShape::Circle,    7.f),
        p("Fast food",                "fast_food",       {255,120,  0}, OsmPointShape::Circle,    6.f),
        p("Café",                     "cafe",            {139, 69, 19}, OsmPointShape::Circle,    6.f),
        p("Bar / Pub",                "bar",             {139, 69,100}, OsmPointShape::Circle,    6.f),
        p("Bakery",                   "bakery",          {210,140, 60}, OsmPointShape::Square,    6.f),
        p("Supermarket / Shop",       "shop",            {  0,102,153}, OsmPointShape::Square,    6.f),
        p("Convenience store",        "convenience",     {  0,120,153}, OsmPointShape::Square,    5.f),
        p("Hospital / Clinic",        "hospital",        {204,  0,  0}, OsmPointShape::MedicalCross,9.f),
        p("Pharmacy",                 "pharmacy",        {  0,153,  0}, OsmPointShape::MedicalCross,8.f),
        p("Doctors / Surgery",        "doctors",         {180, 60, 60}, OsmPointShape::MedicalCross,7.f),
        p("Police",                   "police",          { 51, 51,153}, OsmPointShape::Square,    7.f),
        p("Fire station",             "fire_station",    {204,  0,  0}, OsmPointShape::Triangle,  7.f),
        p("Post office",              "post_office",     {255,204,  0}, OsmPointShape::Square,    7.f),
        p("Post box",                 "post_box",        {255,153,  0}, OsmPointShape::Circle,    5.f),
        p("Bank",                     "bank",            {  0,153, 51}, OsmPointShape::Diamond,   7.f),
        p("ATM",                      "atm",             {  0,180, 60}, OsmPointShape::Diamond,   6.f),
        p("Fuel / Petrol",            "fuel",            {255,204,  0}, OsmPointShape::Diamond,   7.f),
        p("Parking",                  "parking",         {  0, 51,204}, OsmPointShape::Square,    8.f),
        p("Bicycle parking",          "bicycle_parking", {  0,120,204}, OsmPointShape::Triangle,  6.f),
        p("Bus stop",                 "bus_stop",        {  0, 85,187}, OsmPointShape::Diamond,   7.f),
        p("Tram stop",                "tram_stop",       { 80, 80,180}, OsmPointShape::Diamond,   6.f),
        p("Train station",            "station",         { 51, 51, 51}, OsmPointShape::Square,   10.f),
        p("Subway / Metro",           "subway_entrance", {100, 60,180}, OsmPointShape::Circle,    8.f),
        p("Airport",                  "aerodrome",       { 51,102,153}, OsmPointShape::Pentagon,  9.f),
        // ── Tourism ─────────────────────────────────────────────────────
        p("Hotel / Accommodation",    "hotel",           {204,  0, 51}, OsmPointShape::Circle,    7.f),
        p("Campsite",                 "campsite",        {  0,153, 76}, OsmPointShape::Triangle,  7.f),
        p("Information / Visitor",    "information",     { 51,153,255}, OsmPointShape::Circle,    7.f),
        p("Museum",                   "museum",          {102, 51,153}, OsmPointShape::Square,    7.f),
        p("Viewpoint",                "viewpoint",       {204,102,  0}, OsmPointShape::Star,      8.f),
        p("Attraction",               "attraction",      {153,  0,102}, OsmPointShape::Star,      7.f),
        p("Monument / Memorial",      "monument",        {153,102,  0}, OsmPointShape::Pentagon,  7.f),
        // ── Leisure & sport ─────────────────────────────────────────────
        p("Playground",               "playground",      {  0,180,120}, OsmPointShape::Star,      7.f),
        p("Swimming pool",            "swimming",        {  0,153,204}, OsmPointShape::Circle,    7.f),
        p("Sports centre",            "sports_centre",   { 51,153,  0}, OsmPointShape::Pentagon,  7.f),
        // ── Nature & hazard ─────────────────────────────────────────────
        p("Peak / Summit",            "peak",            {153, 51,  0}, OsmPointShape::Triangle,  8.f),
        p("Volcano",                  "volcano",         {200, 60,  0}, OsmPointShape::Triangle,  9.f),
        p("Spring",                   "spring",          {  0,153,204}, OsmPointShape::Circle,    6.f),
        p("Cave entrance",            "cave_entrance",   { 80, 50, 20}, OsmPointShape::Diamond,   7.f),
        p("Cliff",                    "cliff",           {153,153,153}, OsmPointShape::ArrowUp,   8.f),
        // ── Place labels (node only) ─────────────────────────────────────
        p("City / Town",              "city",            { 30, 30, 30}, OsmPointShape::Circle,    9.f),
        p("Village / Hamlet",         "village",         { 80, 80, 80}, OsmPointShape::Circle,    6.f),
        p("Locality",                 "locality",        {120,120,120}, OsmPointShape::Circle,    4.f),
        // ── Power & utilities ────────────────────────────────────────────
        p("Power tower / Pylon",      "power_tower",     {136,136,136}, OsmPointShape::Cross,     7.f),
        p("Power plant",              "power_plant",     {160,160,160}, OsmPointShape::Square,    8.f),
        p("Water tower",              "water_tower",     { 51,153,255}, OsmPointShape::Circle,    8.f),
        // ── Religion ─────────────────────────────────────────────────────
        p("Place of worship",         "worship_generic", {136,136,136}, OsmPointShape::Cross,     7.f),
        p("Church",                   "church",          { 68, 68, 68}, OsmPointShape::Cross,     8.f),
        p("Mosque",                   "mosque",          { 30,100, 30}, OsmPointShape::Star,      8.f),
        p("Temple / Hindu",           "temple",          {200,100,  0}, OsmPointShape::Star,      8.f),
        p("Synagogue",                "synagogue",       { 30, 30,150}, OsmPointShape::Star,      8.f),
        // ── Education ────────────────────────────────────────────────────
        p("School",                   "school",          {136,136, 68}, OsmPointShape::Square,    7.f),
        p("University",               "university",      { 80, 80, 30}, OsmPointShape::Square,    8.f),
        p("Library",                  "library",         {100, 80, 40}, OsmPointShape::Square,    6.f),
        // ── Emergency ────────────────────────────────────────────────────
        p("Emergency phone",          "emergency_phone", {204,  0,  0}, OsmPointShape::Circle,    6.f),
        p("Defibrillator (AED)",      "defibrillator",   {204,  0,  0}, OsmPointShape::MedicalCross,8.f),
    };
    return v;
}

// ─────────────────────────────────────────────────────────────────────────────
// Lookup helpers
// ─────────────────────────────────────────────────────────────────────────────
inline const OsmCartoPreset* findOsmPreset( const QString &key )
{
    for ( const auto &p : osmCartoLines()  ) if ( p.key == key ) return &p;
    for ( const auto &p : osmCartoAreas()  ) if ( p.key == key ) return &p;
    for ( const auto &p : osmCartoPoints() ) if ( p.key == key ) return &p;
    return nullptr;
}
