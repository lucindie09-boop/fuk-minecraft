#include "mesh/mesh_builder.hpp"

namespace VoxelEngine {

PerformanceTimer MeshBuilder::perf_timer;
std::atomic<uint64_t> MeshBuilder::total_vertices{0};
std::atomic<uint64_t> MeshBuilder::total_chunks{0};

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
    const ChunkData* pos_x_pos_z
) {
    build_mesh(chunk_data, neg_x, pos_x, neg_y, pos_y, neg_z, pos_z,
               neg_x_neg_z, neg_x_pos_z, pos_x_neg_z, pos_x_pos_z);
    BuiltMeshData result;
    result.vertices = std::move(vertices);
    result.indices = std::move(indices);
    result.empty = result.vertices.empty() || result.indices.empty();
    return result;
}

void MeshBuilder::clear() {
    vertices.clear();
    indices.clear();
    vertices.reserve(kVertexReserve);
    indices.reserve(kIndexReserve);
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
                             const ChunkData* pos_x_pos_z) {
    auto build_start = std::chrono::high_resolution_clock::now();

    clear();

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

    if (chunk.is_all_air()) {
        return;
    }

    total_chunks.fetch_add(1, std::memory_order_relaxed);

    // solid_cache is laid out [x][z][y] (see header) so this population pass,
    // and every read site below, walk it with y as the fastest-varying index.
    // Populate interior: x and z outer, y inner so y is fastest-varying index
    for (int32_t x = 1; x <= CHUNK_WIDTH; x++) {
        for (int32_t z = 1; z <= CHUNK_DEPTH; z++) {
            for (int32_t y = 0; y < CHUNK_HEIGHT; y++) {
                solid_cache[x][z][y] = chunk.get_block_unsafe(x - 1, y, z - 1) != BlockIDs::AIR;
            }
        }
    }

    // X boundaries
    for (int32_t z = 1; z <= CHUNK_DEPTH; z++) {
        for (int32_t y = 0; y < CHUNK_HEIGHT; y++) {
            solid_cache[0][z][y] = neighbor_x_neg
                ? neighbor_x_neg->get_block_unsafe(CHUNK_WIDTH - 1, y, z - 1) != BlockIDs::AIR : false;
            solid_cache[SC_W - 1][z][y] = neighbor_x_pos
                ? neighbor_x_pos->get_block_unsafe(0, y, z - 1) != BlockIDs::AIR : false;
        }
    }

    // Z boundaries
    for (int32_t x = 1; x <= CHUNK_WIDTH; x++) {
        for (int32_t y = 0; y < CHUNK_HEIGHT; y++) {
            solid_cache[x][0][y] = neighbor_z_neg
                ? neighbor_z_neg->get_block_unsafe(x - 1, y, CHUNK_DEPTH - 1) != BlockIDs::AIR : false;
            solid_cache[x][SC_D - 1][y] = neighbor_z_pos
                ? neighbor_z_pos->get_block_unsafe(x - 1, y, 0) != BlockIDs::AIR : false;
        }
    }

    // Four corner columns (x=0 or SC_W-1, z=0 or SC_D-1)
    for (int32_t y = 0; y < CHUNK_HEIGHT; y++) {
        solid_cache[0][0][y] = neg_x_neg_z
            ? neg_x_neg_z->get_block_unsafe(CHUNK_WIDTH - 1, y, CHUNK_DEPTH - 1) != BlockIDs::AIR : false;
        solid_cache[SC_W - 1][0][y] = pos_x_neg_z
            ? pos_x_neg_z->get_block_unsafe(0, y, CHUNK_DEPTH - 1) != BlockIDs::AIR : false;
        solid_cache[0][SC_D - 1][y] = neg_x_pos_z
            ? neg_x_pos_z->get_block_unsafe(CHUNK_WIDTH - 1, y, 0) != BlockIDs::AIR : false;
        solid_cache[SC_W - 1][SC_D - 1][y] = pos_x_pos_z
            ? pos_x_pos_z->get_block_unsafe(0, y, 0) != BlockIDs::AIR : false;
    }

    if (passive_greedy_enabled) {
        passive_greedy_mesh_horizontal(chunk, accessor, FaceDirection::Top);
        passive_greedy_mesh_horizontal(chunk, accessor, FaceDirection::Bottom);

        passive_greedy_mesh_vertical(chunk, accessor, FaceDirection::Right);
        passive_greedy_mesh_vertical(chunk, accessor, FaceDirection::Left);
        passive_greedy_mesh_vertical(chunk, accessor, FaceDirection::Front);
        passive_greedy_mesh_vertical(chunk, accessor, FaceDirection::Back);
    } else {
        for (int32_t s = 0; s < CHUNK_SECTIONS; s++) {
            if (chunk.is_section_all_air(s)) continue;
            int32_t y0 = s * SECTION_HEIGHT;
            int32_t y1 = y0 + SECTION_HEIGHT;
            for (int32_t z = 0; z < CHUNK_DEPTH; z++) {
                for (int32_t x = 0; x < CHUNK_WIDTH; x++) {
                    for (int32_t y = y0; y < y1; y++) {
                        if (!solid_cache[x + 1][z + 1][y]) continue;
                        const BlockID block_id = chunk.get_block_unsafe(x, y, z);
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
                            BlockID neighbor = solid_cache[nx + 1][nz + 1][ny]
                                ? chunk.get_block_unsafe(nx, ny, nz)
                                : BlockIDs::AIR;
                            if (!should_cull_against_neighbor(chunk, block_id, neighbor, kAllDirections[i], x, y, z)) {
                                all_surrounded = false;
                                break;
                            }
                        }
                        if (all_surrounded) continue;
                        for (int i = 0; i < 6; i++) {
                            FaceDirection dir = kAllDirections[i];
                            int32_t dir_idx = static_cast<int32_t>(dir);
                            int32_t nx = x + kDirectionOffsets[dir_idx][0];
                            int32_t ny = y + kDirectionOffsets[dir_idx][1];
                            int32_t nz = z + kDirectionOffsets[dir_idx][2];
                            BlockID neighbor = accessor.get_block(nx, ny, nz);
                            if (!should_cull_against_neighbor(chunk, block_id, neighbor, dir, x, y, z)) {
                                add_face(chunk, accessor, x, y, z, dir, block_id);
                            }
                        }
                    }
                }
            }
        }
    }

    total_vertices.fetch_add(vertices.size(), std::memory_order_relaxed);
}

