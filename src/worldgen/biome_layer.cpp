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
    stack_ = std::make_unique<LayerStack>();
    memset(stack_.get(), 0, sizeof(LayerStack));
    setupGenerator(stack_.get(), MC_1_7);
    applySeed(stack_.get(), world_seed);
    initialized_ = true;
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
    // Use allocCache to get correct buffer size (genArea needs scratch space for recursion)
    int* cache = allocCache(&stack_->layers[L_RIVER_MIX_4], 10, 10);
    if (!cache) {
        // Fallback on allocation failure
        for (int i = 0; i < 10; ++i) {
            for (int j = 0; j < 10; ++j) {
                out[i][j] = Biomes::plains;
            }
        }
        return;
    }
    genArea(&stack_->layers[L_RIVER_MIX_4], cache,
            chunk_x * 4 - 2, chunk_z * 4 - 2, 10, 10);
    // Copy result to output array (result is at cache[x + z*areaWidth])
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            out[i][j] = cache[i + j * 10];
        }
    }
    free(cache);
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
    // Use allocCache to get correct buffer size (genArea needs scratch space for recursion)
    int* cache = allocCache(&stack_->layers[L_VORONOI_1], 16, 16);
    if (!cache) {
        // Fallback on allocation failure
        for (int i = 0; i < 16; ++i) {
            for (int j = 0; j < 16; ++j) {
                out[i][j] = Biomes::plains;
            }
        }
        return;
    }
    genArea(&stack_->layers[L_VORONOI_1], cache,
            chunk_x * 16, chunk_z * 16, 16, 16);
    // Copy result to output array (result is at cache[x + z*areaWidth])
    for (int i = 0; i < 16; ++i) {
        for (int j = 0; j < 16; ++j) {
            out[i][j] = cache[i + j * 16];
        }
    }
    free(cache);
}

} // namespace VoxelEngine
