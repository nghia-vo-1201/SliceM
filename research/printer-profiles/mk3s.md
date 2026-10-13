# Printer profile facts: Original Prusa i3 MK3S / MK3S+ (legacy firmware family)

Date: 2026-10-14. All values taken from the shipped vendor profile `PrusaResearch.ini` (config version 2.4.0 header), section `[printer:Original Prusa i3 MK3S & MK3S+]` and its parent chain. Values were resolved by following `inherits` (parent first, child overrides). The copied resources bundle is vendor config_version 2.4.0; the live `%APPDATA%` bundle is the newer 2.4.16. For the MK3S the clearance, bed, height, flavour and limit values were identical in both (spot-checked); for MK4S they differ (see mk4s.md, junction deviation).

## Headline: toolhead clearance (sequential / "complete individual objects")

| key | value | where it came from |
|---|---|---|
| `extruder_clearance_radius` | **45** mm | resolved printer section; inherited from `[printer:*common*]`-chain parent `Original Prusa i3 MK3`, identical on 0.25/0.4/0.6/0.8 nozzle variants |
| `extruder_clearance_height` | **20** mm | same |

Important: the installed slicer's own option description says these two values are **ignored for most Prusa printers, which ship a more detailed extruder model**. For MK3 family that model is a data file `resources\data\printer_gantries\geometries.json`, entry selected by regex `.*PRINTER_MODEL_MK3.*` against the printer's notes tags (this profile carries tags `PRINTER_VENDOR_PRUSA3D PRINTER_MODEL_MK3`). It defines a stack of slices (see below). SliceM should therefore implement the simple cylinder (R=45, H=20) as the fallback and the slice stack as the accurate model.

### Gantry/toolhead slice model (geometries.json, MK3 entry)

Coordinates are mm relative to the nozzle (nozzle at 0,0), polygons as `x,y;...`. `type` is `convex` (hotend/fan shroud blocks, hull-able) or `box` (X-beam/rod regions extended far in one direction).

| height | type | polygons |
|---|---|---|
| 0 | convex | `-5,-5;5,-5;5,5;-5,5` and `-30,-12;-14,-12;-14,2;-30,2` |
| 2 | convex | `-20,-38;44,-38;44,18;-20,18` |
| 6 | convex | `-34,-43;37,-43;37,16;-34,16` and `-45,9;37,9;37,69;-45,69` |
| 11 | box | `-8,-82;8,-82;8,-36;-8,-36` and `-8,-82;250,-82;250,-300;-8,-300` |
| 17 | box | `-300,-35;300,-35;300,-21;-300,-21` |

Semantics of `height` (absolute height above nozzle tip at which that slice starts vs thickness of the slice) is **unverified**. Heights are not monotone in other printers' entries (e.g. MK4: 0,3,22,11,13), so they are most plausibly "z above nozzle tip at which the region applies", with boxes representing the X-beam / Y-rail regions (pattern is consistent with the beam at 17 mm vs the generic 20 mm clearance height). Asymmetry is real: the head extends further toward +Y (to 69) than -Y (to -43) and further +X (44) than -X (-45 only in the +Y block).
STL mesh for the visual gantry is `data\printer_gantries\prusa3d_mk3s_gantry.stl` (copyrighted asset; not used).

## Bed / volume

- `bed_shape` = `0x0,250x0,250x210,0x210` (rectangle, origin at front-left corner, 250 x 210 mm)
- `max_print_height` = 210 mm
- `z_offset` = 0
- Bed model asset `mk3_bed.stl`, texture `mk3.svg` (assets, not copied)

## Nozzle / filament / layers

- Variants in model entry: 0.25, 0.4, 0.6, 0.8 mm (`nozzle_diameter`). No high-flow variants.
- Filament diameter in Prusa filament profiles: 1.75 mm (`filament_diameter`).
- Layer height limits by nozzle (`min_layer_height` / `max_layer_height`): 0.25 -> 0.05/0.15; 0.4 -> 0.07/0.25; 0.6 -> 0.15/0.40; 0.8 -> 0.20/0.60.
- Default linked profiles (names only): print `0.15mm QUALITY @MK3`, filament `Prusament PLA`.

## Motion limits (two values each: normal, stealth)

| key | normal | stealth |
|---|---|---|
| `machine_max_feedrate_x` / `_y` | 200 | 100 |
| `machine_max_feedrate_z` | 12 | 12 |
| `machine_max_feedrate_e` | 120 | 120 |
| `machine_max_acceleration_x` / `_y` | 1000 | 960 |
| `machine_max_acceleration_z` | 200 | 200 |
| `machine_max_acceleration_e` | 5000 | 5000 |
| `machine_max_acceleration_extruding` | 1250 | 1250 |
| `machine_max_acceleration_retracting` | 1250 | 1250 |
| `machine_max_acceleration_travel` | not defined in this family | |
| `machine_max_jerk_x` / `_y` | 8 | 8 |
| `machine_max_jerk_z` | 0.4 | 0.4 |
| `machine_max_jerk_e` | 4.5 | 4.5 |
| `machine_min_extruding_rate`, `machine_min_travel_rate` | 0 | 0 |
| junction deviation | **not defined** (jerk model) | |
| `machine_limits_usage` | `emit_to_gcode` (limits are written into the G-code via M-codes) | |

