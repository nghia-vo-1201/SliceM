# Rule: printer safety

SliceM's output drives real hardware that heats to 300 °C and moves a toolhead with enough force to
tear a part off the bed. A slicing bug is not a failing test; it is a crashed gantry, a damaged
hotend, or a thermal event in an enclosed printer. Treat emitted G-code as the highest-risk
artifact in the project.

## Never guess

If the correct command, parameter, or limit for a printer is unknown or ambiguous, **refuse to emit
it and report why**. A missing feature is recoverable; a plausible-looking wrong command is not.
Every command SliceM emits must trace to a `research/` doc for that firmware family
(legacy MK3 vs. Buddy — they are not interchangeable).

## Thermal

- Never emit a temperature outside the limits declared by the printer/filament profile.
- Never drop a required wait (`M109` / `M190` style) where the subsequent move assumes the
  setpoint has been reached.
- Never leave heaters on at the end of a job or at an abort path.
- Enclosed machines (Core One / Core One L) have chamber behaviour the open MK3/MK4 do not. Do not
  carry chamber assumptions across families in either direction.

## Motion and clearance

- Every move must be inside the profile's build volume, including travel and Z-hop.
- No move may be emitted that has not been clearance-verified against the target printer's
  toolhead and gantry geometry.

### The colour-group feature specifically

Reordering by feature group reintroduces the collision hazard of sequential printing. When a
finished group stands taller than the current nozzle height, the toolhead body and (on MK3-class
machines, where the bed travels in Y past a fixed gantry) the X-gantry assembly can strike it.

Required checks before emitting a grouped-colour job:

1. **Order groups shortest-first.** Risk is bounded by the height of whichever group is printed
   first, so this strictly minimises it.
2. `height(first printed group) <= profile.max_printed_group_height_mm`, taken from
   `research/toolhead-clearance-model.md`: **MK3S 11 mm, MK4S 11 mm, Core One L 33 mm**.
   Do **not** use PrusaSlicer's scalar `extruder_clearance_height` — it is a fallback that the
   real machines ignore, and for the MK3S it is optimistic by 9 mm, which is a crash.
3. Horizontal separation between groups must clear the toolhead cross-section at the relevant
   height. Clearance is **directional**, not a radius: the obstruction sits in a specific y band
   relative to the nozzle. A scalar radius is only acceptable as a conservative over-estimate.
4. On Core One L nothing passes above roughly 185 mm; grouped printing is unavailable above that
   height regardless of group size.
4. The groups are genuinely top-most — no geometry anywhere in the model is printed after them.
5. Nothing else in the model rises above the shared surface within the swept envelope.

If any check fails, **do not emit the reordered path**. Fall back to a conventional layer-wise
colour change and tell the user which check failed and why. The feature is allowed to be
unavailable; it is not allowed to be unsafe.

## Before any physical print

- Run the `gcode-validation` skill. Structural validation and a viewer/simulator pass come before
  hardware, every time.
- Escalate across machines in cost order: **MK3S first, then MK4, then Core One L.** Prove the
  pattern on the machine that is cheapest to crash.
- Tell the user, explicitly and before they start: what the print should do, where the pauses
  occur, what the first failure would look like, and that they should stay within reach of the
  stop control for a first-of-kind path.
- First runs of a new motion pattern are watched runs. Never describe a path as safe because it
  validated structurally — say that it passed validation and has not yet been printed.
