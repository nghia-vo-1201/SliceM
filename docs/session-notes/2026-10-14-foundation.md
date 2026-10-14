# Session note — 2026-10-14

First build session. Took SliceM from an empty folder to a slicer that loads meshes, slices them,
generates perimeters, solid surfaces and infill, shows them in a 3D viewport, **emits MK3S G-code**,
and can ship itself as a release.

## What exists now

| Area | State |
|---|---|
| Clean-room process | `.claude/` with CLAUDE.md, 5 rules, 4 skills, 4 subagent personas |
| Mesh input | ASCII + binary STL, vertex welding onto a 1 nm integer grid |
| Slicing | mesh plane-intersection → closed per-layer contours |
| Geometry | polygon offsetting with self-intersection repair, ring orientation by nesting |
| Booleans | union / intersection / difference on scaled-integer rings, shared-boundary safe |
| Surfaces | top/bottom solid-region detection by boolean difference against neighbouring layers |
| Toolpaths | N perimeters, rectilinear infill (density, angle, serpentine) |
| G-code | MK3S (legacy family) emitter: start/end sequences, extrusion maths, retraction, skirt, fan, surface-driven solid shell |
| GUI | Qt 6 OpenGL viewport: shaded solid, build plate, orbit/pan/zoom, layer slider |
| CLI | `slicem.exe` headless harness, `--gcode` export |
| Release | version in CMake → `slicem_version.h`, packaging script, `gh` release script, CI workflow, in-app update check |
| Tests | 1031 checks across 7 suites, warning-free under `-Wall -Wextra -Wpedantic` |

Commits: `8648d4f` scaffold → `4123392` offsetting/viewport/clearance → `89c238b` perimeters/infill/release
→ `c398acf` session note → `b9f87c0` merge of the GitHub repo's own initial commit → booleans/surfaces/G-code.

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

## The second finding: coincident boundaries break the obvious boolean

The polygon boolean was specified the textbook way — split every edge at its intersections, then
keep or drop each piece by testing its **midpoint** against the other operand. That rule cannot
classify a piece whose entire length lies *on* the other operand's boundary, because the midpoint
is neither inside nor outside.

It is not a corner case here. Top/bottom surface detection subtracts layer `i+1` from layer `i`,
and adjacent layers of an extruded solid share most of their boundary exactly — after
quantisation, bit-exactly. A one-sided staircase step produced **0 rings where 200 were correct**:
every top surface would have silently vanished, and the part would have printed open.

Fixed by splitting at collinear overlaps and resolving shared boundaries by the two operands'
local interior sides (the Martínez–Rueda–Feito 2009 treatment), pinned by tests that fail loudly
on the old behaviour. Found by the subagent implementing it, against the brief I wrote — worth
recording, because the brief was wrong and the test was what caught it.

## The third finding: a T-junction is not a crossing

Wiring surface detection into the emitter immediately produced a wrong answer — 14 solid layers on
a cube instead of 6, in a scattered pattern. The cause was not the new code. `generate_perimeters`
was returning an **empty infill region** on exactly two of the cube's 100 layers, so those layers
printed with no infill at all, and the empty region then read as an exposed surface in both
directions and pulled a solid shell around it.

The two layers are z = 0.9 and z = 19.1, where the box's facet diagonals cross the slice plane and
the contour comes out with 8 vertices instead of 4 — four of them collinear, 0.9 mm from a corner.
Inset by exactly 0.9 mm, two displaced edges meet precisely at the endpoint of a third. That is a
**T-junction**, not a crossing, and `interior_intersection` requires both parameters strictly
inside (0, 1), so it was never split. The unsplit crossbar left a spur, the chaining walk turned
into it, the ring never closed, and the whole loop was dropped silently.

The failure is razor-thin: the same contour offsets correctly at −0.225, −0.45, −0.675, −1.0 and
−2.0 mm, and wrongly only at −0.9. Fixed by splitting every segment wherever another segment's
endpoint lies strictly in its interior, tested exactly on the integer grid. Pinned by a test that
offsets the plain and the collinear-vertex square at seven distances and requires identical
results.

Worth stating plainly: this bug was in the shipped code before surface detection existed, and no
test caught it. What caught it was wiring two subsystems together and finding the answer did not
match an oracle computable by hand.

## G-code status: validated structurally, never printed

