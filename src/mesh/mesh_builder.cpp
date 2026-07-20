#include "mesh/mesh_builder.hpp"

namespace VoxelEngine {

// bru bru bru

PerformanceTimer MeshBuilder::perf_timer;
std::atomic<uint64_t> MeshBuilder::total_vertices{0};
std::atomic<uint64_t> MeshBuilder::total_chunks{0};
std::atomic<uint64_t> MeshBuilder::greedy_v_merge_attempts{0};
std::atomic<uint64_t> MeshBuilder::greedy_v_merge_successes{0};
std::atomic<uint64_t> MeshBuilder::greedy_v_reject_ao_mismatch{0};
std::atomic<uint64_t> MeshBuilder::greedy_v_reject_ao_occlusion{0}; 
std::atomic<uint64_t> MeshBuilder::greedy_v_reject_light_mismatch{0}; 
std::atomic<uint64_t> MeshBuilder::greedy_v_reject_rotation_mismatch{0};
std::atomic<uint64_t> MeshBuilder::greedy_v_reject_block_mismatch{0};
std::atomic<uint64_t> MeshBuilder::greedy_v_reject_distance_limit{0};


MeshBuilder::GreedyVerticalStatsSnapshot MeshBuilder::get_greedy_vertical_stats() {
    GreedyVerticalStatsSnapshot stats;
    stats.merge_attempts = greedy_v_merge_attempts.load(std::memory_order_relaxed);
    stats.merge_successes = greedy_v_merge_successes.load(std::memory_order_relaxed);
    stats.reject_ao_mismatch = greedy_v_reject_ao_mismatch.load(std::memory_order_relaxed);
    stats.reject_ao_occlusion = greedy_v_reject_ao_occlusion.load(std::memory_order_relaxed);
    stats.reject_light_mismatch = greedy_v_reject_light_mismatch.load(std::memory_order_relaxed);
    stats.reject_rotation_mismatch = greedy_v_reject_rotation_mismatch.load(std::memory_order_relaxed);
    stats.reject_block_mismatch = greedy_v_reject_block_mismatch.load(std::memory_order_relaxed);
    stats.reject_distance_limit = greedy_v_reject_distance_limit.load(std::memory_order_relaxed);
    return stats;
}

void MeshBuilder::reset_greedy_vertical_stats() {
greedy_v_merge_attempts.store (0, std::memory_order_relaxed); greedy_v_merge_successes.store (0, std::memory_order_relaxed); greedy_v_reject_ao_mismatch.store (0, std::memory_order_relaxed); greedy_v_reject_ao_occlusion.store (0, std::memory_order_relaxed);
greedy_v_reject_light_mismatch.store(0, std::memory_order_relaxed);
greedy_v_reject_rotation_mismatch.store(0, std::memory_order_relaxed);
greedy_v_reject_block_mismatch.store(0, std::memory_order_relaxed);
greedy_v_reject_distance_limit.store(0, std::memory_order_relaxed);
}

void MeshBuilder::set_detail_level(float level) {
    detail_level_ = std::clamp(level, 0.125f, 1.0f);
    int raw = static_cast<int>(std::round(1.0f / detail_level_));
    stride_xz_ = 1;
    while (stride_xz_ * 2 <= raw && stride_xz_ < 8) stride_xz_ *= 2;
}

BuiltMeshData MeshBuilder::build_mesh_data(
    const ChunkData& chunk_data,
    const ChunkData* neg_x,
    const ChunkData* pos_x,
    const ChunkData* neg_y,
    const ChunkData* pos_y,
    const ChunkData* neg_z,
    const ChunkData* pos_z,
    const ChunkData* neg_x_neg_z,
    const ChunkData* neg_x_pos_z,
    const ChunkData* pos_x_neg_z,
    const ChunkData* pos_x_pos_z,
    const ChunkData* neg_x_neg_y,
    const ChunkData* pos_x_neg_y,
    const ChunkData* neg_x_pos_y,
    const ChunkData* pos_x_pos_y,
    const ChunkData* neg_y_neg_z,
    const ChunkData* neg_y_pos_z,
    const ChunkData* pos_y_neg_z,
    const ChunkData* pos_y_pos_z,
    const ChunkData* neg_x_neg_y_neg_z,
    const ChunkData* pos_x_neg_y_neg_z,
    const ChunkData* neg_x_pos_y_neg_z,
    const ChunkData* pos_x_pos_y_neg_z,
    const ChunkData* neg_x_neg_y_pos_z,
    const ChunkData* pos_x_neg_y_pos_z,
    const ChunkData* neg_x_pos_y_pos_z,
    const ChunkData* pos_x_pos_y_pos_z
) {
    build_mesh(chunk_data, neg_x, pos_x, neg_y, pos_y, neg_z, pos_z,
               neg_x_neg_z, neg_x_pos_z, pos_x_neg_z, pos_x_pos_z,
               neg_x_neg_y, pos_x_neg_y, neg_x_pos_y, pos_x_pos_y,
               neg_y_neg_z, neg_y_pos_z, pos_y_neg_z, pos_y_pos_z,
               neg_x_neg_y_neg_z, pos_x_neg_y_neg_z,
               neg_x_pos_y_neg_z, pos_x_pos_y_neg_z,
               neg_x_neg_y_pos_z, pos_x_neg_y_pos_z,
               neg_x_pos_y_pos_z, pos_x_pos_y_pos_z);
    BuiltMeshData result;
    result.vertices = std::move(vertices);
    result.indices = std::move(indices);
    result.water_vertices = std::move(water_vertices);
    result.water_indices = std::move(water_indices);
    result.empty = (result.vertices.empty() || result.indices.empty()) &&
                   (result.water_vertices.empty() || result.water_indices.empty());
    return result;
}

void MeshBuilder::clear() {
    vertices.clear();
    indices.clear();
    water_vertices.clear();
    water_indices.clear();
    vertices.reserve(kVertexReserve);
    indices.reserve(kIndexReserve);
    water_vertices.reserve(kVertexReserve);
    water_indices.reserve(kIndexReserve);
greedy_v_stats_local = {};
}

void MeshBuilder::build_mesh(const ChunkData& chunk,
                             const ChunkData* neighbor_x_neg,
                             const ChunkData* neighbor_x_pos,
                             const ChunkData* neighbor_y_neg,
                             const ChunkData* neighbor_y_pos,
                             const ChunkData* neighbor_z_neg,
                             const ChunkData* neighbor_z_pos,
                             const ChunkData* neg_x_neg_z,
                             const ChunkData* neg_x_pos_z,
                             const ChunkData* pos_x_neg_z,
                             const ChunkData* pos_x_pos_z,
                             const ChunkData* neg_x_neg_y,
                             const ChunkData* pos_x_neg_y,
                             const ChunkData* neg_x_pos_y,
                             const ChunkData* pos_x_pos_y,
                             const ChunkData* neg_y_neg_z,
                             const ChunkData* neg_y_pos_z,
                             const ChunkData* pos_y_neg_z,
                             const ChunkData* pos_y_pos_z,
                             const ChunkData* neg_x_neg_y_neg_z,
                             const ChunkData* pos_x_neg_y_neg_z,
                             const ChunkData* neg_x_pos_y_neg_z,
                             const ChunkData* pos_x_pos_y_neg_z,
                             const ChunkData* neg_x_neg_y_pos_z,
                             const ChunkData* pos_x_neg_y_pos_z,
                             const ChunkData* neg_x_pos_y_pos_z,
                             const ChunkData* pos_x_pos_y_pos_z) {
ScopedTimer build_timer(perf_timer, TimerID::BuildMesh);

    clear();
    for (auto& plane : solid_cache)
        for (auto& row : plane)
            row.fill(0);

    // Initialize the neighbor accessor
    accessor.center = &chunk;
    accessor.neg_x = neighbor_x_neg;
    accessor.pos_x = neighbor_x_pos;
    accessor.neg_y = neighbor_y_neg;
    accessor.pos_y = neighbor_y_pos;
    accessor.neg_z = neighbor_z_neg;
    accessor.pos_z = neighbor_z_pos;
    accessor.neg_x_neg_z = neg_x_neg_z;
    accessor.neg_x_pos_z = neg_x_pos_z;
    accessor.pos_x_neg_z = pos_x_neg_z;
    accessor.pos_x_pos_z = pos_x_pos_z;
    accessor.neg_x_neg_y = neg_x_neg_y;
    accessor.pos_x_neg_y = pos_x_neg_y;
    accessor.neg_x_pos_y = neg_x_pos_y;
    accessor.pos_x_pos_y = pos_x_pos_y;
    accessor.neg_y_neg_z = neg_y_neg_z;
    accessor.neg_y_pos_z = neg_y_pos_z;
    accessor.pos_y_neg_z = pos_y_neg_z;
    accessor.pos_y_pos_z = pos_y_pos_z;
    accessor.neg_x_neg_y_neg_z = neg_x_neg_y_neg_z;
    accessor.pos_x_neg_y_neg_z = pos_x_neg_y_neg_z;
    accessor.neg_x_pos_y_neg_z = neg_x_pos_y_neg_z;
    accessor.pos_x_pos_y_neg_z = pos_x_pos_y_neg_z;
    accessor.neg_x_neg_y_pos_z = neg_x_neg_y_pos_z;
    accessor.pos_x_neg_y_pos_z = pos_x_neg_y_pos_z;
    accessor.neg_x_pos_y_pos_z = neg_x_pos_y_pos_z;
    accessor.pos_x_pos_y_pos_z = pos_x_pos_y_pos_z;

    if (chunk.is_all_air()) {
        return;
    }

    total_chunks.fetch_add(1, std::memory_order_relaxed);


    if (!registry_) { registry_ = &BlockRegistry::get_instance(); }
    const BlockRegistry& registry = *registry_;
    // solid_cache is laid out [y][z][x] (see header) so this population pass
    // walks it with x as the fastest-varying index.
    {
        ScopedTimer cache_timer(perf_timer, TimerID::SolidCachePopulation);
        // Populate interior (x: 1..SC_W-2 = CHUNK_WIDTH, z: 1..SC_D-2 = CHUNK_DEPTH)
        for (int32_t s = 0; s < CHUNK_SECTIONS; s++) {
            if (chunk.is_section_all_air(s)) continue;
            int32_t y0 = s * SECTION_HEIGHT;
            int32_t y1 = y0 + SECTION_HEIGHT;
            for (int32_t y = y0; y < y1; y++) {
                for (int32_t z = 1; z <= CHUNK_DEPTH; z++) {
                    int32_t z_src = ((z - 1) / stride_xz_) * stride_xz_;
                    for (int32_t x = 1; x <= CHUNK_WIDTH; x++) {
                        int32_t x_src = ((x - 1) / stride_xz_) * stride_xz_;
                        BlockID representative = BlockIDs::AIR;
                        int32_t solid_count = 0;
                        const int32_t total = stride_xz_ * stride_xz_;
                        for (int32_t dz = 0; dz < stride_xz_; ++dz) {
                            for (int32_t dx = 0; dx < stride_xz_; ++dx) {
                                BlockID sample = chunk.get_block_unsafe(x_src + dx, y, z_src + dz);
                                if (sample != BlockIDs::AIR) {
                                    ++solid_count;
                                    if (representative == BlockIDs::AIR) representative = sample;
                                }
                            }
                        }
                        solid_cache[y][z][x] = (solid_count * 2 >= total) ? representative : BlockIDs::AIR;
                    }
                }
            }
        }

        // X boundaries — store the actual BlockID (or BlockIDs::AIR if neighbor null)
        for (int32_t y = 0; y < CHUNK_HEIGHT; y++) {
            for (int32_t z = 1; z <= CHUNK_DEPTH; z++) {
                int32_t z_src = ((z - 1) / stride_xz_) * stride_xz_;
                solid_cache[y][z][0] = neighbor_x_neg
                    ? neighbor_x_neg->get_block_unsafe(CHUNK_WIDTH - 1, y, z_src)
                    : BlockIDs::AIR;
                solid_cache[y][z][SC_W - 1] = neighbor_x_pos
                    ? neighbor_x_pos->get_block_unsafe(0, y, z_src)
                    : BlockIDs::AIR;
            }
        }

        // Z boundaries
        for (int32_t y = 0; y < CHUNK_HEIGHT; y++) {
            for (int32_t x = 1; x <= CHUNK_WIDTH; x++) {
                int32_t x_src = ((x - 1) / stride_xz_) * stride_xz_;
                solid_cache[y][0][x] = neighbor_z_neg
                    ? neighbor_z_neg->get_block_unsafe(x_src, y, CHUNK_DEPTH - 1)
                    : BlockIDs::AIR;
                solid_cache[y][SC_D - 1][x] = neighbor_z_pos
                    ? neighbor_z_pos->get_block_unsafe(x_src, y, 0)
                    : BlockIDs::AIR;
            }
        }

        // Four corner columns (x=0 or SC_W-1, z=0 or SC_D-1)
        for (int32_t y = 0; y < CHUNK_HEIGHT; y++) {
            solid_cache[y][0][0] = neg_x_neg_z
                ? neg_x_neg_z->get_block_unsafe(CHUNK_WIDTH - 1, y, CHUNK_DEPTH - 1)
                : BlockIDs::AIR;
            solid_cache[y][0][SC_W - 1] = pos_x_neg_z
                ? pos_x_neg_z->get_block_unsafe(0, y, CHUNK_DEPTH - 1)
                : BlockIDs::AIR;
            solid_cache[y][SC_D - 1][0] = neg_x_pos_z
                ? neg_x_pos_z->get_block_unsafe(CHUNK_WIDTH - 1, y, 0)
                : BlockIDs::AIR;
            solid_cache[y][SC_D - 1][SC_W - 1] = pos_x_pos_z
                ? pos_x_pos_z->get_block_unsafe(0, y, 0)
                : BlockIDs::AIR;
        }
    }

    if (passive_greedy_enabled) {
        {
            ScopedTimer greedy_h_timer(perf_timer, TimerID::GreedyMeshHorizontal);
            passive_greedy_mesh_horizontal(chunk, accessor, FaceDirection::Top, registry);
            // Bottom faces are never visible from a ground-level / top-down view.
            // Skipping them saves ~1/6 of mesh build time and reduces GPU upload bytes.
            // passive_greedy_mesh_horizontal(chunk, accessor, FaceDirection::Bottom, registry);
        }
        {
            ScopedTimer greedy_v_timer(perf_timer, TimerID::GreedyMeshVertical);
            passive_greedy_mesh_vertical(chunk, accessor, registry);
        }
    } else {
        for (int32_t s = 0; s < CHUNK_SECTIONS; s++) {
            if (chunk.is_section_all_air(s)) continue;
            int32_t y0 = s * SECTION_HEIGHT;
            int32_t y1 = y0 + SECTION_HEIGHT;
            for (int32_t y = y0; y < y1; y++) {
                for (int32_t z = 0; z < CHUNK_DEPTH; z += stride_xz_) {
                    for (int32_t x = 0; x < CHUNK_WIDTH; x += stride_xz_) {
                        const BlockID block_id = solid_cache[y][z + 1][x + 1];
                        if (block_id == BlockIDs::AIR) continue;
                        if (stride_xz_ == 1) {
                            bool all_surrounded = true;
                            for (int i = 0; i < 6; i++) {
                                int32_t nx = x + kDirectionOffsets[i][0];
                                int32_t ny = y + kDirectionOffsets[i][1];
                                int32_t nz = z + kDirectionOffsets[i][2];
                                if (nx < 0 || nx >= CHUNK_WIDTH ||
                                    ny < 0 || ny >= CHUNK_HEIGHT ||
                                    nz < 0 || nz >= CHUNK_DEPTH) {
                                    all_surrounded = false;
                                    break;
                                }
                                BlockID neighbor = solid_cache[ny][nz + 1][nx + 1];
                                if (!should_cull_against_neighbor(chunk, block_id, neighbor, kAllDirections[i], x, y, z, registry)) {
                                    all_surrounded = false;
                                    break;
                                }
                            }
                            if (all_surrounded) continue;
                        }
                        for (int i = 0; i < 6; i++) {
                            FaceDirection dir = kAllDirections[i];
                            if (dir == FaceDirection::Bottom) continue;
                            int32_t dir_idx = static_cast<int32_t>(dir);
                            int32_t nx = x + kDirectionOffsets[dir_idx][0] * stride_xz_;
                            int32_t ny = y + kDirectionOffsets[dir_idx][1];
                            int32_t nz = z + kDirectionOffsets[dir_idx][2] * stride_xz_;

                            BlockID neighbor = accessor.get_block(nx, ny, nz);
                            if (!should_cull_against_neighbor(chunk, block_id, neighbor, dir, x, y, z, registry)) {
                                add_face(chunk, accessor, x, y, z, dir, block_id, registry);
                            }
                        }
                    }
                }
            }
        }
    }

    total_vertices.fetch_add(vertices.size(), std::memory_order_relaxed);

    greedy_v_merge_attempts.fetch_add(greedy_v_stats_local.merge_attempts, std::memory_order_relaxed);
    greedy_v_merge_successes.fetch_add(greedy_v_stats_local.merge_successes, std::memory_order_relaxed);
    greedy_v_reject_ao_mismatch.fetch_add(greedy_v_stats_local.reject_ao_mismatch, std::memory_order_relaxed);
    greedy_v_reject_ao_occlusion.fetch_add(greedy_v_stats_local.reject_ao_occlusion, std::memory_order_relaxed);
    greedy_v_reject_light_mismatch.fetch_add(greedy_v_stats_local.reject_light_mismatch, std::memory_order_relaxed);
    greedy_v_reject_rotation_mismatch.fetch_add(greedy_v_stats_local.reject_rotation_mismatch, std::memory_order_relaxed);
    greedy_v_reject_block_mismatch.fetch_add(greedy_v_stats_local.reject_block_mismatch, std::memory_order_relaxed);
    greedy_v_reject_distance_limit.fetch_add(greedy_v_stats_local.reject_distance_limit, std::memory_order_relaxed);
}

bool MeshBuilder::should_cull_against_neighbor(const ChunkData& chunk, BlockID current, BlockID neighbor,
                                                FaceDirection direction, int32_t x, int32_t y, int32_t z,
                                                const BlockRegistry& registry) const {
    if (neighbor == BlockIDs::AIR) {
         return false;
    }
    const BlockType& neighbor_type = registry.get_block(neighbor);
    const BlockType& current_type = registry.get_block(current);
    if (HasProperty(neighbor_type.properties, BlockProperty::Transparent)) {
        if (current != neighbor) {
            if (!HasProperty(neighbor_type.properties, BlockProperty::Liquid) ||
                !HasProperty(current_type.properties, BlockProperty::Liquid)) {
                return false;
            }
        }
    }
    if (current == neighbor && current_type.cull_against_same) return true;
    if (is_side_face(direction)) {
        float current_height = 1.0f - current_type.top_face_offset;
        float neighbor_height = 1.0f - neighbor_type.top_face_offset;
        if (neighbor_height < current_height) return false;
        if (neighbor_height > current_height) return true;
    }
    return true;
}


} // namespace VoxelEngine
