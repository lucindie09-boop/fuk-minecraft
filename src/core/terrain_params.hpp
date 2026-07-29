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
    float sea_level = 64.0f;
    int32_t bedrock_height = 5;

    float cave_threshold = 0.4f;
    float cave_scale = 0.05f;

    float continentalness_scale = 1.0f;
    float ocean_threshold = 0.45f;
    float land_threshold = 0.55f;
    float shelf_width = 0.025f;
    float shelf_depth = 8.0f;
    float deep_ocean_depth = 30.0f;
    float beach_width = 0.002f;
    int32_t subsurface_cover_depth = 4;

    // Climate noise scales — multiplied against block coords before the
    // noise's own octave-based frequency (first_octave).  Default 1.0 means
    // "no extra scaling"; biome_size adjusts all three proportionally so the
    // editor slider actually works.
    float climate_temp_scale = 1.0f;
    float climate_humidity_scale = 1.0f;

    // Biome size multiplier (1.0 = default, >1 = larger biomes)
    float biome_size = 1.0f;
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_TERRAIN_PARAMS_HPP