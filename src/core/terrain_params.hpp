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

    // Base terrain
    float sea_level = 192.0f;
    int32_t bedrock_height = 5;

    // Caves
    float cave_threshold = 0.4f;
    float cave_scale = 0.05f;

    // Continentalness (land vs ocean)
    float continentalness_scale = 0.00010f;
    float ocean_threshold = 0.48f;
    float land_threshold = 0.48f;
    float shelf_width = 0.14f;
    float shelf_depth = 18.0f;
    float deep_ocean_depth = 48.0f;
    float beach_width = 0.05f;

    // Subsurface
    int32_t subsurface_cover_depth = 4;

    // Climate noise scales
    float temp_scale = 0.00012f;
    float humidity_scale = 0.00012f;
    float erosion_scale = 0.00015f;

    // Mountain threshold (erosion above this triggers Mountains biome)
    float mountain_erosion_threshold = 0.65f;

    // Snow line (Y level above which surface becomes snow in cold biomes)
    float snow_line_y = 240.0f;
    // Tree line (Y level above which surface becomes stone in mountains)
    float tree_line_y = 220.0f;
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_TERRAIN_PARAMS_HPP