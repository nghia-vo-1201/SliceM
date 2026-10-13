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

Working today: STL loading (ASCII and binary, with vertex welding), mesh plane-intersection
slicing into closed per-layer contours, a CLI harness, and a minimal Qt window that loads a model
and reports its layer geometry. A 20 mm cube at 0.2 mm slices to exactly 100 layers of 400.000 mm²
with no open contours, which is the analytic answer.

Not implemented yet: perimeters, infill, top-surface detection, supports, G-code export, printer
profiles, and the same-surface colour-group feature itself. **SliceM cannot yet produce anything
printable.**

## Build

Requires MSYS2 UCRT64 (gcc, CMake, Ninja, Qt 6):

```powershell
$env:MSYSTEM='UCRT64'
& C:\msys64\usr\bin\bash.exe -lc "cd '/c/path/to/SliceM' && cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure"
```

```
build/cli/slicem.exe --per-layer --layer-height 0.2 model.stl
```

Running the GUI needs `C:\msys64\ucrt64\bin` on `PATH` for the Qt DLLs.

## Licence

Undecided. The strict clean-room process is what keeps that choice open.
