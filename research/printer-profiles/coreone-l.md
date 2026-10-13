# Printer profile facts: Prusa CORE One L (Buddy firmware family, enclosed CoreXY)

Date: 2026-10-14. Source: `PrusaResearch.ini` in the copied resources tree (vendor config_version 2.4.0), cross-checked with the live bundle in `%APPDATA%\PrusaSlicer\vendor\` (config_version 2.4.16; clearance, bed, height, flavour and limits identical). Section `[printer:Prusa CORE One L 0.4 nozzle]` resolved via `inherits = Prusa CORE One L HF0.4 nozzle`. Model entry `[printer_model:COREONEL]`, family `CORE`.

## Headline: toolhead clearance (sequential / "complete individual objects")

| key | value | where it came from |
|---|---|---|
| `extruder_clearance_radius` | **75** mm | resolved Core One L printer sections; identical on every variant (0.25, 0.3, 0.4, 0.5, 0.6, 0.8, HF0.4/0.5/0.6/0.8). Set in the HF0.4 parent section. |
| `extruder_clearance_height` | **33** mm | same |

For comparison (read from the same file, different machine, not borrowed): the plain Core One also defines 75 / 33.

Caveat: the installed slicer states these two values are ignored for most Prusa printers because a detailed extruder model is used. For Core One L the model is in `resources\data\printer_gantries\geometries.json`, entry `.*SEQ_ARRANGE_MODEL_COREONEL.*` (the L's `printer_notes` carries tags `PRINTER_MODEL_COREONE PG NO_TEMPLATES SEQ_ARRANGE_MODEL_COREONEL`; the entry for the L comes before the generic `.*PRINTER_MODEL_COREONE.*` entry, so first-match ordering matters: unverified, but required for the L to get its own shape). Use slices below as the accurate model, R=75 / H=33 as fallback.

### Gantry/toolhead slice model (geometries.json, Core One L entry)

Coordinates in mm relative to nozzle (0,0).

| height | type | polygons |
|---|---|---|
| 0 | convex | `-33,-44;27,-44;27,19;-33,19` |
| 2 | convex | `-40,-73;27,-73;27,20;-40,20` |
| 23 | convex | `-44,-52;35,-52;35,23;-44,23` |
| 23 | convex | `-38,-96;25,-96;25,-47;-38,-47` |
| 33 | box | `-400,-49;400,-49;400,-24;-400,-24` |
| 120 | box | `-400,-400;400,-400;400,10;-400,10` |
| 185 | box | `-400,-400;400,-400;400,400;-400,400` |

Reading (`unverified`): the 33 mm box is the X-gantry beam (matches `extruder_clearance_height = 33`), 120 mm is the rear/upper structure behind the nozzle (y < 10) and 185 mm is a full-plane ceiling (top frame / gantry above 185 mm, so objects taller than this cannot be printed sequentially). The exact meaning of `height` (absolute z above tip vs thickness) is not settled by the data alone.
For the plain Core One the slices are the same except: slice0 `-33,-37;27,-37;27,19;-33,19`, slice2 `-40,-69;...`, rear block `-37,-98;26,-98;26,-47;-37,-47`, boxes limited to +-300 and ceiling at 170 mm. (This is Core One, not L; included only to show how the L differs.)

## Bed / volume

- `bed_shape` = `0x0,300x0,300x300,0x300` (300 x 300 mm square, origin front-left)
- `max_print_height` = 330 mm
- Assets: `coreonel_bed.stl`, `coreonel.svg` (not copied)

## Nozzle / filament / layers

- Variants: 0.25, 0.3, 0.4, 0.5, 0.6, 0.8 and high-flow HF0.4/0.5/0.6/0.8 (`nozzle_high_flow`). `cooling_tube_*`, `parking_pos_retraction` (84), `extra_loading_move` (-52), `multimaterial_purging` (140) exist for MMU3 use.
- Filament diameter 1.75 mm.
- Layer limits: 0.25 -> 0.05/0.15; 0.3 -> 0.05/0.22; 0.4 and HF0.4 -> 0.07/0.30; 0.6 -> 0.15/0.40; 0.8 -> 0.20/0.60.
- Default profiles: print `0.20mm SPEED @COREONEL 0.4`; filament `Prusament PLA @COREONE`.

## Motion limits (normal, stealth)

| key | normal | stealth |
|---|---|---|
| `machine_max_feedrate_x` / `_y` | 500 | 160 |
| `machine_max_feedrate_z` | 15 | 15 |
| `machine_max_feedrate_e` | 100 | 100 |
| `machine_max_acceleration_x` / `_y` | 10000 | 2500 |
| `machine_max_acceleration_z` | 400 | 200 |
| `machine_max_acceleration_e` | 5000 | 2500 |
| `machine_max_acceleration_extruding` | 7000 | 2500 |
| `machine_max_acceleration_retracting` | 2500 | 1200 |
| `machine_max_acceleration_travel` | 6000 | 2500 |
| `machine_max_jerk_x` / `_y` | 10 | 8 |
| `machine_max_jerk_z` | 2 | 2 |
| `machine_max_jerk_e` | 10 | 10 |
| `machine_max_junction_deviation` | 0.01 | 0.01 |
| `machine_limits_usage` | `emit_to_gcode` | |

Print profile defaults (`0.20mm SPEED @COREONEL 0.4`): layer 0.2, travel_speed 500, travel_speed_z 8, max_print_speed 300, default_acceleration 3000, perimeter 5000, infill 6000, travel 6000, bridge 1500, first layer 500, `gcode_resolution = 0.008`, arachne.

## Temperatures and chamber

- Nozzle in Core One filament presets (about 1258 sections scanned): 185 to 290 C.
- Bed: 30 to 120 C.
- **Chamber control present.** Filament presets carry `chamber_temperature` and `chamber_minimal_temperature`; observed maxima are 55 C nominal / 40 C minimum (ASA/ABS class). Examples: Prusament PLA @COREONE 20/0 (no chamber heating, chamber target 20); PETG 35/0; ASA 55/40. The printer start macro contains M141 (set chamber temp, no wait) and M191 (wait for chamber) alongside the usual nozzle/bed commands. The printer profile itself holds no chamber limit key.

## Retraction (0.4 nozzle)

`retract_length` 0.7, `retract_speed` 45, `deretract_speed` 25, `retract_lift` 0.2, `retract_lift_above` 0, `retract_lift_below` 329, `retract_before_travel` 1.5, `retract_layer_change` 1, `retract_before_wipe` 80%, `wipe` 0, `travel_ramping_lift` 1, `travel_slope` 1, `travel_max_lift` 1.5, `use_firmware_retraction` 0. Variants: 0.25 length 0.8 lift 0.15; 0.8 length 0.6, speed 25, deretract 15, lift 0.25.

## Firmware / dialect / output

- Family: **Buddy**. `gcode_flavor = marlin2`. `binary_gcode = 1`, `silent_mode = 1`, `remaining_times = 1`, `use_relative_e_distances = 1`, thumbnails 16x16 QOI, 313x173 QOI, 480x240 QOI, 380x285 PNG.
- Start/end G-code not copied. Command inventory (counts only): start uses M17, M104/M109 (3 each), M140/M190, M141/M191 (chamber), G28, G29 (mesh leveling, several forms), G5 (Bezier/purge-line move), M84, M862.x, M115, M555, M221, M107, G92, G1; end uses M104 0, M140 0, M141 (chamber off), M107 (3x), M572, M84, G4, G1. Ordering of those commands inside the macros is not recorded here.

## Enclosure

Enclosed (chamber commands and chamber temperature keys prove a chamber-aware profile). The physical enclosure statement is otherwise `unverified` against a spec sheet.

## Not found

- Chamber upper limit as a printer-level key (absent).
- Any `nozzle` temperature cap key (absent).
- Expert-mode flags.

## Open questions

- Gantry `height` semantics; first-match ordering of geometry entries.
- Whether sequential printing on Core One L is also constrained by a max print height lower than 330 (the 185 ceiling slice suggests a limit near 185 mm for sequential prints; `unverified`).

## Provenance

- Files inspected: `C:\Users\Nghia\Documents\HAMK\Teaching Assistant\Apps\SliceM\info_source\resources\profiles\PrusaResearch.ini` (sections `printer_model:COREONEL`, `printer_model:COREONE`, `printer:Prusa CORE One L *`, `printer:Prusa CORE One *`, `print:0.20mm SPEED @COREONEL 0.4`, `filament:*@COREONE*`); `...\resources\data\printer_gantries\geometries.json`; `%APPDATA%\PrusaSlicer\vendor\PrusaResearch.ini`; installed console `--help-fff` text.
- Numbers obtained by local scripts that resolve INI `inherits` and scan filament sections.
- Doc URLs: none fetched.
- No slicer source code was consulted. No binary in `info_source\` was opened, decompiled, disassembled or strings-mined. No shader, icon, font or STL content was read.
