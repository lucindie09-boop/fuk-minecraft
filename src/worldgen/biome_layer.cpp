#include "worldgen/biome_layer.hpp"
#include "worldgen/biome_registry.hpp"
#include <cstring>

extern "C" {
#include "generator.h"
#include "layers.h"
#include "util.h"
}

namespace VoxelEngine {

BiomeLayer::BiomeLayer(uint64_t world_seed) {
    // TODO: cubiomes library causes crash on Windows. Investigate and fix.
    // For now, use fallback plains biome everywhere.
    stack_ = nullptr;
    initialized_ = false;
}

BiomeLayer::~BiomeLayer() = default;

void BiomeLayer::get_terrain_biomes(int chunk_x, int chunk_z, int out[10][10]) const {
    if (!initialized_ || !stack_) {
        // Fallback: fill with plains biome
        for (int i = 0; i < 10; ++i) {
            for (int j = 0; j < 10; ++j) {
                out[i][j] = Biomes::plains;
            }
        }
        return;
    }
    // Quarter-res biome map at 1:4 scale with 2-block padding for interpolation.
    genArea(&stack_->layers[L_RIVER_MIX_4], &out[0][0],
            chunk_x * 4 - 2, chunk_z * 4 - 2, 10, 10);
}

void BiomeLayer::get_surface_biomes(int chunk_x, int chunk_z, int out[16][16]) const {
    if (!initialized_ || !stack_) {
        // Fallback: fill with plains biome
        for (int i = 0; i < 16; ++i) {
            for (int j = 0; j < 16; ++j) {
                out[i][j] = Biomes::plains;
            }
        }
        return;
    }
    // Full-res biome map at 1:1 scale.
    genArea(&stack_->layers[L_VORONOI_1], &out[0][0],
            chunk_x * 16, chunk_z * 16, 16, 16);
}

} // namespace VoxelEngine
