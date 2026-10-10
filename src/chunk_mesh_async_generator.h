#ifndef CHUNK_MESH_ASYNC_GENERATOR_H
#define CHUNK_MESH_ASYNC_GENERATOR_H
#include "chunk_mesh_builder.h"
#include "chunk_task_scheduler.h"
#include <godot_cpp/classes/ref_counted.hpp>
namespace godot {

struct MeshResult {

	ChunkMeshData geometry;
	PackedVector3Array collision_faces;
	PackedVector3Array torch_positions;
	PackedVector3Array selection_positions;
	Vector3i pos;
	uint64_t version	= 0;
	uint64_t request_id = 0;
	uint64_t wait_us = 0, work_us = 0;

};
struct ChunkMeshJob {
	Vector3i pos;
	ChunkNeighbors neighbors;
	uint64_t version	= 0;
	bool priority		= false;
	uint64_t request_id = 0;
	uint64_t queued_us	= 0;
	std::shared_ptr<const ChunkMeshMetadata> metadata;
};

class ChunkMeshAsyncGenerator : public RefCounted {
	GDCLASS(ChunkMeshAsyncGenerator, RefCounted)
	static std::vector<MeshResult> run_batch(const std::vector<ChunkMeshJob> &jobs);
	ChunkTaskScheduler<ChunkMeshJob, MeshResult> scheduler{ run_batch };
	std::shared_ptr<const ChunkMeshMetadata> metadata;

protected:
	static void _bind_methods();

public:
	void prepare_metadata();
	void queue_async_generate_mesh(Vector3i pos, ChunkNeighbors neighbors, uint64_t version, bool priority = false);
	bool is_queued_mesh(Vector3i pos) { return scheduler.contains(pos); }
	bool is_current(const MeshResult &r) { return scheduler.is_current(r); }
	size_t get_queue_size() { return scheduler.ready_size(); }
	bool pop_generated_mesh(MeshResult &r) { return scheduler.pop(r); }
	Dictionary get_stats() { return scheduler.stats(); }
	void configure(int batch, int inflight) { scheduler.configure(batch, inflight); }
	void pump() { scheduler.pump(); }
	void reset() { scheduler.reset(); }
	void cancel_outside(const Vector3i &center, int radius, int height) { scheduler.cancel_outside(center, radius, height); }
	void forget(const Vector3i &pos) { scheduler.forget(pos); }
};
} //namespace godot
#endif
