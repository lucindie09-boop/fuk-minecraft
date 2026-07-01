#ifndef FUK_MINECRAFT_CHUNK_GENERATOR_HPP
#define FUK_MINECRAFT_CHUNK_GENERATOR_HPP
#include "core/terrain_params.hpp"
#include "core/noise.hpp"
#include "core/block_types.hpp"
#include "core/chunk_data.hpp"
#include "core/performance_timer.hpp"
#include <utility>
#include <cstdio>
#include <algorithm>
#include <array>
#include <cmath>
#include <random>

namespace VoxelEngine {

// -------------------------------------------------------------------------
// Biome types — climate-biome mapping uses temperature × humidity
// -------------------------------------------------------------------------
enum class BiomeType : uint8_t {
    Ocean     = 0,
    Beach     = 1,
    Plains    = 2,
    Forest    = 3,
    Desert    = 4,
    Taiga     = 5,
    Tundra    = 6,
    Savanna   = 7,
    Jungle    = 8,
    Swamp     = 9,
    Mountains = 10
};

// -------------------------------------------------------------------------
// Chunk generator — Minecraft-style procedural terrain generation
// with climate-driven biome selection and per-biome height functions.
// -------------------------------------------------------------------------
class ChunkGenerator {
private:
    FastNoise terrain_noise;
    FastNoise cave_noise;
    FastNoise continental_noise;
    FastNoise temp_noise;
    FastNoise humidity_noise;
    FastNoise erosion_noise;

    TerrainParams params;
    std::mt19937 rng;
    static PerformanceTimer perf_timer;

public:
    struct ColumnSample {
        BiomeType biome;
        float height;
        float water_level;   // -1 if no standing water
        bool near_water;
        float cont;          // continentalness (0-1)
        float temp;          // temperature (0-1)
        float hum;           // humidity (0-1)
    };

private:
    // -------------------------------------------------------------------------
    // Math helpers
    // -------------------------------------------------------------------------
    static float clamp01(float v) {
        return std::max(0.0f, std::min(1.0f, v));
    }

    static float smoothstep(float edge0, float edge1, float x) {
        float t = clamp01((x - edge0) / (edge1 - edge0));
        return t * t * (3.0f - 2.0f * t);
    }

    static float lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }

    // -------------------------------------------------------------------------
    // Noise sampling
    // -------------------------------------------------------------------------
    float sample_continentalness(float x, float z) const {
        float raw = continental_noise.noise_2d(x * params.continentalness_scale,
                                                z * params.continentalness_scale);
        return clamp01((raw + 1.0f) * 0.5f);
    }

    float sample_temperature(float x, float z) const {
        float raw = temp_noise.noise_2d(x * params.temp_scale, z * params.temp_scale);
        return clamp01((raw + 1.0f) * 0.5f);
    }

    float sample_humidity(float x, float z) const {
        float raw = humidity_noise.noise_2d(x * params.humidity_scale, z * params.humidity_scale);
        return clamp01((raw + 1.0f) * 0.5f);
    }

    float sample_erosion(float x, float z) const {
        float raw = erosion_noise.noise_2d(x * params.erosion_scale, z * params.erosion_scale);
        return clamp01((raw + 1.0f) * 0.5f);
    }

    // -------------------------------------------------------------------------
    // Climate → biome (Voronoi in temperature–humidity space)
    // -------------------------------------------------------------------------
    BiomeType classify_biome(float temp, float hum) const {
        struct Center { BiomeType type; float t; float h; };
        static constexpr Center centers[] = {
            {BiomeType::Tundra,  0.10f, 0.35f},
            {BiomeType::Taiga,   0.25f, 0.60f},
            {BiomeType::Plains,  0.45f, 0.35f},
            {BiomeType::Forest,  0.45f, 0.65f},
            {BiomeType::Savanna, 0.70f, 0.25f},
            {BiomeType::Desert,  0.80f, 0.08f},
            {BiomeType::Jungle,  0.70f, 0.80f},
            {BiomeType::Swamp,   0.35f, 0.85f},
        };
        float best_dist = 999.0f;
        BiomeType best = BiomeType::Plains;
        for (auto& c : centers) {
            float d = (temp - c.t) * (temp - c.t) + (hum - c.h) * (hum - c.h);
            if (d < best_dist) { best_dist = d; best = c.type; }
        }
        return best;
    }

    // -------------------------------------------------------------------------
    // Per-biome height functions
    // -------------------------------------------------------------------------
    float compute_ocean_floor(float x, float z, float cont) const;

    float compute_standard_height(float x, float z, float base_offset,
                                  float height_scale, float freq_base) const;

