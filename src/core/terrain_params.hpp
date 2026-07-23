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
    float sea_level = 200.0f;
    int32_t bedrock_height = 5;

    float cave_threshold = 0.4f;
    float cave_scale = 0.05f;

    float continentalness_scale = 0.00010f;
    float ocean_threshold = 0.48f;
    float land_threshold = 0.48f;
    float shelf_width = 0.025f;
    float shelf_depth = 18.0f;
    float deep_ocean_depth = 48.0f;
    float beach_width = 0.002f;
    int32_t subsurface_cover_depth = 4;

    // Climate noise scales (lower = broader regions)
    float climate_temp_scale = 0.00015f;
    float climate_humidity_scale = 0.00020f;

    // Biome size multiplier (1.0 = default, >1 = larger biomes)
    float biome_size = 1.0f;
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_TERRAIN_PARAMS_HPP