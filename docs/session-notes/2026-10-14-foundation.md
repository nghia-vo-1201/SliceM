# Session note — 2026-10-14

First build session. Took SliceM from an empty folder to a slicer that loads meshes, slices them,
generates perimeters and infill, shows them in a 3D viewport, and can ship itself as a release.

## What exists now

| Area | State |
|---|---|
| Clean-room process | `.claude/` with CLAUDE.md, 5 rules, 4 skills, 4 subagent personas |
| Mesh input | ASCII + binary STL, vertex welding onto a 1 nm integer grid |
| Slicing | mesh plane-intersection → closed per-layer contours |
| Geometry | polygon offsetting with self-intersection repair, ring orientation by nesting |
| Toolpaths | N perimeters, rectilinear infill (density, angle, serpentine) |
| GUI | Qt 6 OpenGL viewport: shaded solid, build plate, orbit/pan/zoom, layer slider |
| CLI | `slicem.exe` headless harness |
| Release | version in CMake → `slicem_version.h`, packaging script, `gh` release script, CI workflow, in-app update check |
| Tests | 651 checks across 4 suites, warning-free under `-Wall -Wextra -Wpedantic` |

Commits: `8648d4f` scaffold → `4123392` offsetting/viewport/clearance → `89c238b` perimeters/infill/release.

## Decisions that shape everything downstream

**Geometry is quantised to a 1 nm integer grid at load.** Shared mesh edges then compare
bit-exactly, so contour chaining and offset re-chaining need no floating-point tolerance at all.
This is the reason the slicer has almost no epsilons in it.

**A vertex exactly on the slice plane counts as "above".** One convention collapses coplanar
facets, grazing vertices and the division-by-zero case into ordinary code paths. Layer planes are
also sampled mid-layer, which keeps them off the flat faces of axis-aligned models.

**Offsetting is offset-then-repair, not per-vertex displacement.** Displaced edges, round joins,
split every self-intersection, discard pieces closer to the input than |delta|. That discard step
is what makes features thinner than `2*|delta|` vanish instead of reappearing inside out.

**Clean room held.** `info_source/` was treated as two categories: `resources/` (configuration
data — used freely) and the binaries (never opened, decompiled, or strings-mined). `info_source/`
is gitignored so none of it is redistributed. All 8 research docs carry provenance.

## The most important finding

PrusaSlicer's scalar `extruder_clearance_height` is a **fallback that the real machines ignore**.
The actual toolhead model is a stack of cross-sections in
`resources/data/printer_gantries/geometries.json`.

For the MK3S the scalar says 20 mm, but the gantry data puts the first full-width obstruction at
**11 mm**. Building the colour-group feature on the scalar would have authorised a 20 mm-tall first
group that the X assembly then ploughs into. Verified first-hand and written up in
`research/toolhead-clearance-model.md`; the conservative limits now in use are MK3S 11 mm,
MK4S 11 mm, Core One L 33 mm. Core One L also has a hard ceiling near 185 mm.

Clearance is **directional**, not a radius — the obstruction occupies a specific band relative to
the nozzle. The scalar radius is only acceptable as an over-estimate.

## Open questions for Nghia

1. **`M600`/`M601` behaviour on your machines.** The docs do not state whether the printer holds
   temperature, waits for the user, or auto-resumes. You have the hardware; this needs confirming
   before any grouped-colour job is attempted.
2. **Publishing to GitHub.** The remote is configured (`origin` → `nghia-vo-1201/SliceM`, branch
   `main`) but **nothing has been pushed**. The repo is public, so the first push publishes
   everything including the research notes.
3. **Licence** is still undecided, which the clean-room process deliberately preserved.

## Next

Shortest path to something printable, in order:

1. Top/bottom solid surface detection — needs polygon **boolean difference** between adjacent
   layers. This is the next hard algorithm and the last big geometry gap.
2. G-code emission for MK3S first (simplest dialect), from the requirement lists in
   `research/gcode-dialect-legacy-mk3.md`. Extrusion maths, temperatures, fan, retraction, skirt.
3. Validate in `prusa-gcodeviewer`, diff aggregates against a PrusaSlicer reference slice, then
   print a calibration cube on the MK3S.
4. Only then the colour-group feature: layer-to-layer component grouping, top-most detection,
   shortest-first ordering, clearance check against the gantry model, and refusal with fallback.

`research/slicer-feature-landscape.md` holds the full roadmap to a comprehensive slicer, and
`research/slicer-settings-taxonomy.md` lists 364 settings as a feature checklist.

## Process notes

Delegation worked well: four subagents produced the research, the viewport, and perimeters/infill
in parallel while the polygon offsetter — the one piece everything depends on — was kept in-house.
Verification caught real things each time, including an agent correcting a wrong figure in its own
brief rather than coding to it. Raw G-code and build output stayed in scratch, out of the repo.
