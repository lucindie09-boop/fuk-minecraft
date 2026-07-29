#ifndef FUK_MINECRAFT_CHUNK_GENERATOR_HPP
#define FUK_MINECRAFT_CHUNK_GENERATOR_HPP
#include <functional>
#include "core/terrain_params.hpp"
#include "core/noise.hpp"
#include "core/block_types.hpp"
#include "core/chunk_data.hpp"
#include "core/performance_timer.hpp"
#include "worldgen/climate_sampler.hpp"
#include "worldgen/terrain_spline.hpp"
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
    Forest,
    Desert,
    PlainsMountain,   // erosion < 0.25 — stone surface, no topsoil
    ForestMountain,   // erosion < 0.25 — stone surface, no topsoil
    DesertMountain,   // erosion < 0.25 — stone/rocky surface
};

// -------------------------------------------------------------------------
// Chunk generator - Minecraft-style procedural terrain generation
// -------------------------------------------------------------------------
class ChunkGenerator {
private:
    FastNoise terrain_noise;
    FastNoise cave_noise;
    ClimateSampler climate;
    SplineStack spline_stack_;
    SplineStack spline_stack_factor_;
    SplineStack spline_stack_jaggedness_;
    TerrainSpline* spline_root_ = nullptr;
    TerrainSpline* spline_root_factor_ = nullptr;
    TerrainSpline* spline_root_jaggedness_ = nullptr;

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
        float cont;              // continentalness value (0-1)
        float temperature;
        float humidity;
        float erosion;
        float weirdness;
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
        double raw = climate.sample_continentalness(static_cast<double>(x), static_cast<double>(z));
        return clamp01(static_cast<float>((raw + 1.0) * 0.5));
    }

    float sample_temperature(float x, float z) const {
        double raw = climate.sample_temperature(static_cast<double>(x), static_cast<double>(z));
        return clamp01(static_cast<float>((raw + 1.0) * 0.5));
    }

    float sample_humidity(float x, float z) const {
        double raw = climate.sample_humidity(static_cast<double>(x), static_cast<double>(z));
        return clamp01(static_cast<float>((raw + 1.0) * 0.5));
    }

    float sample_erosion(float x, float z) const {
        double raw = climate.sample_erosion(static_cast<double>(x), static_cast<double>(z));
        return clamp01(static_cast<float>((raw + 1.0) * 0.5));
    }

    float sample_weirdness(float x, float z) const {
        double raw = climate.sample_weirdness(static_cast<double>(x), static_cast<double>(z));
        return clamp01(static_cast<float>((raw + 1.0) * 0.5));
    }

    // Grid-based land biome lookup from temperature/humidity.
    //
    // Thresholds were chosen empirically (not guessed) by sampling this exact
    // noise implementation across a wide area and measuring its real
    // distribution: single-octave value/gradient noise here comes out roughly
    // bell-curved around 0.5 with stddev ~0.15, NOT uniform across [0,1].
    // A nearest-center Voronoi pick (the old approach) with an off-center
    // biome point therefore starves that biome almost entirely, because the
    // point only "wins" in a rarely-sampled tail of the distribution.
    //
    // Using tertile thresholds instead (measured ~0.43 / ~0.57 splits the
    // sampled data into even thirds) guarantees each temperature/humidity
    // bin gets a fair, predictable share of land regardless of biome_size,
    // since biome_size only rescales noise frequency, not its distribution.
    static constexpr float TEMP_COLD_MAX  = 0.43f;
    static constexpr float TEMP_HOT_MIN   = 0.57f;
    static constexpr float HUM_DRY_MAX    = 0.43f;
    static constexpr float HUM_HUMID_MIN  = 0.57f;

    static BiomeType land_biome_from_grid(float temperature, float humidity, float jaggedness) {
        bool hot  = temperature >= TEMP_HOT_MIN;
        bool dry  = humidity < HUM_DRY_MAX;
        bool mountain = jaggedness > 0.001f;

        if (hot) {
            if (dry) return mountain ? BiomeType::DesertMountain : BiomeType::Desert;
            return mountain ? BiomeType::ForestMountain : BiomeType::Forest;
        }
        // cold or temperate
        if (dry) return mountain ? BiomeType::PlainsMountain : BiomeType::Plains;
        return mountain ? BiomeType::ForestMountain : BiomeType::Forest;
    }

    BiomeType biome_from_climate(float temperature, float humidity, float cont,
                                  float raw_c, float raw_e, float raw_w) const {
        float beach_t = smoothstep(params.land_threshold, params.land_threshold + params.beach_width, cont);
        if (beach_t < 0.9f && cont >= params.land_threshold - params.beach_width) {
            return BiomeType::Beach;
        }
        if (cont < params.land_threshold) {
            // Split ocean by depth based on how far cont is below threshold
            float cont_from_coast = params.land_threshold - cont;
            if (cont_from_coast <= 0.05f)
                return BiomeType::ShallowOcean;
            else if (cont_from_coast <= 0.17f)
                return BiomeType::DeepOcean;
            else
                return BiomeType::AbyssalTrench;
        }
        float jaggedness = compute_jaggedness(raw_c, raw_e, raw_w, spline_root_jaggedness_);
        return land_biome_from_grid(temperature, humidity, jaggedness);
    }

    // Spline-based terrain height.  Maps raw continentalness/erosion/weirdness
    // through the vanilla offset/factor/jaggedness spline DAG, then adds a
    // light fbm detail layer for surface roughness.
    //
    // The spline output (roughly [-0.22, +0.15]) is scaled to block units via
    // TERRAIN_HEIGHT_SCALE so that deep-ocean floors sit well below sea level
    // and mountain peaks rise comfortably above it.
    float sample_land_shape(float raw_c, float raw_e, float raw_w, float x, float z) const {
        static constexpr float TERRAIN_HEIGHT_SCALE = 150.0f;
        static constexpr float FACTOR_NORM = 3.5f;

        float offset = compute_terrain_offset(raw_c, raw_e, raw_w, spline_root_);
        float factor_val = compute_factor(raw_c, raw_e, raw_w, spline_root_factor_);
        float effective_offset = offset * (factor_val / FACTOR_NORM);
        float base = params.sea_level + effective_offset * TERRAIN_HEIGHT_SCALE;

        // Surface roughness — higher-frequency fbm on top of the spline shape.
        // This stands in for the 3D density noise that provides surface detail in vanilla.
        float detail = terrain_noise.fbm(x, z, 3, 0.50f, 0.008f) * 5.0f;

        // Jaggedness term — modulated by the jaggedness spline, which is near 0 for
        // flat biomes (high erosion) and up to ~0.63 for jagged mountain biomes
        // (low erosion, specific weirdness).  The noise frequency here is higher
        // than the base detail to create actual visible crags, not smooth swells.
        float jagged_val = compute_jaggedness(raw_c, raw_e, raw_w, spline_root_jaggedness_);
        float jagged_noise = terrain_noise.fbm(x, z, 3, 0.50f, 0.025f);
        float jagged_term = jagged_val * jagged_noise * 12.0f;

        float height = base + detail + jagged_term;
        return std::clamp(height, params.sea_level - 40.0f, params.sea_level + 180.0f);
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
        , climate(static_cast<uint64_t>(p.seed),
                  static_cast<double>(p.continentalness_scale),
                  static_cast<double>(p.climate_temp_scale),
                  static_cast<double>(p.climate_humidity_scale))
        , params(p)
        , rng(p.seed)
    {
        spline_root_ = init_terrain_spline(spline_stack_);
        spline_root_factor_ = init_factor_spline(spline_stack_factor_);
        spline_root_jaggedness_ = init_jaggedness_spline(spline_stack_jaggedness_);
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
        auto wc = climate.warp(x, z);
        double raw_c = climate.sample_continentalness(wc);
        double raw_e = climate.sample_erosion(wc);
        double raw_w = climate.sample_weirdness(wc);
        return sample_land_shape(static_cast<float>(raw_c),
                                 static_cast<float>(raw_e),
                                 static_cast<float>(raw_w), x, z);
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

    // -------------------------------------------------------------------------
    // Per-column data used during chunk generation (replaces 7 separate arrays)
    // -------------------------------------------------------------------------
    struct ChunkColumn {
        int32_t height = 0;
        BiomeType biome = BiomeType::Plains;
        int32_t water_level = -1;
        bool near_water = false;
        float temperature = 0.0f;
        float humidity = 0.0f;
    };

    // Cross-chunk block writer callback type
    using CrossChunkWriter = std::function<void(int32_t, int32_t, int32_t, BlockID)>;

    // -------------------------------------------------------------------------
    // Main generation entry point
    // -------------------------------------------------------------------------
    void generate_chunk(ChunkData& chunk, int32_t chunk_x, int32_t chunk_y, int32_t chunk_z,
                        const CrossChunkWriter& cross_writer = nullptr, bool vegetation_enabled = true);

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
        bool scale_changed = (p.continentalness_scale != params.continentalness_scale ||
                              p.climate_temp_scale != params.climate_temp_scale ||
                              p.climate_humidity_scale != params.climate_humidity_scale);
        params = p;
        if (seed_changed) {
            terrain_noise = FastNoise(p.seed);
            cave_noise    = FastNoise(p.seed + 2000);
            rng.seed(p.seed);
        }
        if (seed_changed || scale_changed) {
            climate = ClimateSampler(static_cast<uint64_t>(p.seed),
                                     static_cast<double>(p.continentalness_scale),
                                     static_cast<double>(p.climate_temp_scale),
                                     static_cast<double>(p.climate_humidity_scale));
        }
        if (seed_changed) {
            spline_root_  = init_terrain_spline(spline_stack_);
            spline_root_factor_ = init_factor_spline(spline_stack_factor_);
            spline_root_jaggedness_ = init_jaggedness_spline(spline_stack_jaggedness_);
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