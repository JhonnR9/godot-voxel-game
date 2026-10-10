# Define and persist Survival and Creative mode behavior

Suggested priority: P3. Type: gameplay / product decision.

## Problem and evidence

`project/scenes/create_world.tscn` offers Survival/Creative, but `create_world.gd` does not read the OptionButton or persist the choice. `player.gd` places blocks without consuming the stack and allows flight/noclip; this does not depend on the chosen mode. The README already documents the lack of consumption when placing blocks.

This is incomplete functionality rather than a confirmed regression.

## Proposal

Define the minimum scope of each mode before implementation. Persist the mode per world, define the default for older saves, and apply a consistent policy to the catalog, placement/consumption, collection, and flight/noclip commands. Consumption must go through the authoritative service and occur only after valid placement. If Survival is not supported yet, disable that option with an explanation until implemented.

## Acceptance criteria

- [ ] The UI offers only supported modes, and selection has an observable effect.
- [ ] Reopening a world preserves its mode; older saves receive a documented default.
- [ ] If Survival is enabled, invalid placement consumes no items and valid placement consumes one unit.
- [ ] Creative retains an explicit catalog/quantity/flight policy.
- [ ] Tests cover persistence and item conservation according to the chosen policy.
