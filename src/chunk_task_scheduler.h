#ifndef CHUNK_TASK_SCHEDULER_H
#define CHUNK_TASK_SCHEDULER_H

#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <memory>
#include <mutex>
#include <vector>

namespace godot {
inline uint64_t chunk_clock_ns() {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline uint64_t chunk_clock_us() {
	return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
// Submission/state are owned by the main thread. Workers only publish immutable
// results. At most one executing job per position; pending edits replace a job.
template <typename Job, typename Result>

class ChunkTaskScheduler {

	using Runner = std::vector<Result> (*)(const std::vector<Job> &);
	struct Batch {
		ChunkTaskScheduler *owner;
		std::vector<Job> jobs;
	};
	HashMap<Vector3i, Job> pending;
	HashSet<Vector3i> active;
	HashMap<Vector3i, uint64_t> revisions;
	std::deque<Result> ready;
	std::mutex ready_mutex;
	std::vector<int64_t> tasks;
	uint64_t next_revision = 0;
	Runner runner;
	uint64_t submitted = 0, completed = 0, obsolete = 0;
	uint64_t work_us = 0, wait_us = 0, max_work_us = 0;
	size_t peak_ready		  = 0;
	int configured_batch_size = 1;
	uint64_t publish_wait_ns = 0, publish_hold_ns = 0;
	uint64_t consume_wait_ns = 0, consume_hold_ns = 0;
	int configured_inflight = 8;

	void reap() {
		auto *pool = WorkerThreadPool::get_singleton();
		for (auto it = tasks.begin(); it != tasks.end();) {
			if (pool->is_task_completed(*it)) {
				pool->wait_for_task_completion(*it);
				it = tasks.erase(it);
			} else
				++it;
		}
	}

public:
	explicit ChunkTaskScheduler(Runner p_runner) : runner(p_runner) {}
	~ChunkTaskScheduler() { reset(); }
	void enqueue(Job job) {
		job.request_id = ++next_revision;
		job.queued_us  = chunk_clock_us();
		revisions.insert(job.pos, job.request_id);
		pending.insert(job.pos, job);
	}
	bool contains(const Vector3i &pos) const { return active.has(pos) || pending.has(pos); }
	bool is_current(const Result &r) const {
		const uint64_t *revision = revisions.getptr(r.pos);
		return revision && *revision == r.request_id;
	}
	size_t ready_size() {
		std::lock_guard lock(ready_mutex);
		return ready.size();
	}
	size_t pending_size() const { return pending.size(); }
	size_t active_size() const { return active.size(); }
	// A small finite batch amortizes task submission without monopolizing the
	// shared Godot pool. Unconsumed results count against the in-flight limit.
	void configure(int batch_size, int max_inflight) {
		configured_batch_size = std::clamp(batch_size, 1, 8);
		configured_inflight	  = std::clamp(max_inflight, 1, 16);
	}
	void pump() {
		const int max_inflight = configured_inflight;
		const int batch_size   = configured_batch_size;
		reap();
		while (int(active.size()) < max_inflight) {
			std::vector<Job> jobs;
			std::vector<Vector3i> erase;
			const int capacity = std::min(batch_size, max_inflight - int(active.size()));
			for (bool priority : { true, false }) {
				for (const auto &entry : pending) {
					if (int(jobs.size()) >= capacity)
						break;
					if (entry.value.priority != priority || active.has(entry.key))
						continue;
					jobs.push_back(entry.value);
					active.insert(entry.key);
					erase.push_back(entry.key);
				}
			}
			if (jobs.empty())
				break;
			for (const auto &pos : erase)
				pending.erase(pos);
			const bool priority = jobs.front().priority;
			auto *batch			= new Batch{ this, std::move(jobs) };
			++submitted;
			tasks.push_back(WorkerThreadPool::get_singleton()->add_native_task([](void *data) {
				std::unique_ptr<Batch> batch(static_cast<Batch *>(data));
				auto results		  = batch->owner->runner(batch->jobs);
				const auto wait_start = chunk_clock_ns();
				{
					std::lock_guard lock(batch->owner->ready_mutex);
					const auto acquired = chunk_clock_ns();
					batch->owner->publish_wait_ns += acquired - wait_start;
					for (auto &result : results)
						batch->owner->ready.push_back(std::move(result));
					batch->owner->peak_ready = std::max(batch->owner->peak_ready, batch->owner->ready.size());
					batch->owner->publish_hold_ns += chunk_clock_ns() - acquired;
				}
			},
																			   batch, priority, "chunk_batch"));
		}
	}
	bool pop(Result &result) {
		Result next;
		const auto wait_start = chunk_clock_ns();
		{
			std::lock_guard lock(ready_mutex);
			const auto acquired = chunk_clock_ns();
			consume_wait_ns += acquired - wait_start;
			if (ready.empty()) {
				consume_hold_ns += chunk_clock_ns() - acquired;
				return false;
			}
			next = std::move(ready.front());
			ready.pop_front();
			consume_hold_ns += chunk_clock_ns() - acquired;
		}
		// Release the caller's previous resources after leaving the lock.
		result = std::move(next);
		active.erase(result.pos);
		++completed;
		if (!is_current(result))
			++obsolete;
		work_us += result.work_us;
		wait_us += result.wait_us;
		max_work_us = std::max(max_work_us, result.work_us);
		return true;
	}
	Dictionary stats() {
		Dictionary s;
		s["pending"]  = int64_t(pending.size());
		s["inflight"] = int64_t(active.size());
		std::array<uint64_t, 6> observed;
		{
			std::lock_guard lock(ready_mutex);
			observed = { ready.size(), peak_ready, publish_wait_ns, publish_hold_ns, consume_wait_ns, consume_hold_ns };
		}
		s["ready"]				   = int64_t(observed[0]);
		s["peak_ready"]			   = int64_t(observed[1]);
		s["publish_mutex_wait_ms"] = double(observed[2]) / 1e6;
		s["publish_mutex_hold_ms"] = double(observed[3]) / 1e6;
		s["consume_mutex_wait_ms"] = double(observed[4]) / 1e6;
		s["consume_mutex_hold_ms"] = double(observed[5]) / 1e6;
		s["tasks_submitted"]	   = int64_t(submitted);
		s["chunks_completed"]	   = int64_t(completed);
		s["obsolete_results"]	   = int64_t(obsolete);
		s["average_work_ms"]	   = completed ? double(work_us) / completed / 1000.0 : 0.0;
		s["average_queue_wait_ms"] = completed ? double(wait_us) / completed / 1000.0 : 0.0;
		s["max_work_ms"]		   = double(max_work_us) / 1000.0;
		s["batch_size"]			   = configured_batch_size;
		s["max_inflight"]		   = configured_inflight;
		return s;
	}
	void forget(const Vector3i &pos) {
		if (!contains(pos))
			revisions.erase(pos);
	}
	void cancel_outside(const Vector3i &center, int radius, int height, bool invalidate_running = true) {
		std::vector<Vector3i> remove;
		for (const auto &entry : revisions) {
			const auto delta = entry.key - center;
			if ((std::abs(delta.x) > radius || std::abs(delta.z) > radius || std::abs(delta.y) > height) &&
				(invalidate_running || !active.has(entry.key)))
				remove.push_back(entry.key);
		}
		for (const auto &pos : remove) {
			pending.erase(pos);
			revisions.erase(pos);
		}
	}
	void reset() {
		pending.clear();
		// Never wait while holding the publication mutex.
		if (auto *pool = WorkerThreadPool::get_singleton()) {
			for (auto id : tasks)
				pool->wait_for_task_completion(id);
		}
		tasks.clear();
		active.clear();
		revisions.clear();
		submitted = completed = obsolete = work_us = wait_us = max_work_us = 0;
		peak_ready														   = 0;
		publish_wait_ns = publish_hold_ns = consume_wait_ns = consume_hold_ns = 0;
		std::deque<Result> discarded;
		{
			std::lock_guard lock(ready_mutex);
			discarded.swap(ready);
		}
	}
};
} // namespace godot
#endif
