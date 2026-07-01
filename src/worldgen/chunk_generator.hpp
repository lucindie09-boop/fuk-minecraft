#ifndef FUK_MINECRAFT_CHUNK_GENERATOR_HPP
#define FUK_MINECRAFT_CHUNK_GENERATOR_HPP
#include <functional>
#include "core/terrain_params.hpp"
#include "core/noise.hpp"
#include "core/block_types.hpp"
#include "core/chunk_data.hpp"
#include "core/performance_timer.hpp"
#include "worldgen/mc_spline_data.hpp"
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
    StonePlateau,
    Tundra,
    Taiga,
    Savanna,
};

// -------------------------------------------------------------------------
// Chunk generator - Minecraft-style procedural terrain generation
// -------------------------------------------------------------------------
class ChunkGenerator {
private:
    FastNoise terrain_noise;
    FastNoise cave_noise;
    FastNoise continental_noise;
    FastNoise erosion_noise;
    FastNoise ridge_noise;
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
        float cont;              // continentalness value (0-1)
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

    static float clamp01_signed(float v) {
        return std::max(0.0f, std::min(1.0f, (v + 1.0f) * 0.5f));
    }

    static float fold_ridges(float r) {
        float a = std::fabs(-0.6666666666666666f + std::fabs(r));
        return -3.0f * (a - 0.3333333333333333f);
    }

    // -------------------------------------------------------------------------
    // Noise sampling
    // -------------------------------------------------------------------------
    float sample_continentalness(float x, float z) const {
        float raw = continental_noise.noise_2d(x * params.continentalness_scale,
                                                z * params.continentalness_scale);
        return clamp01((raw + 1.0f) * 0.5f);
    }

    // Multi-octave noise for spline evaluation (targets [-1, 1] range).
    // MC uses firstOctave=-9 with 9 octaves and xz_scale=0.25 for
    // continentalness/erosion, and firstOctave=-7 with 3 octaves for ridge.
    float raw_continentalness(float x, float z) const {
        return continental_noise.fbm(x, z, 9, 0.5f, 0.00025f);
    }
    float raw_erosion(float x, float z) const {
        return erosion_noise.fbm(x, z, 5, 0.5f, 0.00025f);
    }
    float raw_ridges(float x, float z) const {
        return ridge_noise.fbm(x, z, 3, 0.5f, 0.00080f);
    }

    float sample_temperature(float x, float z) const {
        return clamp01((temp_noise.noise_2d(x * params.climate_temp_scale,
                                             z * params.climate_temp_scale) + 1.0f) * 0.5f);
    }

    float sample_humidity(float x, float z) const {
        return clamp01((humidity_noise.noise_2d(x * params.climate_humidity_scale,
                                                 z * params.climate_humidity_scale) + 1.0f) * 0.5f);
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

    static BiomeType land_biome_from_grid(float temperature, float humidity) {
        bool cold = temperature < TEMP_COLD_MAX;
        bool hot  = temperature >= TEMP_HOT_MIN;
        bool dry   = humidity < HUM_DRY_MAX;
        bool humid = humidity >= HUM_HUMID_MIN;

        if (cold) {
            return dry ? BiomeType::Tundra : BiomeType::Taiga;
        }
        if (hot) {
            if (dry) return BiomeType::Desert;
            return BiomeType::Savanna;
        }
        // temperate
        return humid ? BiomeType::Forest : BiomeType::Plains;
    }

    BiomeType biome_from_climate(float temperature, float humidity, float cont) const {
        float beach_t = smoothstep(params.land_threshold, params.land_threshold + params.beach_width, cont);
        if (beach_t < 0.9f && cont >= params.land_threshold - params.beach_width) {
            return BiomeType::Beach;
        }
        if (cont < params.ocean_threshold) {
            float depth = params.ocean_threshold - cont;
            if (depth > 0.10f) return BiomeType::AbyssalTrench;
            if (depth > 0.05f) return BiomeType::DeepOcean;
            return BiomeType::ShallowOcean;
        }
        return land_biome_from_grid(temperature, humidity);
    }

    // Plateau/mountain gate — deliberately a LOW-FREQUENCY field, sampled
    // independently of the actual per-block terrain height. This is the
    // exact same field sample_land_shape() already uses to decide where its
    // large plateau bumps go (~3000+ block wavelength); it is reused here so
    // biome promotion and terrain shape always agree, and so promotion can
    // never be triggered by ordinary hill noise (fine/micro/close/ridge),
    // which is what was causing isolated few-block StonePlateau splotches.
    float sample_plateau_mask(float x, float z) const {
        float plt_raw = terrain_noise.fbm(x + 11000.0f, z + 7000.0f, 2, 0.5f, 0.0003f);
        return smoothstep(0.6f, 0.85f, plt_raw);
    }

    static BiomeType promote_biome_by_plateau(BiomeType biome, float plateau_mask) {
        if (plateau_mask > 0.5f) return BiomeType::StonePlateau;
        return biome;
    }

    // Minecraft-style spline-based shape computation.
    //
    // Spline output ranges (measured empirically):
    //   offset:     [-0.25, +1.49]  land typical: 0.10 ± 0.15
    //   factor:     [ 0.625, 6.30]  land typical: 5.07 ± 1.21
    //   jaggedness: [ 0.00,  0.63]  ~0 on land
    //
    // We map offset directly as a height multiplier applied to VERTICAL_SCALE,
    // centered on sea_level so offset=0 → coast height ≈ sea_level.
    float sample_land_shape(float x, float z, float /*temperature*/, float /*humidity*/) const {
        float cont   = raw_continentalness(x, z);
        float ero    = raw_erosion(x, z);
        float ridges = raw_ridges(x, z);
        float folded = fold_ridges(ridges);

        float offset_v = mc_spline::offset->compute(cont, ero, ridges, folded);
        float factor_v = mc_spline::factor->compute(cont, ero, ridges, folded);
        float jagged_v = mc_spline::jaggedness->compute(cont, ero, ridges, folded);

        // Local noise: multi-octave FBM for small-scale detail.
        float local_noise = terrain_noise.fbm(x, z, 4, 0.55f, 0.006f);

        return std::max(params.bedrock_height + 1.0f,
             params.sea_level + offset_v * 200.0f
             + local_noise * (factor_v * 1.5f + 0.5f)
             + jagged_v * 20.0f);
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
        , erosion_noise(p.seed + 7000)
        , ridge_noise(p.seed + 8000)
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
            erosion_noise     = FastNoise(p.seed + 7000);
            ridge_noise       = FastNoise(p.seed + 8000);
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