`slicem.exe --gcode` on the 20 mm calibration cube produces a 212 KB MK3S file: 100 layers,
top z 20.000 mm, 33615.402 mm of path, 3025.386 mm³ deposited, 1257.809 mm of filament, all
deposition inside x 111.775–138.225 / y 91.775–118.225 (cube centred on the bed plus a 3 mm skirt).
Solid infill lands on layers 1–3 and 98–100 and nowhere else; the other 94 layers are sparse.

Checked three independent ways:

1. **A validation script written against the file, not the engine** — `G21`/`G90`/`M83` all set
   before the first extrusion, every coordinate inside the MK3S volume, no NaN or inf, 100
   `;LAYER_CHANGE` markers, and `M104 S0`/`M140 S0`/`M107`/`M84` all present at the end. My first
   run of it was wrong: it summed only positive `E` and so counted every de-retraction twice.
   Net `E` is 1257.009 mm against a geometric 1257.809 — short by exactly 0.800 mm, which is the
   one retraction at the end of the job that is never recovered.
2. **`prusa-gcodeviewer` 2.9.4** loads it and renders a solid cube with its skirt on the bed; the
   layer slider reads 0.20 (1) to 20.00 (100); its own totals are 1.26 m and 3.77 g, which match
   our footer (1257.81 mm, 3.02539 cm³ at PLA's 1.24 g/cm³) independently.
3. Unit tests in `tests/test_gcode.cpp`, including one requiring every layer of the cube to carry
   infill and the solid shell to land on exactly the first and last three layers.

Known gaps, all deliberate rather than overlooked: no `M73` progress, `M115`, `M907` or windowed
`G80`; no purge line (the skirt primes instead); `M900 K0` and `M221 S100` are emitted as explicit
neutral states; at ≤20% infill the 2.25 mm line spacing exceeds the 2 mm retract threshold, giving
~1180 retractions — safe but slow, and the fix is travel-stays-inside-region logic; fan is a flat
100% from layer 2; no colour change; no Buddy dialect.

**This has not been printed.** It passed structural validation and a viewer pass. That is not the
same as being safe on hardware, and the first run on the MK3S must be a watched one.

## Open questions for Nghia

1. **`M600`/`M601` behaviour on your machines.** The docs do not state whether the printer holds
   temperature, waits for the user, or auto-resumes. You have the hardware; this needs confirming
   before any grouped-colour job is attempted.
2. **Publishing is blocked on a token permission.** `origin` → `nghia-vo-1201/SliceM` is
   configured, the repo's own "Initial commit" has been merged in locally (so its history is
   preserved, no force-push needed), and local `main` is several commits ahead and ready.

   `git push` returns **403: Permission to nghia-vo-1201/SliceM.git denied**, even though the API
   reports `push: true` — that field reflects the *account's* role, not the *token's* grants. A
   test push of a commit containing no workflow files failed identically, which rules out the
   Workflows scope and pins it on **Contents**.

   Fix: at github.com/settings/tokens, edit the fine-grained PAT for this repo and grant
   **Contents: Read and write** (also **Workflows: Read and write**, since the push includes
   `.github/workflows/release.yml`). Then:

   ```powershell
   $env:MSYSTEM='UCRT64'
   & C:\msys64\usr\bin\bash.exe -lc "cd '/c/Users/Nghia/Documents/HAMK/Teaching Assistant/Apps/SliceM' && git push -u origin main"
   ```
3. **Licence** is still undecided, which the clean-room process deliberately preserved.

## Next

Shortest path to something printable, in order:

1. Travel-stays-inside-region logic, to stop retracting ~1180 times on a 20 mm cube.
2. Differential aggregate comparison against a PrusaSlicer reference slice of the same cube —
   layer count, deposited volume, bounding box. Black-box comparison of *outputs*, which the
   clean-room rule permits.
4. **Print the cube on the MK3S**, watched, with the stop control in reach.
5. Then the colour-group feature: layer-to-layer component grouping, top-most detection,
   shortest-first ordering, clearance check against the gantry model, and refusal with fallback.
   Designed in `docs/design/colour-group-feature.md`; must not be built before step 4.

`research/slicer-feature-landscape.md` holds the full roadmap to a comprehensive slicer, and
`research/slicer-settings-taxonomy.md` lists 364 settings as a feature checklist.

## Process notes

Delegation worked well: four subagents produced the research, the viewport, and perimeters/infill
in parallel while the polygon offsetter — the one piece everything depends on — was kept in-house.
Verification caught real things each time, including an agent correcting a wrong figure in its own
brief rather than coding to it. Raw G-code and build output stayed in scratch, out of the repo.
