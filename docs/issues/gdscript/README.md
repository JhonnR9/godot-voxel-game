# GDScript review issues

Drafts ready for publication in `JhonnR9/godot-extension-game`. These are not remote issues yet. Each file contains a title, context, evidence, a proposal, and acceptance criteria.

P1: address first; P2: next iteration; P3: planned evolution. The numbers below are local references, not GitHub issue numbers.

| Local ID | Priority | Type | Issue |
| --- | --- | --- | --- |
| GD-01 | P1 | bug / tests | [Restore lighting tests and automate the suite](01-tests-and-ci.md) |
| GD-02 | P1 | bug | [Validate the world-creation form](02-world-creation.md) |
| GD-03 | P2 | bug | [Clear the list when deleting the last world](03-world-list.md) |
| GD-04 | P2 | bug | [Correct the ocean audio stream](04-ocean-audio.md) |
| GD-05 | P1 | bug / architecture | [Unify mouse state between gameplay and UI](05-input-state.md) |
| GD-06 | P1 | compatibility | [Prevent block ID reuse](06-block-ids.md) |
| GD-07 | P2 | tools | [Publish block assets consistently](07-asset-generation.md) |
| GD-08 | P2 | architecture | [Centralize the GDScript block catalog](08-block-catalog.md) |
| GD-09 | P2 | development | [Add development tasks and checks](09-development-workflow.md) |
| GD-10 | P3 | gameplay | [Define and persist Survival and Creative modes](10-game-modes.md) |

Suggested order: GD-01, GD-02, GD-05, and GD-06; then the remaining P2 issues. GD-09 should reuse the GD-01 test runner. GD-10 depends on a decision about Survival mode scope.

Review and test results: [gdscript_review.md](../../gdscript_review.md).
