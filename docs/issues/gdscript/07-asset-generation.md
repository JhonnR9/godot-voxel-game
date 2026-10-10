# Validate and publish block assets without partial outputs

Suggested priority: P2. Type: tools / reliability.

## Problem and evidence

`block_registry_plugin.gd:240–250` writes the source before calling the generator/validation. `block_asset_generator.gd:104–110` publishes the mapping, generated JSON, C++ header, and array sequentially.

Invalid input can already overwrite a valid source. A failure in a later output can leave only some files updated. These risks were observed through inspection without simulating a write failure in the checkout.

## Proposal

Validate in memory before replacing the source. Produce outputs in a temporary area, verify all of them, and publish the set with rollback or an equivalent consistency mechanism. Close handles before reading/publishing. Use the same path in the plugin and CLI, with diagnostics identifying the failed file/stage.

## Acceptance criteria

- [ ] An invalid registry does not change the valid source or generated files.
- [ ] A simulated write failure preserves the previous usable set.
- [ ] Successful generation keeps source, IDs, mapping, and array aligned to the same generation.
- [ ] Plugin and CLI clearly report failures; the CLI returns a nonzero exit code.
- [ ] Temporary fixtures cover invalid input, publication failure, and valid generation.
