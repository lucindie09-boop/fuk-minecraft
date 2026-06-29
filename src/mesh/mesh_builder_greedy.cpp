#include "mesh/mesh_builder.hpp"
#include "core/light_packing.hpp"

namespace VoxelEngine {

static inline bool lights_similar_enough(uint16_t a, uint16_t b) {
    return a == b;
}

struct GCell {
    BlockID block_id = BlockIDs::AIR;
    int rotation = 0;
    uint16_t light_key = 0;
    bool visible = false;
};

static bool cells_mergeable(const GCell& a, const GCell& b) {
    return a.visible && b.visible &&
           a.block_id == b.block_id &&
           a.rotation == b.rotation &&
           lights_similar_enough(a.light_key, b.light_key);
}

// Build a 2D grid of visible faces for one slice and emit greedy rectangles.
// dim_u, dim_v: grid dimensions (u = first tangent axis, v = second)
// get_cell: fills GCell at grid position (u, v) — returns true if the cell should be considered
// grid_u_to_xxx, grid_v_to_xxx: map grid coords to face origin coords
// emit_face: called with each rectangle (origin_u, origin_v, extent_u, extent_v) + first cell data
template<typename FGetCell, typename FEmit>
static void greedy_2d_slice(
    int32_t dim_u, int32_t dim_v,
    FGetCell&& get_cell,
    FEmit&& emit_face
) {
    // Allocate grids on the stack (max 34×34 for top/bottom, 32×256 for sides — ok on stack)
    // We use a flat array indexed [u * dim_v + v]
    int32_t grid_size = dim_u * dim_v;

    // For small grids, stack-allocate. For side faces (32×256), use heap to be safe.
    // But 32*256 = 8192 which is fine for stack.
    // Use variable-length array or std::vector. Let's use vector for safety.
    std::vector<GCell> grid(static_cast<size_t>(grid_size));
    std::vector<bool> covered(static_cast<size_t>(grid_size), false);

    // Fill the grid
    for (int32_t u = 0; u < dim_u; u++) {
        for (int32_t v = 0; v < dim_v; v++) {
            GCell cell;
            if (get_cell(u, v, cell)) {
                grid[static_cast<size_t>(u * dim_v + v)] = cell;
            }
        }
    }

    // Scan and expand rectangles
    for (int32_t u = 0; u < dim_u; u++) {
        for (int32_t v = 0; v < dim_v; v++) {
            size_t idx = static_cast<size_t>(u * dim_v + v);
            if (!grid[idx].visible || covered[idx]) continue;

            const GCell& first = grid[idx];

            // Expand right (increasing u)
            int32_t w = 1;
            while (u + w < dim_u) {
                size_t nidx = static_cast<size_t>((u + w) * dim_v + v);
                if (!cells_mergeable(first, grid[nidx]) || covered[nidx]) break;
                w++;
            }

            // Expand down (increasing v)
            int32_t h = 1;
            bool can_extend = true;
            while (v + h < dim_v && can_extend) {
                for (int32_t du = 0; du < w; du++) {
                    size_t nidx = static_cast<size_t>((u + du) * dim_v + (v + h));
                    if (!cells_mergeable(first, grid[nidx]) || covered[nidx]) {
                        can_extend = false;
                        break;
                    }
                }
                if (can_extend) h++;
            }

            // Mark covered
            for (int32_t du = 0; du < w; du++) {
                for (int32_t dv = 0; dv < h; dv++) {
                    covered[static_cast<size_t>((u + du) * dim_v + (v + dv))] = true;
                }
            }

            emit_face(u, v, w, h, first);
        }
    }
}

void MeshBuilder::greedy_2d(
    const ChunkData& chunk, const ChunkNeighborAccessor& accessor,
    FaceDirection direction, const BlockRegistry& registry
) {
    const int dir_idx = static_cast<int>(direction);
    const int32_t dx = kDirectionOffsets[dir_idx][0];
    const int32_t dy = kDirectionOffsets[dir_idx][1];
    const int32_t dz = kDirectionOffsets[dir_idx][2];

    switch (direction) {

    // =====================================================================
    // Horizontal faces: Top (+Y) and Bottom (-Y)
    // Face plane is at y+dy. One slice per y.
    // Grid: [x][z]  →  u maps to X, v maps to Z
    // =====================================================================
    case FaceDirection::Top:
    case FaceDirection::Bottom:
        for (int32_t y = 0; y < CHUNK_HEIGHT; y++) {
            int32_t ny = y + dy;

            greedy_2d_slice(CHUNK_WIDTH, CHUNK_DEPTH,
                [&](int32_t x, int32_t z, GCell& cell) -> bool {
                    BlockID block_id = chunk.get_block_unsafe(x, y, z);
                    if (block_id == BlockIDs::AIR) return false;

                    BlockID neighbor = accessor.get_block(x, ny, z);
                    if (should_cull_against_neighbor(chunk, block_id, neighbor, direction, x, y, z, registry)) {
                        return false;
                    }

                    cell.block_id = block_id;
                    cell.rotation = get_face_rotation(block_id, x, y, z, direction, dir_idx);
                    cell.light_key = accessor.get_light_packed(x + dx, ny, z + dz);
                    cell.visible = true;
                    return true;
                },
                [&](int32_t u, int32_t v, int32_t w, int32_t h, const GCell& first) {
                    // Check AO uniformity across the rectangle
                    float first_ao[4];
                    ao.compute_face(accessor, u, y, v, direction, first_ao);
                    bool has_occlusion = (first_ao[0] < 1.0f || first_ao[1] < 1.0f ||
                                          first_ao[2] < 1.0f || first_ao[3] < 1.0f);

                    bool uniform = true;
                    for (int32_t du = 0; du < w && uniform; du++) {
                        for (int32_t dv = 0; dv < h && uniform; dv++) {
                            if (du == 0 && dv == 0) continue;
                            float cell_ao[4];
                            ao.compute_face(accessor, u + du, y, v + dv, direction, cell_ao);
                            if (cell_ao[0] != first_ao[0] || cell_ao[1] != first_ao[1] ||
                                cell_ao[2] != first_ao[2] || cell_ao[3] != first_ao[3]) {
                                uniform = false;
                            }
                        }
                    }

                    if (uniform && !has_occlusion) {
                        Face face;
                        face.x = u;
                        face.y = y;
                        face.z = v;
                        face.direction = direction;
                        face.block_id = first.block_id;
                        face.u_max = w - 1;  // X extent
                        face.v_max = h - 1;  // Z extent
                        add_greedy_face(chunk, accessor, face, first.light_key,
                                        first.rotation, first_ao, registry);
                    } else {
                        for (int32_t du = 0; du < w; du++) {
                            for (int32_t dv = 0; dv < h; dv++) {
                                int32_t cx = u + du;
                                int32_t cz = v + dv;
                                BlockID bid = chunk.get_block_unsafe(cx, y, cz);
                                add_face(chunk, accessor, cx, y, cz, direction, bid, registry);
                            }
                        }
                    }
                }
            );
        }
        break;

    // =====================================================================
    // Side faces: Right (+X) and Left (-X)
    // Face plane is at x+dx. One slice per x.
    // Grid: [z][y]  →  u maps to Z, v maps to Y
    // =====================================================================
    case FaceDirection::Right:
    case FaceDirection::Left:
        for (int32_t x = 0; x < CHUNK_WIDTH; x++) {
            int32_t nx = x + dx;

            greedy_2d_slice(CHUNK_DEPTH, CHUNK_HEIGHT,
                [&](int32_t z, int32_t y, GCell& cell) -> bool {
                    BlockID block_id = chunk.get_block_unsafe(x, y, z);
                    if (block_id == BlockIDs::AIR) return false;

                    BlockID neighbor = accessor.get_block(nx, y, z);
                    if (should_cull_against_neighbor(chunk, block_id, neighbor, direction, x, y, z, registry)) {
                        return false;
                    }

                    cell.block_id = block_id;
                    cell.rotation = get_face_rotation(block_id, x, y, z, direction, dir_idx);
                    cell.light_key = accessor.get_light_packed(nx, y + dy, z + dz);
                    cell.visible = true;
                    return true;
                },
                [&](int32_t u, int32_t v, int32_t w, int32_t h, const GCell& first) {
                    // u maps to z, v maps to y, w is z-extent, h is y-extent
                    float first_ao[4];
                    ao.compute_face(accessor, x, v, u, direction, first_ao);
                    bool has_occlusion = (first_ao[0] < 1.0f || first_ao[1] < 1.0f ||
                                          first_ao[2] < 1.0f || first_ao[3] < 1.0f);

                    bool uniform = true;
                    for (int32_t du = 0; du < w && uniform; du++) {
                        for (int32_t dv = 0; dv < h && uniform; dv++) {
                            if (du == 0 && dv == 0) continue;
                            float cell_ao[4];
                            ao.compute_face(accessor, x, v + dv, u + du, direction, cell_ao);
                            if (cell_ao[0] != first_ao[0] || cell_ao[1] != first_ao[1] ||
                                cell_ao[2] != first_ao[2] || cell_ao[3] != first_ao[3]) {
                                uniform = false;
                            }
                        }
                    }

                    if (uniform && !has_occlusion) {
                        Face face;
                        face.x = x;
                        face.y = v;            // grid v → Y
                        face.z = u;            // grid u → Z
                        face.direction = direction;
                        face.block_id = first.block_id;
                        face.u_max = w - 1;    // Z extent
                        face.v_max = h - 1;    // Y extent
                        add_greedy_face(chunk, accessor, face, first.light_key,
                                        first.rotation, first_ao, registry);
                    } else {
                        for (int32_t du = 0; du < w; du++) {
                            for (int32_t dv = 0; dv < h; dv++) {
                                int32_t cz = u + du;
                                int32_t cy = v + dv;
                                BlockID bid = chunk.get_block_unsafe(x, cy, cz);
                                add_face(chunk, accessor, x, cy, cz, direction, bid, registry);
                            }
                        }
                    }
                }
            );
        }
        break;

    // =====================================================================
    // Side faces: Front (+Z) and Back (-Z)
    // Face plane is at z+dz. One slice per z.
    // Grid: [x][y]  →  u maps to X, v maps to Y
    // =====================================================================
    case FaceDirection::Front:
    case FaceDirection::Back:
        for (int32_t z = 0; z < CHUNK_DEPTH; z++) {
            int32_t nz = z + dz;

            greedy_2d_slice(CHUNK_WIDTH, CHUNK_HEIGHT,
                [&](int32_t x, int32_t y, GCell& cell) -> bool {
                    BlockID block_id = chunk.get_block_unsafe(x, y, z);
                    if (block_id == BlockIDs::AIR) return false;

                    BlockID neighbor = accessor.get_block(x, y, nz);
                    if (should_cull_against_neighbor(chunk, block_id, neighbor, direction, x, y, z, registry)) {
                        return false;
                    }

                    cell.block_id = block_id;
                    cell.rotation = get_face_rotation(block_id, x, y, z, direction, dir_idx);
                    cell.light_key = accessor.get_light_packed(x + dx, y + dy, nz);
                    cell.visible = true;
                    return true;
                },
                [&](int32_t u, int32_t v, int32_t w, int32_t h, const GCell& first) {
                    // u maps to x, v maps to y, w is x-extent, h is y-extent
                    float first_ao[4];
                    ao.compute_face(accessor, u, v, z, direction, first_ao);
                    bool has_occlusion = (first_ao[0] < 1.0f || first_ao[1] < 1.0f ||
                                          first_ao[2] < 1.0f || first_ao[3] < 1.0f);

                    bool uniform = true;
                    for (int32_t du = 0; du < w && uniform; du++) {
                        for (int32_t dv = 0; dv < h && uniform; dv++) {
                            if (du == 0 && dv == 0) continue;
                            float cell_ao[4];
                            ao.compute_face(accessor, u + du, v + dv, z, direction, cell_ao);
                            if (cell_ao[0] != first_ao[0] || cell_ao[1] != first_ao[1] ||
                                cell_ao[2] != first_ao[2] || cell_ao[3] != first_ao[3]) {
                                uniform = false;
                            }
                        }
                    }

                    if (uniform && !has_occlusion) {
                        Face face;
                        face.x = u;            // grid u → X
                        face.y = v;            // grid v → Y
                        face.z = z;
                        face.direction = direction;
                        face.block_id = first.block_id;
                        face.u_max = w - 1;    // X extent
                        face.v_max = h - 1;    // Y extent
                        add_greedy_face(chunk, accessor, face, first.light_key,
                                        first.rotation, first_ao, registry);
                    } else {
                        for (int32_t du = 0; du < w; du++) {
                            for (int32_t dv = 0; dv < h; dv++) {
                                int32_t cx = u + du;
                                int32_t cy = v + dv;
                                BlockID bid = chunk.get_block_unsafe(cx, cy, z);
                                add_face(chunk, accessor, cx, cy, z, direction, bid, registry);
                            }
                        }
                    }
                }
            );
        }
        break;
    }
}

} // namespace VoxelEngine
