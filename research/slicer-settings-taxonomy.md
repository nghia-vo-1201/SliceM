# PrusaSlicer setting surface (taxonomy / feature checklist for SliceM)

Date: 2026-10-14. Source: the option listing printed by the installed PrusaSlicer 2.9.4 console (`--help-fff`), which enumerates every user-settable FFF option with its type hint, unit and default. Purposes below are written by SliceM research in its own words.

## How to read this

- `key` is the config-file key (the CLI flag spelled with underscores). It is the interoperability surface: SliceM should read/write these key names when importing/exporting `.ini`/`.3mf` configs.
- `type`: `number` = numeric, may be `mm or %` where the unit column says so; `enum` = choices listed in the default column; `string` = free text/G-code; `bool/compound` = the listing gave no type hint (usually a checkbox, default 0/1; a few are compound strings such as `bed_shape` polygon, comma lists per-extruder). Booleans are `inferred`, not observed.
- Defaults are PrusaSlicer's *built-in* defaults (not the Prusa profile values, which differ; see printer-profile sheets).
- Per-extruder options (nozzle_diameter, temperatures, retract_*, etc.) are comma-separated vectors in configs, one entry per extruder. Machine-limit keys hold two entries (normal, stealth mode).
- **Expert/advanced/simple mode is NOT marked** in the CLI listing or in any file under `resources/`; per-setting mode levels are `unverified` / not available from the permitted sources. Public help.prusa3d.com pages mark them but were not fetched in this pass.
- Prefix `filament_retract_*`, `filament_travel_*`, `filament_wipe`, `filament_seam_gap_distance`: per-filament overrides of the same-named printer/print setting (`nil` value in profiles means "inherit").
- The CLI listing omits some internal/UI-only keys (e.g. wipe tower position, per-object overrides, `compatible_printers_condition`, `inherits`, `idle_temperature`). Their existence is `unverified` here.
- Total listed options: 364.

## Cross-cutting structure worth copying

- Four preset families: **print** (layers, perimeters, infill, speed, accel, supports, skirt, advanced), **filament** (temps, cooling, retraction overrides, flow, MMU load/unload), **printer** (bed, limits, retraction defaults, G-code macros, firmware flavour, clearance), plus SLA sets (out of scope).
- Presets inherit via `inherits = parentA; parentB`; later parents/own keys override earlier ones. Abstract parents are named `*name*`.
- Compatibility between presets is expressed in `compatible_printers_condition` / `compatible_prints_condition` expressions that test `printer_model`, `nozzle_diameter[0]`, `nozzle_high_flow`, `printer_notes` tags, etc.
- Many speed/width settings accept `mm`, `mm/s` **or** `%` of a related setting; `0` often means "auto" or "use parent value". SliceM needs a tagged `Length | Percent` value type.

### Layers and perimeters (45)

