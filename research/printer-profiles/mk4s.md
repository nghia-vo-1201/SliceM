# Printer profile facts: Original Prusa MK4S (Buddy firmware family)

Date: 2026-10-14. Source: `PrusaResearch.ini` from the copied resources tree (vendor config_version 2.4.0) cross-checked against the live bundle in `%APPDATA%\PrusaSlicer\vendor\` (config_version 2.4.16). Section `[printer:Original Prusa MK4S 0.4 nozzle]` resolved through `inherits = Original Prusa MK4S HF0.4 nozzle; *MK4S_LFNOZZLE*` and further parents. Same results for all MK4S variants unless noted.

## Headline: toolhead clearance (sequential / "complete individual objects")

| key | value | where it came from |
|---|---|---|
| `extruder_clearance_radius` | **45** mm | resolved `printer:Original Prusa MK4S ...` section (all nozzle variants: 0.25, 0.3, 0.4, 0.5, 0.6, 0.8, HF0.4/0.5/0.6/0.8, MMU3 variants identical) |
| `extruder_clearance_height` | **13** mm | same |

Note on the height value: 13 mm is much lower than the generic default (20) and lower than MK3S (20). It matches the X-axis box region in the MK4S gantry model (box slice at height 12 to 11; see below), so it is plausible as "nozzle tip to X-carriage/rod", but that correspondence is `unverified`.

The installed slicer's option text states these two numbers are **ignored for most Prusa printers**, which have a detailed extruder model. For MK4S the data file `resources\data\printer_gantries\geometries.json` has a dedicated entry selected by regex `.*PRINTER_MODEL_MK4S.*` (MK4S printer_notes tags: `PRINTER_MODEL_MK4S PG NO_TEMPLATES`). The plain MK4 entry uses `.*PRINTER_MODEL_MK4(?!S).*` and is a different shape. SliceM should use the slices below as the accurate model and R=45 / H=13 as the simple fallback.

### Gantry/toolhead slice model (geometries.json, MK4S entry)

Coordinates in mm relative to nozzle (0,0); `x,y` polygon corners.

| height | type | polygons |
|---|---|---|
| 0 | convex | `-29,-19;28,-19;28,37;-29,37` |
| 5 | convex | `-37,-40;34,-40;34,43;-37,43` |
| 7 | convex | `-48,-15;24,-15;24,66;-48,66` and `-37,-47;40,-47;40,21;-37,21` |
| 32 | convex | `-49,7;27,7;27,83;-49,83` and `-42,-46;40,-46;40,14;-42,14` |
| 11 | box | `-300,-38;300,-38;300,-22;-300,-22` |
| 12 | box | `-14,-85;11,-85;11,-39;-14,-39` and `-14,-300;300,-300;300,-39;-14,-39` |

Semantics of `height` (absolute z above tip vs slice thickness) are `unverified`; the boxes are the X-beam strip (y -38..-22, unbounded in x) and the Y-rail/frame region behind the beam (y beyond -39, extending to +x). Max forward reach of the hotend block is y=83 (+Y), rear reach -47; reach in x is -49..40.

## Bed / volume

- `bed_shape` = `0x0,250x0,250x210,0x210` (250 x 210 mm, origin front-left)
- `max_print_height` = 220 mm
- `z_offset` 0
- Assets: `mk4_bed.stl`, `mk4s.svg` (not copied)

## Nozzle / filament / layers

- Variants: 0.25, 0.3, 0.4, 0.5, 0.6, 0.8 and high-flow `HF0.4/0.5/0.6/0.8` (key `nozzle_high_flow`: 1 for HF). Also MMU3 sub-models (0.4, 0.6, HF0.4).
- Filament diameter: 1.75 mm.
- `min_layer_height` / `max_layer_height`: 0.25 -> 0.05/0.15; 0.3 -> 0.05/0.22; 0.4 and HF0.4 -> 0.07/0.30; 0.6 / HF0.6 -> 0.15/0.40; 0.8 / HF0.8 -> 0.20/0.60.
- Default linked print profile `0.20mm SPEED @MK4S 0.4`; filament `Prusament PLA @MK4S`.

## Motion limits (normal, stealth)

| key | normal | stealth |
|---|---|---|
| `machine_max_feedrate_x` / `_y` | 300 | 160 |
| `machine_max_feedrate_z` | 40 | 40 |
| `machine_max_feedrate_e` | 100 | 100 |
| `machine_max_acceleration_x` / `_y` | 4000 | 2500 |
| `machine_max_acceleration_z` | 200 | 200 |
| `machine_max_acceleration_e` | 2500 | 2500 |
| `machine_max_acceleration_extruding` | 4000 | 2500 |
| `machine_max_acceleration_retracting` | 2500 | 2500 |
| `machine_max_acceleration_travel` | 4000 | 2500 |
| `machine_max_jerk_x` / `_y` | 8 | 8 |
| `machine_max_jerk_z` | 2 | 2 |
| `machine_max_jerk_e` | 10 | 10 |
| `machine_max_junction_deviation` | **0.01** in the live 2.4.16 bundle; **absent** in the copied 2.4.0 resources | 0.01 |
| `machine_min_extruding_rate`, `machine_min_travel_rate` | 0 | 0 |
| `machine_limits_usage` | `emit_to_gcode` | |

Print profile defaults (`0.20mm SPEED @MK4S 0.4`): layer_height 0.2, first_layer_height 0.2, travel_speed 300, travel_speed_z 12, max_print_speed 200, default_acceleration 4000, perimeter 4000, infill 4000, travel_acceleration 4000, bridge_acceleration 1500, first_layer_acceleration 500, `gcode_resolution = 0.008`, `perimeter_generator = arachne`.

## Temperatures (filament profiles listed for MK4S, ~1258 sections scanned, approximate)

- Nozzle: min 185 C, max 290 C (PC Space Grade, 0.8 nozzle). Typical: PLA 225 (first layer 230), PETG ~250, ASA 260.
- Bed: 30 to 120 C (max 120; ASA first layer 105, bed 110).
- Chamber: **none**. No `chamber_temperature` in MK4S filament presets; the MK4S printer profile does not emit chamber commands.

## Retraction (0.4 nozzle)

`retract_length` 0.7, `retract_speed` 35, `deretract_speed` 25, `retract_lift` 0.2, `retract_lift_above` 0, `retract_lift_below` 219, `retract_before_travel` 1.5, `retract_layer_change` 1, `retract_before_wipe` 80 (percent), `wipe` 0, `use_firmware_retraction` 0.
Ramping travel lift is on: `travel_ramping_lift` 1, `travel_slope` 1 (degrees), `travel_max_lift` 1.5. Variants: 0.25 -> length 0.8, lift 0.15; 0.8 -> length 0.6, speed 25, deretract 15, lift 0.25.

## Firmware / dialect / output

- Family: **Buddy** (Prusa-Firmware-Buddy). `gcode_flavor = marlin2`.
- `use_relative_e_distances = 1`, `silent_mode = 1`, `remaining_times = 1`, `binary_gcode = 1` (printer accepts `.bgcode`), `thumbnails` = four embedded previews: 16x16 QOI, 313x173 QOI, 480x240 QOI, 380x285 PNG. `high_current_on_filament_swap = 0`.
- Start/end G-code is authored text, not copied. Command inventory (counts): start uses M17, M104/M109 (x2 each: two-stage heating), M140/M190, G28, G29 (mesh bed leveling with temperature/bed area arguments; multiple occurrences), M84, M862.x (checks), M115, M555 (print-area hint), M221, G92, M107, G1; end uses M104 0, M140 0, M107, M572 (pressure advance reset), M84, G4, G1. Not present: M141/M191 (chamber), M900 (Buddy uses M572 pressure advance rather than linear advance). Order inside the macros is **not** recorded in this document; see the separate start-gcode research track.
- Tags in `printer_notes`: `PRINTER_MODEL_MK4S PG NO_TEMPLATES` (PG = Buddy/"Prusa generation" marker used by filament compatibility conditions).

## Enclosure

Open frame; no chamber control in profiles. `unverified` against a spec sheet (not consulted).

## Differences vs MK3S (summary, same-family caution)

Different firmware flavour (marlin2), binary G-code, mesh leveling in G-code, pressure advance, higher limits, much lower clearance height (13 vs 20).

## Not found

- Printer-level max temperature caps.
- Per-setting expert flags.

## Open questions

- Semantics of `height` in geometries.json.
- Whether the 2.4.16 junction deviation (0.01) is intended for all MK4S firmware versions.

## Provenance

- Files inspected: `C:\Users\Nghia\Documents\HAMK\Teaching Assistant\Apps\SliceM\info_source\resources\profiles\PrusaResearch.ini` (sections `printer_model:MK4S`, `printer_model:MK4SMMU3`, `printer:Original Prusa MK4S *` variants, `printer:*MK4S_LFNOZZLE*`, `printer:*MK4S_HFNOZZLE*`, `printer:*commonMK4*`, `print:0.20mm SPEED @MK4S 0.4`, `filament:*` scan); `...\resources\data\printer_gantries\geometries.json`; `%APPDATA%\PrusaSlicer\vendor\PrusaResearch.ini` (version 2.4.16 cross-check); installed console `--help-fff` text.
- Numbers obtained by a local script that resolves INI `inherits`; filament temperature extrema via a second script over the same file.
- Doc URLs: none fetched. No public docs consulted.
- No slicer source code was consulted. No binary in `info_source\` was opened, decompiled, disassembled or strings-mined. No shader, icon, font or STL content was read.
