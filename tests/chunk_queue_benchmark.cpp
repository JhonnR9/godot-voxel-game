// Standalone microbenchmark of the mesh-result drain algorithm before optimization.
// Uses this checkout's Godot HashSet; no renderer, workers or engine API calls.
// g++ -O3 -DNDEBUG -std=c++17 -Igodot-cpp/include -Igodot-cpp/gen/include
//     -Igodot-cpp/gdextension tests/chunk_queue_benchmark.cpp -o /tmp/chunk_queue_benchmark
#include <godot_cpp/templates/hash_set.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

// Minimal allocator/error adapters allow the actual template to run standalone.
namespace godot {
namespace internal {
void add_engine_class_registration_callback(void (*)()) {}
}
void *Memory::alloc_static(size_t n, bool pad) {
    auto *p = static_cast<unsigned char *>(std::malloc(n + (pad ? DATA_OFFSET : 0)));
    if (!p) std::abort();
    return p + (pad ? DATA_OFFSET : 0);
}
void *Memory::realloc_static(void *p, size_t n, bool pad) {
    if (!p) return alloc_static(n, pad);
    if (!n) { free_static(p, pad); return nullptr; }
    auto *base = static_cast<unsigned char *>(p) - (pad ? DATA_OFFSET : 0);
    base = static_cast<unsigned char *>(std::realloc(base, n + (pad ? DATA_OFFSET : 0)));
    if (!base) std::abort();
    return base + (pad ? DATA_OFFSET : 0);
}
void Memory::free_static(void *p, bool pad) {
    if (p) std::free(static_cast<unsigned char *>(p) - (pad ? DATA_OFFSET : 0));
}
void _err_print_error(const char *, const char *, int, const char *e, bool, bool) {
    std::fprintf(stderr, "%s\n", e ? e : "error"); std::abort();
}
void _err_print_error(const char *, const char *, int, const char *e, const char *m, bool, bool) {
    std::fprintf(stderr, "%s: %s\n", e ? e : "error", m ? m : ""); std::abort();
}
void _err_print_index_error(const char *, const char *, int, int64_t, int64_t,
        const char *, const char *, const char *, bool, bool) { std::abort(); }
void _err_flush_stdout() { std::fflush(stdout); }
}

struct Result {
    std::shared_ptr<const int> mesh, faces, extra;
    int x, y, z;
    uint64_t version;
    bool operator==(const Result &b) const { return x == b.x && y == b.y && z == b.z; }
};
struct Hasher {
    static uint32_t hash(const Result &r) {
        auto h = godot::hash_murmur3_one_32(r.x);
        h = godot::hash_murmur3_one_32(r.y, h);
        return godot::hash_murmur3_one_32(r.z, h);
    }
};
using Set = godot::HashSet<Result, Hasher>;
using Clock = std::chrono::steady_clock;
static volatile uint64_t checksum = 0;

static Set current_drain(Set &queue, int amount, std::mutex &mutex) {
    Set consumed, remaining;
    std::lock_guard lock(mutex);
    int n = std::min(amount, int(queue.size()));
    consumed.reserve(n);
    remaining.reserve(queue.size() - n);
    int count = 0;
    for (const auto &r : queue) {
        if (count++ < n) consumed.insert(r);
        else remaining.insert(r);
    }
    queue = std::move(remaining);
    return consumed;
}
static std::vector<Result> deque_drain(std::deque<Result> &queue, int amount, std::mutex &mutex) {
    std::vector<Result> consumed;
    consumed.reserve(amount);
    std::lock_guard lock(mutex);
    while (amount-- > 0 && !queue.empty()) {
        consumed.push_back(std::move(queue.front()));
        queue.pop_front();
    }
    return consumed;
}

template<class Queue, class Drain>
static std::pair<double, double> measure(int size, int amount, Drain drain) {
    std::vector<double> first, total;
    auto payload = std::make_shared<const int>(42);
    for (int trial = 0; trial < 7; ++trial) {
        Queue queue;
        for (int i = 0; i < size; ++i) {
            Result r{payload, payload, payload, i, i % 17, i % 31, 1};
            if constexpr (std::is_same_v<Queue, Set>) queue.insert(r);
            else queue.push_back(r);
        }
        std::mutex mutex;
        auto start = Clock::now();
        auto batch = drain(queue, amount, mutex);
        first.push_back(std::chrono::duration<double, std::micro>(Clock::now() - start).count());
        uint64_t sum = 0, count = 0;
        for (const auto &r : batch) { sum += r.x; ++count; }
        for (;;) {
            auto next = drain(queue, amount, mutex);
            bool empty;
            if constexpr (std::is_same_v<Queue, Set>) empty = next.is_empty();
            else empty = next.empty();
            if (empty) break;
            for (const auto &r : next) { sum += r.x; ++count; }
        }
        total.push_back(std::chrono::duration<double, std::milli>(Clock::now() - start).count());
        if (count != uint64_t(size) || sum != uint64_t(size) * (size - 1) / 2) std::abort();
        checksum = sum;
    }
    std::sort(first.begin(), first.end()); std::sort(total.begin(), total.end());
    return {first[3], total[3]};
}

int main() {
    // Equality by position keeps the first version, rather than replacing it.
    Set versions;
    Result old{nullptr, nullptr, nullptr, 1, 2, 3, 1};
    Result newer = old;
    newer.version = 2;
    versions.insert(old);
    versions.insert(newer);
    if (versions.size() != 1 || versions.begin()->version != 1) std::abort();

    std::puts("queue_size,batch,hash_first_us,deque_first_us,hash_drain_ms,deque_drain_ms");
    for (int size : {200, 1000, 10000}) for (int amount : {1, 2, 5, 100}) {
        auto h = measure<Set>(size, amount, current_drain);
        auto d = measure<std::deque<Result>>(size, amount, deque_drain);
        std::printf("%d,%d,%.3f,%.3f,%.3f,%.3f\n", size, amount, h.first, d.first, h.second, d.second);
    }
}