| key | type | unit | default | controls |
|---|---|---|---|---|
| `arc_fitting` | enum |  | disabled ; options: disabled, emit_center | Emit G2/G3 arcs (disabled/emit_center) |
| `avoid_crossing_perimeters_max_detour` | number | mm or % (zero to disable) | 0 | Max detour length allowed when avoiding perimeter crossings |
| `bottom_solid_layers` | number |  | 3 | Solid layers on bottom surfaces |
| `bottom_solid_min_thickness` | number | mm | 0 | Minimum bottom shell thickness (adds layers if needed) |
| `bridge_angle` | number | Â° | 0 | Override bridge direction (0 = auto) |
| `elefant_foot_compensation` | number | mm | 0 | Shrink first layer to offset squish |
| `ensure_vertical_shell_thickness` | enum |  | enabled ; options: disabled, partial, enabled | Adds solid infill near slopes to keep shell thickness (disabled/partial/enabled) |
| `external_perimeters_first` | bool/compound |  |  | Print outermost perimeter first instead of last |
| `extra_perimeters` | bool/compound |  |  | Auto-add perimeters under sloping walls to avoid gaps |
| `extra_perimeters_on_overhangs` | bool/compound |  |  | Add perimeters on overhangs that bridges cannot anchor |
| `first_layer_height` | number | mm | 0.35 | Thickness of the first layer |
| `gap_fill_enabled` | bool/compound |  |  | Fill gaps between perimeters and infill |
| `gcode_resolution` | number | mm | 0.0125 | Max deviation of exported path simplification |
| `interface_shells` | bool/compound |  |  | Force solid shells between adjacent volumes/materials |
| `layer_height` | number | mm | 0.3 | Nominal slice thickness |
| `max_layer_height` | number | mm | 0 | Printer/extruder upper layer-height bound (0 = 75% nozzle) |
| `min_layer_height` | number | mm | 0.07 | Printer/extruder lower layer-height bound |
| `only_one_perimeter_first_layer` | bool/compound |  |  | Single perimeter on first layer |
| `overhangs` | bool/compound |  |  | Use bridge flow/speed/fan on overhang perimeters |
| `perimeter_generator` | enum |  | arachne ; options: classic, arachne | Classic (constant width) vs Arachne (variable width) |
| `perimeters` | number | (minimum) | 3 | Minimum number of perimeter loops |
| `prefer_clockwise_movements` | bool/compound |  |  | Print loops clockwise instead of counter-clockwise |
| `resolution` | number | mm | 0 | Input mesh simplification tolerance |
| `scarf_seam_entire_loop` | bool/compound |  |  | Extend scarf around whole loop |
| `scarf_seam_length` | number | mm | 20 | Length of scarf joint |
| `scarf_seam_max_segment_length` | number | mm | 1 | Max segment length inside scarf |
| `scarf_seam_on_inner_perimeters` | bool/compound |  |  | Apply scarf to inner perimeters |
| `scarf_seam_only_on_smooth` | bool/compound |  |  | Apply scarf only on smooth perimeters |
| `scarf_seam_placement` | enum |  | nowhere ; options: nowhere, contours, everywhere | Where scarf seams apply (nowhere/contours/everywhere) |
| `scarf_seam_start_height` | number | % | 0% | Scarf start height as fraction of layer |
| `seam_gap_distance` | number | mm or % | 15% | Gap between loop endpoints at seam |
| `seam_position` | enum |  | aligned ; options: random, nearest, aligned, rear | Perimeter start placement (random/nearest/aligned/rear) |
| `slice_closing_radius` | number | mm | 0.049 | Crack-closing radius when slicing mesh |
| `slicing_mode` | enum |  | regular ; options: regular, even_odd, close_holes | Mesh interpretation: regular / even_odd / close_holes |
| `solid_layers` | number |  |  | Alias: top+bottom solid layers |
| `solid_min_thickness` | number |  |  | Alias: top+bottom min shell thickness |
| `spiral_vase` | bool/compound |  |  | Continuous Z ramp, single wall, no infill (vase mode) |
| `staggered_inner_seams` | bool/compound |  |  | Shift inner seams back by depth in zig-zag |
| `thick_bridges` | bool/compound |  |  | Thicker, more reliable bridge extrusions |
| `thin_walls` | bool/compound |  |  | Detect single-width walls and print them as one trace |
| `top_one_perimeter_type` | enum |  | none ; options: none, top, topmost | Single perimeter on top surfaces (none/top/topmost) |
| `top_solid_layers` | number |  | 3 | Solid layers on top surfaces |
| `top_solid_min_thickness` | number | mm | 0 | Minimum top shell thickness (adds layers if needed) |
| `variable_layer_height` | bool/compound |  |  | Printer flag: allow variable layer height |
| `xy_size_compensation` | number | mm | 0 | Grow/shrink object in XY |

### G-code output / custom G-code (19)

| key | type | unit | default | controls |
|---|---|---|---|---|
| `after_layer_gcode` | string |  |  | Alias of layer_gcode |
| `autoemit_temperature_commands` | bool/compound |  |  | Auto temp commands unless start gcode has them |
| `before_layer_gcode` | string |  |  | Macro before each layer Z move |
| `between_objects_gcode` | string |  |  | Macro between sequential objects |
| `color_change_gcode` | string |  | M600 | Macro for colour change |
| `colorprint_heights` | number |  |  | Layer heights for colour changes |
| `custom_parameters_print` | string |  |  | User JSON params for macros |
| `custom_parameters_printer` | string |  |  | User JSON params for macros |
| `end_gcode` | string |  | M104 S0 ; turn off temperature\nG28 X0 ; home X axis\nM84 ; disable motors\n | Printer end macro |
| `gcode_comments` | bool/compound |  |  | Annotate G-code |
| `gcode_label_objects` | enum |  | disabled ; options: disabled, octoprint, firmware | Object labels (disabled/octoprint/firmware) |
| `gcode_substitutions` | string |  |  | Find/replace post pass |
| `notes` | string |  |  | Project notes in G-code header |
| `output_filename_format` | string |  | [input_filename_base].gcode | Output filename template |
| `pause_print_gcode` | string |  | M601 | Macro for pause |
| `post_process` | string |  |  | External scripts run on output |
| `start_gcode` | string |  | G28 ; home all axes\nG1 Z5 F5000 ; lift nozzle\n | Printer start macro |
| `template_custom_gcode` | string |  |  | Macro for custom code |
| `toolchange_gcode` | string |  |  | Macro on tool change |

### Retraction / travel (23)