    float compute_mountain_height(float x, float z, float erosion) const;

    float compute_desert_height(float x, float z) const;

    float compute_biome_height(float x, float z, BiomeType biome,
                                float temp, float hum, float erosion) const;

    // -------------------------------------------------------------------------
    // Per-column terrain evaluation
    // -------------------------------------------------------------------------
    ColumnSample sample_column(int32_t world_x, int32_t world_z) const;

    // -------------------------------------------------------------------------
    // Block selection helpers
    // -------------------------------------------------------------------------
    BlockID get_surface_block(BiomeType biome, int32_t y, bool has_surface_water, bool near_water) const;
    BlockID get_subsurface_block(BiomeType biome, bool near_water) const;

public:
    // -------------------------------------------------------------------------
    // Fast chunk content estimation (for surface-aware generation)
    // -------------------------------------------------------------------------
    struct HeightRange {
        float min_h = 0.0f;
        float max_h = 0.0f;
        float max_water_h = -1.0f;
    };
    HeightRange get_chunk_height_range(int32_t chunk_x, int32_t chunk_z) const;
    BlockID get_chunk_subsurface_block(int32_t chunk_x, int32_t chunk_z) const;

    // Debug accessors
    float sample_continentalness_debug(float x, float z) const {
        return sample_continentalness(x, z);
    }
    ColumnSample sample_column_debug(int32_t world_x, int32_t world_z) const {
        return sample_column(world_x, world_z);
    }

    ChunkGenerator(const TerrainParams& p = TerrainParams())
        : terrain_noise(p.seed)
        , cave_noise(p.seed + 2000)
        , continental_noise(p.seed + 6000)
        , temp_noise(p.seed + 3000)
        , humidity_noise(p.seed + 4000)
        , erosion_noise(p.seed + 5000)
        , params(p)
        , rng(p.seed)
    {
    }

    BiomeType get_biome(int32_t world_x, int32_t world_z) const {
        return sample_column(world_x, world_z).biome;
    }

    float get_terrain_height(int32_t world_x, int32_t world_z) const {
        return sample_column(world_x, world_z).height;
    }

    float quick_height_estimate(int32_t world_x, int32_t world_z) const {
        float x = static_cast<float>(world_x);
        float z = static_cast<float>(world_z);
        return compute_standard_height(x, z, 8.0f, 16.0f, 1.0f);
    }

    bool is_cave(int32_t x, int32_t y, int32_t z) {
        if (y < params.bedrock_height + 3 || y > params.sea_level + 10) {
            return false;
        }
        float nx = static_cast<float>(x) * params.cave_scale;
        float ny = static_cast<float>(y) * params.cave_scale;
        float nz = static_cast<float>(z) * params.cave_scale;
        return cave_noise.noise_3d(nx, ny, nz) > params.cave_threshold;
    }

    // -------------------------------------------------------------------------
    // Per-column data used during chunk generation
    // -------------------------------------------------------------------------
    struct ChunkColumn {
        int32_t height = 0;
        BiomeType biome = BiomeType::Plains;
        int32_t water_level = -1;
        bool near_water = false;
    };

    // -------------------------------------------------------------------------
    // Main generation entry point
    // -------------------------------------------------------------------------
    void generate_chunk(ChunkData& chunk, int32_t chunk_x, int32_t chunk_y, int32_t chunk_z);

    // -------------------------------------------------------------------------
    // Debug: render continentalness / biome PGM images
    // -------------------------------------------------------------------------
    void render_continentalness_pgm(const char* filename, int img_w, int img_h,
                                    float world_x_start, float world_z_start,
                                    float step) const;

    void render_biome_pgm(const char* filename, int img_w, int img_h,
                          float world_x_start, float world_z_start,
                          float step) const;

    // -------------------------------------------------------------------------
    // Parameter management
    // -------------------------------------------------------------------------
    void set_params(const TerrainParams& p) {
        bool seed_changed = (p.seed != params.seed);
        params = p;
        if (seed_changed) {
            terrain_noise     = FastNoise(p.seed);
            cave_noise        = FastNoise(p.seed + 2000);
            continental_noise = FastNoise(p.seed + 6000);
            temp_noise        = FastNoise(p.seed + 3000);
            humidity_noise    = FastNoise(p.seed + 4000);
            erosion_noise     = FastNoise(p.seed + 5000);
            rng.seed(p.seed);
        }
    }

    const TerrainParams& get_params() const {
        return params;
    }

    static PerformanceTimer& get_perf_timer() {
        return perf_timer;
    }
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_CHUNK_GENERATOR_HPP
