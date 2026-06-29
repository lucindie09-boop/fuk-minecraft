#ifndef FUK_MINECRAFT_CHUNK_SCHEDULER_HPP
#define FUK_MINECRAFT_CHUNK_SCHEDULER_HPP
#include "core/chunk_types.hpp"
#include "core/thread_pool.hpp"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <queue>
#include <unordered_set>

namespace VoxelEngine {

// -------------------------------------------------------------------------
// Chunk scheduler ??? manages async chunk generation and mesh completion queues.
// This is a pure queue manager; the actual processing logic stays in ChunkManager.
// -------------------------------------------------------------------------
class ChunkScheduler {
public:
    void clear() {
        std::scoped_lock lock(generating_mutex, completed_mutex, completed_mesh_mutex, completed_group_mesh_mutex, completed_light_mutex);
        generating_chunks.clear();
        while (!completed_chunks.empty()) completed_chunks.pop();
        while (!completed_meshes.empty()) completed_meshes.pop();
        while (!completed_meshes_high_priority.empty()) completed_meshes_high_priority.pop();
        while (!completed_group_meshes.empty()) completed_group_meshes.pop();
        while (!completed_light_propagations.empty()) completed_light_propagations.pop();
        chunk_count_a.store(0, std::memory_order_relaxed);
        mesh_count_a.store(0, std::memory_order_relaxed);
        group_mesh_count_a.store(0, std::memory_order_relaxed);
        light_count_a.store(0, std::memory_order_relaxed);
    }

    // Returns true if the completed queue has room for more chunks.
    // Call this before enqueue_generation to prevent unbounded queue growth.
    // Lock-free fast-path: uses atomic counter so the generation loop never
    // acquires a mutex just to check queue depth.
    [[nodiscard]] bool can_enqueue(size_t max_completed) const noexcept {
        return chunk_count_a.load(std::memory_order_relaxed) < static_cast<int32_t>(max_completed);
    }

    // Returns true if the chunk was enqueued for generation, false if already generating or loaded.
    template<typename IsLoaded, typename GenerateFn, typename EpochFn>
    bool enqueue_generation(ThreadPool* pool, int32_t chunk_x, int32_t chunk_y, int32_t chunk_z, uint64_t epoch,
                            uint64_t chunk_key,
                            IsLoaded&& is_already_loaded,
                            GenerateFn&& generate_fn,
                            EpochFn&& epoch_provider) {
        if (!pool) return false;

        {
            std::lock_guard<std::mutex> lock(generating_mutex);
            if (is_already_loaded(chunk_key) || generating_chunks.find(chunk_key) != generating_chunks.end()) {
                return false;
            }
            generating_chunks.insert(chunk_key);
        }

        pool->fire_and_forget([this, chunk_x, chunk_y, chunk_z, chunk_key, epoch,
                               generate_fn = std::forward<GenerateFn>(generate_fn),
                               epoch_provider = std::forward<EpochFn>(epoch_provider)]() mutable {
            bool loaded = false;
            auto chunk_data = generate_fn(chunk_x, chunk_y, chunk_z, loaded);

            CompletedChunk completed;
            completed.chunk_x = chunk_x;
            completed.chunk_y = chunk_y;
            completed.chunk_z = chunk_z;
            completed.epoch = epoch;
            completed.chunk_data = std::move(chunk_data);
            completed.was_loaded_from_disk = loaded;

            {
                std::lock_guard<std::mutex> lock(generating_mutex);
                generating_chunks.erase(chunk_key);
            }

            if (epoch != epoch_provider()) {
                return;
            }

            {
                std::lock_guard<std::mutex> lock(completed_mutex);
                completed_chunks.push(std::move(completed));
                chunk_count_a.fetch_add(1, std::memory_order_relaxed);
            }
        });
        return true;
    }

    // Poll the next completed chunk. Returns false if the queue is empty.
    bool poll_completed_chunk(CompletedChunk& out) {
        std::lock_guard<std::mutex> lock(completed_mutex);
        if (completed_chunks.empty()) return false;
        out = std::move(completed_chunks.front());
        completed_chunks.pop();
        chunk_count_a.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }

    // Poll the next completed mesh. Returns false if both queues are empty.
    bool poll_completed_mesh(CompletedMesh& out, bool& high_priority) {
        std::lock_guard<std::mutex> lock(completed_mesh_mutex);
        if (!completed_meshes_high_priority.empty()) {
            out = std::move(completed_meshes_high_priority.front());
            completed_meshes_high_priority.pop();
            high_priority = true;
            mesh_count_a.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }
        if (!completed_meshes.empty()) {
            out = std::move(completed_meshes.front());
            completed_meshes.pop();
            high_priority = false;
            mesh_count_a.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }
        return false;
    }

    void push_completed_mesh(CompletedMesh&& mesh, bool high_priority) {
        std::lock_guard<std::mutex> lock(completed_mesh_mutex);
        if (high_priority) {
            completed_meshes_high_priority.push(std::move(mesh));
        } else {
            completed_meshes.push(std::move(mesh));
        }
        mesh_count_a.fetch_add(1, std::memory_order_relaxed);
    }

    [[nodiscard]] size_t generating_count() const {
        std::lock_guard<std::mutex> lock(generating_mutex);
        return generating_chunks.size();
    }

    // Lock-free count: uses atomic counter maintained by push/poll.
    // May be momentarily stale by ??1 ??? safe for the "is anything pending?" fast-path.
    [[nodiscard]] size_t completed_chunk_count() const noexcept {
        return static_cast<size_t>(std::max(0, chunk_count_a.load(std::memory_order_relaxed)));
    }

    [[nodiscard]] size_t completed_mesh_count() const noexcept {
        return static_cast<size_t>(std::max(0, mesh_count_a.load(std::memory_order_relaxed)));
    }

    void push_completed_group_mesh(CompletedGroupMesh&& mesh, bool) {
        std::lock_guard<std::mutex> lock(completed_group_mesh_mutex);
        completed_group_meshes.push(std::move(mesh));
        group_mesh_count_a.fetch_add(1, std::memory_order_relaxed);
    }

    bool poll_completed_group_mesh(CompletedGroupMesh& out, bool&) {
        std::lock_guard<std::mutex> lock(completed_group_mesh_mutex);
        if (completed_group_meshes.empty()) return false;
        out = std::move(completed_group_meshes.front());
        completed_group_meshes.pop();
        group_mesh_count_a.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }

    [[nodiscard]] size_t completed_group_mesh_count() const noexcept {
        return static_cast<size_t>(std::max(0, group_mesh_count_a.load(std::memory_order_relaxed)));
    }

    void push_completed_light_propagation(CompletedLightPropagation&& prop) {
        std::lock_guard<std::mutex> lock(completed_light_mutex);
        completed_light_propagations.push(std::move(prop));
        light_count_a.fetch_add(1, std::memory_order_relaxed);
    }

    bool poll_completed_light_propagation(CompletedLightPropagation& out) {
        std::lock_guard<std::mutex> lock(completed_light_mutex);
        if (completed_light_propagations.empty()) return false;
        out = std::move(completed_light_propagations.front());
        completed_light_propagations.pop();
        light_count_a.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }

private:
    std::unordered_set<uint64_t> generating_chunks;
    std::queue<CompletedChunk> completed_chunks;
    std::queue<CompletedMesh> completed_meshes;
    std::queue<CompletedMesh> completed_meshes_high_priority;
    mutable std::mutex generating_mutex;
    mutable std::mutex completed_mutex;
    mutable std::mutex completed_mesh_mutex;
    mutable std::mutex completed_group_mesh_mutex;
    mutable std::mutex completed_light_mutex;
    // Atomic counters mirror queue sizes for lock-free fast-path reads.
    std::atomic<int32_t> chunk_count_a{0};
    std::atomic<int32_t> mesh_count_a{0};
    std::atomic<int32_t> group_mesh_count_a{0};
    std::atomic<int32_t> light_count_a{0};
    std::queue<CompletedGroupMesh> completed_group_meshes;
    std::queue<CompletedLightPropagation> completed_light_propagations;
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_CHUNK_SCHEDULER_HPP