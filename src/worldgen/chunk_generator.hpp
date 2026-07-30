#ifndef FUK_MINECRAFT_CHUNK_GENERATOR_HPP
#define FUK_MINECRAFT_CHUNK_GENERATOR_HPP
#include <functional>
#include "core/terrain_params.hpp"
#include "core/noise.hpp"
#include "core/perlin_noise.hpp"
#include "core/java_rng.hpp"
#include "core/block_types.hpp"
#include "core/chunk_data.hpp"
#include "core/performance_timer.hpp"
#include "worldgen/biome_registry.hpp"
#include "worldgen/biome_layer.hpp"
#include <utility>
#include <cstdio>
#include <algorithm>
#include <array>
#include <cmath>
#include <random>

namespace VoxelEngine {

// Chunk generator — Minecraft 1.7-style terrain generation.
class ChunkGenerator {
private:
    // 1.7-style noise octaves — seeded via Java LCG chain in constructor.
    JavaOctaveNoise minLimitNoise;
    JavaOctaveNoise maxLimitNoise;
    JavaOctaveNoise mainNoise;
    JavaOctaveNoise scaleNoise;
    JavaOctaveNoise depthNoise;
    JavaPerlinNoise surfaceNoise;

    // Cave noise (same approach, deterministic per chunk)
    FastNoise cave_noise;

    // Biome layer (cubiomes)
    BiomeLayer biome_layer_;

    TerrainParams params;

    static PerformanceTimer perf_timer;

    // Gaussian biome weight kernel for density blending: 10/sqrt(i²+j²+0.2)
    struct WeightEntry { int di, dj; float w; };
    std::array<WeightEntry, 25> biome_weights_;

public:
    struct ColumnSample {
        int biome_id = 0;
        float height = 0.0f;
        float water_level = -1.0f;
    };

private:
    static float clamp01(float v) {
        return std::max(0.0f, std::min(1.0f, v));
    }

    static float lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }

    static double clamped_lerp(double a, double b, double t) {
        if (t < 0.0) return a;
        if (t > 1.0) return b;
        return a + (b - a) * t;
    }

    // Compute biome-weighted depth and scale for a 5×5 node position.
    void compute_biome_blend(int node_x, int node_z,
                             const int terrain_biomes[10][10],
                             float& out_depth, float& out_scale) const;

    // 2D height/density approximation for quick estimation.
    ColumnSample sample_column(int32_t world_x, int32_t world_z) const;

public:
    struct HeightRange {
        float min_h = 0.0f;
        float max_h = 0.0f;
        float max_water_h = -1.0f;
    };
    HeightRange get_chunk_height_range(int32_t chunk_x, int32_t chunk_z) const;
    BlockID get_chunk_subsurface_block(int32_t chunk_x, int32_t chunk_z) const;

    // Debug accessors
    float sample_continentalness_debug(float x, float z) const { return 0.0f; }
    ColumnSample sample_column_debug(int32_t world_x, int32_t world_z) const {
        return sample_column(world_x, world_z);
    }

    ChunkGenerator(const TerrainParams& p = TerrainParams())
        : cave_noise(p.seed + 2000)
        , biome_layer_(static_cast<uint64_t>(p.seed))
        , params(p)
    {
        // Pre-compute Gaussian biome weights: 10 / sqrt(di² + dj² + 0.2)
        int idx = 0;
        float total = 0.0f;
        for (int di = -2; di <= 2; ++di) {
            for (int dj = -2; dj <= 2; ++dj) {
                float w = 10.0f / std::sqrt(static_cast<float>(di * di + dj * dj) + 0.2f);
                biome_weights_[idx] = {di, dj, w};
                total += w;
                ++idx;
            }
        }
        // Normalize weights
        for (auto& bw : biome_weights_) bw.w /= total;

        // Java LCG noise seeding chain from world seed.
        uint64_t seed_state = static_cast<uint64_t>(p.seed);
        java_set_seed(seed_state, static_cast<uint64_t>(p.seed));
        minLimitNoise = JavaOctaveNoise(seed_state, -15, 16);
        maxLimitNoise = JavaOctaveNoise(seed_state, -15, 16);
        mainNoise = JavaOctaveNoise(seed_state, -7, 16);
        surfaceNoise = JavaPerlinNoise(seed_state, 4);
        scaleNoise = JavaOctaveNoise(seed_state, -10, 16);
        depthNoise = JavaOctaveNoise(seed_state, -10, 16);
    }

    int get_biome(int32_t world_x, int32_t world_z) const {
        return sample_column(world_x, world_z).biome_id;
    }

    float get_terrain_height(int32_t world_x, int32_t world_z) const {
        return sample_column(world_x, world_z).height;
    }

    float quick_height_estimate(int32_t world_x, int32_t world_z) const {
        return get_terrain_height(world_x, world_z);
    }

    bool is_cave(int32_t x, int32_t y, int32_t z) {
        if (y < params.bedrock_height + 3 || static_cast<float>(y) > params.sea_level + 10.0f) {
            return false;
        }
        float nx = static_cast<float>(x) * params.cave_scale;
        float ny = static_cast<float>(y) * params.cave_scale;
        float nz = static_cast<float>(z) * params.cave_scale;
        return cave_noise.noise_3d(nx, ny, nz) > params.cave_threshold;
    }

    struct ChunkColumn {
        int32_t top_solid_y = 0;
        int biome_id = 0;
        int32_t water_level = -1;
        float temperature = 0.5f;
        float humidity = 0.5f;
    };

    using CrossChunkWriter = std::function<void(int32_t, int32_t, int32_t, BlockID)>;

    void generate_chunk(ChunkData& chunk, int32_t chunk_x, int32_t chunk_y, int32_t chunk_z,
                        const CrossChunkWriter& cross_writer = nullptr, bool vegetation_enabled = true);

    // Debug: render biome map as PGM
    void render_biome_pgm(const char* filename, int img_w, int img_h,
                          float world_x_start, float world_z_start,
                          float step) const;

    void set_params(const TerrainParams& p) {
        params = p;
    }

    const TerrainParams& get_params() const {
        return params;
    }

    static PerformanceTimer& get_perf_timer() {
        return perf_timer;
    }

private:
    // Surface block selection based on biome_id (vanilla 1.7 rules).
    BlockID get_surface_block(int biome_id, int32_t y, bool has_surface_water) const;
    BlockID get_subsurface_block(int biome_id, bool aquatic) const;
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_CHUNK_GENERATOR_HPP
