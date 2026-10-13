# FDM slicer feature landscape and SliceM roadmap

Date: 2026-10-14. Clean-room: this document describes what features exist and what they must achieve. No slicer source code was consulted (see Provenance).

SliceM today: STL load with vertex welding; mesh/plane intersection giving closed per-layer contours. Nothing else.

---

## 1. Capability inventory

### 1.1 Model import and repair
- **Formats** (STL binary/ASCII, 3MF, OBJ, later AMF/STEP): get geometry in; 3MF also carries units, multi-object scenes, per-object settings and colour.
- **Vertex welding / topology build**: makes the mesh watertight-checkable; needed before any slicing. (Done.)
- **Repair** (degenerate triangles, duplicate faces, flipped normals, open edges, non-manifold edges, self-intersections, tiny islands): real-world STLs are frequently broken; unrepaired meshes give missing or inverted contours.
- **Slicing-time robustness** (even-odd/winding resolution, open-contour stitching, gap closing): lets slightly broken meshes still print.
- **Units and scale heuristics**: avoids a 25.4x mistake (inches vs mm).

### 1.2 Transforms and arrangement
- **Move / rotate / scale / mirror, lay-flat-on-face, drop to bed**: orientation controls strength, support need, and surface quality.
- **Auto-orient**: picks orientation minimising support or maximising strength.
- **Auto-arrange / multi-object plates**: packs objects on the bed, respecting printer bed shape and exclusion zones.
- **Cut (plane, with connectors) and split to parts/objects**: fit large models, flat bottoms.
- **Per-object and per-region settings (modifiers)**: different settings in a volume (e.g. denser infill around a screw hole).

### 1.3 3D visualisation and layer preview
- **3D viewport** (orbit/pan/zoom, bed, build volume, out-of-bounds highlight): the main UI for placement.
- **Layer preview slider** with toolpaths coloured by feature type, speed, flow, layer time, tool: the primary way users spot problems before printing.
- **G-code viewer / simulation**: verifies the real output rather than the intent.
- **Overhang, thin wall and support visualisation**: helps diagnose why a print would fail.

### 1.4 Perimeters / shells
- **Perimeter count and widths**: the walls carry most of the strength and define surface finish.
- **Inner/outer ordering** (outer-first vs inner-first, "external perimeters first"): trades dimensional accuracy and surface quality against overhang behaviour.
- **Thin-wall handling** (gap fill, medial-axis/variable-width extrusion, Arachne-style variable-width walls): prints features thinner than two nozzle widths without gaps or overfill.
- **Elephant-foot compensation, XY size/hole compensation**: dimensional accuracy for fit.
- **Fuzzy skin, "only one perimeter on top"**: surface texture and top-surface quality options.
- **Spiral vase mode**: single continuous outer wall, no seam, no retraction.

### 1.5 Infill
Pattern list and what each is good for (per end-user documentation):
- **Rectilinear / Line / Aligned rectilinear**: fast, simple, no crossing build-up; required for 100% infill.
- **Grid**: strong, good layer adhesion; crossings can cause material build-up.
- **Triangles / Stars**: three-direction stiffness; moderate time.
- **Gyroid**: near-isotropic strength, non-crossing, continuous; a common default.
- **Cubic / Adaptive cubic / Support cubic**: 3D lattices; adaptive varies density with distance from surfaces to save material on large parts; support cubic only supports top layers.
- **Lightning**: branches toward top surfaces only; minimum material for display parts, no real strength.
- **Honeycomb / 3D honeycomb**: crossing-free, strong, but material- and time-hungry.
- **Concentric**: follows the outline; good for flexible or transparent parts, slow.
- **Zig-zag**: continuous line, fewer retractions and travels.
- **Hilbert / Archimedean / Octagram spiral**: mostly cosmetic, flexibility, or liquid-filling cases.
- Supporting features: **density, angle, infill-every-N-layers (combine layers), anchor lines to walls, solid-infill threshold area, variable infill density regions, infill overlap**. They ensure good bonding to walls and avoid pillowing.

### 1.6 Top/bottom solid surfaces and ironing
- **Top/bottom shell layers and thickness**: closes the part and gives a flat skin over sparse infill.
- **Solid-surface detection**: finds regions where the layer above/below is absent and converts infill there to solid; the foundation of clean tops.
- **Skin patterns** (monotonic, rectilinear, concentric): visual quality of top surfaces; monotonic avoids ridge artefacts.
- **Ironing**: a second low-flow pass over top surfaces that smooths them.
- **Extra solid layers over sparse infill**: avoids pillowing.