| key | type | unit | default | controls |
|---|---|---|---|---|
| `avoid_crossing_curled_overhangs` | bool/compound |  |  | Plan travel to avoid curled overhang areas |
| `avoid_crossing_perimeters` | bool/compound |  |  | Plan travel to avoid crossing perimeters |
| `deretract_speed` | number | mm/s | 0 | Deretract speed (0 = retract speed) |
| `only_retract_when_crossing_perimeters` | bool/compound |  |  | Skip retract when travel stays inside upper perimeters |
| `ooze_prevention` | bool/compound |  |  | Drop temp of idle extruders |
| `retract_before_travel` | number | mm | 2 | Min travel length that triggers retract |
| `retract_before_wipe` | number | % | 0% | Fast retract % before wipe |
| `retract_layer_change` | bool/compound |  | 0 | Retract on every Z move |
| `retract_length` | number | mm (zero to disable) | 2 | Retract distance |
| `retract_length_toolchange` | number | mm (zero to disable) | 10 | Retract before tool change |
| `retract_lift` | number | mm | 0 | Z-hop height |
| `retract_lift_above` | number | mm | 0 | Z-hop only above this Z |
| `retract_lift_below` | number | mm | 0 | Z-hop only below this Z |
| `retract_restart_extra` | number | mm | 0 | Extra feed after deretract |
| `retract_restart_extra_toolchange` | number | mm | 0 | Extra feed after tool change |
| `retract_speed` | number | mm/s | 40 | Retract speed |
| `standby_temperature_delta` | number | âˆ†Â°C | -5 | Idle extruder temp delta |
| `travel_lift_before_obstacle` | bool/compound |  | 0 | Steeper ramp near obstacles |
| `travel_max_lift` | number | mm | 0 | Max ramp lift |
| `travel_ramping_lift` | bool/compound |  | 0 | Ramp lift instead of vertical |
| `travel_slope` | number | Â° | 0 | Ramp slope angle |
| `use_firmware_retraction` | bool/compound |  |  | Emit G10/G11 |
| `wipe` | bool/compound |  | 0 | Wipe while retracting |

### Printer / machine (45)

| key | type | unit | default | controls |
|---|---|---|---|---|
| `bed_custom_model` | string |  |  | UI bed model |
| `bed_custom_texture` | string |  |  | UI bed texture |
| `bed_shape` | bool/compound |  | 0x0,200x0,200x200,0x200 | Bed outline polygon |
| `binary_gcode` | bool/compound |  |  | Emit .bgcode |
| `extruder_clearance_height` | number | mm | 20 | Sequential printing: nozzle-tip to carriage height |
| `extruder_clearance_radius` | number | mm | 20 | Sequential printing: toolhead radius |
| `extruder_colour` | string |  | "" | UI colour |
| `extruder_offset` | number | mm | 0x0 | XY offset per extruder |
| `extrusion_axis` | string |  | E | E axis letter |
| `gcode_flavor` | enum |  | reprap ; options: reprap, reprapfirmware, repetier, teacup, makerware, marlin, marlin2, klipper, sailfish, mach3, machinekit, smoothie, n... | Firmware dialect |
| `machine_limits_usage` | enum |  | time_estimate_only ; options: emit_to_gcode, time_estimate_only, ignore | emit_to_gcode / time_estimate_only / ignore |
| `machine_max_acceleration_e` | number | mm/sÂ² | 10000,5000 | E accel limit |
| `machine_max_acceleration_extruding` | number | mm/sÂ² | 1500,1250 | Accel while extruding |
| `machine_max_acceleration_retracting` | number | mm/sÂ² | 1500,1250 | Accel while retracting |
| `machine_max_acceleration_travel` | number | mm/sÂ² | 1500,1250 | Accel for travel |
| `machine_max_acceleration_x` | number | mm/sÂ² | 9000,1000 | X accel limit (normal,silent) |
| `machine_max_acceleration_y` | number | mm/sÂ² | 9000,1000 | Y accel limit |
| `machine_max_acceleration_z` | number | mm/sÂ² | 500,200 | Z accel limit |
| `machine_max_feedrate_e` | number | mm/s | 120,120 | E feedrate limit |
| `machine_max_feedrate_x` | number | mm/s | 500,200 | X feedrate limit |
| `machine_max_feedrate_y` | number | mm/s | 500,200 | Y feedrate limit |
| `machine_max_feedrate_z` | number | mm/s | 12,12 | Z feedrate limit |
| `machine_max_jerk_e` | number | mm/s | 2.5,2.5 | E jerk |
| `machine_max_jerk_x` | number | mm/s | 10,10 | X jerk |
| `machine_max_jerk_y` | number | mm/s | 10,10 | Y jerk |
| `machine_max_jerk_z` | number | mm/s | 0.2,0.4 | Z jerk |
| `machine_max_junction_deviation` | number | mm | 0,0 | Junction deviation (M205 J) |
| `machine_min_extruding_rate` | number | mm/s | 0,0 | M205 S |
| `machine_min_travel_rate` | number | mm/s | 0,0 | M205 T |
| `max_print_height` | number | mm | 200 | Max Z |
| `nozzle_diameter` | number |  |  | Nozzle diameter |
| `nozzle_high_flow` | bool/compound |  | 0 | High-flow nozzle flag |
| `preset_name` | string |  |  | Internal |
| `preset_names` | string |  |  | Physical printer preset list |
| `printer_model` | string |  |  | Model id string |
| `printer_notes` | string |  |  | Free text; also carries machine tags |
| `printer_technology` | bool/compound | FFF, SLA | FFF | FFF or SLA |
| `printer_variant` | string |  |  | Variant (nozzle) string |
| `remaining_times` | bool/compound |  |  | Emit M73 progress |
| `silent_mode` | bool/compound |  |  | Stealth mode capability |
| `thumbnails` | string |  |  | Embedded preview image sizes/formats |
| `thumbnails_format` | bool/compound | PNG, JPG, QOI | PNG | PNG/JPG/QOI |
| `use_relative_e_distances` | bool/compound |  |  | Relative E |
| `use_volumetric_e` | bool/compound |  |  | E in mm3 |
| `z_offset` | number | mm | 0 | Global Z offset |

