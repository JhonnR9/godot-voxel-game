#include "chunk_mesh_async_generator.h"

namespace godot {

void ChunkMeshAsyncGenerator::_bind_methods() {
}

void ChunkMeshAsyncGenerator::prepare_metadata() {
	metadata = ChunkMeshBuilder::load_metadata();
}

void ChunkMeshAsyncGenerator::queue_async_generate_mesh(Vector3i pos, ChunkNeighbors neighbors, uint64_t version, bool priority) {
	scheduler.enqueue(ChunkMeshJob{ pos, std::move(neighbors), version, priority, 0, 0, metadata });
}

std::vector<MeshResult> ChunkMeshAsyncGenerator::run_batch(const std::vector<ChunkMeshJob> &jobs) {
	ChunkMeshBuilder builder(jobs.front().metadata);
	std::vector<MeshResult> results;
	results.reserve(jobs.size());

	for (const auto &job : jobs) {
		const auto start = chunk_clock_us();
		auto mesh		 = builder.build(job.neighbors);
		results.push_back(
				MeshResult{ mesh, builder.get_last_collision_faces(),
						builder.get_torch_positions(), builder.get_selection_positions(),
						job.pos, job.version, job.request_id,
						start - job.queued_us, chunk_clock_us() - start });
	}

	return results;
}

} //namespace godot
