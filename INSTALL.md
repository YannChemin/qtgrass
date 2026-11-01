# Installing qtgrass

This document describes how to build qtgrass from source and how to install it
from a Debian package. For an overview of the application, see
[README.md](README.md).

## Prerequisites

### Build tools

- CMake 3.22 or later
- A C++17 compiler (GCC or Clang)
- `pkg-config`
- GNU Make (optional, used by the convenience `Makefile`)

### Libraries

- **Qt 6.2+** with these modules: Core, Gui, Widgets, Network, Xml, Concurrent,
  Sql, PrintSupport, Svg, Charts, OpenGL, OpenGLWidgets
- **GRASS GIS 8.0+** with development headers and libraries
  (`libgrass_gis`, `libgrass_raster`, `libgrass_vector`, `libgrass_vedit`,
  `libgrass_dbmiclient`, `libgrass_dbmibase`)
- **PROJ** 6+ with its `proj.db` database
- **GDAL** (used for vector headers and raster tile handling)
- OpenGL development files

Optional:

- **Doxygen** — enables the `doxy` target for API documentation
- **ffmpeg** — required at runtime to export animations to MP4

### Debian/Ubuntu packages

The authoritative dependency list is in [`debian/control`](debian/control).
The equivalent manual install is:

```sh
sudo apt install build-essential cmake pkg-config \
    qt6-base-dev qt6-base-dev-tools libqt6svg6-dev qt6-charts-dev \
    libcups2-dev libgdal-dev libproj-dev libgl1-mesa-dev libglu1-mesa-dev \
    grass-dev
```

`grass-dev` must match the GRASS installation you build against. When GRASS is
installed from source or from the GRASS project packages, point the build at it
with `GRASS_PREFIX` (see below) instead of relying on `grass-dev`.

## Building from source

### With the Makefile

The `Makefile` wraps the CMake configure and build steps:

```sh
# Release build (RelWithDebInfo) into ./build/
make build

# Debug build with full symbols into ./build-debug/
make build-debug
```

GRASS is auto-detected from the newest `/usr/local/grass[0-9]*` directory,
falling back to `/usr/local/grass86`. Override it when needed:

```sh
make build GRASS_PREFIX=/usr/local/grass86
```

### With CMake directly

```sh
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DGRASS_PREFIX=/usr/local/grass86
cmake --build build -j"$(nproc)"
```

The resulting binary is `build/src/qtgrass`.

### CMake options

| Option | Default | Purpose |
| --- | --- | --- |
| `GRASS_PREFIX` | auto-detected | Path to the GRASS `GISBASE` installation |
| `GRASS_ADDONS_DIR` | `~/dev/grass-addons` | Source tree used to generate the grass-addons menus |
| `CMAKE_BUILD_TYPE` | empty | `RelWithDebInfo`, `Release` or `Debug` |
| `CMAKE_INSTALL_PREFIX` | `/usr/local` | Installation prefix used by `cmake --install` |

GRASS is located in this order:

1. `-DGRASS_PREFIX=...`
2. the `GRASS_PREFIX` environment variable
3. the `GISBASE` environment variable
4. `grass --config path` from a running GRASS session
5. common prefixes (`/usr/local/grass8*`, `/usr/local/grass*`,
   `/usr/lib/grass8*`, `/opt/grass8*`), newest first

### grass-addons menus

At configure time qtgrass scans `GRASS_ADDONS_DIR` and generates a header that
populates the addons menus. If the directory does not contain a `src/`
subdirectory, addon menus are disabled and a status message is printed.

To build without the addons menus, point the option at a path that does not
exist:

```sh
cmake -S . -B build -DGRASS_ADDONS_DIR=
```

Menu placement is described in [`ADDONS_MENU.md`](ADDONS_MENU.md).

### API documentation

If Doxygen is installed, generate the API documentation with:

```sh
cmake --build build --target doxy
```

Output is written to `doc/api/`.

## Installing

### Into the GRASS installation

The Makefile copies the binary into the GRASS `bin` directory:

```sh
sudo make install GRASS_PREFIX=/usr/local/grass86
```

This installs `$(GRASS_PREFIX)/bin/qtgrass`, so it is available inside GRASS
sessions without changing `PATH`.

### With CMake

```sh
cmake --install build
```

or, to choose the prefix explicitly:

```sh
cmake --install build --prefix /usr/local
```

The binary is installed to `<prefix>/bin/qtgrass`.

## Debian packages

Unsigned packages can be built from the repository root:

```sh
make deb          # release package
make deb-nc       # same as deb (unsigned)
make deb-debug    # debug package with full symbols
```

The packages are written to the parent directory. Install one with:

```sh
sudo dpkg -i ../qtgrass_*_amd64.deb
```

The package installs the `qtgrass` binary, a desktop entry and icon, the MIME
type for `.gxw` workspaces, a man page, and a `qtgrass-debug` wrapper that
enables logging. GRASS GIS itself is declared as a recommended dependency and
must be installed separately.

## Running

```sh
qtgrass                                   # startup wizard
qtgrass path/to/workspace.gxw             # open a workspace
qtgrass --module r.slope.aspect elevation=dem slope=slope
```

If `GISBASE` is not set, the GRASS location used at build time is baked into
the binary. Override it at runtime with:

```sh
qtgrass --grassdir /usr/local/grass86
```

Set `QTGRASS_DEBUG=1` to write startup and Qt log output to
`~/qtgrass-debug.log`.

## Troubleshooting

- **`GRASS not found`** — pass `-DGRASS_PREFIX=/path/to/grass` (the directory
  containing `include/grass/gis.h` and `lib/libgrass_gis.so`). The binary is
  not built against a `grass-dev` package if a source installation is
  preferred.
- **Qt module not found** — install the matching development package; the
  missing component is named in the CMake error (for example `Qt6::Sql` comes
  from `qt6-base-dev`).
- **`proj.db` errors at runtime** — install `proj-data` (or the distribution's
  equivalent) or set `PROJ_DATA` to a directory containing `proj.db`.
- **MP4 export fails** — install `ffmpeg`; other animation/export formats do
  not require it.
- **No addons menus** — configure with a valid `-DGRASS_ADDONS_DIR` pointing
  at a grass-addons checkout.

## Uninstalling

If installed with `make install`, remove the binary:

```sh
sudo rm "$(GRASS_PREFIX)/bin/qtgrass"
```

If installed from a Debian package:

```sh
sudo apt remove qtgrass
```