### Filament (60)

| key | type | unit | default | controls |
|---|---|---|---|---|
| `bed_temperature` | number | Â°C | 0 | Bed temp |
| `bed_temperature_extruder` | number |  | 0 | Which extruder decides bed temps |
| `chamber_minimal_temperature` | number | Â°C | 0 | Chamber temp to wait for before start |
| `chamber_temperature` | number | Â°C | 0 | Target chamber temp (0 = none emitted) |
| `custom_parameters_filament` | string |  | "" | User JSON params for macros |
| `end_filament_gcode` | string |  | "; Filament-specific end gcode \n;END gcode for filament\n" | Per-filament end macro |
| `filament_abrasive` | bool/compound |  | 0 | Flag: needs hardened nozzle |
| `filament_colour` | string |  | #29B2B2 | UI colour |
| `filament_cooling_final_speed` | number | mm/s | 3.4 | End speed of cooling moves |
| `filament_cooling_initial_speed` | number | mm/s | 2.2 | Start speed of cooling moves |
| `filament_cooling_moves` | number |  | 4 | Back-and-forth cooling moves |
| `filament_cost` | number | money/kg | 0 | Cost per kg (stats) |
| `filament_density` | number | g/cmÂ³ | 0 | Density (stats only) |
| `filament_deretract_speed` | number | mm/s | 0 | Per-filament override |
| `filament_diameter` | number | mm | 1.75 | Filament diameter |
| `filament_infill_max_crossing_speed` | number | mm/s | 0 | Per-filament cap for self-crossing infill |
| `filament_infill_max_speed` | number | mm/s | 0 | Per-filament infill speed cap |
| `filament_load_time` | number | s | 0 | Firmware load time (estimator) |
| `filament_loading_speed` | number | mm/s | 28 | Wipe-tower load speed |
| `filament_loading_speed_start` | number | mm/s | 3 | Load speed at start of load |
| `filament_max_volumetric_speed` | number | mmÂ³/s | 0 | Per-filament flow cap |
| `filament_minimal_purge_on_wipe_tower` | number | mmÂ³ | 15 | Min prime into tower after change |
| `filament_multitool_ramming` | bool/compound |  | 0 | Ramming for multitool printers |
| `filament_multitool_ramming_flow` | number | mmÂ³/s | 10 | Ramming flow |
| `filament_multitool_ramming_volume` | number | mmÂ³ | 10 | Ramming volume |
| `filament_notes` | string |  | "" | Free-text notes |
| `filament_purge_multiplier` | number | % | 100% | Scale wipe-tower purge volume |
| `filament_ramming_parameters` | string |  | "120 100 6.6 6.8 7.2 7.6 7.9 8.2 8.7 9.4 9.9 10.0/ 0.05 6.6 0.45 6.8 0.95 7.8 1.45 8.3 1.95 9.7 2.45 10 2.95 7.6 3.45 7.6 3.95 7.6 4.45 7... | Ramming profile blob (edited by dialog) |
| `filament_retract_before_travel` | number | mm | 2 | Per-filament override of retract_before_travel |
| `filament_retract_before_wipe` | number | % | 0% | Per-filament override |
| `filament_retract_layer_change` | bool/compound |  | 0 | Per-filament override |
| `filament_retract_length` | number | mm (zero to disable) | 2 | Per-filament override |
| `filament_retract_length_toolchange` | number | mm (zero to disable) | 10 | Per-filament override |
| `filament_retract_lift` | number | mm | 0 | Per-filament override |
| `filament_retract_lift_above` | number | mm | 0 | Per-filament override |
| `filament_retract_lift_below` | number | mm | 0 | Per-filament override |
| `filament_retract_restart_extra` | number | mm | 0 | Per-filament override |
| `filament_retract_restart_extra_toolchange` | number | mm | 0 | Per-filament override |
| `filament_retract_speed` | number | mm/s | 40 | Per-filament override |
| `filament_seam_gap_distance` | number | mm or % | 15% | Per-filament seam gap |
| `filament_shrinkage_compensation_xy` | number | % | 0% | XY scaling for shrinkage |
| `filament_shrinkage_compensation_z` | number | % | 0% | Z scaling for shrinkage |
| `filament_soluble` | bool/compound |  | 0 | Flag: soluble material |
| `filament_spool_weight` | number | g | 0 | Empty spool weight |
| `filament_stamping_distance` | number | mm | 0 | Stamping move length |
| `filament_stamping_loading_speed` | number | mm/s | 20 | Stamping speed |
| `filament_toolchange_delay` | number | s | 0 | Wait after unload |
| `filament_travel_lift_before_obstacle` | bool/compound |  | 0 | Per-filament override |
| `filament_travel_max_lift` | number | mm | 0 | Per-filament override |
| `filament_travel_ramping_lift` | bool/compound |  | 0 | Per-filament override |
| `filament_travel_slope` | number | Â° | 0 | Per-filament override |
| `filament_type` | string | PLA, PET, ABS, ASA, FLEX, HIPS, EDGE, NGEN, PA, NYLON, PVA, PC, PP, PEI, PEEK, PEKK, POM, PSU, PVDF, SCAFF | PLA | Material class string |
| `filament_unload_time` | number | s | 0 | Firmware unload time (estimator) |
| `filament_unloading_speed` | number | mm/s | 90 | Wipe-tower unload speed |
| `filament_unloading_speed_start` | number | mm/s | 100 | Unload speed right after ramming |
| `filament_wipe` | bool/compound |  | 0 | Per-filament override |
| `first_layer_bed_temperature` | number | Â°C | 0 | Bed temp first layer |
| `first_layer_temperature` | number | Â°C | 200 | Nozzle temp first layer |
| `start_filament_gcode` | string |  | "; Filament gcode\n" | Per-filament start macro |
| `temperature` | number | Â°C | 200 | Nozzle temp (layers after first) |