### 1.7 Supports
- **Normal (grid/snug) supports**: columns/lattices under overhangs; robust, easy to design, scar the surface.
- **Tree / organic supports**: branching structures touching the model at points; use less material and remove more easily.
- **Overhang threshold and bridge-aware logic**: decides what needs support at all.
- **Z distance, XY distance, interface layers (top/bottom), interface patterns, density**: balance removability against underside finish.
- **Paint-on enforcers/blockers, "support on build plate only"**: user control where automatic is wrong.
- **Soluble support material (multi-material)**: clean removal.

### 1.8 Adhesion
- **Skirt**: primes the nozzle and checks levelling; also a boundary for the first-layer.
- **Brim** (outer/inner, with ears variant): increases bed contact area to stop warping/lifting.
- **Raft**: sacrificial base for poor bed contact or warp-prone material.
- **First-layer settings** (height, width, speed, temperature, squish): the first layer decides whether anything prints.

### 1.9 Speed and acceleration control
- **Per-feature speeds** (perimeters, infill, top, bridges, supports, travel, first layer): quality-critical features slow, bulk features fast.
- **Acceleration/jerk/junction deviation, per-feature limits**: reduces ringing and mechanical stress.
- **Volumetric flow limit**: keeps below what the hotend can melt; prevents under-extrusion.
- **Pressure/linear advance commands and per-material values**: sharp corners and consistent extrusion.
- **Minimum layer time slowdown**: lets small layers cool.

### 1.10 Cooling
- **Part cooling fan curves** (min/max, by layer time, bridges, overhangs, first layers off): solidifies plastic for overhangs and detail; too much hurts layer adhesion.
- **Slowdown below minimum layer time**: avoids heat accumulation.
- **Auxiliary/chamber fans** (printer-dependent).

### 1.11 Retraction, travel, wipe, seam
- **Retraction length/speed, Z-hop, minimum travel for retraction, deretract extra**: strings and blobs vs. wear and clogging.
- **Wipe on retract, coasting**: hides ooze.
- **Travel optimisation** (path ordering, avoid-crossing-perimeters, avoid retraction inside infill): shorter time and fewer visible blemishes.
- **Seam placement** (nearest, aligned, random, rear, paint-on, concave-preferring, scarf seams): the visible vertical line is controllable by position and concealment.

### 1.12 Bridging and overhang handling
- **Bridge detection**: identifies unsupported spans where infill/skin spans anchors on both sides.
- **Bridge direction choice, speed, flow, fan, thick bridges**: sets direction to span the shortest gap; slow/tight flow stops sagging.
- **Overhang perimeter slowdown and fan ramp (by overhang degree)**: keeps steep overhangs from curling.
- **Anti-curl, dynamic overhang speeds**: advanced quality on near-horizontal edges.

### 1.13 Variable and adaptive layer height
- **Fixed layer height + first layer height**: basic quality/time trade.
- **Manual layer-height ranges / painted smoothing**: fine layers only where curvature requires it.
- **Adaptive (slope-driven) layering**: automatic fine layers on shallow surfaces, coarse on vertical walls.
- **Z-aligned feature snapping**: layers land on top-surface heights to avoid thin or stepped top shells.

### 1.14 Multi-material and colour changes
- **Colour-change at height (M600 or equivalent pause)**: the simplest multi-colour on single-extruder printers.
- **Multi-extruder / MMU / AMS-class tool changes**: tool-change G-code, per-tool temperatures, per-tool offsets.
- **Wipe/prime tower, purge into infill/objects**: clears the old colour; costs waste and time.
- **Painting by region (colour, seam, support)**: per-face assignment.
- **Per-material profiles and compatibility**.

### 1.15 Sequential ("one at a time") printing
- **Complete-object printing**: each object printed fully before the next to cut travel and permit independent removal.
- **Gantry/extruder clearance model** (height limit, nozzle-to-carriage clearances, object order): avoids crashes into finished objects.

