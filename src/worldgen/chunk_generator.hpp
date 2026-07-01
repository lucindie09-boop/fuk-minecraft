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

    // Continuous Voronoi-weighted blend of all land-biome height parameters.
    // Each biome gets its own noise profile (ridge for mountains, dunes for desert,
    // smooth for plains) and the results are blended smoothly across ecotones.
    // Uses fixed base-frequency climate for Voronoi weights so biome boundaries
    // in the height field remain smooth regardless of biome_size.
    float sample_land_shape(float x, float z, float /*temperature*/, float /*humidity*/) const {
        static constexpr float BASE_TEMP_SCALE = 0.00015f;
        static constexpr float BASE_HUM_SCALE  = 0.00020f;
        // One entry per land biome. base_off/scale_m shape the *height*;
        // noise_recipe is selected per-biome below so each biome reads the
        // shared noise layers with completely different weights, giving each
        // one a genuinely distinct silhouette rather than just a height shift.
        static constexpr struct { float t, h, base_off, scale_m; } centers[] = {
            {0.50f, 0.35f,   6.0f, 3.20f},   // 0 Plains     — gentle rolling
            {0.50f, 0.78f,   4.0f, 1.00f},   // 1 Forest     — temperate, humid: hilly
            {0.78f, 0.22f, -12.0f, 0.75f},   // 2 Desert     — hot, dry: dunes
            {0.22f, 0.22f, -20.0f, 0.35f},   // 3 Tundra     — cold, dry: near-flat frozen ground
            {0.22f, 0.65f,   8.0f, 1.10f},   // 4 Taiga      — cold, wetter: rugged rocky hills
            {0.78f, 0.65f,  -6.0f, 0.55f},   // 5 Savanna    — hot, humid-ish: flat with rare mesas
        };
        static constexpr int NUM_BIOMES = 6;
        // Sample climate at the base frequency for smooth height blending
        float blend_temp = clamp01((temp_noise.noise_2d(x * BASE_TEMP_SCALE, z * BASE_TEMP_SCALE) + 1.0f) * 0.5f);
        float blend_hum  = clamp01((humidity_noise.noise_2d(x * BASE_HUM_SCALE,  z * BASE_HUM_SCALE)  + 1.0f) * 0.5f);

        float w_total = 0.0f, w_base = 0.0f, w_scale = 0.0f;
        float weights[NUM_BIOMES];
        for (int i = 0; i < NUM_BIOMES; i++) {
            float dsq = (blend_temp - centers[i].t) * (blend_temp - centers[i].t)
                      + (blend_hum  - centers[i].h) * (blend_hum  - centers[i].h);
            float w = 1.0f / (dsq + 0.0001f);
            weights[i] = w;
            w_base  += w * centers[i].base_off;
            w_scale += w * centers[i].scale_m;
            w_total += w;
        }
        float base  = params.base_height + w_base / w_total;
        float scale = params.height_scale * (w_scale / w_total);

        // Shared noise layers at multiple scales
        float broad  = terrain_noise.fbm(x, z, 4, 0.52f, 0.0016f);
        float medium = terrain_noise.fbm(x + 2000.0f, z - 2000.0f, 4, 0.55f, 0.0048f);
        float fine   = terrain_noise.noise_2d(x * 0.012f, z * 0.012f);
        float micro  = terrain_noise.noise_2d((x - 9000.0f) * 0.022f, (z + 9000.0f) * 0.022f);
        float close  = terrain_noise.noise_2d(x * 0.050f, z * 0.050f); // ~20-block features

        // Ridge noise — 3 octaves for sharper terrain
        float ridge = terrain_noise.ridged_noise(x + 5000.0f, z + 3000.0f, 3, 0.55f, 0.0032f);

        // Dune noise — broad undulating ridges for desert
        float dune = std::abs(terrain_noise.fbm(x + 7000.0f, z + 5000.0f, 2, 0.50f, 0.0020f));

        // Amplitude modulation: areas where medium noise is near zero get flatter,
        // areas where it's strongly positive/negative get hillier.
        float detail_mod = std::abs(medium) * 0.6f + 0.4f;

        // Per-biome noise recipes — each biome interprets the shared noise
        // layers with its own weights, so the terrain shape is unique per
        // biome, not just an amplitude/offset tweak on one shared formula.
        float per_noise[NUM_BIOMES] = {};
        // 0 Plains — gentle rolling: broad + light detail
        per_noise[0] = broad * 0.55f + medium * 0.32f
                     + (fine * 0.10f + micro * 0.05f) * detail_mod;
        // 1 Forest — hillier: adds a touch of ridge, more fine detail
        per_noise[1] = broad * 0.20f + ridge * 0.08f + medium * 0.16f
                     + (fine * 0.22f + micro * 0.14f + close * 0.08f) * detail_mod;
        // 2 Desert — dune-dominated, fine chaos suppressed for smooth dune faces
        per_noise[2] = broad * 0.14f + dune  * 0.22f + medium * 0.12f
                     + (fine * 0.18f + micro * 0.10f + close * 0.06f) * detail_mod;
        // 3 Tundra — mostly flat frozen plains: broad only, fine detail heavily damped
        per_noise[3] = broad * 0.26f + medium * 0.08f
                     + (fine * 0.08f + micro * 0.04f) * detail_mod * 0.4f;
        // 4 Taiga — rugged/rocky: strongest ridge contribution of any land biome
        per_noise[4] = broad * 0.18f + ridge * 0.16f + medium * 0.18f
                     + (fine * 0.20f + micro * 0.12f + close * 0.08f) * detail_mod;
        // 5 Savanna — broad flat expanses with rare sharp outcrops (sparse ridge)
        per_noise[5] = broad * 0.18f + medium * 0.10f + ridge * 0.05f
                     + (fine * 0.14f + micro * 0.08f + close * 0.04f) * detail_mod * 0.7f;

        float blended = 0.0f;
        for (int i = 0; i < NUM_BIOMES; i++) blended += weights[i] * per_noise[i];
        blended /= w_total;

        // Plateau boost: discrete elevated plateaus via noise mask + ridge texture.
        // This same plateau_mask is what promote_biome_by_plateau() uses to decide
        // StonePlateau, so the visible bump and the biome label always agree.
        float pre_height = base + blended * scale;
        float plt_mask = sample_plateau_mask(x, z);
        return pre_height + plt_mask * (ridge * 60.0f + 140.0f);
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