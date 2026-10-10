#ifndef CHUNK_MODEL_GENERATOR_H
#define CHUNK_MODEL_GENERATOR_H
#include "chunk_generator.h"
#include "chunk_task_scheduler.h"
#include <godot_cpp/classes/ref_counted.hpp>
namespace godot {

struct ChunkJob {
	Vector3i pos;
	TerrainSettings settings;
	std::shared_ptr<const ChunkGenerationPipeline> pipeline;
	bool priority		= false;
	uint64_t request_id = 0;
	uint64_t queued_us	= 0;
};

struct ChunkModelResult {
	Vector3i pos;
	std::shared_ptr<Chunk> model;
	uint64_t request_id = 0;
	uint64_t wait_us = 0, work_us = 0;
};

class ChunkModelGenerator final : public RefCounted {
	GDCLASS(ChunkModelGenerator, RefCounted)
	static std::vector<ChunkModelResult> run_batch(const std::vector<ChunkJob> &jobs);
	ChunkTaskScheduler<ChunkJob, ChunkModelResult> scheduler{ run_batch };

protected:
	static void _bind_methods();

public:
	std::vector<ChunkModelResult> consume_generated_results(int amount = 16);
	void _queue_async_generate_chunk_model(Vector3i pos, const TerrainSettings &settings,
										   std::shared_ptr<const ChunkGenerationPipeline> pipeline, bool priority = false);
	bool is_loading_chunk(const Vector3i &pos) { return scheduler.contains(pos); }
	Dictionary get_stats() { return scheduler.stats(); }
	void configure(int batch, int inflight) { scheduler.configure(batch, inflight); }
	void pump() { scheduler.pump(); }
	void reset() { scheduler.reset(); }
	void cancel_outside(const Vector3i &center, int radius, int height) { scheduler.cancel_outside(center, radius, height, false); }
	void forget(const Vector3i &pos) { scheduler.forget(pos); }
};
} //namespace godot
#endif