Print profile defaults seen (`0.15mm QUALITY @MK3` via `*common*`): travel_speed 180, travel_speed_z 12, max_print_speed 200, default_acceleration 1000, perimeter_acceleration 800, infill_acceleration 1000, bridge_acceleration 1000, first_layer_acceleration 800, first_layer_height 0.2, `complete_objects = 0`.

## Temperatures (from filament profiles listed for this family, approximate scan of ~100 sections)

- Nozzle: lowest nonzero 195 C, highest 285 C (a PA-GF filament). Typical PLA 215, PETG 250, ASA/ABS ~255-260.
- Bed: 50 to 110 C observed in filament profiles (max first-layer 105).
- Chamber: none. No `chamber_temperature` keys in MK3-family filaments; the machine is an open frame.
- Printer-profile-level temperature limits (max nozzle/bed) are **not stored** in the profile; only filament presets carry temperatures.

## Retraction (printer level, 0.4 nozzle)

`retract_length` 0.8, `retract_speed` 35, `deretract_speed` 0 (= same as retract), `retract_lift` (Z-hop) 0.4, `retract_lift_above` 0, `retract_lift_below` 209, `retract_before_travel` 1 (family inherits 1; value as read), `retract_layer_change` 1, `retract_before_wipe` 0%, `wipe` 1, `retract_length_toolchange` 4, `use_firmware_retraction` 0.
Variant differences: 0.25 nozzle lift 0.15; 0.6 nozzle deretract 25; 0.8 nozzle retract_length 0.7, lift 0.25, deretract 20.

## Firmware / dialect / output

- Family: **legacy** (MK3 / MK3S, Einsy/Marlin-derived Prusa firmware). `gcode_flavor = marlin` (note: not `marlin2`, which Buddy machines use).
- `use_relative_e_distances = 1`, `use_volumetric_e = 0`, `silent_mode = 1` (stealth mode supported), `remaining_times = 1` (M73 progress emitted), `binary_gcode` not set (plain text G-code), `thumbnails = 160x120` (PNG default format), `variable_layer_height = 1`, `single_extruder_multi_material = 0`.
- Start/end G-code are authored text and are not reproduced. Command inventory (counted occurrences, order not asserted): start uses M104/M109 (nozzle), M140/M190 (bed), G28 (home), G80 (clear mesh), M862.x (printer/nozzle/gcode-level checks), M115, G92, M221 (flow), G1 moves; end uses M104 0, M140 0, M107, G1, G4, M84, M900 (linear advance reset), M221. No G29 mesh-bed run in the printer start macro and no M141/M191 (no chamber).
- The `printer_notes` tag set (`PRINTER_VENDOR_PRUSA3D PRINTER_MODEL_MK3`) is how print/filament presets and the gantry model are matched.

## Enclosure

Open frame (no chamber keys, no chamber commands). This is inference from profile content; physical spec-sheet not consulted: `unverified`.

## Not found

- Explicit "gantry" size in mm beyond the slice stack above.
- Max nozzle/bed temperature limits as printer-level keys (do not exist in profile).
- Per-setting expert mode flags.

## Open questions

- Semantics of `height` in geometries.json (see above).
- Whether `retract_before_travel = 1` vs another value is final after all parents are applied (the resolver's order is parent-first; not verified against an exported G-code).

## Provenance

- Files inspected: `C:\Users\Nghia\Documents\HAMK\Teaching Assistant\Apps\SliceM\info_source\resources\profiles\PrusaResearch.ini` (sections `printer_model:MK3`, `printer_model:MK3S`, `printer:Original Prusa i3 MK3S & MK3S+` and variants 0.25/0.6/0.8, `printer:*common*`, `print:*common*`, `print:0.15mm QUALITY @MK3`, `filament:*` temperature scan); `...\resources\data\printer_gantries\geometries.json`; `%APPDATA%\PrusaSlicer\vendor\PrusaResearch.ini` (spot-check of the same keys); `prusa-slicer-console.exe --help-fff` text (installed 2.9.4) for the description that clearance values are ignored for most Prusa printers.
- Method: a local script parsed the INI, followed `inherits` chains and printed resolved keys. G-code template keys were counted/tokenised, not copied.
- Doc URLs: none accessed (no public docs fetched). Printer spec claims (enclosure) are therefore `unverified`.
- No slicer source code was consulted. No binary (exe, dll) in `info_source\` was opened, decompiled, disassembled or strings-mined; only the installed console was run for its documented `--help-fff` output. No shaders, icons, fonts or STL contents were read.
