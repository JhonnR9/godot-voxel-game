# Read and validate the form when creating a world

Suggested priority: P1. Type: bug.

## Problem and reproduction

In `project/scenes/create_world.tscn`, SeedLineEdit connects only `text_submitted`. `project/scripts/create_world.gd:7` picks a random seed, and creation uses the internal variable instead of reading the field.

Open creation, type `12345`, and click Create without Enter: the world is created with another seed. Headless reproduction showed `12345` in the field while `editing_seed` remained random.

The handler also does not check the returned ID. `SaveService.create_world` returns 0 on an opening/creation failure, but the UI still switches to the game scene.

## Proposal

Read fields on click, validate an integer seed within the range accepted by the 32-bit C++ contract, and treat an empty field as random. Trim the name and keep the default name for empty input. On ID 0, remain in the form and show a useful error. Prevent duplicate creation while the operation/transition is in progress.

## Acceptance criteria

- [ ] Typing a seed and clicking Create without Enter persists exactly the entered value.
- [ ] An empty seed keeps random generation; invalid or out-of-range input does not silently become zero.
- [ ] A whitespace-only name uses the default or receives explicit validation.
- [ ] Persistence failure does not start world 0 and leaves the form usable.
- [ ] An integration test covers input without Enter, and a controlled test covers creation failure.