bool MeshBuilder::should_cull_against_neighbor(const ChunkData& chunk, BlockID current, BlockID neighbor,
                                                FaceDirection direction, int32_t x, int32_t y, int32_t z) const {
    if (neighbor == BlockIDs::AIR) {
        if (is_side_face(direction)) {
            int32_t nx = x + kDirectionOffsets[static_cast<int32_t>(direction)][0];
            int32_t ny = y + kDirectionOffsets[static_cast<int32_t>(direction)][1];
            int32_t nz = z + kDirectionOffsets[static_cast<int32_t>(direction)][2];
            if (nx >= 0 && nx < CHUNK_WIDTH && ny >= 0 && ny < CHUNK_HEIGHT && nz >= 0 && nz < CHUNK_DEPTH) {
                BlockID actual = chunk.get_block_unsafe(nx, ny, nz);
                if (actual != BlockIDs::AIR) {
                    return should_cull_against_neighbor(chunk, current, actual, direction, x, y, z);
                }
            }
        }
        return false;
    }
    const BlockRegistry& registry = BlockRegistry::get_instance();
    const BlockType& current_type = registry.get_block(current);
    if (current == neighbor && current_type.cull_against_same) return true;
    if (is_side_face(direction)) {
        float current_height = 1.0f - current_type.top_face_offset;
        float neighbor_height = 1.0f - registry.get_block(neighbor).top_face_offset;
        if (neighbor_height < current_height) return false;
        if (neighbor_height > current_height) return true;
    }
    return true;
}

// -------------------------------------------------------------------------
// Ambient occlusion helpers
// -------------------------------------------------------------------------
int MeshBuilder::vertex_ao_level(const ChunkNeighborAccessor& accessor,
                                 int32_t s1x, int32_t s1y, int32_t s1z,
                                 int32_t s2x, int32_t s2y, int32_t s2z,
                                 int32_t cx, int32_t cy, int32_t cz) const {
    bool side1 = accessor.is_occluder(s1x, s1y, s1z);
    bool side2 = accessor.is_occluder(s2x, s2y, s2z);
    bool corner = accessor.is_occluder(cx, cy, cz);
    return compute_ao(side1, side2, corner);
}

