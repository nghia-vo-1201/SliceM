# SliceM

An independently authored FDM slicer (C++17 engine, Qt GUI) that emits G-code for Prusa printers —
MK3/MK3S, MK4/MK4S, and Core One / Core One L.

## Why it exists

PrusaSlicer can only change filament colour at a **layer** boundary: the print pauses after a
layer, the filament is swapped, and printing resumes on the next layer. Everything on a given layer
is therefore one colour.

SliceM adds **same-surface sequential multi-colour printing**. When two or more features rise from
the same flat surface and are all top-most — two blocks of embossed text on one face, say — SliceM
reorders the toolpaths by *feature group* rather than by layer:

```
print everything up to the shared surface
  -> pause for filament change -> print feature group A completely
  -> pause for filament change -> print feature group B completely
```

Each group gets its own colour, from one single-material printer, with no hardware modification.

### The constraint that bounds the feature

Reordering this way reintroduces the collision hazard of sequential printing: once one group stands
tall, printing the other at a lower Z can drive the finished group into the toolhead or gantry. So
SliceM prints the **shorter group first**, requires that group to fit under the printer's toolhead
clearance height with adequate horizontal separation, and otherwise **refuses** and falls back to a
conventional layer-wise colour change. The feature is allowed to be unavailable; it is not allowed
to be unsafe.

## Clean room

SliceM is built by clean-room reverse engineering. No slicer's source code is read — not
PrusaSlicer's, Slic3r's, Cura's, or any other. Everything learned about PrusaSlicer comes from
running the installed application, inspecting the files it writes, and reading public end-user and
firmware documentation. Findings live in `research/`, each with a provenance record.

Machine parameters (bed size, nozzle diameter, temperature and acceleration limits) are facts and
are used directly. Authored content such as start/end G-code templates is never copied; the
research notes record *which commands are required and in what order*, and SliceM authors its own.

See `.claude/rules/clean-room-boundary.md`.

## Layout

```
engine/        core library — no Qt, no GUI dependency
  geometry/    2D polygon operations on scaled integer coordinates
  slicing/     mesh -> per-layer polygons
  infill/      infill patterns, top/bottom solid-region detection
  gcode/       G-code emission and printer profiles
  scheduling/  layer ordering and same-surface colour-group detection
cli/           thin harness over engine/ — how milestones get validated
gui/           Qt application
research/      clean-room findings, each with provenance
tests/         unit tests, including degenerate-geometry cases
```

## Status

Early, but building and tested.

Working today:

- STL loading (ASCII and binary) with vertex welding onto a 1 nm integer grid
- mesh plane-intersection slicing into closed per-layer contours
- **2D polygon offsetting** with correct handling of the self-intersections offsetting creates, so
  features thinner than twice the offset vanish rather than inverting — this is what perimeters,
  infill boundaries, solid-surface detection and supports all build on
- **polygon booleans** (union, intersection, difference) that handle boundaries shared exactly
  between operands, which is the normal case when subtracting adjacent layers
- **top/bottom solid-surface detection**, and a solid shell grown back through the layers that
  carry each exposed surface
- perimeters and rectilinear infill
- **G-code export for the Original Prusa i3 MK3S**, with every emitted command traced to a
  research doc, and a writer that refuses rather than clamps when a move or temperature would
  leave the profile's limits
- a Qt **3D viewport**: shaded solid rendering, build plate, orbit/pan/zoom, and a layer-preview
  slider
- a CLI harness for headless validation

A 20 mm cube at 0.2 mm slices to exactly 100 layers of 400.000 mm² with no open contours, which is
the analytic answer.

Not implemented yet: supports, the Buddy-firmware dialect (MK4, Core One), and the same-surface
colour-group feature itself.

> **No G-code SliceM has produced has ever been printed.** It passes structural validation and a
> G-code viewer pass. That is not the same as being safe on hardware. Treat a first print as a
> watched run, on the cheapest machine you own.

`research/` holds the clean-room findings, including a verified toolhead clearance model derived
from gantry cross-section data — the input the colour-group feature's safety check depends on.

## Build

Requires MSYS2 UCRT64 (gcc, CMake, Ninja, Qt 6):

```powershell
$env:MSYSTEM='UCRT64'
& C:\msys64\usr\bin\bash.exe -lc "cd '/c/path/to/SliceM' && cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure"
```

## Running

The build copies the Qt and MinGW runtime next to the binaries, so both can be started directly —
including by double-clicking `build\gui\slicem-gui.exe` in Explorer. No `PATH` setup is needed.

```
build\gui\slicem-gui.exe model.stl         # load, preview layers, export G-code
build\cli\slicem.exe --per-layer --layer-height 0.2 model.stl
build\cli\slicem.exe --gcode out.gcode model.stl
```

That runtime deployment is what `scripts/deploy_runtime.ps1` does: `windeployqt` covers Qt's own
DLLs, but MSYS2's Qt is linked against roughly 25 further MinGW libraries it knows nothing about,
and missing any one of them makes the executable fail with `0xC0000135` before `main()` runs — no
window and no message. The script walks the real import tables instead of carrying a list that
goes stale with every toolchain update. Configure with `-DSLICEM_DEPLOY_QT=OFF` to skip it.

## Releases and updating

Releases are published to <https://github.com/nghia-vo-1201/SliceM/releases>.

The version lives in one place — the `project(SliceM VERSION ...)` line in `CMakeLists.txt` — and
both the packaging script and CI refuse to publish when a tag disagrees with it.

**Cutting a release locally**

```powershell
.\scripts\package.ps1          # Release build + tests + windeployqt + dist/SliceM-vX.Y.Z-win64.zip
.\scripts\release.ps1          # dry run: validates tag, clean tree, package, gh auth
.\scripts\release.ps1 -Confirm # tags, pushes and publishes
```

**Or via CI**: pushing a `v*` tag runs `.github/workflows/release.yml`, which builds under MSYS2,
runs the tests, checks the tag against the project version, and uploads the zip to the release.

The zip bundles the Qt and MinGW runtime DLLs via `windeployqt`, so it runs on a machine that has
never had MSYS2 installed.

**In-app update check**: *Help → Check for updates* asks the GitHub releases API for the newest tag
and offers to open the release page. It deliberately does **not** download or execute anything —
an updater that fetches and runs a binary is a remote code execution path, which is not a
reasonable thing to add to software that drives a 300 °C hotend to save one click.

## Licence

Undecided. The strict clean-room process is what keeps that choice open.
