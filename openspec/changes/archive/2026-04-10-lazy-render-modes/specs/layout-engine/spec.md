## MODIFIED Requirements

### Requirement: Layout Factory Mode Tags
LayoutFactory SHALL tag each registered algorithm with compatible render modes. The `available_algorithms(LayoutMode)` method SHALL return only algorithms valid for the given mode.

#### Scenario: Filtered algorithm list
- **WHEN** `available_algorithms(LayoutMode::Simple2D)` is called
- **THEN** only community-weighted layout algorithms are returned

#### Scenario: Unfiltered algorithm list
- **WHEN** `available_algorithms()` is called without mode filter
- **THEN** all registered algorithms are returned (backward compatible)

### Requirement: New Layout Registrations
LayoutFactory SHALL register the new algorithms: `community-fa` (Simple2D), `community-fa3d` (Simple3D).

#### Scenario: Community FA available
- **WHEN** `LayoutFactory::available_algorithms(LayoutMode::Simple2D)` is called
- **THEN** `community-fa` appears in the returned list

#### Scenario: Community FA3D available
- **WHEN** `LayoutFactory::available_algorithms(LayoutMode::Simple3D)` is called
- **THEN** `community-fa3d` appears in the returned list
