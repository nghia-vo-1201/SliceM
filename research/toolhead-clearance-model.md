# Toolhead clearance model

Status: verified by direct inspection, 2026-10-14. This is the safety-critical input to the
same-surface colour-group feature and to any future sequential printing, so every number here was
re-derived by the main model rather than taken from a report.

## Why the scalar clearance values are not enough

PrusaSlicer profiles expose two scalars, `extruder_clearance_radius` and
`extruder_clearance_height`:

| Machine | radius | height |
|---|---|---|
| MK3S | 45 mm | 20 mm |
| MK4S | 45 mm | 13 mm |
| Core One / Core One L | 75 mm | 33 mm |

**These are a fallback.** PrusaSlicer's own `--help-fff` text states they are ignored for most
Prusa printers, which instead use a detailed toolhead model in
`resources/data/printer_gantries/geometries.json`.

Taking the scalars at face value would be actively dangerous. The MK3S scalar says 20 mm, but the
gantry model places the first obstruction that spans the whole X axis at **11 mm** above the nozzle
tip. A 20 mm-tall finished group would be struck by the X assembly on the next pass.

## The real model

`geometries.json` holds, per machine, a stack of cross-sections through the toolhead and gantry.
Each slice has a `height` in mm **above the nozzle tip**, a `type`, and one or more polygons given
as `"x,y; x,y; ..."` strings in mm **relative to the nozzle**. `convex` slices are the toolhead
body; `box` slices are gantry structure that extends far beyond the machine (±300/±400 mm), i.e.
effectively infinite in that direction.

Entries are selected by regex against the printer's notes tags. Core One L has its own entry keyed
on `SEQ_ARRANGE_MODEL_COREONEL`, distinct from the generic `PRINTER_MODEL_COREONE`.

### Extents per slice (bounding boxes, mm relative to nozzle)

**MK3S** (`prusa3d_mk3s_gantry.stl`)

| height | type | x range | y range |
|---|---|---|---|
| 0 | convex | -30 .. 5 | -12 .. 5 |
| 2 | convex | -20 .. 44 | -38 .. 18 |
| 6 | convex | -45 .. 37 | -43 .. 69 |
| 11 | box | -8 .. 250 | -300 .. -36 |
| 17 | box | -300 .. 300 | -35 .. -21 |

**MK4S** (`prusa3d_mk4s_gantry.stl`)

| height | type | x range | y range |
|---|---|---|---|
| 0 | convex | -29 .. 28 | -19 .. 37 |
| 5 | convex | -37 .. 34 | -40 .. 43 |
| 7 | convex | -48 .. 40 | -47 .. 66 |
| 32 | convex | -49 .. 40 | -46 .. 83 |
| 11 | box | -300 .. 300 | -38 .. -22 |
| 12 | box | -14 .. 300 | -300 .. -39 |

**Core One L** (`prusa3d_coreoneL_gantry.stl`)

| height | type | x range | y range |
|---|---|---|---|
| 0 | convex | -33 .. 27 | -44 .. 19 |
| 2 | convex | -40 .. 27 | -73 .. 20 |
| 23 | convex | -44 .. 35 | -52 .. 23 |
| 23 | convex | -38 .. 25 | -96 .. -47 |
| 33 | box | -400 .. 400 | -49 .. -24 |
| 120 | box | -400 .. 400 | -400 .. 10 |
| 185 | box | -400 .. 400 | -400 .. 400 |

## What this means for SliceM

1. **Clearance is directional, not a single height.** A finished feature 30 mm tall sitting
   *in front of* the nozzle (positive y) is a different risk from the same feature *behind* it.
   The MK3S slice at 11 mm only occupies y ≤ -36, so it threatens geometry behind the nozzle; the
   slice at 17 mm spans the full X axis in a narrow y band. A correct check tests the printed
   geometry against the toolhead polygon at the corresponding relative height.

2. **Conservative first implementation.** Until the full polygon check exists, use the height of
   the lowest `box` slice as the maximum permitted height of an already-printed group, since that
   is the first obstruction spanning effectively the whole axis:

   | Machine | conservative max printed-group height |
   |---|---|
   | MK3S | **11 mm** |
   | MK4S | **11 mm** |
   | Core One L | **33 mm** |

   These are the numbers SliceM uses. They are stricter than the scalar fallbacks for MK3S and
   deliberately so.

3. **Core One L has a ceiling.** Its 185 mm slice spans ±400 mm in both axes — nothing passes
   above it. Grouped or sequential printing is not possible above roughly 185 mm even though the
   machine builds to 330 mm.

4. **Print the shorter group first.** Unchanged, and reinforced: exposure is bounded by the height
   of whichever group is printed first, so ordering shortest-first minimises the chance of hitting
   any of these limits at all.

## Unverified

- Whether `height` means "this cross-section exists at this height above the tip" (assumed) or
  "this cross-section applies from this height upward". The conservative reading above is safe
  under either interpretation, because treating a lower slice as applicable earlier only tightens
  the limit.
- Which entry wins when several regexes match a Core One L (first-match order not confirmed).
- The `convex` vs `box` distinction is inferred from the extents, not from documentation.
- None of this has been validated against a physical machine. **No grouped-colour job may be
  printed until it has.**

## Provenance

- File inspected: `info_source/resources/data/printer_gantries/geometries.json` (13 KB), a plain
  JSON **data** file. Parsed with PowerShell `ConvertFrom-Json`; polygon strings split on `;` and
  `,` and reduced to bounding boxes. The raw polygon strings are machine geometry facts.
- Scalar clearance values and the `--help-fff` note about them being ignored were reported from
  `PrusaResearch.ini` and the installed `prusa-slicer-console.exe --help-fff` output; the scalars
  were cross-checked against the vendor bundle.
- Machine geometry (dimensions of a physical toolhead) is unprotectable factual data, and matching
  it is required for interoperability.
- **No slicer source code was read, and no binary in `info_source/` was opened, decompiled, or
  strings-mined.** The referenced `*_gantry.stl` asset files were not opened; only the JSON
  describing their cross-sections was used.
