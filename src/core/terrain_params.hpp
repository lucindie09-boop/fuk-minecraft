#ifndef FUK_MINECRAFT_TERRAIN_PARAMS_HPP
#define FUK_MINECRAFT_TERRAIN_PARAMS_HPP
#include <cstdint>

namespace VoxelEngine {

struct TerrainParams {
    int32_t seed = 12345;
    float sea_level = 63.0f;
    int32_t bedrock_height = 5;

    float cave_threshold = 0.4f;
    float cave_scale = 0.05f;
    int32_t subsurface_cover_depth = 4;
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_TERRAIN_PARAMS_HPP
