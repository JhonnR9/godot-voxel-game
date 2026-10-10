# Chunk performance investigation

The improvements were implemented later; see [chunk_pipeline_optimization.md](chunk_pipeline_optimization.md). This document records the investigation before those changes.

Date: October 2, 2026. Analysis of the workspace code at that time, including existing local changes. No engine behavior was modified during this investigation. A reproducible microbenchmark was added.

## Conclusion

The first improvement should target the path between production and consumption: eliminate full mesh-queue reconstruction, prevent duplicate work, shorten critical sections, and limit installation by time. Changing chunk size or grouping many generations into one task beforehand could shift the bottleneck to the main thread and increase edit latency.

These costs and risks are confirmed by code inspection. The relative weight of generation, meshing, scheduling, rendering, and physics still needs in-game instrumentation; FPS, actual contention, and native task times were not measured.

## Findings and proposals

### 1. Consuming a few meshes costs proportionally to the entire queue

In `src/chunk_mesh_async_generator.cpp:65`, consuming K results traverses N results and recreates two HashSets under `_generated_meshes_mutex`. Producers need that same mutex to publish results. A complete drain in small batches adds up to approximately O(N²/K) reconstruction work.

The assignment at line 91 uses `std::move`, but this checkout's HashSet has only copy assignment (`godot-cpp/include/godot_cpp/templates/hash_set.hpp:417`). The remaining queue is therefore also copied; its data is not simply transferred. Result copies copy references, not all geometry, but repeat reference operations, allocations, and table copies.

**Proposal:** use `std::deque<MeshResult>` for ready results; remove K with `pop_front` into a local vector and finalize outside the mutex. Cost is O(K) per removal. For full consumption, `swap` with a local queue further reduces the critical section. Destroy stale results and resources outside the lock. Keep separate sets/maps for presence, running jobs, and desired versions.

A deque handles FIFO transport; distance priority requires priority-class queues or another structure. Completion FIFO alone does not guarantee that the nearest chunk is installed first. Do not replace active or dirty chunk HashSets with a deque: deduplication and presence queries are useful there.

### 2. Duplicate jobs and lost newer results

`queue_async_generate_mesh` inserts the position into `_generating_meshes` without checking whether it already exists. `_rebuild_chunk` can submit another job while the previous one is still running. The result set considers two meshes at the same position equal regardless of version. HashSet returns the existing entry when reinserting a key; it does not update its value.

If v1 finishes and remains in the queue, and v2 finishes afterward, v2 can be ignored. Finalization rejects v1 against the current version, but v2 has already been lost. In addition, the first job to finish removes the position from the set even if another job at the same position is still running. The set therefore does not represent the actual number of pending jobs.

**Proposal:** one running job per position, with a desired revision and a resubmission marker. Edits during execution update the desired revision; on completion, publish a valid result or schedule a single update. Use world identity/epoch and request revision in addition to block version. Neighbor and AO-halo changes also invalidate the mesh; the central chunk's version alone does not cover those dependencies.

### 3. Finalization can cause main-thread spikes

`src/voxel_api.cpp:228` normally chooses 1 or 2 meshes per frame, 5 when there are more than 200 results, and **100** when there are more than 1,000. Abruptly increasing work precisely when the queue grows can worsen spikes. `delta` represents the previous frame and includes other costs; it does not measure current installation.

`_finalize_chunk` installs resources, retrieves surface arrays, and configures collision and lights. `ChunkNode::set_collision_faces` calls `set_faces`; its share of the cost has not been measured. During a rebuild, removing the previous node clears collision and lights, and installation recreates them.

**Proposal:** a configurable time budget, initially experimenting with 1–2 ms/frame of installation, plus a count cap. Check time between results; a single heavy installation can still exceed the budget. Update the existing node on rebuilds. Carry surface classification in the result to avoid retrieving arrays to identify water. Prioritize collisions near the player and evaluate installation separately from visuals.

### 4. Repeated scanning and synchronization

`_update_visible_chunks` copies all active chunks and traverses them every frame. Each repository lookup acquires its mutex. `_get_neighbors_for` performs 27 lookups, hence 27 separate acquisitions per mesh attempt. Chunks in `WAITING_NEIGHBORS` can repeat those attempts every frame.

