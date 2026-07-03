#include "world/generation_scheduler.hpp"

#include "worldgen/chunk_generator.hpp"
#include "world/chunk_world.hpp"
#include "core/thread_pool.hpp"
#include "core/performance_timer.hpp"
#include <cmath>
#include <algorithm>

namespace VoxelEngine {

void GenerationScheduler::init(ChunkWorld* cw, ThreadPool* tp, PerformanceTimer* pt) {
    chunk_world = cw;
    thread_pool = tp;
    perf_timer = pt;
}

void GenerationScheduler::set_terrain_params(const TerrainParams& params) {
    terrain_params = params;
    if (height_estimator) {
        height_estimator->set_params(terrain_params);
    }
    invalidate_height_cache();
}

void GenerationScheduler::update(bool is_editor, int32_t active_render_distance, uint64_t epoch,
                                 int32_t pcx, int32_t pcy, int32_t pcz, bool chunk_changed,
                                 const FrameBudgets& budgets) {
    ScopedTimer t(*perf_timer, TimerID::ChunkLoadUnload);
    if (pre_sorted_offsets.empty() ||
        current_render_distance != active_render_distance) {
        initialize_view_distance(active_render_distance);
    }

    size_t generating_count = chunk_world->get_scheduler().generating_count();
    size_t worker_queue_size = thread_pool ? thread_pool->get_queue_size() : 0;
    size_t worker_count = thread_pool ? thread_pool->get_worker_count() : 0;
    const double worker_pressure =
        budgets.worker_queue_backlog > 0 ? static_cast<double>(worker_queue_size) / static_cast<double>(budgets.worker_queue_backlog) : 0.0;
    const double generating_pressure =
        worker_count > 0 ? static_cast<double>(generating_count) / static_cast<double>(worker_count * budgets.generating_per_worker) : 0.0;
    const double thread_pool_pressure = std::max(worker_pressure, generating_pressure);

    int32_t dynamic_max_generations = budgets.chunk_generations;
    if (thread_pool_pressure > 0.75) {
        const double generation_scale = std::clamp(1.5 - thread_pool_pressure, 0.25, 1.0);
        dynamic_max_generations = std::max(1, static_cast<int32_t>(
            std::round(static_cast<double>(budgets.chunk_generations) * generation_scale)));
    }

    if (chunk_changed) {
        generation_cursor          = 0;
        generation_pass_complete   = false;
        generation_sweep_generated = false;
    }

    if (!generation_pass_complete && !pre_sorted_offsets.empty()) {
        const size_t total_offsets       = pre_sorted_offsets.size();
        const size_t max_checks_per_frame = static_cast<size_t>(
            std::max(dynamic_max_generations * 2, 512));
        size_t   checks              = 0;
        int32_t  generations_this_frame = 0;

        while (checks < max_checks_per_frame &&
               generations_this_frame < dynamic_max_generations &&
               chunk_world->get_scheduler().can_enqueue(budgets.completed_queue_backlog)) {

            if (generation_cursor >= total_offsets) {
                generation_cursor = 0;
                if (!generation_sweep_generated) {
                    generation_pass_complete = true;
                    break;
                }
                generation_sweep_generated = false;
            }

            const ChunkPos& offset = pre_sorted_offsets[generation_cursor++];
            ++checks;

            int32_t cx = pcx + offset.x;
            int32_t cy = pcy + offset.y;
            int32_t cz = pcz + offset.z;

            uint64_t key = chunk_world->get_chunk_map().get_chunk_key(cx, cy, cz);
            if (chunk_world->get_chunk_map().contains(key)) {
                continue;
            }

            int32_t chunk_bottom = cy * CHUNK_HEIGHT;
            int32_t chunk_top    = (cy + 1) * CHUNK_HEIGHT;
            float   surface_h    = get_column_surface_height(cx, cz);
            bool near_player = std::abs(offset.x) <= 1 && std::abs(offset.z) <= 1 && std::abs(offset.y) <= 1;
            if (!near_player) {
                if (chunk_top   < surface_h - 32.0f) continue;
                if (chunk_bottom > surface_h + 32.0f) continue;
            }

            if (generate_chunk(cx, cy, cz, epoch)) {
                ++generations_this_frame;
                generation_sweep_generated = true;
            }
        }
    }
}

void GenerationScheduler::clear() {
    pre_sorted_offsets.clear();
    invalidate_height_cache();
    generation_cursor          = 0;
    generation_pass_complete   = false;
    generation_sweep_generated = false;
}

void GenerationScheduler::reset() {
    clear();
    current_render_distance = 64;
}

float GenerationScheduler::get_column_surface_height(int32_t cx, int32_t cz) {
    uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32)
                 | static_cast<uint64_t>(static_cast<uint32_t>(cz));
    auto it = column_height_cache.find(key);
    if (it != column_height_cache.end()) {
        return it->second;
    }
    if (column_height_cache.size() >= 65536) {
        uint64_t oldest = column_height_fifo.front();
        column_height_fifo.pop_front();
        column_height_cache.erase(oldest);
    }
    float h = height_estimator->get_terrain_height(
        cx * CHUNK_WIDTH + CHUNK_WIDTH / 2,
        cz * CHUNK_DEPTH + CHUNK_DEPTH / 2);
    column_height_cache[key] = h;
    column_height_fifo.push_back(key);
    return h;
}

void GenerationScheduler::invalidate_height_cache() {
    column_height_cache.clear();
    column_height_fifo.clear();
}

void GenerationScheduler::initialize_view_distance(int32_t horizontal_rd) {
    if (!height_estimator) {
        height_estimator = std::make_unique<ChunkGenerator>(terrain_params);
    } else {
        height_estimator->set_params(terrain_params);
    }
    column_height_cache.reserve(65536);
    constexpr int32_t VERTICAL_BUFFER = 2;
    current_render_distance = horizontal_rd;
    pre_sorted_offsets.clear();

    for (int32_t x = -horizontal_rd; x <= horizontal_rd; ++x) {
        for (int32_t y = -VERTICAL_BUFFER; y <= VERTICAL_BUFFER; ++y) {
            for (int32_t z = -horizontal_rd; z <= horizontal_rd; ++z) {
                int32_t horiz_dist2 = x * x + z * z;
                if (horiz_dist2 <= (horizontal_rd * horizontal_rd)) {
                    pre_sorted_offsets.push_back(ChunkPos{x, y, z});
                }
            }
        }
    }

    std::sort(pre_sorted_offsets.begin(), pre_sorted_offsets.end(),
        [](const ChunkPos& a, const ChunkPos& b) {
            return (a.x * a.x + a.y * a.y + a.z * a.z) < (b.x * b.x + b.y * b.y + b.z * b.z);
        }
    );
}

bool GenerationScheduler::generate_chunk(int32_t cx, int32_t cy, int32_t cz, uint64_t epoch) {
    return chunk_world->generate_chunk(cx, cy, cz, epoch, terrain_params);
}

} // namespace VoxelEngine
