#ifndef FUK_MINECRAFT_BIOME_REGISTRY_HPP
#define FUK_MINECRAFT_BIOME_REGISTRY_HPP
#include <cstdint>

namespace VoxelEngine {

// Vanilla 1.7 biome ID constants (from cubiomes BiomeID enum)
namespace Biomes {
    constexpr int ocean = 0;
    constexpr int plains = 1;
    constexpr int desert = 2;
    constexpr int extremeHills = 3;
    constexpr int forest = 4;
    constexpr int taiga = 5;
    constexpr int swamp = 6;
    constexpr int river = 7;
    constexpr int frozen_ocean = 10;
    constexpr int frozen_river = 11;
    constexpr int icePlains = 12;
    constexpr int iceMountains = 13;
    constexpr int mushroomIsland = 14;
    constexpr int mushroomIslandShore = 15;
    constexpr int beach = 16;
    constexpr int desertHills = 17;
    constexpr int forestHills = 18;
    constexpr int taiga_hills = 19;
    constexpr int extremeHillsEdge = 20;
    constexpr int jungle = 21;
    constexpr int jungle_hills = 22;
    constexpr int jungle_edge = 23;
    constexpr int deep_ocean = 24;
    constexpr int stoneBeach = 25;
    constexpr int coldBeach = 26;
    constexpr int birch_forest = 27;
    constexpr int birch_forest_hills = 28;
    constexpr int roofedForest = 29;
    constexpr int coldTaiga = 30;
    constexpr int coldTaigaHills = 31;
    constexpr int megaTaiga = 32;
    constexpr int megaTaigaHills = 33;
    constexpr int extremeHillsPlus = 34;
    constexpr int savanna = 35;
    constexpr int savannaPlateau = 36;
    constexpr int mesa = 37;
    constexpr int mesaPlateau_F = 38;
    constexpr int mesaPlateau = 39;
    constexpr int snowy_tundra = 12; // alias for icePlains
}

// Vanilla 1.7 biome terrain parameters — "depth" (base_height) and "scale" (height_scale).
struct BiomeTerrainParams {
    float base_height;
    float height_scale;
};

// Indexed by vanilla biome ID (0–255). Unused IDs have default {0, 0}.
extern const BiomeTerrainParams BIOME_TERRAIN_TABLE[256];

inline const BiomeTerrainParams& get_biome_terrain(int biome_id) {
    return BIOME_TERRAIN_TABLE[biome_id & 255];
}

} // namespace VoxelEngine
#endif // FUK_MINECRAFT_BIOME_REGISTRY_HPP
