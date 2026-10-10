# Chunk pipeline optimization

Implementation dated October 2, 2026, following the investigation in [chunk_performance_review.md](chunk_performance_review.md).

## Applied changes

- Results use `std::deque` without rebuilding the entire queue. Resources from the previous result are released outside the publication mutex.
- A shared scheduler implementation serves the model generator and mesher: one running job per position, pending-request replacement, and monotonic revisions. Neighbor/AO updates also create new revisions. Old results are not installed.
- The default limit is eight chunks running or waiting for consumption **per stage**. Requests are deduplicated by position; distant requests are canceled when focus changes. Already-started model jobs can finish and be reused if the player returns; mesh jobs invalidate their revision when they leave the area.
- Finite tasks with configurable batches from 1 to 8. Godot's pool remains shared with other tasks; no worker blocks waiting for requests.
- Workers generate geometry only. ArrayMesh creation/upload, collision installation, and node configuration occur on the main thread. This eliminated RID errors encountered when creating meshes in workers during headless tests with multiple active worlds.
- Finalization has a default budget of 2 ms and a cap of 32 results per frame. A single installation can exceed the budget: time is checked between results. The peak is exposed in statistics.
- Block metadata is loaded once per world instance into immutable tables indexed by ID/face. Workers do not open JSON or load textures.
- Removed placeholders that allocated 256 KiB per position solely to indicate pending generation.
- Cache limited to 256 XZ chunk columns per configuration/world. The same terrain data is reused across different Y heights. Noise sampling occurs outside the cache mutex.
- All 27 neighbors are queried in one repository call. Edits copy a chunk only when references external to the repository exist; job-retained snapshots preserve previous blocks. Without external readers, edits occur in place.
- A non-empty block count allows the mesher to skip air chunks. Plant scans use X as the inner axis, matching voxel layout. Face masks are reused within each task/batch.
- Streaming requests a data halo, including diagonals, beyond the visible area. The mesh waits for this halo to avoid temporary AO at boundaries. Attempts occur on focus changes, model arrivals, or edits instead of scanning the active world every frame.
- Rebuilds update the existing ChunkNode; torches whose positions remain unchanged preserve their lights. Finalization uses surface classifications received with geometry instead of retrieving mesh arrays to identify water.
- TaskIDs are collected and awaited when switching/closing worlds; old queues, edits, and data are cleared before changing the seed/noise/repository. The node pool respects its prewarm cap and can grow with actual demand.

The halo has a memory cost: at radius 4 and height 2, there are 623 models for 245 visible positions. Blocks alone occupy approximately 155.75 MiB, excluding meshes and caches. Removing placeholders avoids redundant allocation but does not guarantee less total memory than the previous engine, which left boundaries without required neighbors. Compacting uniform data or storing only required halo strips is a future optimization.

## Configuration and measurements

After adding VoxelAPI to the tree:

```gdscript
world.set_pipeline_settings({
    "batch_size": 1,
    "max_inflight": 8,
    "finalize_budget_ms": 2.0,
})
var stats: Dictionary = world.get_pipeline_stats()
```

`batch_size` is clamped to 1–8, `max_inflight` to 1–16 per stage, and the budget to 0.1–8 ms. Lowering the limit does not interrupt already-started jobs; they finish before new submissions use the reduced capacity.

Statistics include pending requests, chunks running/waiting for consumption, ready results, peak queue size, submitted tasks, stale results, average/maximum work time, average wait since request, cumulative queue-mutex holding/waiting times, and finalization duration. Waiting includes the local queue and, in batches, earlier chunks in the task. Statistics reads copy counters under lock and construct the Dictionary after releasing it.

Work time excludes visual/physics installation on the main thread. Statistics reset when switching worlds. The main thread remains the sole owner of submission state, revisions, stages, and chunk flags.

## Batch comparison

The reproducible test is in `project/tests/chunk_pipeline_benchmark.gd`; complete results are in [chunk_batch_benchmark.json](chunk_batch_benchmark.json). All runs use seed 12345, the same focus and physical coverage, radius 4, height 2, a limit of 8 per stage, and a 2 ms budget. Three runs per batch size, `template_debug` build, headless, capped at 120 frames/s. Timing starts at `start_world`, after the world has been added to the tree and the pool prepared.

The comparison measures initial loading of the central region and draining all requests for the area and halo. It does not measure FPS/GPU or directly compare with the previous engine. Chunk arrival order can change how many jobs need updating, so raw statistics accompany timings. Frame caps also affect dispatch/consumption frequency.

| Batch | Central load (median) | Total drain (median) | Model submissions | Mesh submissions (median) |
|---:|---:|---:|---:|---:|
| 1 | 280.4 ms | 780.5 ms | 623 | 245 |
| 2 | 242.9 ms | 818.1 ms | 312 | 147 |
| 4 | 276.2 ms | 884.7 ms | 156 | 93 |
| 8 | 301.4 ms | 934.9 ms | 78 | 75 |

One task per chunk remains the default because it provided the fastest total drain in this test. Batch 2 delivered the central region earlier and may be preferable for initial loading. The limit is counted in chunks: with a limit of 8, batch 8 can use only one task per stage, reducing parallelism. Larger batches remain available for measurement in release builds and on other machines; fewer submissions do not necessarily shorten the time until a chunk becomes visible.

## Validation

- Extension build: `scons -j6 target=template_debug`.
- `chunk_pipeline_test.gd`: queue limits, submission amortization with batch 2, halo, idle world, latest revision after corner edits, node/light preservation, rapid focus return, save/reload, switching, and destruction with pending jobs.
- `torch_test.gd`: placement, icon, light, selection, reload, and removal of the last torch.
- `biome_registry_test.gd`: generation/sampler, trees in different loading orders, height bounds, and bedrock. Fixtures were adjusted to the 32×64×32 dimensions already present in the workspace.
- `voxel_ao_test.gd` with the OpenGL Compatibility renderer: six faces, chunk diagonals, transparency, and greedy meshing. Fixture boundary coordinates were also adjusted to current dimensions.

The 32×64×32 size and save format were preserved. Changing dimensions, separating mesh/collision sections, comparing group tasks, and implementing LOD remain separate experiments. No FPS gain is claimed without comparison in a release build using the game's renderer and route.
