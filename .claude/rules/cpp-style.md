# Rule: C++ conventions

C++17, CMake >= 3.21, Windows-first but no gratuitously platform-locked code in `engine/`.

## Module boundary (hard)

`engine/` must not include or link Qt, OpenGL, or anything GUI. The engine is a library that takes
a mesh plus settings and returns layers/toolpaths/G-code; `cli/` and `gui/` are the only places
allowed to know about presentation. A Qt include appearing under `engine/` is a review blocker, not
a nit — it is what keeps the engine testable headlessly, which matters more here than usual because
the GUI cannot currently be built at all.

## Geometry numerics

Slicing is a numerics problem before it is a software problem.

- User-facing dimensions: `double`, millimetres.
- Internal 2D polygon coordinates: **scaled integers** (`int64_t`, 1 unit = 1 nm, i.e. mm * 1e6).
  Integer coordinates make polygon boolean and offset operations exact and terminating.
- **Never compare floats for equality**, and never test a point against a plane with `==`. Use an
  explicit epsilon that is documented where it is defined, and prefer deciding
  above/below/on-plane once, consistently, over re-deriving it per call site.
- Degenerate input is the norm, not the exception: vertices exactly on the slice plane, zero-area
  triangles, duplicate vertices, non-manifold edges, self-intersections, inverted normals. A
  function that assumes clean input is incomplete.

## Naming and shape

- Types `PascalCase`; functions and variables `snake_case`; constants `kPascalCase`.
- `#pragma once` in headers.
- Prefer free functions over classes where there is no state to hold.
- Errors at the public API edge are returned, not thrown; exceptions may be used internally.
- No `using namespace` in headers.

## Tests

Every geometry, slicing, infill, or scheduling function ships with unit tests in `tests/`,
including at least one degenerate case from the list above. For algorithms, a test that pins the
expected output on a hand-checkable input (a cube, a wedge, two disjoint columns) is worth more
than a test that merely asserts no crash.

## Comments

Default to none. Write one only where the *why* is not recoverable from the code: a numeric
tolerance and the reason for its value, a non-obvious invariant, a deliberate deviation from the
obvious algorithm. Do not describe what the code does, and do not leave notes about the task or
change that introduced it.
