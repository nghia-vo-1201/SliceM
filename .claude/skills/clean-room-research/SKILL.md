---
name: clean-room-research
description: Black-box reverse-engineer one PrusaSlicer behaviour into a research spec — controlled slicing experiments through prusa-slicer-console.exe, G-code diffing, and a provenance-backed doc under research/. Use when SliceM needs to know what PrusaSlicer emits or how it behaves, and never when the question would require reading slicer source.
---

# Clean-room research pass

Turns a question about PrusaSlicer's behaviour into an auditable spec in `research/`, without ever
reading slicer source. Read `rules/clean-room-boundary.md` first — the permitted/forbidden lists
govern this whole procedure.

## 1. Frame the question as an observable difference

A researchable question is phrased in terms of what the program *does or writes*:

- good: "which commands does PrusaSlicer emit around a colour change, and how do they differ
  between MK3S and Core One L?"
- good: "what Z heights does it choose for the first layer vs. subsequent layers at 0.2 mm?"
- not researchable this way: "how is its polygon offsetting implemented?" — that needs source.
  Answer that class of question from computational-geometry literature and SliceM's own design
  instead, and say so.

## 2. Design a controlled experiment

- Use the **smallest model that exhibits the behaviour** — a 20 mm cube, a two-column part, a
  plate with two embossed patches. Keep test models in `research/models/`.
- Vary **one setting at a time**. A diff across two configs differing in one option is evidence; a
  diff across two unrelated exports is noise.
- Capture the configs used. `File > Export > Export Config` writes an `.ini` you can re-feed to the
  CLI for reproducibility.

## 3. Discover the CLI surface rather than assuming it

```
& "C:\Program Files\Prusa3D\PrusaSlicer\prusa-slicer-console.exe" --help
```

Read the actual flag list before scripting a batch. Flags, defaults, and exit codes are the
interface you are observing — do not write them from memory.

## 4. Run the batch — delegate it

Batch slicing produces thousands of lines of G-code that nobody needs to keep in context. Per
`rules/delegation.md`, hand the run to the `clean-room-researcher` agent: it slices the matrix,
diffs the outputs, and returns only the extracted pattern. Restate the clean-room boundary in the
prompt if you brief a fresh agent.

Keep raw artifacts in the session scratchpad. Commit only the extracted spec plus short
representative excerpts — raw dumps bloat the repo and the graph index.

## 5. Write the spec

Create or update `research/<topic>.md` with:

- **Finding** — the behaviour, in plain English.
- **Requirement list** — the commands involved, their order, and what each one accomplishes. This
  is the artifact SliceM implements against.
- **Firmware differences** — legacy (MK3/MK3S) vs. Buddy (MK4/MK4S/Core One/Core One L) stated
  separately. Never assume one family's command works on the other.
- **Open questions** — what the experiment did *not* settle.
- **Provenance** — mandatory: commands run, files inspected, models and configs used, doc URLs
  with access date, and an explicit line confirming no slicer source was consulted.

## 6. Respect the data/expression line

Record *which commands are required and in what order*. Do not paste PrusaSlicer's authored
start/end G-code templates, notes, or descriptions into the spec or into SliceM. Machine
parameters (bed size, limits, temperatures) are facts and may be used directly. When unsure, treat
it as authored text and write our own equivalent.

## 7. Verify before accepting

Every claim traces to an observed artifact or a cited document, or it is labelled `unverified`. A
doc that cannot be audited later is worse than no doc, because it will be trusted anyway.
