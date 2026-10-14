# Design: same-surface sequential multi-colour printing

The feature SliceM exists for. Status: **design only — not implemented.** It must not be built
until G-code emission works and has been validated on hardware, because every failure mode here is
mechanical.

## The problem

A conventional slicer changes filament colour only at a layer boundary: print layer N, pause, the
user swaps filament, resume at layer N+1. Everything on a given layer is therefore one colour.

That makes a common case impossible. Take a flat plate with two separate blocks of embossed text
rising from the same face. Both blocks occupy the same layers, so a layer-wise colour change gives
them the same colour. Printing them in different colours would need either a second extruder or a
tool changer.

But if both blocks are the **top-most** geometry — nothing in the model is printed above them —
the layer-by-layer constraint is unnecessary. The print can be reordered by *feature group*:

```
print everything up to and including the shared surface
  -> pause -> print group A completely, bottom-up
  -> pause -> print group B completely, bottom-up
```

Each group gets its own colour from a single-material printer.

## Why this is a mechanics problem, not a scheduling problem

Reordering reintroduces the collision hazard of sequential printing. Once group A stands at height
`H_A`, printing group B at any `z < H_A` puts the toolhead and gantry at a height where the
finished group A is above them. On an MK3-class machine the bed travels in Y past a gantry sitting
near nozzle height, so the finished part is swept into it; on Core One the toolhead sweeps in XY
instead. Either way, the part gets knocked off the bed or the hotend is damaged.

`research/toolhead-clearance-model.md` has the verified envelope. The headline numbers — the
maximum height of already-printed geometry the toolhead can pass over — are **MK3S 11 mm,
MK4S 11 mm, Core One L 33 mm**, taken from the lowest full-width obstruction in each machine's
gantry cross-section stack. These are stricter than PrusaSlicer's scalar
`extruder_clearance_height`, which is a fallback the real machines ignore and which overstates the
MK3S by 9 mm.

Clearance is also **directional**: the obstruction occupies a specific band relative to the nozzle,
so "is the other group far enough away" depends on direction, not just distance.

## Algorithm

### 1. Per-layer islands

For each layer, the slicer already produces disjoint closed regions. Each is an *island*.

### 2. Link islands into towers

Island `a` on layer `i` is the parent of island `b` on layer `i+1` when their XY regions
intersect (a polygon intersection test — `polygon_intersection` is non-empty).

This gives a directed acyclic graph over layers. A **branch point** is an island with two or more
children that have no other ancestors in common above it. The subtree rooted at each child is a
**tower**.

### 3. Find candidate shared surfaces

A layer `S` is a candidate when:

- an island on `S` has ≥2 children on `S+1`;
- those children's towers are pairwise disjoint in XY at every layer (they never merge again — if
  two towers rejoin higher up, printing one completely would leave the other's overhang
  unsupported);
- each tower terminates: following it upward, it ends with no further children.

### 4. Top-most test

The towers must be the last thing printed. Formally: no island anywhere in the model at any layer
above `S` belongs to a component outside the candidate towers. If any unrelated geometry exists
above the shared surface, reordering would print it out of sequence, so the candidate is rejected.

This is the condition that makes the whole thing safe and is also why the feature is inherently
limited to the top of a model. That limit is inherent, not an implementation shortcut.

### 5. Safety gate

Reject the candidate unless **all** hold:

1. Groups ordered **shortest first** — exposure is bounded by the height of whichever group is
   printed first, so this strictly minimises risk.
2. `height(first printed group) <= profile.max_printed_group_height_mm`.
3. Each already-printed group lies outside the toolhead cross-section at the relevant relative
   height, for every travel and print move of the group being printed. A scalar radius is
   acceptable only as a conservative over-estimate.
4. No group overhangs — the first implementation refuses any tower whose layers grow outward,
   because an overhang in a reordered group has nothing beneath it when printed.
5. On Core One L, the shared surface is below the ~185 mm ceiling.

On any failure: **do not emit the reordered path.** Fall back to a conventional layer-wise colour
change and tell the user which check failed and why. The feature is allowed to be unavailable; it
is not allowed to be unsafe.

### 6. Emission

```
layers 0..S                      normal order
pause (M600) + colour prompt
tower with the smaller height    all its layers, bottom-up, perimeters then infill
pause (M600) + colour prompt
remaining tower                  all its layers, bottom-up
end-of-job sequence
```

`M600` is the filament-change command on both the legacy and Buddy families
(`research/gcode-dialect-*.md`). Its exact behaviour — whether the printer holds temperature,
waits for the user and auto-resumes — is **unverified** and must be confirmed on hardware before
the first real print.

Z-handling needs care at each transition: the nozzle returns from the finished tower's top down to
the shared surface. That descent is a travel move subject to check 3 above, not a normal layer
change.

## What must exist first

1. Polygon boolean intersection (for island linking) — *in progress*.
2. Working G-code emission validated on the MK3S.
3. A printer profile carrying the gantry cross-section stack, not just scalars.
4. Confirmed `M600` behaviour on the user's machines.

## Validation plan

1. Unit tests on synthetic meshes: two embossed blocks on a plate (accept); blocks of unequal
   height (accept, shortest first); a tall mast elsewhere on the model (reject, not top-most);
   towers that rejoin (reject); a group exceeding the height cap (reject with fallback); an
   overhanging tower (reject).
2. G-code structural validation plus a viewer pass, with explicit inspection of the descent back to
   the shared surface.
3. A physical test on the **MK3S first** — a plate with two 5 mm-tall text blocks, well inside the
   11 mm cap — watched, with the user within reach of the stop control. Only then MK4, then
   Core One L.