### Acceleration (11)

| key | type | unit | default | controls |
|---|---|---|---|---|
| `bridge_acceleration` | number | mm/sÂ² | 0 | Bridges |
| `default_acceleration` | number | mm/sÂ² | 0 | Acceleration restored after role-specific values |
| `external_perimeter_acceleration` | number | mm/sÂ² | 0 | Outer perimeters |
| `first_layer_acceleration` | number | mm/sÂ² | 0 | First layer |
| `first_layer_acceleration_over_raft` | number | mm/sÂ² | 0 | First layer above raft |
| `infill_acceleration` | number | mm/sÂ² | 0 | Sparse infill |
| `perimeter_acceleration` | number | mm/sÂ² | 0 | Perimeters |
| `solid_infill_acceleration` | number | mm/sÂ² | 0 | Solid infill |
| `top_solid_infill_acceleration` | number | mm/sÂ² | 0 | Top solid infill |
| `travel_acceleration` | number | mm/sÂ² | 0 | Travel |
| `travel_short_distance_acceleration` | number | mm/sÂ² | 0 | Short travel moves |

### Cooling / fan (16)

| key | type | unit | default | controls |
|---|---|---|---|---|
| `bridge_fan_speed` | number | % | 100 | Fan during bridges/overhangs |
| `cooling` | bool/compound |  | 1 | Automatic cooling logic on/off |
| `cooling_perimeter_transition_distance` | number | mm | 0 | Distance to restore speed before fast perimeter |
| `cooling_slowdown_logic` | enum |  | options: uniform_cooling, consistent_surface | uniform_cooling / consistent_surface |
| `disable_fan_first_layers` | number | layers | 3 | Fan off for first N layers |
| `enable_dynamic_fan_speeds` | bool/compound |  | 0 | Overhang-dependent fan |
| `fan_always_on` | bool/compound |  | 0 | Never switch fan fully off |
| `fan_below_layer_time` | number | approximate seconds | 60 | Layer-time threshold below which fan ramps |
| `full_fan_speed_layer` | number |  | 0 | Layer at which fan reaches max |
| `max_fan_speed` | number | % | 100 | Fan maximum |
| `min_fan_speed` | number | % | 35 | Fan minimum |
| `overhang_fan_speed_0` | number | % | 0 | Fan at overhang bin 0 |
| `overhang_fan_speed_1` | number | % | 0 | Fan at overhang bin 1 |
| `overhang_fan_speed_2` | number | % | 0 | Fan at overhang bin 2 |
| `overhang_fan_speed_3` | number | % | 0 | Fan at overhang bin 3 |
| `slowdown_below_layer_time` | number | approximate seconds | 5 | Layer-time threshold below which print slows |

### Skirt / brim / raft (15)

| key | type | unit | default | controls |
|---|---|---|---|---|
| `brim_separation` | number | mm | 0 | Gap brim-to-object |
| `brim_type` | enum |  | outer_only ; options: no_brim, outer_only, inner_only, outer_and_inner | Brim placement (no_brim/outer_only/inner_only/outer_and_inner) |
| `brim_width` | number | mm | 0 | Brim width |
| `complete_objects` | bool/compound |  |  | Sequential printing: finish each object before next |
| `draft_shield` | enum |  | disabled ; options: disabled, limited, enabled | Tall skirt as wind shield (disabled/limited/enabled) |
| `duplicate_distance` | number | mm | 6 | Gap used by auto-arrange |
| `min_skirt_length` | number | mm | 0 | Min filament length for skirt |
| `raft_contact_distance` | number | mm | 0.1 | Z gap object-to-raft |
| `raft_expansion` | number | mm | 1.5 | XY raft growth |
| `raft_first_layer_density` | number | % | 90% | Density of first raft layer |
| `raft_first_layer_expansion` | number | mm | 3 | XY growth of first raft/support layer |
| `raft_layers` | number | layers | 0 | Number of raft layers under object |
| `skirt_distance` | number | mm | 6 | Gap skirt-to-object |
| `skirt_height` | number | layers | 1 | Skirt height in layers |
| `skirts` | number |  | 1 | Number of skirt loops |

