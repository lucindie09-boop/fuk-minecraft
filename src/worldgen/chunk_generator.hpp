#ifndef FUK_MINECRAFT_CHUNK_GENERATOR_HPP
#define FUK_MINECRAFT_CHUNK_GENERATOR_HPP
#include <functional>
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
// Biome types
// -------------------------------------------------------------------------
enum class BiomeType : uint8_t {
    AbyssalTrench,
    DeepOcean,
    ShallowOcean,
    Beach,
    Plains,
    Desert,
    Forest,
    Jungle,
    Tundra,
    Mountains,
    Peaks
};

// -------------------------------------------------------------------------
// Chunk generator - Minecraft-style procedural terrain generation
// -------------------------------------------------------------------------
class ChunkGenerator {
private:
    FastNoise terrain_noise;
    FastNoise cave_noise;
    FastNoise continental_noise;
    FastNoise temp_noise;
    FastNoise humidity_noise;

    TerrainParams params;
    std::mt19937 rng;
    static PerformanceTimer perf_timer;



public:
    struct ColumnSample {
        BiomeType biome;
        float height;
        float water_level;
        bool near_water;
        float land_height;
        // Cached per-column values reused by lake fixup and bank-blend passes
        float lake_raw;          // blended lake noise value (0-1)
        float cont;              // continentalness value (0-1)
        float lake_water_level;  // computed lake water level (valid when biome==Lake or lake_raw used)
        // Climate values
        float temperature;
        float humidity;
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
        return clamp01((temp_noise.noise_2d(x * params.climate_temp_scale,
                                             z * params.climate_temp_scale) + 1.0f) * 0.5f);
    }

    float sample_humidity(float x, float z) const {
        return clamp01((humidity_noise.noise_2d(x * params.climate_humidity_scale,
                                                 z * params.climate_humidity_scale) + 1.0f) * 0.5f);
    }

    // Voronoi land biome from temperature/humidity (no beach/ocean/promotion).
    static BiomeType voronoi_land_biome(float temperature, float humidity) {
        static constexpr struct { float t; float h; BiomeType b; } centers[] = {
            {0.15f, 0.50f, BiomeType::Tundra},
            {0.40f, 0.35f, BiomeType::Plains},
            {0.40f, 0.75f, BiomeType::Forest},
            {0.80f, 0.15f, BiomeType::Desert},
            {0.80f, 0.80f, BiomeType::Jungle},
        };
        float best_dist = 999.0f;
        BiomeType result = BiomeType::Plains;
        for (auto& c : centers) {
            float d = (temperature - c.t) * (temperature - c.t) + (humidity - c.h) * (humidity - c.h);
            if (d < best_dist) { best_dist = d; result = c.b; }
        }
        return result;
    }

    BiomeType biome_from_climate(float temperature, float humidity, float cont) const {
        float beach_t = smoothstep(params.land_threshold, params.land_threshold + params.beach_width, cont);
        if (beach_t < 0.9f && cont >= params.land_threshold - 0.05f) {
            return BiomeType::Beach;
        }
        if (cont < params.ocean_threshold) {
            float depth = params.ocean_threshold - cont;
            if (depth > 0.10f) return BiomeType::AbyssalTrench;
            if (depth > 0.05f) return BiomeType::DeepOcean;
            return BiomeType::ShallowOcean;
        }
        return voronoi_land_biome(temperature, humidity);
    }

    static BiomeType promote_biome_by_height(BiomeType biome, float land_height, float sea_level) {
        if (land_height > sea_level + 60.0f) return BiomeType::Peaks;
        if (land_height > sea_level + 30.0f) return BiomeType::Mountains;
        return biome;
    }

    // Continuous Voronoi-weighted blend of all land-biome height parameters.
    // Produces smooth terrain transitions even when the discrete biome label flips.
    float sample_land_shape(float x, float z, float temperature, float humidity) const {
        static constexpr struct { float t, h, base_off, scale_m; } centers[] = {
            {0.15f, 0.50f, -12.0f, 0.33f},   // Tundra
            {0.40f, 0.35f, -12.0f, 0.33f},   // Plains
            {0.40f, 0.75f,   4.0f, 0.75f},   // Forest
            {0.80f, 0.15f,  -8.0f, 0.42f},   // Desert
            {0.80f, 0.80f,  20.0f, 1.08f},   // Jungle
        };
        float w_base = 0.0f, w_scale = 0.0f, w_total = 0.0f;
        for (auto& c : centers) {
            float dsq = (temperature - c.t) * (temperature - c.t) + (humidity - c.h) * (humidity - c.h);
            float w = 1.0f / (dsq + 0.0001f);
            w_base += w * c.base_off;
            w_scale += w * c.scale_m;
            w_total += w;
        }
        float base = params.base_height + w_base / w_total;
        float scale = params.height_scale * (w_scale / w_total);

        float broad_roll  = terrain_noise.fbm(x, z, 4, 0.52f, 0.0016f);
        float medium_roll = terrain_noise.fbm(x + 2000.0f, z - 2000.0f, 4, 0.55f, 0.0048f);
        float fine_roll   = terrain_noise.noise_2d(x * 0.012f, z * 0.012f);
        float micro_roll  = terrain_noise.noise_2d((x - 9000.0f) * 0.022f, (z + 9000.0f) * 0.022f);

        return base
             + broad_roll  * scale * 0.28f
             + medium_roll * scale * 0.16f
             + fine_roll   * scale * 0.07f
             + micro_roll  * scale * 0.03f;
    }

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

    // Debug accessors (expose private members for standalone tools)
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
        , temp_noise(p.seed + 4000)
        , humidity_noise(p.seed + 5000)
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

    // Cheaper than sample_column: only land shape, no biome/lake evaluation.
    float quick_height_estimate(int32_t world_x, int32_t world_z) const {
        float x = static_cast<float>(world_x);
        float z = static_cast<float>(world_z);
        float t = sample_temperature(x, z);
        float h = sample_humidity(x, z);
        return sample_land_shape(x, z, t, h);
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
    // Per-column data used during chunk generation (replaces 7 separate arrays)
    // -------------------------------------------------------------------------
    struct ChunkColumn {
        int32_t height = 0;
        BiomeType biome = BiomeType::Plains;
        int32_t water_level = -1;
        bool near_water = false;
        float temperature;
        float humidity;
    };

    // Cross-chunk block writer callback type
    using CrossChunkWriter = std::function<void(int32_t, int32_t, int32_t, BlockID)>;

    // -------------------------------------------------------------------------
    // Main generation entry point
    // -------------------------------------------------------------------------
    void generate_chunk(ChunkData& chunk, int32_t chunk_x, int32_t chunk_y, int32_t chunk_z,
                        const CrossChunkWriter& cross_writer = nullptr);

    // -------------------------------------------------------------------------
    // Debug: render continentalness as a PGM image (portable graymap)
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
            temp_noise        = FastNoise(p.seed + 4000);
            humidity_noise    = FastNoise(p.seed + 5000);
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