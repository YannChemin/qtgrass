#!/usr/bin/env python3
"""
scan_addons.py  —  run by cmake/GenerateAddonsList.cmake at configure time.

Flow:
  1. Parse ADDONS_MENU.md  (the authoritative human-edited placement table).
  2. Scan the grass-addons/src tree for every module present on disk.
  3. Any module on disk but NOT in the .md  → NEW:
       • Append to ADDONS_MENU.md  marked "⚠️ NEW — review placement"
       • Print a conspicuous cmake WARNING so the developer notices.
  4. Write grassaddonslist.h  from the (possibly updated) .md table.
     The header contains AddonInfo {name, label, menuPath} for every row.

Usage:
    python3 scan_addons.py <addons_dir> <output_header> <md_file>
"""
import sys, re, os
from pathlib import Path

# ── ANSI helpers (cmake echoes stdout, so colours work in terminal) ────
RED    = "\033[1;31m"
YELLOW = "\033[1;33m"
RESET  = "\033[0m"

WARN_BORDER = "=" * 70

def warn(msg: str) -> None:
    print(f"{RED}{WARN_BORDER}{RESET}", flush=True)
    for line in msg.splitlines():
        print(f"{YELLOW}  {line}{RESET}", flush=True)
    print(f"{RED}{WARN_BORDER}{RESET}\n", flush=True)


# ── Markdown table helpers ─────────────────────────────────────────────
def parse_md(md_path: Path):
    """Return list of (name, menu_path, description, raw_line) from the table."""
    entries = []
    if not md_path.exists():
        return entries
    in_table = False
    for line in md_path.read_text(errors="ignore").splitlines():
        if line.startswith("| Module"):
            in_table = True
            continue
        if line.startswith("|---"):
            continue
        if in_table and line.startswith("|"):
            cols = [c.strip() for c in line.split("|")[1:-1]]
            if len(cols) >= 3:
                name = cols[0].strip("`").strip()
                menu = cols[1].strip()
                desc = cols[2].strip()
                entries.append((name, menu, desc, line))
        elif in_table and not line.startswith("|"):
            in_table = False
    return entries


def write_md(md_path: Path, header: str, entries):
    """Rewrite the table section of the md file."""
    lines = [
        header,
        "",
        "| Module | Menu Path | Description |",
        "|--------|-----------|-------------|",
    ]
    for name, menu, desc, _raw in entries:
        lines.append(f"| `{name}` | {menu} | {desc} |")
    md_path.write_text("\n".join(lines) + "\n")


MD_HEADER = """\
# GRASS Addons Menu Placement

This file is the **authoritative source** for where every grass-addon module
appears in the qtgrass menus.  Edit the **Menu Path** column to move a module.

* `Menu Path` uses `/`-separated lowercase slugs matching the Qt menu hierarchy.
  Example: `imagery/evapotranspiration` → Imagery → Evapotranspiration submenu.
* Rows marked `⚠️ NEW` were auto-detected and need a human to confirm placement.
* Re-run cmake after editing this file to regenerate the C++ header."""


# ── Description extraction from addon source ──────────────────────────
def extract_label(moddir: Path, modname: str) -> str:
    # C: module->label = _("…");
    for src in moddir.glob("*.c"):
        try:
            txt = src.read_text(errors="ignore")
            m = re.search(r'module\s*->\s*label\s*=\s*_\(\s*"([^"]+)"', txt)
            if m:
                return m.group(1).strip()
            m = re.search(r'module\s*->\s*description\s*=\s*_\(\s*"([^"]+)"', txt)
            if m:
                return m.group(1).strip()
        except Exception:
            pass

    # Python: #% label: …
    for src in moddir.glob("*.py"):
        try:
            txt = src.read_text(errors="ignore")
            m = re.search(r"#%\s*label:\s*(.+)", txt)
            if m:
                return m.group(1).strip()
            m = re.search(r"#%\s*description:\s*(.+)", txt)
            if m:
                return m.group(1).strip()
        except Exception:
            pass

    # HTML: first clean sentence after <h2>…</h2>
    for html in moddir.glob("*.html"):
        try:
            txt = html.read_text(errors="ignore")
            # Remove all tags, grab first meaningful sentence
            plain = re.sub(r"<[^>]+>", " ", txt)
            plain = re.sub(r"\s+", " ", plain).strip()
            # Skip the module name itself at the start
            plain = re.sub(r"^" + re.escape(modname) + r"\s*", "", plain, flags=re.I)
            # First sentence, max 120 chars
            sentence = re.split(r"[.!?]", plain)[0].strip()
            if len(sentence) > 10:
                return sentence[:120]
        except Exception:
            pass

    return ""


# ── Default menu path heuristic (used for NEW modules only) ───────────
_FAM_LABELS = {
    "stream": "stream-analysis", "sun": "solar-radiation",
    "li": "landscape-indices", "sim": "simulation", "slope": "slope-aspect",
    "series": "raster-series", "surf": "surface-interpolation",
    "terrain": "terrain-analysis", "texture": "texture",
    "zonal": "zonal-statistics", "learn": "machine-learning",
    "maxent": "maxent-modelling", "regression": "regression",
    "hydro": "hydrology", "gsflow": "gsflow", "hazard": "hazard",
    "object": "object-based-analysis", "soils": "soils",
    "pops": "pops-disease", "mcda": "multi-criteria-decision",
    "richdem": "richdem-terrain", "viewshed": "viewshed",
    "colors": "color-tables", "in": "import", "out": "export",
    "to": "conversions", "net": "network-analysis",
    "lidar": "lidar-point-cloud", "class": "classification",
    "habitat": "habitat-analysis", "db": "database",
    "eb": "energy-balance", "evapo": "evapotranspiration",
    "vi": "vegetation-indices", "segment": "segmentation",
    "ann": "neural-networks", "sar": "sar-radar",
    "histo": "histograms", "spec": "spectral-analysis",
    "fusion": "image-fusion", "nightlights": "night-time-lights",
    "pysptools": "hyperspectral-pysptools", "superpixels": "super-pixels",
    "hyper": "hyperspectral-analysis", "signature": "signature-management",
    "fuzzy": "fuzzy-logic", "landscape": "landscape-ecology",
    "area": "area-analysis", "convert": "format-conversion",
    "rast": "raster-time-series", "fill": "gap-filling",
    "null": "null-handling", "buildvrt": "virtual-raster",
    "category": "category-management", "bearing": "bearing-distance",
}
_CAT_MENU = {
    "r": "raster", "r3": "raster-3d", "v": "vector", "i": "imagery",
    "t": "temporal", "db": "database", "d": "settings", "g": "settings",
    "m": "tools",
}