### 1.16 G-code flavours and post-processing
- **Flavours** (Marlin legacy, Prusa Buddy/Marlin 2, Klipper, RepRap, etc.): command dialects differ for pressure advance, M-codes, firmware retraction, relative/absolute E.
- **Custom start/end/layer-change/colour-change G-code with placeholders**: printer-specific setup.
- **Binary G-code (.bgcode), thumbnails, metadata**: Prusa Buddy-class firmware accepts compact form; thumbnails aid printer UI.
- **Arc fitting (G2/G3)**: smaller files and smoother motion on supporting firmware.
- **Post-process scripts, G-code export checks, time-lapse / pause hooks**.

### 1.17 Profile and preset management
- **Printer / filament / print profiles** with inheritance, compatibility conditions, import/export: reproducibility; most users never touch raw settings.
- **Simple/advanced/expert visibility, search, dirty-state tracking**: usability.
- **Per-object overrides and modifiers**; **calibration tools** (temperature tower, flow, pressure advance, retraction).

### 1.18 Print time and material estimation
- **Time estimate with acceleration-aware kinematics**: accuracy requires modelling accel, junction speeds, and firmware lookahead.
- **Filament length/weight/cost**: planning.
- **Per-feature breakdown, layer-time chart**: finds where time goes and drives min-layer-time cooling.

---

## 2. Algorithmic dependency order

```
Mesh load + weld + repair
   -> plane slicing to contours (done)
      -> robust 2D polygon engine (integer coords; union/intersect/diff/XOR; offset in/out; simplify; point-in-polygon; ExPolygon = outer + holes)
         -> perimeters (repeated inward offsets; ordering; thin-wall/gap)
            -> infill boundary = innermost perimeter offset inward
               -> region classification across layers
                  (compare layer N with N-1 / N+1 via boolean ops)
                  -> top / bottom solid surface detection
                  -> bridge detection (unsupported solid regions, with anchors)
                  -> overhang detection (perimeter vs layer below offset)
                  -> infill generation (line patterns clipped to boundary; 3D patterns sampled per layer)
                     -> support generation (needs overhang regions, layer stack, collision with model, interface layers)
                        -> brim/raft (need first-layer outline + support footprint)
   -> path planning (ordering, travel, seam, retraction) needs all extrusion paths per layer
      -> toolpath -> G-code emitter (E computation, feedrates, retraction, temperatures, fan)
         -> time estimator (needs the final move list)
         -> cooling slowdown (needs layer time estimate)
         -> arc fitting, binary G-code (post stage)
Layer-stack cross-references:
   adaptive layer height -> changes slice Z list; must come BEFORE slicing; interacts with top-surface snapping
   multi-material/colour changes -> per-layer tool assignment; wipe tower needs plate-wide planning
   sequential printing -> per-object whole pipeline + collision model; needs arrangement
GUI chain: 3D viewport -> layer preview (needs toolpaths) -> G-code viewer
Profiles: needed by everything; the settings schema should exist before perimeters so every parameter is data from day one.
```

Key explicit chain: **offsetting/booleans -> perimeters -> infill boundary -> solid-surface (top/bottom) detection -> bridge/overhang detection -> support generation**. Seam, retraction, cooling, time estimation sit downstream of "all paths exist". Anything per-region (modifiers, multi-colour) requires the region-aware layer representation to exist before perimeters, not after.

---

## 3. The hard parts, ranked

