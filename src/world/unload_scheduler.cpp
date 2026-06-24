#include "world/unload_scheduler.hpp"

#include "world/chunk_world.hpp"
#include "mesh/mesh_manager.hpp"
#include "core/chunk_map.hpp"

namespace VoxelEngine {

void UnloadScheduler::init(ChunkWorld* cw, MeshManager* mm) {
    chunk_world = cw;
    mesh_manager = mm;
}

void UnloadScheduler::update(int32_t active_render_distance, int32_t pcx, int32_t pcz, bool chunk_changed,
                             const FrameBudgets& budgets) {
    if (chunk_changed || (++unload_scan_skip_counter >= kUnloadScanSkipFrames)) {
        unload_scan_skip_counter = 0;
        int32_t unload_hrd  = active_render_distance + 2;
        int32_t unload_hrd2 = unload_hrd * unload_hrd;

        chunk_world->get_chunk_map().for_each_limited_resumable([&](uint64_t key, const std::unique_ptr<ChunkRenderData>&) {
            int32_t cx, cy, cz;
            ChunkMap::decode_chunk_key(key, cx, cy, cz);
            int32_t dx = cx - pcx;
            int32_t dz = cz - pcz;
            int32_t horiz_dist2 = dx * dx + dz * dz;
            if (horiz_dist2 > unload_hrd2) {
                queue_unload(key);
            } else {
                unload_pending.erase(key);
            }
        }, budgets.unload_checks_per_frame, unload_scan_bucket_cursor);
    }

    if (!unload_queue.empty()) {
        int32_t unload_hrd = active_render_distance + 2;
        int32_t unload_hrd2 = unload_hrd * unload_hrd;
        int32_t unloads_this_frame = 0;
        while (!unload_queue.empty() && unloads_this_frame < budgets.unloads_per_frame) {
            uint64_t key = unload_queue.back();
            unload_queue.pop_back();
            if (chunk_world->get_chunk_map().contains(key)) {
                int32_t cx, cy, cz;
                ChunkMap::decode_chunk_key(key, cx, cy, cz);
                int32_t dx = cx - pcx;
                int32_t dz = cz - pcz;
                int32_t horiz_dist2 = dx * dx + dz * dz;
                if (horiz_dist2 > unload_hrd2) {
                    try_unload(key);
                } else {
                    unload_pending.erase(key);
                }
            } else {
                unload_pending.erase(key);
            }
            unloads_this_frame++;
        }
    }
}

void UnloadScheduler::clear() {
    unload_queue.clear();
    unload_pending.clear();
    unload_scan_skip_counter   = 0;
    unload_scan_bucket_cursor  = 0;
}

void UnloadScheduler::reset() {
    clear();
}

void UnloadScheduler::queue_unload(uint64_t key) {
    if (unload_pending.insert(key).second) {
        unload_queue.push_back(key);
    }
}

void UnloadScheduler::try_unload(uint64_t key) {
    if (!chunk_world->try_unload_chunk(key, mesh_manager)) {
        unload_queue.push_back(key);
    } else {
        unload_pending.erase(key);
    }
}

} // namespace VoxelEngine
