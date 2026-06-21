#ifndef FUK_MINECRAFT_TERRAIN_PARAMS_HPP
#define FUK_MINECRAFT_TERRAIN_PARAMS_HPP
#include <cstdint>

namespace VoxelEngine {

// -------------------------------------------------------------------------
// Terrain generation parameters — kept in its own header so that
// world scheduling code (WorldUpdater, ChunkWorld) does not have to
// include the heavy chunk_generator.hpp / noise.hpp transitively.
// -------------------------------------------------------------------------
struct TerrainParams {
    int32_t seed = 12345;
    float sea_level = 62.0f;
    float base_height = 64.0f;
    float height_scale = 32.0f;
    float mountain_scale = 64.0f;
    int32_t bedrock_height = 5;

    float cave_threshold = 0.4f;
    float cave_scale = 0.05f;

    float continentalness_scale = 0.00010f;
    float ocean_threshold = 0.48f;
    float land_threshold = 0.48f;
    float shelf_width = 0.14f;
    float shelf_depth = 18.0f;
    float deep_ocean_depth = 48.0f;
    float beach_width = 0.05f;
    // Lake generation parameters (noise-based placement)
    float lake_noise_scale = 0.004f;
    float lake_detail_scale = 0.02f;
    float lake_threshold = 0.64f;
    float lake_max_height_above_sea = 60.0f;
    float lake_depth = 10.0f;
    float lake_height_variation_scale = 0.0008f;
    float lake_min_height_above_sea = -5.0f;
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_TERRAIN_PARAMS_HPP