void MeshBuilder::compute_face_ao(const ChunkNeighborAccessor& accessor, int32_t x, int32_t y, int32_t z,
                                  FaceDirection direction, float ao_out[4]) const {
    BlockID current_block = accessor.center->get_block(x, y, z);
    bool is_lowered_block = BlockRegistry::get_instance().get_block(current_block).top_face_offset > 0.0f;

    switch (direction) {
        case FaceDirection::Top: {
            int dx[4] = {-1,  1,  1, -1};
            int dz[4] = {-1, -1,  1,  1};
            for (int i = 0; i < 4; i++) {
                if (is_lowered_block) {
                    int ao_y = vertex_ao_level(accessor, x + dx[i], y,   z,   x, y,   z + dz[i], x + dx[i], y,   z + dz[i]);
                    int ao_yp1 = vertex_ao_level(accessor, x + dx[i], y + 1, z,   x, y + 1, z + dz[i], x + dx[i], y + 1, z + dz[i]);
                    ao_out[i] = ao_to_brightness(std::max(ao_y, ao_yp1));
                } else {
                    int ao = vertex_ao_level(accessor, x + dx[i], y + 1, z,   x, y + 1, z + dz[i], x + dx[i], y + 1, z + dz[i]);
                    ao_out[i] = ao_to_brightness(ao);
                }
            }
            break;
        }
        case FaceDirection::Bottom: {
            int dx[4] = {-1,  1,  1, -1};
            int dz[4] = {-1, -1,  1,  1};
            for (int i = 0; i < 4; i++) {
                int ao = vertex_ao_level(accessor, x + dx[i], y - 1, z,   x, y - 1, z + dz[i], x + dx[i], y - 1, z + dz[i]);
                ao_out[i] = ao_to_brightness(ao);
            }
            break;
        }
        case FaceDirection::Right: {
            int dy[4] = {-1, -1,  1,  1};
            int dz[4] = {-1,  1,  1, -1};
            for (int i = 0; i < 4; i++) {
                if (is_lowered_block && dy[i] == 1) {
                    int ao_y = vertex_ao_level(accessor, x + 1, y + dy[i],     z,   x + 1, y,         z + dz[i], x + 1, y + dy[i],     z + dz[i]);
                    int ao_yp1 = vertex_ao_level(accessor, x + 1, y + dy[i] + 1, z,   x + 1, y + 1,     z + dz[i], x + 1, y + dy[i] + 1, z + dz[i]);
                    ao_out[i] = ao_to_brightness(std::max(ao_y, ao_yp1));
                } else {
                    int ao = vertex_ao_level(accessor, x + 1, y + dy[i], z,   x + 1, y, z + dz[i], x + 1, y + dy[i], z + dz[i]);
                    ao_out[i] = ao_to_brightness(ao);
                }
            }
            break;
        }
        case FaceDirection::Left: {
            int dy[4] = {-1, -1,  1,  1};
            int dz[4] = { 1, -1, -1,  1};
            for (int i = 0; i < 4; i++) {
                if (is_lowered_block && dy[i] == 1) {
                    int ao_y = vertex_ao_level(accessor, x - 1, y + dy[i],     z,   x - 1, y,         z + dz[i], x - 1, y + dy[i],     z + dz[i]);
                    int ao_yp1 = vertex_ao_level(accessor, x - 1, y + dy[i] + 1, z,   x - 1, y + 1,     z + dz[i], x - 1, y + dy[i] + 1, z + dz[i]);
                    ao_out[i] = ao_to_brightness(std::max(ao_y, ao_yp1));
                } else {
                    int ao = vertex_ao_level(accessor, x - 1, y + dy[i], z,   x - 1, y, z + dz[i], x - 1, y + dy[i], z + dz[i]);
                    ao_out[i] = ao_to_brightness(ao);
                }
            }
            break;
        }
        case FaceDirection::Front: {
            int dx[4] = { 1, -1, -1,  1};
            int dy[4] = {-1, -1,  1,  1};
            for (int i = 0; i < 4; i++) {
                if (is_lowered_block && dy[i] == 1) {
                    int ao_y = vertex_ao_level(accessor, x + dx[i], y,         z + 1, x,         y + dy[i],     z + 1, x + dx[i], y + dy[i],     z + 1);
                    int ao_yp1 = vertex_ao_level(accessor, x + dx[i], y + 1,     z + 1, x,         y + dy[i] + 1, z + 1, x + dx[i], y + dy[i] + 1, z + 1);
                    ao_out[i] = ao_to_brightness(std::max(ao_y, ao_yp1));
                } else {
                    int ao = vertex_ao_level(accessor, x + dx[i], y, z + 1, x, y + dy[i], z + 1, x + dx[i], y + dy[i], z + 1);
                    ao_out[i] = ao_to_brightness(ao);
                }
            }
            break;
        }
        case FaceDirection::Back: {
            int dx[4] = {-1,  1,  1, -1};
            int dy[4] = {-1, -1,  1,  1};
            for (int i = 0; i < 4; i++) {
                if (is_lowered_block && dy[i] == 1) {
                    int ao_y = vertex_ao_level(accessor, x + dx[i], y,         z - 1, x,         y + dy[i],     z - 1, x + dx[i], y + dy[i],     z - 1);
                    int ao_yp1 = vertex_ao_level(accessor, x + dx[i], y + 1,     z - 1, x,         y + dy[i] + 1, z - 1, x + dx[i], y + dy[i] + 1, z - 1);
                    ao_out[i] = ao_to_brightness(std::max(ao_y, ao_yp1));
                } else {
                    int ao = vertex_ao_level(accessor, x + dx[i], y, z - 1, x, y + dy[i], z - 1, x + dx[i], y + dy[i], z - 1);
                    ao_out[i] = ao_to_brightness(ao);
                }
            }
            break;
        }
    }
}

} // namespace VoxelEngine
