# Reserve removed IDs to prevent reinterpreting blocks in saves

Suggested priority: P1. Type: compatibility / tools.

## Problem and evidence

The Block Registry README states that IDs are persisted in chunks. In `block_registry_plugin.gd:192`, Add calculates `max(current IDs) + 1`; Remove deletes the record at `:224`.

Code inspection shows that removing the highest ID and adding another reuses that ID. An old save can then treat the removed block as a different block. The real registry was not modified, and no save test was run for this case.

## Proposal

Persist reserved IDs/tombstones or a monotonic counter, prevent reuse, and document the policy for manual edits. Define behavior for removed IDs and protect biome references. Preserve ID 0/air and the 0–1023 range.

## Acceptance criteria

- [ ] Deleting the highest ID and adding a block never reuses the old ID, including after reopening the editor.
- [ ] Manual editing that attempts to reuse a reserved ID produces a validation error.
- [ ] Old blocks have defined behavior when loading saves.
- [ ] A save fixture proves that removing/adding a block does not reinterpret it as another.
