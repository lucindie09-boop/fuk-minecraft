#ifndef FUK_MINECRAFT_MERGED_MESH_BUILDER_HPP
#define FUK_MINECRAFT_MERGED_MESH_BUILDER_HPP

#include "core/chunk_map.hpp"
#include "mesh/lod_types.hpp"
#include "mesh/mesh_builder.hpp"
#include "mesh/mesh_types.hpp"

namespace VoxelEngine {

class MergedMeshBuilder {
public:
    static BuiltMeshData build_merged(const ChunkMap& chunk_map,
                                      int32_t anchor_cx, int32_t anchor_cy, int32_t anchor_cz,
                                      int32_t merge_shift,
                                      MeshBuilder& builder);
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_MERGED_MESH_BUILDER_HPP
