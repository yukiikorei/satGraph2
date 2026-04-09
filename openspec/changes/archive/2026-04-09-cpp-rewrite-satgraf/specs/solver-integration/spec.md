# Solver Integration

Integration layer for launching external SAT solver binaries (cadical, kissat, minisat, or any compatible solver) as child processes, communicating solver events via named pipe (FIFO).

## ADDED Requirements

### Requirement: External Solver Binary Launch
The system SHALL launch a user-specified solver binary as an external process, passing the CNF file path and pipe output path as arguments. The solver path is mandatory — there is no embedded solver.

#### Scenario: Launch solver with explicit path
- **WHEN** the user runs the application with `-s /usr/local/bin/cadical --pipe /tmp/satgraf-pipe-12345 input.cnf`
- **THEN** cadical is spawned as a child process, and the engine begins reading events from the named pipe

#### Scenario: Solver path not provided
- **WHEN** evolution mode is requested but no `-s` option is given
- **THEN** the system reports an error indicating that a solver binary path is required (`-s <path>`)

#### Scenario: Invalid solver path
- **WHEN** the specified solver path does not exist or is not executable
- **THEN** the system reports an error and does not attempt to solve

### Requirement: Pipe Output Protocol
The solver binary SHALL write event lines to a named pipe (FIFO) during solving. The system SHALL create the FIFO and pass its path to the solver via the `--pipe` argument. Event lines follow the format: 'v' lines (variable assignment), 'c' lines (clause add/remove), '!' lines (conflict).

#### Scenario: FIFO creation and path passing
- **WHEN** the system prepares to launch a solver
- **THEN** a named FIFO is created at a temporary path (e.g., `/tmp/satgraf-pipe-<pid>`), and the path is passed to the solver binary as `--pipe <path>`

#### Scenario: Solver writes events to FIFO
- **WHEN** the solver processes the CNF instance
- **THEN** it writes event lines ('v', 'c', '!') to the FIFO, and the evolution engine reads them in real time

#### Scenario: Pipe output does not affect solver behavior
- **WHEN** the solver runs with pipe output enabled
- **THEN** the solver's search behavior and result (SAT/UNSAT) are identical to running without pipe output

### Requirement: Solver Process Management
The system SHALL manage the solver process lifecycle: start, monitor, and stop.

#### Scenario: Start solver process
- **WHEN** the system launches the solver binary with the CNF file and pipe path as arguments
- **THEN** the solver process starts, and the engine begins reading from the FIFO

#### Scenario: Monitor solver completion
- **WHEN** the solver process is running
- **THEN** the system monitors the process state and detects normal completion (exit code 10 for SAT, 20 for UNSAT, following SAT competition conventions) or abnormal termination

#### Scenario: Stop solver on user request
- **WHEN** the user cancels the solve operation
- **THEN** the solver process is terminated (SIGTERM or equivalent), and all resources (FIFO, pipes) are cleaned up

#### Scenario: Solver crashes
- **WHEN** the solver process terminates unexpectedly with a non-zero exit code (not 10 or 20)
- **THEN** the system reports the error to the user and cleans up the FIFO and any open pipes

### Requirement: Named FIFO Lifecycle
The system SHALL create and manage a named FIFO for inter-process communication with the solver.

#### Scenario: FIFO creation
- **WHEN** the system prepares to launch a solver
- **THEN** a named FIFO is created at a temporary path using `mkfifo`

#### Scenario: FIFO cleanup on normal exit
- **WHEN** the solver finishes normally (SAT or UNSAT)
- **THEN** the FIFO file is removed from the filesystem

#### Scenario: FIFO cleanup on error
- **WHEN** the solver crashes or is killed
- **THEN** the FIFO file is still removed from the filesystem (RAII cleanup)

#### Scenario: FIFO reads block until data available
- **WHEN** the engine reads from the FIFO and the solver has not yet written data
- **THEN** the read blocks (with timeout) until the solver produces output

### Requirement: Solver Binary Path Configuration
The solver binary path SHALL be configurable via CLI option (`-s`) and GUI file selector.

#### Scenario: CLI solver path
- **WHEN** the application is launched with `-s /usr/local/bin/cadical`
- **THEN** the specified solver binary is used for solving

#### Scenario: GUI solver selection
- **WHEN** the user selects a solver binary via file dialog in the GUI
- **THEN** the selected binary is used for subsequent solve operations

#### Scenario: Solver path persisted
- **WHEN** the user selects a solver path via GUI
- **THEN** the path is remembered for future sessions (e.g., in application settings)

### Requirement: Any Compatible Solver
The system SHALL work with any SAT solver binary that supports the pipe output protocol, not just cadical or kissat.

#### Scenario: Using kissat
- **WHEN** the user provides a kissat binary built with Pipe.cc via `-s`
- **THEN** kissat is launched and its events are captured through the FIFO

#### Scenario: Using minisat
- **WHEN** the user provides a minisat binary built with Pipe.cc via `-s`
- **THEN** minisat is launched and its events are captured through the FIFO

#### Scenario: Unknown solver
- **WHEN** the user provides an arbitrary binary via `-s`
- **THEN** the system attempts to launch it with the CNF file and `--pipe` argument; if the solver does not produce expected output, the engine reports a communication error
