# qtgrass Planetary Menu

The **Planetary** menu integrates the [`grass-planetary-addons`][addons]
modules (`p.*` and a few `g.*` companions) into the qtgrass GUI.
Twenty-eight modules are exposed across nine submenus:

```
Planetary
├── Import / Export       (PDS3, PDS4, ISIS3 import/export, SPICE Kernel Manager)
├── Camera & Geometry     (p.spiceinit, p.caminfo, p.cam2map)
├── Photometric Analysis  (p.phocube, p.photomet, p.atcorr.hapke, p.photrim, p.albedo)
├── Spectral & Mineral    (p.spectral.planet, p.mineral.indices, p.specpix)
├── Terrain Analysis      (p.dem.prep, p.slope.planet, p.shadow.planet)
├── Ring Plane Analysis   (p.rings.project, p.rings.stats)
├── Noise & Calibration   (p.dstripe, p.desmear, p.cubenorm, p.bandnorm)
├── Mosaic                (p.automos)
├── Crater Analysis       (p.crater, p.crater.freq)              ← bespoke Qt6 dialogs
└── Utilities             (p.target.info, g.isis3mt)
```

[addons]: https://github.com/yannchemin/Planetary

## Two dialog tiers

Most module entries open the **generic** auto-generated dialog
(`GrassModuleDialog`), which introspects the module's
`--interface-description` XML and builds a tabbed form on the fly.
This is the same UI behaviour as `<module> --ui` on the command line.

A few entries open a **bespoke Qt6 dialog** designed around the
specific workflow needs of that module. Bespoke dialogs offer:

- planetary-aware controls (e.g. 48-body picker for `p.crater`),
- richer input widgets (file pickers grouped by SPICE kernel type),
- live `QProcess`-driven output streaming,
- post-run visualisation (e.g. CSFD chart for `p.crater.freq`).

Bespoke dialogs are listed below.

---

## SPICE Kernel Manager

**Location:** `Planetary → Import / Export → SPICE Kernel Manager…`
**Source:** `src/planetary/grassspicekerneldialog.{h,cpp}`

Manages NAIF SPICE kernel file assignments per kernel type
(LSK, SCLK, CK, SPK, IK, FK, PCK, DSK). Files are organised in a
`QTreeWidget` grouped by type; per-row status (✓ found / ✗ missing)
is rendered from the live filesystem.

### Features

- Type-specific `QFileDialog` filters (`*.tls`, `*.bsp`, `*.bc`, …)
- Persists kernel paths to `QSettings` under `spice/<type>`
- "Apply to session" exports a semicolon-separated `PLANETSPICE_PATH`
  environment variable, in NAIF load order
  (LSK→SCLK→FK→PCK→IK→SPK→CK→DSK), so `p.spiceinit` and downstream
  modules pick up the kernels as defaults

### Notes

The dialog does **not** load CSPICE itself — kernel loading is
delegated to `p.spiceinit`, which links CSPICE and runs in a
subprocess so a malformed kernel cannot crash qtgrass.

---

## Impact Crater Scaling (`p.crater`)

**Location:** `Planetary → Crater Analysis → Impact crater scaling…`
**Source:** `src/planetary/grasscraterdialog.{h,cpp}`

Five-tab form wrapping `p.crater` (vector-based Melosh/Holsapple
impact-crater scaling).

### Tab 1 — Geometry & Body

- **Input crater rim vector** — editable combo populated from the
  current mapset's `vector/` subdir
- **Output vector name** — line edit
- **Planetary body** — combo with 48 entries (Mercury through 67P
  and `custom`); selection updates an italic info line showing
  `g`, surface density, and target type
- **Backward mode** checkbox (default ON) — estimates impactor
  diameter from the rim; disables the "Impactor diameter" field

### Tab 2 — Impactor

- Velocity [m/s], impact angle [deg from horizontal], projectile
  density [kg/m³], optional diameter [m] for forward mode
- Defaults: 20 km/s, 45°, 3000 kg/m³ (typical stony asteroid)

### Tab 3 — Geology & DEM

Three groups for optional rasters that override the body defaults
on a per-crater basis:

- **Surface layer** — density raster + thickness [m]
- **Subsurface layer** — density raster + thickness [m]
  (a thickness of zero means "infinite", i.e. the subsurface
  extends below the deepest excavation)
- **Optional raster inputs** — surface-type raster (1/2/3) and
  DEM (used to measure crater depth from rim-mean − centre)

The depth-weighted effective density follows Melosh 1989 §5.4:
excavation depth ≈ Dat / 3, and the two layers are linearly
weighted over that column.

### Tab 4 — Custom body

Enabled only when `body=custom` is selected. Mandatory fields:
gravity [m/s²], target surface density [kg/m³], target type (1/2/3).
The dialog refuses to run with `body=custom` if any field is unset.

### Tab 5 — Run log

`QPlainTextEdit` in monospace, streaming merged stdout/stderr from
the `QProcess` launching `p.crater`. The exact command line is
echoed before each run.

### Validation

Before launching, the dialog checks that:

- input vector and output name are non-empty
- if `body=custom`, all three overrides are set

---

## Crater Size-Frequency Dating (`p.crater.freq`)

**Location:** `Planetary → Crater Analysis → Crater size-frequency dating…`
**Source:** `src/planetary/grasscraterfreqdialog.{h,cpp}`

Three-tab dialog with a chart visualisation of the CSFD fit.

### Tab 1 — Inputs & Parameters

Two crater-source modes selected via radio buttons (`QStackedWidget`):

| Mode | Inputs |
|---|---|
| **CSV file** (default) | path picker; one diameter [km] per line |
| **Vector + column** | vector combo + column combo pre-populated with `Df_pi`, `Dat_pi`, `D_eq`, `Dat_gault`, `Dat_yield` (the column names produced by `p.crater`) |

Analysis parameters: mapped area [km²], target body (`moon`, `mars`),
diameter bin range, number of bins, Hartmann 2005 toggle (pending).

Output section:

- **Output CSV** — optional; writes the enriched 5-column SFD table
- **Output PNG graph** — optional; renders the chart at
  **4961 × 3496 px (A5 landscape, 600 dpi)** via
  `QChart::scene()->render()` into a QImage backend
- **Show graph in Chart tab** — checkbox (default ON); if the
  Output CSV is empty, a temp CSV is staged in `$TMPDIR` so the
  chart can still be drawn

### Tab 2 — Run log

Same streaming pattern as `p.crater`.

### Tab 3 — Chart

Populated automatically after a successful run. Built on Qt6's
`QChart` with `QLogValueAxis` axes:

- **Observed N(≥D)** — `QScatterSeries`, markers at populated bins
- **NPF curve** — `QLineSeries`, Neukum 1983/Ivanov 2001 production
  function scaled to the estimated age
- **Power-law fit** — dashed `QLineSeries`, slope and intercept
  read from the CSV header
- **Title**: `"{body} - age = {x.xxx} Ga"`

The dialog parses the module's output CSV (or its staged temp copy)
to drive the plot, so the chart always reflects exactly what the
CLI module computed — no double-fitting in the GUI.

### PNG export details

A5 landscape at 600 dpi is the international scientific-figure
default for inserting into journal manuscripts:

- 210 mm × 600 dpi / 25.4 mm/in ≈ 4961 px
- 148 mm × 600 dpi / 25.4 mm/in ≈ 3496 px

The render uses the chart's `QGraphicsScene::render()` with
antialiasing, so curves and text remain sharp at print resolution.

---

## Architecture notes for developers

All three bespoke dialogs follow the same pattern:

1. **No CSPICE / GRASS-internal calls.** Each dialog spawns the
   target module via `QProcess` so the heavy lifting (and any
   crashes from malformed inputs) happens out-of-process.
2. **`QProcess::MergedChannels`** — stdout + stderr stream live
   into a single `QPlainTextEdit`.
3. **Available-modules gate.** `GrassPlanetaryMenu::populate()`
   greys out menu entries whose module binary is not on `PATH`,
   with a tooltip explaining how to install it.
4. **No external chart deps.** Qt6 Charts (`qt6-charts-dev`) is
   the only addition over the qtgrass base set; everything else
   uses standard Widgets.

[mainsrc]: ../src/planetary/