1. **Robust polygon offsetting and booleans.** Everything depends on it. Floating-point or sloppy implementations cause self-intersections, vanishing slivers, wrong hole handling, non-deterministic output on near-coincident edges. Typical failures: offsets of concave corners producing loops, tiny islands/holes appearing from rounding, collinear points breaking later steps, orientation mixing. Mitigation: scaled integer coordinates, a proven clipping approach (the Vatti scanline algorithm; the Clipper2 library is a mature option under the Boost licence), explicit simplification tolerance, property-based tests. Decide build-vs-adopt early (licence-compatible library is a legitimate choice).
2. **Support generation.** Needs overhang detection, collision avoidance against the model at arbitrary Z, interface layers, removability trade-offs, and tree-style branching optimisation. Typical failures: supports floating or not touching anything, supports growing through the model, merged columns fusing to the part, huge print time, impossible-to-remove interfaces, instability of tall thin columns. Tree/organic supports need a geometry-optimisation approach; published descriptions exist (e.g. work on tree supports and "Clever support"-style approaches, see Provenance).
3. **Bridging and overhang detection.** Requires correct 2D region differences between layers, anchor identification, direction choice, and distinguishing real bridges from slightly overhanging edges. Typical failures: bridging over infill that should have been solid, bridges in the wrong direction, tiny slivers triggering bridge settings, wrong flow for partial anchors.
4. **Seam placement.** Visual quality, but a hard heuristic problem: needs curvature/concavity analysis, overhang avoidance, alignment between layers, and handling of paint-on user hints. Typical failures: seams wandering, landing on overhangs, scattering across smooth surfaces, or zipping along sharp corners on layers that differ slightly.
5. **Adaptive layer height.** Needs surface-slope analysis over the mesh, smooth transitions between heights, and Z snapping to horizontal-surface heights. Typical failures: oscillating heights, thin top shells from misaligned layers, discontinuous settings downstream (cooling, speeds per layer height).
6. **Arc fitting.** Replacing polyline runs with G2/G3 segments within tolerance; must preserve extrusion amount, direction, and firmware limits. Typical failures: extrusion errors, too many tiny arcs, arcs that cross-section deviate beyond tolerance, firmware that does not accept arcs well.
7. **Collision-free sequential printing.** Requires a precise gantry/nozzle clearance model per printer and an order of objects such that earlier-printed objects never hit the carriage or fan shroud. Typical failures: crashes in the last layers, wrong assumption about extruder geometry, taller-than-clearance parts, no valid order existing. A safety-critical piece, hence heavy validation. (Directly shared with the SliceM differentiator, section 5.)
8. **Thin-wall / variable-width perimeters.** Medial-axis style computation with varying extrusion widths; typical failures are width discontinuities, gaps, and instability on noisy contours.
9. **Time estimation accuracy** and **non-planar-feeling issues like pillowing and 3D infill continuity** are lower-risk but fiddly.

---

## 4. Prioritised roadmap

Effort scale: S (days), M (1-3 weeks), L (a month or more).

**M0. Foundation hardening (S-M).** Settings schema and config structs (print/filament/printer), unit tests on the existing slicer, mesh repair basics (open edges, flipped normals, report), deterministic test fixtures (cube, cylinder, torus, hollow box).
Unblocks: everything. Cheap, and it prevents later rework.

**M1. 2D polygon engine (M).** Integer-coordinate polygon type with holes; union/intersection/difference; offset; simplification; contour orientation normalisation; adopt or write, with property tests.
Unblocks: perimeters, infill, region classification.

**M2. Shortest path to first printable G-code (M-L, in total M2-M4 minimal).**
- Printer profile for one machine (suggest MK3S first: simplest legacy dialect), start and end sequences authored from a requirement list.
- Perimeters (N loops, inner-to-outer or outer-to-inner), nearest-start seam only.
- Rectilinear infill with fixed density and angle alternating by layer.
- Solid top/bottom layers (simple N-layer rule using region differences).
- Skirt.
- Retraction and travel (straight travel with retract).
- Extrusion math (E from width, height, filament diameter), temperatures, fan on after layer 2.
- G-code writer with relative E, M82/M83 right, header/footer.
- Validate with the gcode viewer then print a calibration cube (no supports, no overhangs).
This milestone is the **shortest path**: cube-class geometry, one infill pattern, one seam strategy. No bridging, no supports, no GUI preview needed (CLI output is enough).

**M3. Quality baseline (M).** Bridge and overhang detection, bridge flow/speed/fan; per-feature speeds; min-layer-time cooling; first-layer settings; brim; improved travel (avoid crossing perimeters); gap fill; ironing optional.
Unblocks: realistic parts without support.

**M4. Viewport and layer preview (M-L, can run in parallel with M2/M3).** 3D view, placement transforms, bed model, toolpath preview with colour by feature, layer slider, G-code load.
Unblocks: user-visible validation and the colour-group selection UI.

**M5. Infill and surfaces breadth (M).** Gyroid, cubic/adaptive cubic, grid, triangles, honeycomb; monotonic top surfaces; combine-infill layers; anchors; sparse-infill region handling; seam options (aligned, rear, paint).

**M6. Profiles and presets (M).** Inheritance, import/export, compatibility, UI; Klipper and Buddy flavours; thumbnails, binary G-code; time/material estimation (accel-aware).

