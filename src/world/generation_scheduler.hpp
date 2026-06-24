#ifndef FUK_MINECRAFT_GENERATION_SCHEDULER_HPP
#define FUK_MINECRAFT_GENERATION_SCHEDULER_HPP

#include "core/chunk_types.hpp"
#include "core/terrain_params.hpp"
#include "core/frame_budgets.hpp"
#include <deque>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace VoxelEngine {

class ChunkWorld;
class ThreadPool;
class PerformanceTimer;
class ChunkGenerator;

class GenerationScheduler {
public:
    GenerationScheduler() = default;

    void init(ChunkWorld* cw, ThreadPool* tp, PerformanceTimer* pt);

    void set_terrain_params(const TerrainParams& params);

    void update(bool is_editor, int32_t active_render_distance, uint64_t epoch,
                int32_t pcx, int32_t pcy, int32_t pcz, bool chunk_changed,
                const FrameBudgets& budgets);

    void clear();
    void reset();

    float get_column_surface_height(int32_t cx, int32_t cz);
    void invalidate_height_cache();

private:
    ChunkWorld* chunk_world = nullptr;
    ThreadPool* thread_pool = nullptr;
    PerformanceTimer* perf_timer = nullptr;
    TerrainParams terrain_params;

    std::vector<ChunkPos> pre_sorted_offsets;
    int32_t current_render_distance = 64;

    std::unique_ptr<ChunkGenerator> height_estimator;
    std::unordered_map<uint64_t, float> column_height_cache;
    std::deque<uint64_t> column_height_fifo;

    size_t generation_cursor          = 0;
    bool   generation_pass_complete   = false;
    bool   generation_sweep_generated = false;

    void initialize_view_distance(int32_t horizontal_rd);
    bool generate_chunk(int32_t cx, int32_t cy, int32_t cz, uint64_t epoch);
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_GENERATION_SCHEDULER_HPP
