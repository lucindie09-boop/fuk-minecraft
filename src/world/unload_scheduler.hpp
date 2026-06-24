#ifndef FUK_MINECRAFT_UNLOAD_SCHEDULER_HPP
#define FUK_MINECRAFT_UNLOAD_SCHEDULER_HPP

#include "core/frame_budgets.hpp"
#include <vector>
#include <unordered_set>
#include <cstdint>

namespace VoxelEngine {

class ChunkWorld;
class MeshManager;

class UnloadScheduler {
public:
    UnloadScheduler() = default;

    void init(ChunkWorld* cw, MeshManager* mm);

    void update(int32_t active_render_distance, int32_t pcx, int32_t pcz, bool chunk_changed,
                const FrameBudgets& budgets);

    void clear();
    void reset();

    void queue_unload(uint64_t key);
    void try_unload(uint64_t key);

private:
    ChunkWorld* chunk_world = nullptr;
    MeshManager* mesh_manager = nullptr;

    std::vector<uint64_t> unload_queue;
    std::unordered_set<uint64_t> unload_pending;

    int32_t unload_scan_skip_counter   = 0;
    static constexpr int32_t kUnloadScanSkipFrames = 15;
    size_t unload_scan_bucket_cursor = 0;
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_UNLOAD_SCHEDULER_HPP