### Multi-material / extruders / wipe tower (35)

| key | type | unit | default | controls |
|---|---|---|---|---|
| `cooling_tube_length` | number | mm | 5 | MMU cooling tube length |
| `cooling_tube_retraction` | number | mm | 91.5 | Cooling tube centre from tip |
| `extra_loading_move` | number | mm | -2 | Extra/less load vs unload distance |
| `extruder` | number |  |  | Default extruder |
| `high_current_on_filament_swap` | bool/compound |  |  | Boost motor current for swap |
| `infill_extruder` | number |  |  | Extruder for infill |
| `interlocking_beam` | bool/compound |  |  | Interlocking structure at material boundaries |
| `interlocking_beam_layer_count` | number |  | 2 | Beam height in layers |
| `interlocking_beam_width` | number | mm | 0.8 | Beam width |
| `interlocking_boundary_avoidance` | number |  | 2 | Keep-out from model outside (cells) |
| `interlocking_depth` | number |  | 2 | Interlock depth from boundary (cells) |
| `interlocking_orientation` | number | Â° | 22.5 | Beam angle |
| `mmu_segmented_region_interlocking_depth` | number | mm (zero to disable) | 0 | Painted-region interlocking depth |
| `mmu_segmented_region_max_width` | number | mm (zero to disable) | 0 | Max painted-region width |
| `multimaterial_purging` | number | mmÂ³ | 140 | Default purge volume |
| `parking_pos_retraction` | number | mm | 92 | Parking distance from tip |
| `perimeter_extruder` | number |  | 1 | Extruder for perimeters/brim |
| `single_extruder_multi_material` | bool/compound |  |  | SEMM flag (MMU) |
| `single_extruder_multi_material_priming` | bool/compound |  |  | Prime all extruders at start |
| `solid_infill_extruder` | number |  | 1 | Extruder for solid infill |
| `support_material_extruder` | number |  | 1 | Extruder for supports/raft/skirt |
| `support_material_interface_extruder` | number |  | 1 | Extruder for support interface |
| `wipe_into_infill` | bool/compound |  |  | Purge inside object infill |
| `wipe_into_objects` | bool/compound |  |  | Purge into sacrificial objects |
| `wipe_tower` | bool/compound |  |  | Enable wipe tower |
| `wipe_tower_bridging` | number | mm | 10 | Max bridge distance on tower sparse parts |
| `wipe_tower_brim_width` | number | mm | 2 | Tower brim width |
| `wipe_tower_cone_angle` | number | Â° | 0 | Stabilising cone apex angle |
| `wipe_tower_extra_flow` | number | % | 100% | Purge line flow scale |
| `wipe_tower_extra_spacing` | number | % | 100% | Purge line spacing scale |
| `wipe_tower_extruder` | number |  | 0 | Extruder for tower perimeter |
| `wipe_tower_no_sparse_layers` | bool/compound |  |  | Skip tower on layers w/o changes |
| `wipe_tower_width` | number | mm | 60 | Tower width |
| `wiping_volumes_matrix` | number |  | 0,140,140,140,140,140,0,140,140,140,140,140,0,140,140,140,140,140,0,140,140,140,140,140,0 | Purge volumes per tool pair |
| `wiping_volumes_use_custom_matrix` | bool/compound |  |  | Use custom matrix |

### Extrusion width / flow (11)

| key | type | unit | default | controls |
|---|---|---|---|---|
| `automatic_extrusion_widths` | bool/compound |  |  | Derive widths automatically from nozzle |
| `bridge_flow_ratio` | number |  | 1 | Flow scale for bridges |
| `external_perimeter_extrusion_width` | number | mm or % | 0 | Outer perimeter width |
| `extrusion_multiplier` | number |  | 1 | Flow scale factor |
| `extrusion_width` | number | mm or % | 0 | Default manual extrusion width |
| `first_layer_extrusion_width` | number | mm or % | 200% | First-layer width |
| `infill_extrusion_width` | number | mm or % | 0 | Sparse infill width |
| `perimeter_extrusion_width` | number | mm or % | 0 | Perimeter width |
| `solid_infill_extrusion_width` | number | mm or % | 0 | Solid infill width |
| `support_material_extrusion_width` | number | mm or % | 0 | Support width |
| `top_infill_extrusion_width` | number | mm or % | 0 | Top infill width |

