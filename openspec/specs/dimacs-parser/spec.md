# DIMACS CNF Parser

Parses DIMACS CNF files into an in-memory graph representation, supporting both Variable Interaction Graph (VIG) and Literal Interaction Graph (LIG) modes.

## ADDED Requirements

### Requirement: Problem Line Parsing
The parser SHALL read the problem line (`p cnf <vars> <clauses>`) to establish the expected variable count and clause count for the instance.

#### Scenario: Valid problem line
- **WHEN** a line matching `p cnf 500 1200` is encountered
- **THEN** the parser records 500 variables and 1200 clauses, and allocates internal structures accordingly

#### Scenario: Missing problem line
- **WHEN** no `p cnf` line appears before the first clause line
- **THEN** the parser SHALL report an error indicating the problem line is required

#### Scenario: Duplicate problem line
- **WHEN** a second `p cnf` line appears after the first
- **THEN** the parser SHALL report an error and refuse to proceed

### Requirement: Comment and Variable Naming
The parser SHALL interpret `c`-lines of the form `c <id> <name>` as variable name assignments. Named variables SHALL be matched against user-provided regex patterns to form groups.

#### Scenario: Variable naming
- **WHEN** the parser reads `c 7 solver_backtrack_count`
- **THEN** variable 7 gets the display name "solver_backtrack_count"

#### Scenario: Regex-based grouping
- **WHEN** the user provides a regex pattern `^solver_(.*)` and variable 7 is named `solver_backtrack_count`
- **THEN** variable 7 is placed into a group whose label matches the captured subgroup, and all variables matching the same pattern share that group

#### Scenario: Multiple regex patterns
- **WHEN** multiple regex patterns are provided (e.g., `^solver_.*` and `^clause_.*`)
- **THEN** each named variable is matched against all patterns in order, and assigned to the first matching group

#### Scenario: Unmatched variable names
- **WHEN** a named variable does not match any provided regex pattern
- **THEN** the variable remains in a default "ungrouped" category

### Requirement: Clause Parsing
The parser SHALL read clause lines as space-separated literal sequences terminated by `0`. A positive integer represents the variable in positive polarity; a negative integer represents negated polarity.

#### Scenario: Simple clause
- **WHEN** the parser reads the line `1 -3 5 0`
- **THEN** a clause is created containing literals (variable 1, positive), (variable 3, negated), (variable 5, positive)

#### Scenario: Unit clause
- **WHEN** the parser reads the line `42 0`
- **THEN** a unit clause is created containing a single literal (variable 42, positive)

#### Scenario: Empty clause
- **WHEN** the parser reads a line containing only `0`
- **THEN** the parser SHALL record an empty clause and emit a warning, as an empty clause indicates unsatisfiability

#### Scenario: Multi-line clause
- **WHEN** a clause spans multiple lines before the terminating `0`
- **THEN** the parser SHALL accumulate literals across lines until the `0` terminator, producing a single clause

### Requirement: VIG Mode
In Variable Interaction Graph (VIG) mode, the parser SHALL create an edge between any two variables that co-occur in the same clause, using absolute variable IDs as node identifiers.

#### Scenario: Clause produces VIG edges
- **WHEN** the parser processes clause `1 -3 5 0` in VIG mode
- **THEN** edges are created between nodes (1, 3), (1, 5), and (3, 5), ignoring polarity

#### Scenario: Duplicate edges in VIG
- **WHEN** two variables co-occur in multiple clauses in VIG mode
- **THEN** a single edge connects them, but its weight increments for each shared clause

### Requirement: LIG Mode
In Literal Interaction Graph (LIG) mode, the parser SHALL create edges between signed literals (variable and polarity), producing distinct nodes for positive and negative forms of each variable.

#### Scenario: Clause produces LIG edges
- **WHEN** the parser processes clause `1 -3 5 0` in LIG mode
- **THEN** edges are created between signed literals (+1, -3), (+1, +5), and (-3, +5), preserving polarity in node identity

#### Scenario: Complementary literals in LIG
- **WHEN** a clause contains both +X and -X
- **THEN** both signed forms exist as separate nodes connected by the clause edge

### Requirement: Progress Reporting
The parser SHALL report progress during file reading by exposing the current byte position relative to total file size.

#### Scenario: Progress callback invoked
- **WHEN** the parser reads a 10 MB file and reaches the 5 MB mark
- **THEN** a registered progress callback receives a value of approximately 0.5 (50%)

#### Scenario: No progress callback
- **WHEN** no progress callback is registered
- **THEN** the parser completes without error and without attempting to call any callback

### Requirement: File Format Errors
The parser SHALL detect and report malformed input gracefully.

#### Scenario: Invalid literal
- **WHEN** the parser encounters a non-numeric token where a literal is expected (e.g., `1 abc 0`)
- **THEN** the parser SHALL report an error with the line number and the offending token

#### Scenario: Unterminated clause at EOF
- **WHEN** the file ends without a terminating `0` for the current clause
- **THEN** the parser SHALL report a warning and treat the accumulated literals as a complete clause
