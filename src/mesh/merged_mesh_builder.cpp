#include "mesh/merged_mesh_builder.hpp"

namespace VoxelEngine {

namespace {

void append_mesh(BuiltMeshData& merged, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices,
                 float offset_x, float offset_y, float offset_z) {
    const uint32_t base_index = static_cast<uint32_t>(merged.vertices.size());
    merged.vertices.reserve(merged.vertices.size() + vertices.size());
    merged.indices.reserve(merged.indices.size() + indices.size());

    for (const Vertex& v : vertices) {
        Vertex out = v;
        out.x += offset_x;
        out.y += offset_y;
        out.z += offset_z;
        merged.vertices.push_back(out);
    }
    for (uint32_t idx : indices) {
        merged.indices.push_back(base_index + idx);
    }
}

void append_water_mesh(BuiltMeshData& merged, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices,
                       float offset_x, float offset_y, float offset_z) {
    const uint32_t base_index = static_cast<uint32_t>(merged.water_vertices.size());
    merged.water_vertices.reserve(merged.water_vertices.size() + vertices.size());
    merged.water_indices.reserve(merged.water_indices.size() + indices.size());

    for (const Vertex& v : vertices) {
        Vertex out = v;
        out.x += offset_x;
        out.y += offset_y;
        out.z += offset_z;
        merged.water_vertices.push_back(out);
    }
    for (uint32_t idx : indices) {
        merged.water_indices.push_back(base_index + idx);
    }
}

} // namespace

BuiltMeshData MergedMeshBuilder::build_merged(const ChunkMap& chunk_map,
                                              int32_t anchor_cx, int32_t anchor_cy, int32_t anchor_cz,
                                              int32_t merge_shift,
                                              MeshBuilder& builder) {
    BuiltMeshData merged;
    const int32_t merge_size = lod_merge_size(merge_shift);

    builder.set_smooth_lighting(false);
    builder.set_greedy_enabled(true);

    for (int32_t oz = 0; oz < merge_size; ++oz) {
        for (int32_t oy = 0; oy < merge_size; ++oy) {
            for (int32_t ox = 0; ox < merge_size; ++ox) {
                const int32_t cx = anchor_cx + ox;
                const int32_t cy = anchor_cy + oy;
                const int32_t cz = anchor_cz + oz;

                ChunkRenderData* render_data = chunk_map.get_chunk_render_data(cx, cy, cz);
                if (!render_data || !render_data->data || render_data->data->is_all_air()) {
                    continue;
                }

                ChunkRenderData* neighbors[6] = {};
                ChunkRenderData* diag[4] = {};
                chunk_map.get_extended_neighbors(cx, cy, cz, neighbors, diag);

                builder.clear();
                builder.build_mesh(
                    *render_data->data,
                    neighbors[0] && neighbors[0]->data ? neighbors[0]->data.get() : nullptr,
                    neighbors[1] && neighbors[1]->data ? neighbors[1]->data.get() : nullptr,
                    neighbors[2] && neighbors[2]->data ? neighbors[2]->data.get() : nullptr,
                    neighbors[3] && neighbors[3]->data ? neighbors[3]->data.get() : nullptr,
                    neighbors[4] && neighbors[4]->data ? neighbors[4]->data.get() : nullptr,
                    neighbors[5] && neighbors[5]->data ? neighbors[5]->data.get() : nullptr,
                    diag[0] && diag[0]->data ? diag[0]->data.get() : nullptr,
                    diag[1] && diag[1]->data ? diag[1]->data.get() : nullptr,
                    diag[2] && diag[2]->data ? diag[2]->data.get() : nullptr,
                    diag[3] && diag[3]->data ? diag[3]->data.get() : nullptr
                );

                append_mesh(
                    merged,
                    builder.get_vertices(),
                    builder.get_indices(),
                    static_cast<float>(ox * CHUNK_WIDTH),
                    static_cast<float>(oy * CHUNK_HEIGHT),
                    static_cast<float>(oz * CHUNK_DEPTH)
                );
                append_water_mesh(
                    merged,
                    builder.get_water_vertices(),
                    builder.get_water_indices(),
                    static_cast<float>(ox * CHUNK_WIDTH),
                    static_cast<float>(oy * CHUNK_HEIGHT),
                    static_cast<float>(oz * CHUNK_DEPTH)
                );
            }
        }
    }

    merged.empty = (merged.vertices.empty() || merged.indices.empty()) &&
                   (merged.water_vertices.empty() || merged.water_indices.empty());
    return merged;
}

} // namespace VoxelEngine