At horizontal radius 4 and height 3, there are 49 × 7 = **343** active chunks away from the world's vertical bounds. The count grows approximately with R² × H. Streaming does not use `cache_radius` to generate an extra neighbor layer: boundary chunks can wait for neighbors never requested in a newly created world. This also sustains pointless attempts every frame.

**Proposal:** generate data in a halo beyond visible chunks; drive mesh attempts through events (chunk entered, data arrived, neighbor arrived, edit occurred), deduplicating pending positions. Query all 27 pointers in one repository call under a single lock. Adopt this only after defining how data remains consistent during meshing.

### 5. Repeated initialization per mesh

Each job constructs a `ChunkMeshBuilder`. Its constructor requests the texture through ResourceLoader and opens/parses `block_registry.generated.json`, rebuilding texture and color maps. The loader can reuse a cached texture, but JSON opening and parsing explicitly occur on every construction. `block_texture_array` is not used by `build` in the current code.

**Proposal:** load metadata once and share an immutable snapshot, preferably using tables indexed by ID/face. Do not share mutable builder buffers. Reuse scratch buffers per task processing a small batch, or per worker with a well-defined reentrancy strategy.

### 6. Memory per chunk and voxel access

Current chunks are **32 × 64 × 32 = 65,536 voxels**. `Block` is 4 bytes: **256 KiB** of blocks per chunk, plus stage, flags, and allocation overhead. The generation placeholder is also a full zeroed Chunk even though it only reports stage. For 343 positions, placeholders represent approximately 85.75 MiB of blocks; generated results waiting for consumption can coexist with those placeholders. Each generation additionally allocates 64 KiB of write priorities and data for 1,024 columns, plus pass buffers.

The mesher already uses greedy meshing. It still performs six volume scans for faces, mask scans, and another volume scan for plants. The latter uses z as the inner axis, but layout is `x + y*SIZE_X + z*SIZE_X*SIZE_Y`: the inner stride is 8,192 bytes. Using x as the inner axis offers a concrete locality improvement. Not all face orientations will have ideal locality with the same layout.

`BiomeSelectionPass` recalculates the same 1,024 XZ columns for each Y chunk in the same chunk column. The sampler calls six noise resources per column.

**Proposal:** lightweight placeholders (position, stage, revision), an immutable sampling cache by XZ column and world configuration, contiguous traversal where possible, and empty-chunk/occupancy indicators to avoid meshing air. Maintain these indicators on edits; a solid chunk can skip faces only if the halo proves they are unexposed. Also measure the tree cache, which performs mutex-protected LRU lookups/updates and copies candidates.

### 7. Correct synchronization before increasing parallelism

Jobs retain raw pointers to generators/loaders and ignore the TaskID returned by WorkerThreadPool. There is no explicit wait for jobs to finish before freeing their owners. `_clear_world` is empty. A world change can mix old and new state and leave old jobs pending.

The repository mutex protects the map, but the returned shared_ptr does not protect blocks: `set_block` writes to the same Chunk that workers read in the mesher. Placeholder stage is also written by the worker and read outside the lock on the main thread. Checking the version afterward does not eliminate these data races.

**Proposal:** immutable meshing snapshots or explicit synchronization of block reads/writes; lifecycle management with TaskIDs, logical cancellation, and epoch-based rejection. When closing/switching worlds, prevent new submissions and ensure jobs do not access destroyed objects or reconfigured noise resources. A central-chunk snapshot plus a one-voxel halo can avoid copying 27 complete chunks. Define this snapshot's cost and consistency before implementing it.

## One task per chunk or batches?

The code already uses Godot's global WorkerThreadPool; it does not create a thread per chunk. It currently creates **one native task per chunk** for both models and meshes. There is no measured evidence that submission overhead dominates the passes.

There are three separate decisions:

1. **Publish/consume in batches:** reduces mutex acquisitions and queue work, with a clear structural benefit. Publishing small batches avoids retaining ready results for too long.
2. **Submit a list to the pool:** compare individual tasks with the native group API available in local headers. A group can distribute indices dynamically among workers without requiring one task to process the entire list.
3. **Each task executes K chunks sequentially:** experiment with K = 1, 2, 4, 8. This amortizes submission and preparation, but large batches worsen balancing, priority, cancellation, and time to the first result. Empty terrain, caves, vegetation, and surface meshes have different costs.