def default_path(name: str) -> str:
    parts = name.split(".")
    if len(parts) < 2:
        return "tools/addons"
    cat = parts[0]
    sub = parts[1] if len(parts) > 1 else ""
    menu = _CAT_MENU.get(cat, "tools")
    fam  = _FAM_LABELS.get(sub, sub.replace("_", "-"))
    return f"{menu}/{fam}"


# ── Main ───────────────────────────────────────────────────────────────
def main():
    if len(sys.argv) != 4:
        sys.exit(f"Usage: {sys.argv[0]} <addons_dir> <output_header> <md_file>")

    addons_dir  = Path(sys.argv[1])
    output_h    = Path(sys.argv[2])
    md_path     = Path(sys.argv[3])

    addons_src = addons_dir / "src"
    if not addons_src.is_dir():
        sys.exit(f"ERROR: {addons_src} not found")

    # ── 1. Parse existing .md ─────────────────────────────────────────
    known_entries = parse_md(md_path)
    known_names   = {e[0] for e in known_entries}

    # ── 2. Scan disk ─────────────────────────────────────────────────
    disk_modules = {}  # name → (moddir, label)
    for catdir in sorted(addons_src.iterdir()):
        if not catdir.is_dir():
            continue
        for moddir in sorted(catdir.iterdir()):
            if not moddir.is_dir():
                continue
            name = moddir.name
            if not re.match(r"^[a-z][0-9a-z]*\.[a-z]", name):
                continue
            disk_modules[name] = (moddir, extract_label(moddir, name))

    # ── 3. Detect NEW modules ─────────────────────────────────────────
    new_names = sorted(set(disk_modules) - known_names)
    if new_names:
        msg_lines = [
            "NEW GRASS ADDON MODULES DETECTED",
            "",
            "The following modules exist in the grass-addons source tree but are",
            "NOT yet in ADDONS_MENU.md.  They have been appended with ⚠️ NEW.",
            "Please open ADDONS_MENU.md and assign a proper Menu Path:",
            "",
        ]
        for n in new_names:
            path = default_path(n)
            msg_lines.append(f"  {n:40s} → {path}  ⚠️ NEW")
        msg_lines += [
            "",
            f"File: {md_path}",
            "Re-run cmake after editing ADDONS_MENU.md.",
        ]
        warn("\n".join(msg_lines))

        # Append new entries to the .md
        new_rows = []
        for n in new_names:
            _, label = disk_modules[n]
            path = default_path(n) + "  ⚠️ NEW — review placement"
            new_rows.append((n, path, label, ""))

        # Rewrite .md with new rows at bottom
        all_entries = known_entries + new_rows
        write_md(md_path, MD_HEADER, all_entries)
        print(f"scan_addons.py: updated {md_path} with {len(new_names)} new module(s)")

    # ── 4. Write grassaddonslist.h from the (updated) .md ────────────
    # Re-read to pick up the new rows we may have just written
    final_entries = parse_md(md_path) if new_names else known_entries

    def esc(s):
        return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", " ").strip()

    lines = [
        "// *** AUTO-GENERATED by cmake/scan_addons.py ***",
        f"// Source: {md_path}",
        "// Do NOT edit — edit ADDONS_MENU.md and re-run cmake.",
        "#pragma once",
        "",
        "#include <QList>",
        "#include <QString>",
        "",
        "namespace GrassAddons {",
        "",
        "struct AddonInfo {",
        "    const char *name;      // e.g. \"i.evapo.potrad\"",
        "    const char *label;     // human description (may be empty)",
        "    const char *menuPath;  // e.g. \"imagery/evapotranspiration\"",
        "    bool        isNew;     // true when marked ⚠️ NEW in the .md",
        "};",
        "",
        "inline const QList<AddonInfo> &allAddons() {",
        "    static const QList<AddonInfo> list = {",
    ]

    for name, menu_path, desc, _raw in final_entries:
        is_new = "true" if "⚠️ NEW" in menu_path else "false"
        # Strip the ⚠️ NEW annotation from the actual path used at runtime
        clean_path = menu_path.split("⚠️")[0].strip().rstrip("—").strip()
        lines.append(
            f'        {{ "{esc(name)}", "{esc(desc)}", "{esc(clean_path)}", {is_new} }},'
        )

    lines += [
        "    };",
        "    return list;",
        "}",
        "",
        "// Convenience: just the names",
        "inline QStringList allModules() {",
        "    QStringList out;",
        "    for (const auto &a : allAddons()) out << a.name;",
        "    return out;",
        "}",
        "",
        "} // namespace GrassAddons",
        "",
    ]

    output_h.write_text("\n".join(lines))
    total = len(final_entries)
    new_c  = len(new_names)
    print(f"scan_addons.py: wrote {total} addons "
          f"({new_c} new) to {output_h}")


if __name__ == "__main__":
    main()
