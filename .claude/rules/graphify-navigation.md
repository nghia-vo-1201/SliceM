# Rule: navigate via the Graphify graph, not by grepping the tree

A slicer grows into a large codebase fast — geometry, slicing, infill, G-code emission, scheduling,
plus a Qt GUI. Locating things by grepping and reading files re-derives the same structure over and
over and is the main avoidable token cost on this project. The knowledge graph is a cheap index:
query it, then read only the lines it points at.

## The graph

The project has a graph when `graphify-out/graph.json` exists at the project root. If a session
starts in a parent folder, also check one level down (e.g. `Apps/SliceM/graphify-out`).

Bootstrap it once `engine/` holds real code — both steps are offline and cost nothing:

```
graphify extract . --code-only
graphify cluster-only . --no-label
```

A `.graphifyignore` must exist first, covering build output, caches, Qt build dirs, and the raw
G-code sample dumps under `research/`. Those are bulky and worthless to index.

The semantic docs pass (`/graphify .`) costs tokens — run it **only** when the user asks.

## Query before you Grep or Read

For any "where is X / how does Y connect / what does Z touch" question:

```
graphify query "<question>" --budget 1500
graphify path "<A>" "<B>"
graphify explain "<Symbol>"
graphify affected "<Symbol>"
```

Add `--graph <dir>/graphify-out/graph.json` when it is not in the current folder. Then `Read` only
the ranges the graph reports. Reading whole files to find one function is the thing this rule
exists to prevent.

`graphify affected` is the one to reach for before changing an `engine/` interface — it gives the
blast radius directly, which is otherwise a multi-grep exercise.

For broad architecture orientation, read `graphify-out/GRAPH_REPORT.md` rather than walking the
directory tree.

## Keep it current

After modifying code, run `graphify update <project root>` before the final report. It is AST-only,
so it costs nothing, and a stale graph is worse than no graph because it sends the next session to
the wrong line.

## Give subagents the query, not the tree

A fresh subagent that starts grepping is the most expensive thing that happens in this project —
it re-derives the entire layout from zero. When delegating (`rules/delegation.md`), state that the
graph exists and hand over the exact command to run. Forks inherit this context and already know.

## The limit of the map

The graph is an index, not a source of truth. It can be stale, and it summarises rather than
reproduces. For anything correctness- or safety-critical — clearance and collision maths, thermal
commands, the colour-group scheduling logic — use the graph to find the code, then read the actual
code before judging it. Never quote the graph as evidence that a function behaves a certain way.
