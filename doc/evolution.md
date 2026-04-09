# Evolution Engine & Solver Protocol

## Overview

The evolution module replays SAT solver execution events on the graph in real time. It supports both forward replay (processing new events from a running solver) and backward replay (undoing events to inspect earlier states).

## Event Types

| Type | Fields | Description |
|------|--------|-------------|
| `VariableAssignment` | `NodeId var; int state; double activity` | Variable assignment. state: 0=unassigned, 1=true, −1=false |
| `ClauseEvent` | `ClauseAction action; vector<int> literals` | Clause add/remove with signed literals |
| `ConflictEvent` | `int conflict_num` | Conflict notification |
| `SolverEvent` | `variant<VariableAssignment, ClauseEvent, ConflictEvent>` | Discriminated union |

`ClauseAction`: `Add` or `Remove`.

---

## Pipe Protocol

The external solver communicates via a POSIX named FIFO. Each line is one event:

| Prefix | Format | Example | Meaning |
|--------|--------|---------|---------|
| `v` | `v <d|p> <state> <activity> <id>` | `v d 1 0.5 42` | Variable assignment (d=decision, p=propagation) |
| `c` | `c <+|-> <literals...> 0` | `c + 1 -2 3 0` | Add/remove clause with signed literals |
| `!` | `! <conflict_num>` | `! 7` | Conflict #7 |

**Simplified format** (also supported): `v <id> <state> <activity>`

### Literal-to-Node Mapping

| Mode | Positive `+X` | Negative `-X` |
|------|--------------|---------------|
| VIG | `NodeId(X)` | `NodeId(X)` |
| LIG | `NodeId(2×X)` | `NodeId(2×X + 1)` |

---

## History & Undo System

Every event records a snapshot before modifying the graph:

### Snapshot Types

| Type | Fields | Purpose |
|------|--------|---------|
| `NodeSnapshot` | `NodeId id; Assignment prev_assignment; double prev_activity` | Pre-change node state |
| `ClauseSnapshot` | `vector<int> literals; bool was_added` | Clause change direction |
| `HistoryEntry` | `vector<NodeSnapshot> node_changes; optional<ClauseSnapshot> clause_change; optional<int> conflict_num; optional<NodeId> decision_var` | Complete undo record |

### State Transitions

```
[Initial]
    │
    ├─ process_event(VariableAssignment)
    │     Update node.assignment + activity
    │     Record NodeSnapshot
    │
    ├─ process_event(ClauseEvent{Add})
    │     Add edges for all literal pairs in clause
    │     Record ClauseSnapshot{was_added=true}
    │
    ├─ process_event(ClauseEvent{Remove})
    │     Remove edges between literal pairs
    │     Record ClauseSnapshot{was_added=false}
    │
    ├─ process_event(ConflictEvent)
    │     Increment conflict counter
    │     Clear decision_variable
    │     Record conflict_num in HistoryEntry
    │
    ├─ step_backward()
    │     Pop HistoryEntry
    │     Restore node assignments/activities from snapshots
    │     Reverse clause change (add→remove, remove→add)
    │     Decrement conflict counter if applicable
    │
    └─ jump_to_conflict(target)
          Repeated step_backward() until current_conflict == target
```

---

## EvolutionEngine API

### Forward Evolution

| Method | Description |
|--------|-------------|
| `process_event(const SolverEvent&)` | Apply single event, record history |
| `process_line(const string&)` | Parse + process pipe-format line |

### Backward Evolution

| Method | Description |
|--------|-------------|
| `step_backward() -> bool` | Undo last event. Returns false if history empty |
| `jump_to_conflict(int target) -> bool` | Step backward to specific conflict number |

### Observers

| Method | Description |
|--------|-------------|
| `add_observer(EvolutionObserver*)` | Register observer |
| `remove_observer(EvolutionObserver*)` | Unregister observer |

Observer callbacks: `node_assigned`, `clause_added`, `clause_removed`, `conflict`, `update_graph`, `new_file_ready`, `decision_variable`.

### File Buffering

| Method | Description |
|--------|-------------|
| `buffer_file(const string& path)` | Background thread reads file, notifies observers |
| `has_buffered_file() -> bool` | Check if read complete |
| `take_buffered_file() -> optional<vector<string>>` | Take buffered lines |

### Accessors

`current_conflict()`, `history_depth()`, `event_count()`, `decision_variable()`, `mode()`.

---

## External Solver Interface (`solver.hpp`)

### NamedFifo

RAII wrapper for POSIX `mkfifo`. Auto-deletes on destruction.

### ExternalSolver

| Method | Description |
|--------|-------------|
| `start(solver_path, cnf_path, fifo_path)` | Fork+exec solver. Child invoked as `solver cnf --pipe=fifo_path` |
| `wait_for_result(timeout) -> SolverResult` | Polling waitpid (50ms interval) |
| `cancel()` | SIGTERM → 5s grace → SIGKILL |
| `is_running() -> bool` | Non-blocking waitpid |
| `open_fifo_for_read(timeout=5s) -> int` | Non-blocking open with poll loop |
| `read_fifo_line(timeout=30s) -> string` | Read one newline-terminated line via poll() + read() |

### SolverResult

| Value | Meaning |
|-------|---------|
| `SAT` | Exit code 10 |
| `UNSAT` | Exit code 20 |
| `UNKNOWN` | Other exit codes |
| `CRASH` | Killed by signal |
| `TIMEOUT` | Wait exceeded deadline |

### Lifecycle

```
start(solver, cnf, fifo) → open_fifo_for_read() → read_fifo_line() [loop]
                                                    │
                                                    ├─ parser → engine.process_line()
                                                    └─ empty line → solver finished
                                                         │
                                                         └─ wait_for_result() or cancel()
```
