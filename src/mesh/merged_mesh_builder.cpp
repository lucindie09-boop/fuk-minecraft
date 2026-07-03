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

                ChunkRenderData* all_neighbors[26] = {};
                chunk_map.get_all_neighbors(cx, cy, cz, all_neighbors);

                auto data_or_null = [](ChunkRenderData* rd) -> const ChunkData* {
                    return (rd && rd->data) ? rd->data.get() : nullptr;
                };

                builder.clear();
                builder.build_mesh(
                    *render_data->data,
                    data_or_null(all_neighbors[0]),  // neg_x
                    data_or_null(all_neighbors[1]),  // pos_x
                    data_or_null(all_neighbors[2]),  // neg_y
                    data_or_null(all_neighbors[3]),  // pos_y
                    data_or_null(all_neighbors[4]),  // neg_z
                    data_or_null(all_neighbors[5]),  // pos_z
                    data_or_null(all_neighbors[6]),  // neg_x_neg_z
                    data_or_null(all_neighbors[7]),  // neg_x_pos_z
                    data_or_null(all_neighbors[8]),  // pos_x_neg_z
                    data_or_null(all_neighbors[9]),  // pos_x_pos_z
                    data_or_null(all_neighbors[10]), // neg_x_neg_y
                    data_or_null(all_neighbors[11]), // pos_x_neg_y
                    data_or_null(all_neighbors[12]), // neg_x_pos_y
                    data_or_null(all_neighbors[13]), // pos_x_pos_y
                    data_or_null(all_neighbors[14]), // neg_y_neg_z
                    data_or_null(all_neighbors[15]), // neg_y_pos_z
                    data_or_null(all_neighbors[16]), // pos_y_neg_z
                    data_or_null(all_neighbors[17]), // pos_y_pos_z
                    data_or_null(all_neighbors[18]), // neg_x_neg_y_neg_z
                    data_or_null(all_neighbors[19]), // pos_x_neg_y_neg_z
                    data_or_null(all_neighbors[20]), // neg_x_pos_y_neg_z
                    data_or_null(all_neighbors[21]), // pos_x_pos_y_neg_z
                    data_or_null(all_neighbors[22]), // neg_x_neg_y_pos_z
                    data_or_null(all_neighbors[23]), // pos_x_neg_y_pos_z
                    data_or_null(all_neighbors[24]), // neg_x_pos_y_pos_z
                    data_or_null(all_neighbors[25])  // pos_x_pos_y_pos_z
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
