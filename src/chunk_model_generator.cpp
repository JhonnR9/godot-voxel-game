#include "chunk_model_generator.h"
namespace godot {
void ChunkModelGenerator::_bind_methods() {}
void ChunkModelGenerator::_queue_async_generate_chunk_model(Vector3i pos, const TerrainSettings &settings,
															std::shared_ptr<const ChunkGenerationPipeline> pipeline, bool priority) {
	ERR_FAIL_COND_MSG(!pipeline, "Cannot generate a chunk without a generation pipeline.");
	if (scheduler.contains(pos))
		return;
	scheduler.enqueue(ChunkJob{ pos, settings, std::move(pipeline), priority });
}

std::vector<ChunkModelResult> ChunkModelGenerator::run_batch(const std::vector<ChunkJob> &jobs) {
	std::vector<ChunkModelResult> results;
	results.reserve(jobs.size());
	for (const auto &job : jobs) {
		const auto start = chunk_clock_us();
		auto model		 = std::make_shared<Chunk>(ChunkGenerator::generate(job.pos, job.settings, *job.pipeline));
		model->stage	 = ChunkStage::LOADED;
		results.push_back({ job.pos, std::move(model), job.request_id, start - job.queued_us, chunk_clock_us() - start });
	}
	return results;
}

std::vector<ChunkModelResult> ChunkModelGenerator::consume_generated_results(int amount) {
	std::vector<ChunkModelResult> results;
	ChunkModelResult result;
	while ((amount < 0 || int(results.size()) < amount) && scheduler.pop(result)) {
		if (scheduler.is_current(result))
			results.push_back(std::move(result));
		scheduler.forget(result.pos);
	}
	return results;
}
} //namespace godot
