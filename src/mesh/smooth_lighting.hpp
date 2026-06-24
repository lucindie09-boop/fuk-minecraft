#ifndef FUK_MINECRAFT_SMOOTH_LIGHTING_HPP
#define FUK_MINECRAFT_SMOOTH_LIGHTING_HPP

#include <cstdint>
#include "mesh/mesh_types.hpp"
#include "mesh/chunk_neighbor_accessor.hpp"

namespace VoxelEngine {

// Computes per-vertex light for a block face by averaging the light of the
// 4 blocks meeting at each vertex corner (face-adjacent block + 3 corner blocks).
// Returns 4 packed light keys (one per vertex in standard face vertex order).
// These can be unpacked and assigned per-vertex for smooth light gradients
// across faces instead of flat per-face lighting.
void compute_smooth_light(
    const ChunkNeighborAccessor& accessor,
    int32_t x, int32_t y, int32_t z,
    FaceDirection direction,
    uint16_t light_keys_out[4]
);

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_SMOOTH_LIGHTING_HPP
