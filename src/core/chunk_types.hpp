#ifndef FUK_MINECRAFT_CHUNK_TYPES_HPP
#define FUK_MINECRAFT_CHUNK_TYPES_HPP
#include "core/chunk_data.hpp"
#include "mesh/mesh_types.hpp"
#include <godot_cpp/variant/rid.hpp>
#include <memory>
#include <atomic>

namespace VoxelEngine {

// -------------------------------------------------------------------------
// Per-chunk render data (stored in the chunk map)
// -------------------------------------------------------------------------
struct ChunkRenderData {
    std::unique_ptr<ChunkData> data;
    godot::RID mesh_rid;
    godot::RID instance_rid;
    bool is_mesh_dirty = true;
    std::atomic<int> pending_mesh_builds{0};
    std::atomic<int> pending_mesh_uploads{0};
    std::atomic<uint64_t> mesh_job_serial{0};
};

// -------------------------------------------------------------------------
// Completed mesh from worker thread
// -------------------------------------------------------------------------
struct CompletedMesh {
    int32_t chunk_x = 0;
    int32_t chunk_y = 0;
    int32_t chunk_z = 0;
    uint64_t epoch = 0;
    uint64_t mesh_job_serial = 0;
    ChunkRenderData* source_chunk = nullptr;
    PackedBuiltMeshData mesh_data;
};

// -------------------------------------------------------------------------
// Completed chunk from worker thread
// -------------------------------------------------------------------------
struct CompletedChunk {
    int32_t chunk_x = 0;
    int32_t chunk_y = 0;
    int32_t chunk_z = 0;
    uint64_t epoch = 0;
    std::unique_ptr<ChunkData> chunk_data;
    bool was_loaded_from_disk = false;
};

// -------------------------------------------------------------------------
// Pending chunk stage (for staged install: chunk ??? light ??? mesh)
// -------------------------------------------------------------------------
struct PendingChunkStage {
    int32_t chunk_x = 0;
    int32_t chunk_y = 0;
    int32_t chunk_z = 0;
    uint64_t epoch = 0;
};

// -------------------------------------------------------------------------
// Dirty mesh queue entry (priority by distance)
// -------------------------------------------------------------------------
struct DirtyChunkEntry {
    uint64_t key = 0;
    int32_t dist_sq = 0;
    bool urgent = false;
    bool operator>(const DirtyChunkEntry& other) const {
        if (urgent != other.urgent) {
            return !urgent && other.urgent;
        }
        return dist_sq > other.dist_sq;
    }
};

// -------------------------------------------------------------------------
// Pending block placement for chunks that haven't loaded yet
// -------------------------------------------------------------------------
struct PendingBlockPlacement {
    int32_t world_x = 0;
    int32_t world_y = 0;
    int32_t world_z = 0;
    int block_id = 0;
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_CHUNK_TYPES_HPP