### Speed (27)

| key | type | unit | default | controls |
|---|---|---|---|---|
| `bridge_speed` | number | mm/s | 60 | Bridge feed rate |
| `enable_dynamic_overhang_speeds` | bool/compound |  |  | Speed varies with overhang degree |
| `external_perimeter_speed` | number | mm/s or % | 50% | Outer perimeter feed rate |
| `first_layer_infill_speed` | number | mm/s or % | 0 | First layer solid infill rate |
| `first_layer_speed` | number | mm/s or % | 30 | First layer feed rate (mm/s or % of defaults) |
| `first_layer_speed_over_raft` | number | mm/s or % | 30 | First object layer rate over raft |
| `gap_fill_speed` | number | mm/s | 20 | Gap-fill feed rate |
| `infill_speed` | number | mm/s | 80 | Sparse infill feed rate |
| `max_print_speed` | number | mm/s | 80 | Auto-speed cap |
| `max_volumetric_extrusion_rate_slope_negative` | number | mmÂ³/sÂ² | 0 | Ramp limit for decelerating flow |
| `max_volumetric_extrusion_rate_slope_positive` | number | mmÂ³/sÂ² | 0 | Ramp limit for accelerating flow |
| `max_volumetric_speed` | number | mmÂ³/s | 0 | Print-wide volumetric flow cap |
| `min_print_speed` | number |  |  | Lowest speed cooling may reduce to |
| `over_bridge_speed` | number | mm/s or % | 0 | Solid infill over bridges |
| `overhang_speed_0` | number | mm/s or % | 15 | Speed at overhang bin 0 (full overhang) |
| `overhang_speed_1` | number | mm/s or % | 15 | Speed at overhang bin 1 |
| `overhang_speed_2` | number | mm/s or % | 20 | Speed at overhang bin 2 |
| `overhang_speed_3` | number | mm/s or % | 25 | Speed at overhang bin 3 |
| `perimeter_speed` | number | mm/s | 60 | Perimeter feed rate |
| `small_perimeter_speed` | number | mm/s or % | 15 | Feed rate for small (r<=6.5mm) perimeters |
| `solid_infill_speed` | number | mm/s or % | 20 | Solid infill feed rate |
| `support_material_interface_speed` | number | mm/s or % | 100% | Support interface feed rate |
| `support_material_speed` | number | mm/s | 60 | Support feed rate |
| `top_solid_infill_speed` | number | mm/s or % | 15 | Top surface feed rate |
| `travel_speed` | number | mm/s | 130 | Travel move rate |
| `travel_speed_z` | number | mm/s | 0 | Z travel rate |
| `wipe_tower_acceleration` | number | mm/sÂ² | 0 | Acceleration while printing wipe tower |

### Infill (14)

| key | type | unit | default | controls |
|---|---|---|---|---|
| `automatic_infill_combination` | bool/compound |  |  | Auto-combine infill layers |
| `automatic_infill_combination_max_layer_height` | number |  | 100% | Cap for combined infill layer height |
| `bottom_fill_pattern` | enum |  | monotonic ; options: rectilinear, monotonic, monotoniclines, alignedrectilinear, concentric, hilbertcurve, archimedeanchords, octagramspiral | Pattern of bottom visible layer |
| `fill_angle` | number | Â° | 45 | Base infill angle |
| `fill_density` | number | % | 20% | Sparse infill density (%) |
| `fill_pattern` | enum |  | stars ; options: rectilinear, alignedrectilinear, grid, triangles, stars, cubic, line, concentric, honeycomb, 3dhoneycomb, gyroid, hilber... | Sparse infill pattern (see enum list) |
| `infill_anchor` | number | mm or % | 600% | Anchor length of infill to inner perimeter (single side) |
| `infill_anchor_max` | number | mm or % | 50 | Max anchor length (connecting two lines) |
| `infill_every_layers` | number | layers | 1 | Combine infill over N layers (thicker infill) |
| `infill_first` | bool/compound |  |  | Print infill before perimeters |
| `infill_overlap` | number | mm or % | 25% | Infill/perimeter overlap |
| `solid_infill_below_area` | number | mmÂ² | 70 | Force solid infill for small regions |
| `solid_infill_every_layers` | number | layers | 0 | Force a solid layer every N layers |
| `top_fill_pattern` | enum |  | monotonic ; options: rectilinear, monotonic, monotoniclines, alignedrectilinear, concentric, hilbertcurve, archimedeanchords, octagramspiral | Pattern of topmost visible layer |

### Arachne / thin features (6)

| key | type | unit | default | controls |
|---|---|---|---|---|
| `min_bead_width` | number | mm or % | 85% | Perimeter width used for thin features |
| `min_feature_size` | number | mm or % | 25% | Thinnest feature that will be printed |
| `wall_distribution_count` | number |  | 1 | How many perimeters absorb width variation |
| `wall_transition_angle` | number | Â° | 10 | Wedge angle above which no even/odd transition is made |
| `wall_transition_filter_deviation` | number | mm or % | 25% | Hysteresis margin on perimeter-count transitions |
| `wall_transition_length` | number | mm or % | 100% | Space allotted to split/join perimeters |