The initial proposal is a limited number of running tasks pulling small batches from a priority queue and checking cancellation between chunks. Rebuilds near the player should use small batches. Do not keep worker loops blocked indefinitely inside the shared pool, which also handles IO and other tasks. Limit pending results and memory; simply increasing producers can accumulate more meshes than the main thread can install.

## Size assessment

| Dimensions | Voxels | Blocks/chunk | Chunks for the same physical volume |
|---|---:|---:|---:|
| 16 × 32 × 16 | 8,192 | 32 KiB | 8 times the current count |
| 32 × 32 × 32 | 32,768 | 128 KiB | 2 times the current count |
| 32 × 64 × 32 (current) | 65,536 | 256 KiB | Baseline |
| 64 × 64 × 64 | 262,144 | 1 MiB | 1/4 of the current count |

Smaller chunks can reduce the cost and latency of an isolated rebuild but increase jobs, nodes, boundaries, collisions, and potential surfaces/draw calls. Larger chunks amortize scheduling but make edits and uploads more expensive.

First compare the current size with **32³**, then **16 × 32 × 16**, keeping the same distance in meters and vertical extent. Evaluate separating storage size from mesh/collision section size: larger data units with smaller rebuilds can be an alternative to a global size change.

Do not change constants alone: total world height depends on SIZE_Y, saves store chunk/local block coordinates, and trees use density/candidates per chunk. Preserving physical coverage, determinism, density, and world compatibility requires coordinated changes.

## Microbenchmark performed

`tests/chunk_queue_benchmark.cpp` uses this checkout's HashSet and reproduces the current removal algorithm, comparing it with deque + local vector. A representative record contains three shared_ptr values, position, and version; allocation adapters use malloc/realloc/free. No Godot resources are instantiated. Seven repetitions; medians. Queue preparation is outside the timed section. The benchmark checks the count and sum of consumed IDs. Local CPU: Intel Core i5-12400F; compiled with `-O3`.

| Initial queue | Batch | First removal, HashSet | First removal, deque | Full drain, HashSet | Full drain, deque |
|---:|---:|---:|---:|---:|---:|
| 200 | 2 | 5.126 µs | 0.048 µs | 0.304 ms | 0.002 ms |
| 1,000 | 2 | 54.595 µs | 0.052 µs | 7.431 ms | 0.011 ms |
| 10,000 | 2 | 771.457 µs | 0.059 µs | 894.443 ms | 0.115 ms |
| 1,000 | 100 | 44.614 µs | 0.728 µs | 0.212 ms | 0.008 ms |

These timings assess the data structure and consumption algorithm. They exclude generation, meshing, physics, GPU, actual Godot reference costs, producer concurrency, and mutex waiting. A full drain adds removals without spacing them across frames. These are not predictions of FPS gains. Sub-microsecond measurements are sensitive to clocks, caches, and scheduling; the complexity change is the main evidence. The 10,000-entry queue is a stress scenario, not a queue observed in the game.

Reproduce from the project root:

```sh
g++ -O3 -DNDEBUG -std=c++17 -Igodot-cpp/include -Igodot-cpp/gen/include \
    -Igodot-cpp/gdextension tests/chunk_queue_benchmark.cpp -o /tmp/chunk_queue_benchmark
/tmp/chunk_queue_benchmark
```

## Recommended order and required measurements

1. Instrument timestamps for requests, job start/end, publication, and installation. Separate pool waiting, mutex waiting/holding time, generation per pass, metadata, meshing, mesh creation, and visual/physics installation. Also measure memory, maximum queue size, discarded/duplicate jobs, and rebuild count per position.
2. Correct versions, deduplication, races, and lifecycle; replace the ready queue with a deque and shorten lock duration. Implement a time budget for finalization.
3. Share metadata, remove bulky placeholders, batch neighbor queries, and replace scanning with events, with a defined data halo.
4. Compare individual tasks, groups, and batches of 2/4/8 with pending-work limits.
5. Only then compare sizes and independent mesh sections.

Use the same seed, area in meters, configuration, and route; repeat initial loading, idle player, boundary crossing, rapid movement, and face/edge/corner edit scenarios. Record p50/p95/p99 frame time and latency to rendering, chunk throughput, and peak memory in release builds. Validate visuals and collisions with the game's renderer, including AO, water, plants, and torches. Headless helps isolate CPU costs but does not validate GPU costs.
