---
name: printer-profile-builder
description: Turn researched machine facts into a SliceM printer profile — required fields, how to derive the toolhead clearance values the colour-group feature depends on, authoring our own start/end sequences from a requirement list, and the validation gate before a profile is marked usable. Use when adding or correcting support for a Prusa model.
---

# Printer profile builder

Produces one profile under `engine/gcode/profiles/` from a researched spec in
`research/printer-profiles/<model>.md`. No profile is written from memory — if the spec does not
exist, run the `clean-room-research` skill first.

## Required fields

Geometry and limits (these are machine facts, free to use — see `rules/clean-room-boundary.md`):

- bed shape, size, origin; maximum print height
- nozzle diameter; filament diameter
- per-axis maximum feedrate, acceleration, jerk
- temperature limits: nozzle, bed, and chamber where applicable
- retraction defaults

Identity and dialect:

- `firmware_family`: `legacy` (MK3, MK3S) or `buddy` (MK4, MK4S, Core One, Core One L)
- the colour-change / pause command and its exact semantics for that family
- whether the machine is enclosed (chamber behaviour differs; do not carry assumptions across
  families)

Clearance — required by the flagship feature:

- `toolhead_clearance_height`
- `toolhead_clearance_radius`

## Deriving the clearance values

These two numbers decide whether the colour-group feature is permitted on a machine, and a wrong
value crashes a gantry. Derive them from the physical geometry of the toolhead and gantry —
published dimensions, manual diagrams, or direct measurement of the user's machine — and **record
in the profile comment how each number was obtained**.

Note the kinematic difference: on MK3-class machines the bed travels in Y past a gantry that sits
at roughly nozzle height, so the part is swept under the X assembly; on MK4/Core One the toolhead
body sweeps instead. The swept envelope is not the same shape, so the two numbers are not
transferable between families.

If a value cannot be established, **leave it unset and let the feature be unavailable on that
printer**. An absent feature is recoverable; a guessed clearance is not. This is
`rules/printer-safety.md`, not a judgement call.

## Start and end sequences

Work from the *requirement list* in the research doc — which commands are needed, in what order,
and why. Author SliceM's own sequence satisfying those requirements. Never paste PrusaSlicer's
authored templates into the profile.

Each emitted command must be traceable to the research doc for that firmware family.

## Validation gate

A new profile is marked usable only after:

1. slicing a reference 20 mm cube through `cli/` with that profile;
2. a full pass of the `gcode-validation` skill;
3. a `prusa-gcodeviewer` visual inspection;
4. a differential comparison against a PrusaSlicer slice of the same cube (layer heights, extruded
   volume, bounding box).

Then update the printer-support status in `.claude/CLAUDE.md`. Until a physical print has run, the
profile is recorded as **validated, not yet printed** — and physical validation goes MK3S first,
then MK4, then Core One L.

## Delegation

Writing the data file from a settled spec is mechanical and a good candidate for a subagent. The
clearance derivation is not — keep it with the main model and re-derive every number that will end
up in emitted G-code (`rules/delegation.md`).
