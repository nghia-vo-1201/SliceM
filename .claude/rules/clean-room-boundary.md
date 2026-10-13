# Rule: clean-room boundary

This is the project's licence foundation, not a style preference. SliceM must remain provably
independently authored so it carries no AGPL obligation from PrusaSlicer.

## Forbidden — never read, fetch, grep, or quote

- Source code of **PrusaSlicer, Slic3r, libslic3r, OrcaSlicer, SuperSlicer, Cura, CuraEngine,
  BambuStudio, Simplify3D** — any slicer. Includes repos, forks, mirrors, vendored copies, patches,
  pull requests, code review threads, and code quoted inside issues, blog posts, or Stack Overflow
  answers.
- Decompilation, disassembly, or binary inspection of `PrusaSlicer.dll`, `prusa-slicer.exe`, or any
  slicer binary. Do not run a disassembler, a decompiler, or a strings-mining pass over them.
- Any text that is plainly a transcription of slicer source (e.g. a forum post pasting
  `TriangleMeshSlicer` internals).

If such material is encountered accidentally: **stop reading, do not quote or summarise it, and
note in the task output that it was skipped.** Do not "just look for orientation".

## Permitted

- **Running** the installed PrusaSlicer, including `prusa-slicer-console.exe`, and observing its
  behaviour, UI labels, warnings, and exit codes.
- **Files PrusaSlicer writes**: exported G-code, `.3mf` / `.gcode.3mf` project archives, exported
  config `.ini` / config bundles, its `PrusaSlicer.ini`.
- **Config data files it ships** (`resources/profiles/*.ini`, `vendor/*.ini`). These are
  configuration data, not program source.
- **Public end-user documentation**: help.prusa3d.com, Prusa's G-code reference pages, printer
  manuals and spec sheets, release notes, user forum discussion of *behaviour*.
- **Firmware-side documentation**: Marlin and Prusa-Firmware-Buddy G-code references. A documented
  command interface may be targeted freely — that is interoperability, the same basis on which any
  third-party slicer supports Prusa machines.
- **General literature**: computational-geometry papers and textbooks, published slicing algorithm
  descriptions, course material. Cite them.

## The data-vs-expression line in shipped profiles

Vendor `.ini` profiles are permitted reading, but treat their contents in two buckets:

- **Facts — free to use.** Bed dimensions, nozzle diameter, temperature and acceleration limits,
  filament densities, firmware flavour, retraction values. Functional parameters of a physical
  machine are not protectable expression, and matching them is required for compatibility.
- **Authored text — do not copy verbatim.** Custom start/end G-code templates, notes, and
  descriptions are written prose. Use them to determine *which commands are required and in what
  order*, record that as a requirement list in `research/`, then author SliceM's own sequence.
  Never paste a template into SliceM's source or profiles.

When unsure which bucket something falls in, treat it as authored text.

## Provenance is mandatory

Every file in `research/` carries a `## Provenance` section stating:

- how each finding was derived (which command was run, which file was inspected, which setting was
  toggled);
- the exact paths of observed artifacts and the sample models used;
- URLs and access date for any documentation cited;
- an explicit line confirming no slicer source was consulted.

A research doc without provenance is not accepted — it cannot be audited later, which defeats the
purpose of working this way.

## Scope note

This rule governs how SliceM learns about *other slicers*. It places no limits on reading SliceM's
own code, nor on normal open-source dependencies (Qt, a unit-test framework, zip/STL IO), which are
used as libraries under their own licences rather than as sources of slicing know-how.
