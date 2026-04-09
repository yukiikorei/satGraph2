# Evolution Engine

Tracks SAT solver execution in real time, parsing variable assignments, clause changes, and conflicts from solver output. Supports forward/backward stepping through solver events with state history.

## ADDED Requirements

### Requirement: SAT Solver Launch
The system SHALL launch an external SAT solver binary (user-specified via `-s`) as a child process with pipe output enabled so that solver events are emitted to a named FIFO during solving.

#### Scenario: External solver start
- **WHEN** the evolution engine is given a CNF file path and solver binary path `-s /usr/local/bin/cadical`
- **THEN** cadical is launched as a child process with `--pipe /tmp/satgraf-pipe-<pid>` argument, and the engine begins reading events from the FIFO

#### Scenario: Solver completion
- **WHEN** the solver finishes solving (satisfiable or unsatisfiable)
- **THEN** the engine reports the solver result and stops accepting new events

### Requirement: Event Line Parsing
The engine SHALL parse three types of event lines from solver output: variable assignment lines (prefix 'v'), clause add/remove lines (prefix 'c'), and conflict lines (prefix '!').

#### Scenario: Variable assignment event
- **WHEN** the engine reads the line `v 42 1 0.85`
- **THEN** variable 42 is recorded with assignment state "true" and activity level 0.85

#### Scenario: Variable unassignment event
- **WHEN** the engine reads the line `v 42 0 0.85`
- **THEN** variable 42's assignment state is set to "unassigned" (the state value 0 indicates cleared)

#### Scenario: Clause addition event
- **WHEN** the engine reads the line `c + 1 -3 5 0`
- **THEN** a new clause is added containing literals (var 1, positive), (var 3, negated), (var 5, positive)

#### Scenario: Clause removal event
- **WHEN** the engine reads the line `c - 1 -3 5 0`
- **THEN** the clause matching those literals is marked as removed from the active set

#### Scenario: Conflict event
- **WHEN** the engine reads the line `! 47`
- **THEN** conflict number 47 is recorded, and the current conflict count advances to 47

### Requirement: Forward Evolution
The engine SHALL process event lines sequentially, updating node assignment states, adding/removing edges, and tracking the current solver state.

#### Scenario: Sequential event processing
- **WHEN** events `v 1 1 0.5`, `v 2 0 0.3`, `c + 1 -2 0`, `! 1` arrive in order
- **THEN** after processing, node 1 is assigned true, node 2 is unassigned, the clause (1, -2) exists, and the conflict count is 1

#### Scenario: Activity update during forward evolution
- **WHEN** a 'v' line reports activity 0.9 for variable 7
- **THEN** node 7's activity counter is updated to 0.9

### Requirement: Backward Evolution
The engine SHALL support reverting to a previous state by popping from a state history stack. Each backward step undoes the most recent forward step.

#### Scenario: Undo variable assignment
- **WHEN** the engine processes `v 7 1 0.5` (forward), then receives a backward command
- **THEN** node 7's assignment reverts to its previous state (unassigned), and its activity reverts to its previous value

#### Scenario: Undo clause addition
- **WHEN** a clause was added in the last forward step, and a backward step is issued
- **THEN** the clause is removed from the active set

#### Scenario: Undo clause removal
- **WHEN** a clause was removed in the last forward step, and a backward step is issued
- **THEN** the clause is restored to the active set

#### Scenario: State history stack depth
- **WHEN** 1,000 forward steps have been processed
- **THEN** the state history stack contains 1,000 entries, each representing one reversible step

### Requirement: File Buffering
The engine SHALL split solver output into buffered files of approximately 1 million lines each. While the current file is being processed, the next file SHALL be buffered in a background thread.

#### Scenario: Large output split across files
- **WHEN** the solver produces 3.5 million lines of output
- **THEN** the output is split into 4 files: file 1 (lines 1-1M), file 2 (lines 1M-2M), file 3 (lines 2M-3M), file 4 (lines 3M-3.5M)

#### Scenario: Background buffering
- **WHEN** the engine is processing file 2
- **THEN** file 3 is being loaded into memory by a background thread, so the transition from file 2 to file 3 is near-instantaneous

#### Scenario: Last file has no next buffer
- **WHEN** the engine is processing the final file
- **THEN** no background buffering occurs and the engine reports end-of-input when the file is exhausted

### Requirement: Conflict Scanning
The engine SHALL support jumping directly to a specific conflict number, scanning forward or backward through events to reach it.

#### Scenario: Jump forward to conflict
- **WHEN** the engine is at conflict 10 and the user requests conflict 50
- **THEN** the engine processes forward events until conflict number 50 is reached, then pauses

#### Scenario: Jump backward to conflict
- **WHEN** the engine is at conflict 50 and the user requests conflict 10
- **THEN** the engine pops backward through the state history until conflict 10 is reached, then pauses

#### Scenario: Conflict number out of range
- **WHEN** the user requests conflict 999 but only 100 conflicts have occurred
- **THEN** the engine reports that the requested conflict number has not yet been reached and remains at the current position

### Requirement: Decision Variable Tracking
The engine SHALL track the current decision variable, highlighting it in the graph. When a new decision variable is chosen by the solver, the previous one is cleared.

#### Scenario: Decision variable highlighted
- **WHEN** the solver assigns variable 15 as a decision (indicated by the event stream)
- **THEN** node 15 is flagged as the current decision variable, and any previously flagged decision variable is unflagged

#### Scenario: Decision variable cleared on conflict
- **WHEN** a conflict occurs after decision variable 15
- **THEN** node 15's decision variable flag is cleared

### Requirement: Observer Pattern
The engine SHALL support registered observers that receive callbacks for: `nodeAssigned(nodeId, state)`, `addEdge(edge)`, `removeEdge(edge)`, `updateGraph()`, and `newFileReady(filename)`.

#### Scenario: Observer notified on assignment
- **WHEN** the engine processes a variable assignment for node 7
- **THEN** all registered observers receive `nodeAssigned(7, true)` (or the appropriate state)

#### Scenario: Observer notified on clause event
- **WHEN** a clause is added, creating a new edge
- **THEN** observers receive `addEdge(edge)` with the new edge

#### Scenario: Observer notified on graph update
- **WHEN** a batch of events has been processed
- **THEN** observers receive `updateGraph()` to trigger a visual refresh

#### Scenario: Observer notified on file transition
- **WHEN** the background buffer finishes loading the next file
- **THEN** observers receive `newFileReady(filename)` so they can prepare for the transition

#### Scenario: Multiple observers
- **WHEN** three observers are registered and an event occurs
- **THEN** all three observers receive the callback in registration order

### Requirement: VIG and LIG Mode Support
The engine SHALL support operating in both VIG and LIG graph modes during evolution, affecting how edges are created and how node identity is interpreted.

#### Scenario: VIG mode evolution
- **WHEN** the engine is in VIG mode and a clause with literals +3 and -5 is added
- **THEN** an edge is created between unsigned variable nodes 3 and 5

#### Scenario: LIG mode evolution
- **WHEN** the engine is in LIG mode and a clause with literals +3 and -5 is added
- **THEN** an edge is created between signed literal nodes (+3) and (-5), preserving polarity

#### Scenario: Mode switching during evolution
- **WHEN** the mode is switched from VIG to LIG mid-evolution
- **THEN** the engine rebuilds the graph in LIG mode from the current event state and continues
