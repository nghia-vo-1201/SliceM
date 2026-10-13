# Rule: delegate simple work, then verify it

Bounded, mechanical subtasks **should** be handed to subagents when doing so saves tokens or
wall-clock time. The main model then verifies the result before accepting it. Delegation is a tool
for efficiency, not a default — a task that takes one tool call is faster done directly.

## Delegate these

- Batch runs that produce **verbose output nobody needs to keep**: slicing a matrix of test models
  through `prusa-slicer-console.exe`, diffing G-code, summarising what changed. Raw G-code is
  thousands of lines; only the extracted pattern matters. This is the single best use of delegation
  on this project.
- Writing a printer-profile data file from an already-settled spec.
- Boilerplate: CMake targets, header/source skeletons, build glue.
- Unit tests for a function whose signature and semantics are already decided.
- Mechanical repeats of a pattern already applied once, across many files.
- Formatting and documentation passes.
- Independent research questions that can run in parallel.

## Keep these with the main model

- Design of slicing and geometry algorithms.
- The same-surface colour-group detection and scheduling logic.
- Anything touching **clearance, collision, or thermal behaviour** — being wrong here breaks
  hardware, not a build.
- Clean-room compliance judgement (`rules/clean-room-boundary.md`).
- Deciding what a research finding *means* for SliceM's design.

## Verification is not optional

A subagent's report describes what it *intended* to do. Before accepting:

- read the files it actually wrote or changed, not just its summary;
- check research docs carry real provenance and contain no slicer-source-derived content;
- re-derive any number that will end up in emitted G-code;
- for code, confirm it matches the agreed interface — and say plainly that it is unbuilt if the
  toolchain cannot compile it yet.

If verification costs more than doing the task would have, that task should not have been
delegated. Adjust next time.

## Choosing the agent

- Project-specific personas live in `.claude/agents/`: `clean-room-researcher`,
  `geometry-engineer`, `gcode-engineer`, `qt-ui-engineer`. Prefer these — their constraints are
  already written down, especially the researcher's clean-room restrictions.
- A **fork** inherits the current conversation context and keeps noisy tool output out of the main
  context; use it for open-ended work that depends on what has already been established here.
- A **fresh agent** starts cold and needs a full briefing: the goal, what has been ruled out, the
  constraints, and the clean-room rule restated explicitly. Never send a fresh agent to do
  PrusaSlicer research without quoting the boundary rule to it.