**M7. Supports v1 (L).** Overhang area detection, grid/snug supports, Z/XY gaps, interfaces, build-plate-only, enforcers/blockers.
**M8. Supports v2: tree/organic (L).**

**M9. Advanced geometry (M-L).** Variable-width/thin-wall perimeters, adaptive layer height, auto-arrange, cut tools, per-object modifiers, fuzzy skin, spiral vase, arc fitting.

**M10. Multi-material and colour (M-L).** Tool changes/MMU-class, wipe tower, painting, M600 colour changes by height.

**M11. Sequential printing (L).** Clearance models and ordering; paired with the differentiator.

**M12. Polish and calibration (M).** Calibration generators, plugin/post-processing hooks, localisation, packaging.

Where the differentiator slots in: after M2 minimum plus M3 (see section 5), which is much earlier than the generic multi-material milestone M10.

---

## 5. SliceM's differentiator: same-surface sequential multi-colour printing

Concept: when two or more features rise from the same flat top-most surface, print the base, pause (swap filament), print feature group A completely, pause, print group B completely. Two colours from one extruder, without a wipe tower or purging into the model, because each swap happens between complete groups and not per layer.

**Natural landing point: immediately after M3, in parallel with M4 and before supports (M7).** It is a scheduling layer over a working slicer, not an extension of exotic geometry.

Prerequisites:
1. **Working end-to-end G-code (M2) and reliable layer pause/colour-change commands** for the Prusa dialects (legacy M600 behaviour vs Buddy firmware behaviour must be researched by black-box experiments, not source).
2. **Per-layer region classification (M1+M2):** find the flat top-most surface at height Z_s (the layer where the shared base ends and features start), and the connected components of the cross-section above it.
3. **Grouping:** component labelling in 2D at each layer above Z_s and consistent identity of features across layers (a feature is a connected column stack). Users assign components to colour groups (needs M4 viewport/UI selection).
4. **Toolpath scheduling:** for layers above Z_s, emit all of group A for every layer, then group B for every layer (the inverse of the normal layer-by-layer order). This is a pure reordering as long as the groups are physically separate.
5. **Collision and clearance model (shared with sequential printing, M11):** while group B prints after group A is complete, the nozzle, heatblock, fan shroud and gantry must not hit group A. This requires a toolhead clearance profile (taken from research, not guessed), height limits, and a safe-order checker. It is the biggest safety dependency.
6. **Adhesion of the group to the base and thermal behaviour:** the second group starts on the base layer's top surface at the same Z_s, so layer-height alignment and cooling of early layers need care. Floating islands (groups with no connection to the base) must be refused.
7. **Support and bridge awareness:** groups containing overhangs need supports that do not cross groups; for v1, refuse or warn on overhangs in grouped features.
8. **Validation tooling:** G-code checks for clearance, plus the viewer pass before any hardware print.

Suggested staging: (a) one base plus two groups with simple extruded features (prove with a flat plaque with two raised shapes); (b) clearance checker enforcing printer-specific limits; (c) multi-height features and nested groups; (d) integrate with general sequential printing (M11).

---

## Provenance

Accessed 2026-10-14 (end-user documentation and library documentation only):
- https://help.prusa3d.com/article/infill-patterns_177130 (infill and top/bottom pattern behaviour)
- https://help.prusa3d.com/article/seam-position_151069 (seam options behaviour)
- https://help.prusa3d.com/article/support-material_1698 (support styles, interfaces, enforcers/blockers)
- https://angusj.com/clipper2/Docs/Overview.htm (polygon clipping/offsetting library overview; surfaced by search, search-result summary only)
- Academic references named from general knowledge, not fetched this session and to be verified before citing in code or papers: Vatti, "A generic solution to polygon clipping", CACM 35(7), 1992; Chen and McMains, "Polygon Offsetting by Computing Winding Numbers", ASME DETC2005-85513; Vanek et al., "Clever Support: Efficient Support Structure Generation for Digital Fabrication", 2014.
- The remainder of the inventory comes from general 3D-printing knowledge.

Statement: no slicer source code was read. No PrusaSlicer, Slic3r, libslic3r, OrcaSlicer, SuperSlicer, Cura, CuraEngine, BambuStudio or other slicer source (GitHub, forks, mirrors, PRs, pasted code) was fetched, quoted or summarised, and no binary was decompiled. No search result led to slicer source, so none had to be skipped.