### Fuzzy skin (3)

| key | type | unit | default | controls |
|---|---|---|---|---|
| `fuzzy_skin` | enum |  | none ; options: none, external, all | Fuzzy skin mode (none/external/all) |
| `fuzzy_skin_point_dist` | number | mm | 0.8 | Spacing of inserted offset points |
| `fuzzy_skin_thickness` | number | mm | 0.3 | Max random offset amplitude |

### Ironing (5)

| key | type | unit | default | controls |
|---|---|---|---|---|
| `ironing` | bool/compound |  |  | Enable ironing pass |
| `ironing_flowrate` | number | % | 15% | Flow used during ironing (% of normal) |
| `ironing_spacing` | number |  |  | Line spacing of ironing |
| `ironing_speed` | number | mm/s | 15 | Ironing feed rate |
| `ironing_type` | enum |  | top ; options: top, topmost, solid | Which surfaces to iron (top/topmost/solid) |

### Supports (29)

| key | type | unit | default | controls |
|---|---|---|---|---|
| `dont_support_bridges` | bool/compound |  |  | No support under bridges |
| `support_material` | bool/compound |  |  | Enable supports |
| `support_material_angle` | number | Â° | 0 | Pattern rotation |
| `support_material_auto` | bool/compound |  |  | Auto-generate from overhang angle (else enforcers only) |
| `support_material_bottom_contact_distance` | number | mm | 0 | Z gap bottom contact |
| `support_material_bottom_interface_layers` | number | layers | -1 | Bottom interface layers (-1 = same as top) |
| `support_material_buildplate_only` | bool/compound |  |  | Supports only from bed |
| `support_material_closing_radius` | number | mm | 2 | Snug-support merge radius |
| `support_material_contact_distance` | number | mm | 0.2 | Z gap top contact |
| `support_material_enforce_layers` | number | layers | 0 | Force supports for first N layers |
| `support_material_interface_contact_loops` | bool/compound |  |  | Loops on top contact layer |
| `support_material_interface_layers` | number | layers | 3 | Top interface layers |
| `support_material_interface_pattern` | enum |  | rectilinear ; options: auto, rectilinear, concentric | Interface pattern (auto/rectilinear/concentric) |
| `support_material_interface_spacing` | number | mm | 0 | Interface line spacing |
| `support_material_pattern` | enum |  | rectilinear ; options: rectilinear, rectilinear-grid, honeycomb | rectilinear / rectilinear-grid / honeycomb |
| `support_material_spacing` | number | mm | 2.5 | Line spacing |
| `support_material_style` | enum |  | grid ; options: grid, snug, organic | grid / snug / organic |
| `support_material_synchronize_layers` | bool/compound |  |  | Sync support layers with object layers |
| `support_material_threshold` | number | Â° | 0 | Overhang angle threshold (0 = auto) |
| `support_material_with_sheath` | bool/compound |  |  | Perimeter sheath around support |
| `support_material_xy_spacing` | number | mm or % | 50% | XY gap to object |
| `support_tree_angle` | number | Â° | 40 | Max organic branch angle (avoiding model) |
| `support_tree_angle_slow` | number | Â° | 25 | Preferred branch angle |
| `support_tree_branch_diameter` | number | mm | 2 | Thinnest branch diameter |
| `support_tree_branch_diameter_angle` | number | Â° | 5 | Branch taper angle |
| `support_tree_branch_diameter_double_wall` | number | mm | 3 | Area threshold for double-wall branches |
| `support_tree_branch_distance` | number |  | 1 | Branch spacing at model contact |
| `support_tree_tip_diameter` | number | mm | 0.8 | Branch tip diameter |
| `support_tree_top_rate` | number | % | 15% | Tip density |

## Fan/overhang binning

`overhang_speed_N` and `overhang_fan_speed_N` (N=0..3) define a piecewise-linear lookup on overlap fraction with the previous layer (100% = fully supported, 0% = bridge-like).

## Provenance

- Command run: `"C:\Program Files\Prusa3D\PrusaSlicer\prusa-slicer-console.exe" --help-fff` (stdout saved in session scratchpad as helpfff.txt; 1369 lines; PrusaSlicer 2.9.4 banner). A follow-up `--save` run was blocked by the sandbox and not performed, so defaults come from the help text only.
- Table generated by a local Python script that parses the help text (key, unit, default, enum choices); the "controls" column and grouping were authored by hand.
- No file under `info_source\` outside `resources\` was opened; no executable or DLL was read, decompiled, disassembled or strings-mined. The only program interaction was running the installed console binary for its documented `--help-fff` output (observing behaviour, permitted).
- No PrusaSlicer, Slic3r, libslic3r, Orca, Cura or other slicer source code was consulted. Tooltip prose was summarised, not copied.
- Public docs (help.prusa3d.com) NOT accessed in this pass.
