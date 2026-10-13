---
name: gcode-validation
description: Validate SliceM-generated G-code before it goes near a printer — structural and limit checks, clearance checks for grouped-colour jobs, a prusa-gcodeviewer visual pass, and differential comparison against a PrusaSlicer reference slice. Use before claiming any G-code is ready to print, and before every physical test.
---

# G-code validation

Nothing SliceM emits reaches a printer without this pass. Read `rules/printer-safety.md` — this
skill is how that rule gets enforced in practice.

## 1. Structural and limit checks

Against the target printer profile:

- Every move lies inside the build volume, travel and Z-hop included. Z never below zero.
- Temperatures within the profile's declared nozzle/bed/chamber limits; required waits present
  wherever the next move assumes the setpoint was reached.
- Feedrates and accelerations within machine limits.
- No `NaN`, no `inf`, no unset coordinates, no absolute/relative mode confusion (`G90`/`G91`,
  `M82`/`M83` set explicitly, not inherited).
- Extrusion is monotonic except at deliberate retractions.
- The colour-change / pause command matches the **firmware family** of the target — legacy MK3 and
  Buddy are not interchangeable.
- The file ends cleanly: heaters off, motors disengaged, nozzle parked clear of the part.

## 2. Clearance checks for a grouped-colour job

If the job uses same-surface feature-group reordering, confirm all five conditions from
`rules/printer-safety.md`:

1. groups ordered shortest-first;
2. `height(first group) <= profile.toolhead_clearance_height`;
3. XY separation `>= profile.toolhead_clearance_radius`;
4. the groups are genuinely top-most;
5. nothing else rises above the shared surface inside the swept envelope.

Any failure means the reordered path is not emitted at all — fall back to a layer-wise colour
change and report which check failed. Do not "validate with a warning".

## 3. Visual pass in prusa-gcodeviewer

```
& "C:\Program Files\Prusa3D\PrusaSlicer\prusa-gcodeviewer.exe" <file.gcode>
```

The installed viewer is a legitimate independent oracle — running the app and inspecting our own
output is squarely inside the clean-room boundary. Step through the layers and specifically check
the pause points, the travel moves immediately after each pause, and the return to the shared
surface for the second group. A path that looks wrong here is wrong.

## 4. Differential check against a PrusaSlicer reference

Slice the same model in PrusaSlicer with equivalent settings and compare aggregates — not text:

- layer count and the Z height of each layer;
- total extruded filament volume (expect agreement within a few percent, not exactly);
- bounding box of deposited material;
- per-layer extrusion volume curve.

Large divergence points at a real bug in the engine. This is the strongest correctness signal
available before hardware, and it copies nothing.

## 5. Report honestly

State what was checked and what was not. Distinguish:

- **validated** — passed the checks above;
- **not yet printed** — no physical confirmation exists.

Never let "passed validation" be read as "safe to print unattended". For a first-of-kind motion
pattern, say plainly that the user should watch the run and stay within reach of the stop control,
and tell them where the pauses will occur and what the first sign of failure would look like.
Escalate MK3S, then MK4, then Core One L.
