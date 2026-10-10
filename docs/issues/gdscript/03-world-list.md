# Remove old rows when deleting the last world

Suggested priority: P2. Type: bug.

## Problem and reproduction

`project/scripts/world_selector.gd:16` returns when `get_saved_worlds()` is empty before clearing old children, which occurs only at lines 20–22.

Create a single world, open selection, and delete it: the placeholder appears, but the deleted world's row remains. Reproduction found two children before and after deletion instead of just the placeholder.

## Proposal

Clear rows and reset selection/buttons before handling an empty list. Emit the empty state based on valid models actually displayed. Pass the already-loaded model to the view, avoiding the second read in `world_list_container.gd`.

## Acceptance criteria

- [ ] Deleting the last world leaves only the placeholder after the cleanup frame.
- [ ] Repeated refreshes do not duplicate rows.
- [ ] Load/Delete are unavailable without a valid selection.
- [ ] A test covers deleting the last world and creating another after the list becomes empty.
