---
name: slicing-algorithm-review
description: Review a geometry, slicing, infill, or scheduling algorithm in engine/ — degenerate-input coverage, numerics discipline, determinism, complexity, test quality, and adversarial review of clearance/safety logic. Use when reviewing or self-checking any engine algorithm before it is relied on.
---

# Slicing algorithm review

Slicing fails on real meshes, not on clean ones. This checklist is what separates an algorithm
that works on a cube from one that survives a user's STL.

Locate the code with `graphify query` / `graphify explain` rather than reading the tree
(`rules/graphify-navigation.md`), then read the actual implementation — the graph finds code, it
does not vouch for it.

## 1. Degenerate input

The algorithm must have a defined, tested answer for each of these, not merely "not crash":

- empty mesh; single triangle; fewer than 3 distinct vertices
- **a vertex or an entire edge lying exactly on the slice plane** — the classic source of
  duplicated or dropped contours
- a triangle coplanar with the slice plane
- zero-area and sliver triangles
- duplicate and near-duplicate vertices
- non-manifold edges; holes; inverted or inconsistent normals
- self-intersecting geometry; nested shells; disjoint components
- coordinates far from the origin, and very large or very small models

Open contours are the symptom to watch for: if a layer's polygons do not close, the cause is
almost always one of the first four cases.

## 2. Numerics

- 2D coordinates are scaled integers, not floats (`rules/cpp-style.md`).
- No float equality, anywhere. Above/below/on-plane is decided **once**, consistently, not
  re-derived per call site with a different epsilon.
- Every tolerance has a documented reason for its value where it is defined.
- Degenerate-case handling is explicit branching, not accidental behaviour that happens to work.

## 3. Determinism

Same input must give byte-identical output. A slicer that reorders paths between runs cannot be
diffed, cannot be regression-tested, and makes field bug reports unreproducible.

Watch for: iteration over `unordered_map`/`unordered_set` leaking into output order, sorts without
a total-order tiebreak, pointer-address or allocation-order dependence, and parallel loops that
append results in completion order.

## 4. Complexity and cost

- Stated complexity in the layer count, triangle count, and polygon vertex count.
- No accidental quadratic behaviour in a per-layer inner loop — the usual cause is a linear scan
  over all triangles per layer instead of bucketing triangles by Z span.
- No allocation inside the innermost loop.

## 5. Tests

- Expected output pinned on a hand-checkable input: a cube's layer count and per-layer area, a
  wedge's shrinking cross-section, two disjoint columns yielding two polygons per layer.
- At least one degenerate case from section 1.
- Analytic oracle where one exists — a cube sliced at 0.2 mm has a known layer count and known
  cross-sectional area; assert the number, not the absence of a crash.

## 6. Clearance and scheduling logic: review adversarially

For `engine/scheduling/` and anything feeding clearance decisions, invert the question: **what
input makes this emit an unsafe path?** Specifically confirm:

- groups are genuinely top-most, including the case where an unrelated tall feature exists
  elsewhere on the model;
- the shortest-first ordering actually holds after any later sort;
- the fallback to layer-wise colour change is reachable and tested, and the refusal path cannot be
  bypassed by an edge case (equal heights, touching footprints, a group of zero height);
- clearance comparisons use the right envelope for the printer's kinematics, not a shared default.

A missing fallback here is a hardware failure, so it outranks every other finding in this list.

## 7. Boundary

No Qt, OpenGL, or GUI dependency anywhere under `engine/`. This is a blocker, not a nit — the
engine is currently the only part that can be tested headlessly.
