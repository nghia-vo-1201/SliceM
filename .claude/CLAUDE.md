# SliceM — standing instructions

SliceM is an **independently authored** FDM slicer (C++17 engine + Qt GUI) that emits G-code for
Prusa printers. It is built by clean-room reverse engineering of the installed PrusaSlicer, and its
reason to exist is one feature PrusaSlicer does not have: **same-surface sequential multi-colour
printing** (see below).

## 1. The hard rule: clean room

Read `rules/clean-room-boundary.md` before any research task. In short: **never open
PrusaSlicer / Slic3r / libslic3r / OrcaSlicer / Cura / CuraEngine / BambuStudio source code**, and
never decompile `PrusaSlicer.dll`. Knowledge about PrusaSlicer comes only from black-box
observation (running it, reading what it *writes*) and public end-user documentation. This keeps
SliceM free of AGPL obligations. Violating it contaminates the whole codebase, so it is not a
preference — it is the project's licence foundation.

## 2. The flagship feature

PrusaSlicer can only change colour at a **layer** boundary: it pauses after layer N, the user
swaps filament, printing resumes at layer N+1. Everything on one layer is therefore one colour.

SliceM adds: when two or more features rise from the **same flat surface** and are all
**top-most** (no model geometry is printed after them), SliceM reorders the toolpaths by *feature
group* instead of by layer:

```
print everything up to the shared surface
  -> pause for filament change -> print feature-group A completely (bottom-up)
  -> pause for filament change -> print feature-group B completely (bottom-up)
```

Canonical case: two blocks of embossed text on one flat face, each in its own colour.

### Safety constraint that defines this feature's limits

This reordering recreates the collision hazard of sequential ("complete individual objects")
printing. On an MK3-class machine the bed moves in Y and the X-gantry rods plus extruder body
sweep across the part at roughly nozzle height; on MK4/Core One the toolhead body does the same.
Once group A stands at height `H_A`, printing group B at any `z < H_A` can drag the finished group
A into the toolhead or gantry.

Therefore:

- **Print the shorter group first.** Exposure is bounded by the height of the group printed first,
  so ordering shortest-first strictly minimises risk.
- **Cap the first group's height** at the verified limit in `research/toolhead-clearance-model.md`
  — MK3S 11 mm, MK4S 11 mm, Core One L 33 mm — and clear the toolhead cross-section in XY.
  Never use PrusaSlicer's scalar `extruder_clearance_height`: it is an ignored fallback and is
  optimistic by 9 mm on the MK3S.
- If that cannot be satisfied, **refuse and fall back** to a plain layer-wise colour change with an
  explicit explanation. Never silently emit a path that was not clearance-verified.

See `rules/printer-safety.md`.

## 3. Target printers

Profiles confirmed present in the installed vendor bundle: `MK3`, `MK3S`, `MK4`, `MK4S`,
`COREONE`, `COREONEL`. Two distinct firmware families, which means two distinct G-code dialects:

| Family | Models | Firmware |
|---|---|---|
| Legacy | MK3, MK3S | Prusa-Firmware (Marlin derivative) |
| Buddy | MK4, MK4S, Core One, Core One L | Prusa-Firmware-Buddy |

Do not assume a command valid on one family is valid on the other. Each dialect needs its own
research doc under `research/`.

## 4. Layout

```
engine/      C++ core library — MUST NOT depend on Qt or any GUI library
  geometry/  2D polygon ops (clip/offset), original implementations
  slicing/   mesh -> per-layer polygons
  infill/    infill patterns, top/bottom solid-region detection
  gcode/     G-code emission + printer profiles
  scheduling/ layer ordering + the same-surface colour-group algorithm
cli/         thin harness over engine/ — the main way milestones get validated
gui/         Qt application (minimal: load, slice, preview, export)
research/    clean-room black-box findings; every doc needs a Provenance section
tests/       unit tests, incl. degenerate-geometry cases
```

## 5. Toolchain

MSYS2 UCRT64 at `C:\msys64`: gcc 16.2, CMake 4.4.4, Ninja 1.13.2, Qt 6.11.2, git 2.56.
Qt lives only on the UCRT64 PATH, so build and run from that environment:

```powershell
$env:MSYSTEM='UCRT64'
& C:\msys64\usr\bin\bash.exe -lc "cd '/c/Users/Nghia/Documents/HAMK/Teaching Assistant/Apps/SliceM' && cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure"
```

Gotcha: launching `build/gui/slicem-gui.exe` from plain PowerShell fails on missing Qt DLLs
unless `C:\msys64\ucrt64\bin` is prepended to `PATH`.

Build artifacts: `build/cli/slicem.exe`, `build/gui/slicem-gui.exe`, `build/tests/slicem_tests.exe`.

## 6. Working agreements

- **Find code via the Graphify graph, not by grepping the tree** — `graphify query "<question>"
  --budget 1500`, then read only the reported ranges. Run `graphify update .` after changing code.
  This is the project's main token-saving habit: `rules/graphify-navigation.md`.
- Delegate bounded, mechanical subtasks to subagents and verify their output yourself —
  `rules/delegation.md`.
- `engine/` stays Qt-free; geometry uses scaled integer coordinates, never float equality —
  `rules/cpp-style.md`.
- Any G-code headed for a physical printer passes the `gcode-validation` skill first, and is
  validated on MK3S before MK4, and on MK4 before Core One L.
- State what is verified vs. merely written. Unbuilt code, unprinted G-code, and untested
  algorithms get said out loud.
