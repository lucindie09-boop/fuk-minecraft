#include "worldgen/chunk_generator.hpp"
#include "core/chunk_data.hpp"

namespace VoxelEngine {

PerformanceTimer ChunkGenerator::perf_timer;

// -------------------------------------------------------------------------
// Height functions
// -------------------------------------------------------------------------

float ChunkGenerator::compute_ocean_floor(float x, float z, float cont) const {
    float cont_from_coast = params.land_threshold - cont;
    float depth;
    if (cont_from_coast <= 0.05f) {
        depth = lerp(1.0f, params.shelf_depth, cont_from_coast / 0.05f);
    } else {
        depth = lerp(params.shelf_depth, params.deep_ocean_depth, (cont_from_coast - 0.05f) / 0.12f);
    }
    depth = std::min(params.deep_ocean_depth, depth);
    float height = params.sea_level - depth;
    float bed_noise = terrain_noise.noise_2d(x * 0.002f, z * 0.002f) * 2.0f;
    height += bed_noise;
    height = std::min(height, params.sea_level - 1.0f);
    return std::max(params.bedrock_height + 1.0f, height);
}

float ChunkGenerator::compute_standard_height(float x, float z, float base_offset,
                                                float height_scale, float freq_base) const {
    float freq = 0.0016f * freq_base;
    float broad  = terrain_noise.fbm(x, z, 4, 0.52f, freq);
    float medium = terrain_noise.fbm(x + 2000.0f, z - 2000.0f, 4, 0.55f, freq * 3.0f);
    float fine   = terrain_noise.noise_2d(x * freq * 7.5f, z * freq * 7.5f);
    float micro  = terrain_noise.noise_2d((x - 9000.0f) * freq * 13.75f, (z + 9000.0f) * freq * 13.75f);

    return params.sea_level + base_offset
         + broad  * height_scale * 0.28f
         + medium * height_scale * 0.16f
         + fine   * height_scale * 0.07f
         + micro  * height_scale * 0.03f;
}

float ChunkGenerator::compute_mountain_height(float x, float z, float erosion) const {
    float ridged = terrain_noise.ridged_noise(x, z, 5, 0.6f, 0.0006f);
    float detail = terrain_noise.fbm(x + 5000.0f, z + 5000.0f, 3, 0.5f, 0.003f);
    float scale = 48.0f + erosion * 32.0f;
    return params.sea_level + 16.0f + ridged * scale + detail * 12.0f;
}

float ChunkGenerator::compute_desert_height(float x, float z) const {
    float base = compute_standard_height(x, z, 2.0f, 6.0f, 1.5f);
    float dune = terrain_noise.noise_2d(x * 0.008f, z * 0.008f) * 3.0f;
    return base + std::max(0.0f, dune);
}

float ChunkGenerator::compute_biome_height(float x, float z, BiomeType biome,
                                             float temp, float hum, float erosion) const {
    switch (biome) {
        case BiomeType::Mountains: return compute_mountain_height(x, z, erosion);
        case BiomeType::Desert:    return compute_desert_height(x, z);
        case BiomeType::Plains:    return compute_standard_height(x, z, 4.0f, 10.0f, 1.0f);
        case BiomeType::Forest:    return compute_standard_height(x, z, 6.0f, 14.0f, 1.0f);
        case BiomeType::Taiga:     return compute_standard_height(x, z, 8.0f, 20.0f, 0.8f);
        case BiomeType::Tundra:    return compute_standard_height(x, z, 2.0f, 4.0f, 0.7f);
        case BiomeType::Savanna:   return compute_standard_height(x, z, 4.0f, 10.0f, 1.0f);
        case BiomeType::Jungle:    return compute_standard_height(x, z, 8.0f, 24.0f, 1.2f);
        case BiomeType::Swamp:     return compute_standard_height(x, z, 0.0f, 4.0f, 1.3f);
        case BiomeType::Beach:     return compute_standard_height(x, z, 2.0f, 3.0f, 1.0f);
        default:                   return compute_standard_height(x, z, 4.0f, 10.0f, 1.0f);
    }
}

// -------------------------------------------------------------------------
// Per-column terrain evaluation
// -------------------------------------------------------------------------
ChunkGenerator::ColumnSample ChunkGenerator::sample_column(int32_t world_x, int32_t world_z) const {
    float x = static_cast<float>(world_x);
    float z = static_cast<float>(world_z);

    float cont = sample_continentalness(x, z);
    float coast_width = std::max(0.0001f, params.shelf_width * 0.4f);

    // Ocean
    if (cont < params.ocean_threshold) {
        float height = compute_ocean_floor(x, z, cont);
        BiomeType biome = BiomeType::Ocean;
        float beach_t = smoothstep(params.land_threshold, params.land_threshold + params.beach_width, cont);
        if (beach_t < 0.9f && cont >= params.land_threshold - 0.05f) {
            biome = BiomeType::Beach;
        }
        return {biome, height, params.sea_level, false, cont, 0.5f, 0.5f};
    }

    // Climate
    float temp = sample_temperature(x, z);
    float hum  = sample_humidity(x, z);
    float erosion = sample_erosion(x, z);

    // Biome selection: mountains by erosion, else climate
    BiomeType biome;
    if (erosion > params.mountain_erosion_threshold) {
        biome = BiomeType::Mountains;
    } else {
        biome = classify_biome(temp, hum);
    }

    // Beach overrides coastal land
    float beach_t = smoothstep(params.land_threshold, params.land_threshold + params.beach_width, cont);
    if (beach_t < 0.9f && cont >= params.land_threshold - 0.05f) {
        biome = BiomeType::Beach;
    }

    float height = compute_biome_height(x, z, biome, temp, hum, erosion);

    // Blend height down near coast
    float coast_t = smoothstep(params.ocean_threshold, params.ocean_threshold + coast_width, cont);
    height = lerp(params.sea_level, height, coast_t);

    height = std::max(params.bedrock_height + 1.0f, height);

    return {biome, height, -1.0f, false, cont, temp, hum};
}

// -------------------------------------------------------------------------
// Block selection
// -------------------------------------------------------------------------
BlockID ChunkGenerator::get_surface_block(BiomeType biome, int32_t y, bool has_surface_water, bool near_water) const {
    if (has_surface_water) {
        return BlockIDs::SAND;
    }
    switch (biome) {
        case BiomeType::Ocean:  return BlockIDs::SAND;
        case BiomeType::Beach:  return near_water ? BlockIDs::WET_SAND : BlockIDs::SAND;
        case BiomeType::Plains:
            return (y >= static_cast<int32_t>(params.snow_line_y))
                ? BlockIDs::SNOW
                : (near_water ? BlockIDs::MUD : BlockIDs::GRASS);
        case BiomeType::Forest:
            return (y >= static_cast<int32_t>(params.snow_line_y)) ? BlockIDs::SNOW : BlockIDs::GRASS;
        case BiomeType::Desert: return BlockIDs::SAND;
        case BiomeType::Taiga:
            return (y >= static_cast<int32_t>(params.snow_line_y)) ? BlockIDs::SNOW : BlockIDs::GRASS;
        case BiomeType::Tundra: return BlockIDs::SNOW;
        case BiomeType::Savanna:
            return (y >= static_cast<int32_t>(params.snow_line_y)) ? BlockIDs::SNOW : BlockIDs::GRASS;
        case BiomeType::Jungle:
            return (y >= static_cast<int32_t>(params.snow_line_y)) ? BlockIDs::SNOW : BlockIDs::GRASS;
        case BiomeType::Swamp:  return near_water ? BlockIDs::MUD : BlockIDs::GRASS;
        case BiomeType::Mountains: {
            if (y >= static_cast<int32_t>(params.snow_line_y)) return BlockIDs::SNOW;
            if (y >= static_cast<int32_t>(params.tree_line_y)) return BlockIDs::STONE;
            return BlockIDs::GRASS;
        }
        default: return BlockIDs::GRASS;
    }
}

BlockID ChunkGenerator::get_subsurface_block(BiomeType biome, bool near_water) const {
    switch (biome) {
        case BiomeType::Ocean:     return BlockIDs::SAND;
        case BiomeType::Beach:     return near_water ? BlockIDs::WET_SAND_FULL : BlockIDs::SAND;
        case BiomeType::Desert:    return BlockIDs::SANDSTONE;
        case BiomeType::Tundra:    return BlockIDs::GRAVEL;
        case BiomeType::Swamp:     return BlockIDs::MUD_FULL;
        case BiomeType::Mountains: return BlockIDs::STONE;
        default:                   return BlockIDs::DIRT;
    }
}

// -------------------------------------------------------------------------
// Fast chunk content estimation
// -------------------------------------------------------------------------
ChunkGenerator::HeightRange ChunkGenerator::get_chunk_height_range(int32_t chunk_x, int32_t chunk_z) const {
    int32_t wx_start = chunk_x * CHUNK_WIDTH;
    int32_t wz_start = chunk_z * CHUNK_DEPTH;
    float min_h = 10000.0f;
    float max_h = -10000.0f;
    float max_water_h = -1.0f;
    for (int32_t x : {0, CHUNK_WIDTH - 1}) {
        for (int32_t z : {0, CHUNK_DEPTH - 1}) {
            ColumnSample col = sample_column(wx_start + x, wz_start + z);
            min_h = std::min(min_h, col.height);
            max_h = std::max(max_h, col.height);
            if (col.water_level > max_water_h) max_water_h = col.water_level;
        }
    }
    ColumnSample center = sample_column(wx_start + CHUNK_WIDTH / 2, wz_start + CHUNK_DEPTH / 2);
    min_h = std::min(min_h, center.height);
    max_h = std::max(max_h, center.height);
    if (center.water_level > max_water_h) max_water_h = center.water_level;
    return HeightRange{min_h, max_h, max_water_h};
}

BlockID ChunkGenerator::get_chunk_subsurface_block(int32_t chunk_x, int32_t chunk_z) const {
    int32_t wx = chunk_x * CHUNK_WIDTH;
    int32_t wz = chunk_z * CHUNK_DEPTH;
    ColumnSample col = sample_column(wx + CHUNK_WIDTH / 2, wz + CHUNK_DEPTH / 2);
    return get_subsurface_block(col.biome, false);
}

// -------------------------------------------------------------------------
// Main generation entry point
// -------------------------------------------------------------------------
void ChunkGenerator::generate_chunk(ChunkData& chunk, int32_t chunk_x, int32_t chunk_y, int32_t chunk_z) {
    ScopedTimer timer(perf_timer, TimerID::GenerateChunk);
    chunk.clear();

    int32_t world_x_start = chunk_x * CHUNK_WIDTH;
    int32_t world_y_start = chunk_y * CHUNK_HEIGHT;
    int32_t world_z_start = chunk_z * CHUNK_DEPTH;

    ChunkColumn columns[CHUNK_WIDTH][CHUNK_DEPTH];

    for (int32_t x = 0; x < CHUNK_WIDTH; x++) {
        for (int32_t z = 0; z < CHUNK_DEPTH; z++) {
            ColumnSample col = sample_column(world_x_start + x, world_z_start + z);
            columns[x][z].biome       = col.biome;
            columns[x][z].height      = static_cast<int32_t>(std::round(col.height));
            columns[x][z].water_level = col.water_level >= 0.0f
                ? static_cast<int32_t>(std::round(col.water_level))
                : -1;
        }
    }

    // 2-pass Manhattan distance transform for near_water detection
    constexpr int32_t INF_DIST = 999;
    int32_t dist[CHUNK_WIDTH][CHUNK_DEPTH];

    for (int32_t x = 0; x < CHUNK_WIDTH; x++) {
        for (int32_t z = 0; z < CHUNK_DEPTH; z++) {
            dist[x][z] = (columns[x][z].water_level > columns[x][z].height) ? 0 : INF_DIST;
        }
    }

    for (int32_t x = 0; x < CHUNK_WIDTH; x++) {
        for (int32_t z = 0; z < CHUNK_DEPTH; z++) {
            if (dist[x][z] == 0) continue;
            int32_t best = dist[x][z];
            if (x > 0)             best = std::min(best, dist[x-1][z] + 1);
            if (z > 0)             best = std::min(best, dist[x][z-1] + 1);
            dist[x][z] = best;
        }
    }
    for (int32_t x = CHUNK_WIDTH - 1; x >= 0; x--) {
        for (int32_t z = CHUNK_DEPTH - 1; z >= 0; z--) {
            if (dist[x][z] == 0) continue;
            int32_t best = dist[x][z];
            if (x < CHUNK_WIDTH - 1)  best = std::min(best, dist[x+1][z] + 1);
            if (z < CHUNK_DEPTH - 1)  best = std::min(best, dist[x][z+1] + 1);
            dist[x][z] = best;

            if (dist[x][z] > 0 && dist[x][z] <= 3) {
                columns[x][z].near_water = true;
            }
        }
    }

    // Voxelize columns into this chunk's local Y range
    for (int32_t x = 0; x < CHUNK_WIDTH; x++) {
        for (int32_t z = 0; z < CHUNK_DEPTH; z++) {
            int32_t surface_y = columns[x][z].height;
            BiomeType biome = columns[x][z].biome;
            bool near_water = columns[x][z].near_water;
            int32_t water_top = columns[x][z].water_level;
            bool has_surface_water = water_top > surface_y;

            BlockID surface_block    = get_surface_block(biome, surface_y, has_surface_water, near_water);
            BlockID subsurface_block = get_subsurface_block(biome, near_water);
            int32_t bed = params.bedrock_height;

            int32_t world_y_end = world_y_start + CHUNK_HEIGHT;

            // Bedrock
            int32_t bedrock_overlap_start = std::max(0, world_y_start);
            int32_t bedrock_overlap_end   = std::min(bed, world_y_end);
            if (bedrock_overlap_start < bedrock_overlap_end) {
                for (int32_t local_y = bedrock_overlap_start - world_y_start; local_y < bedrock_overlap_end - world_y_start; local_y++) {
                    chunk.set_block(x, local_y, z, BlockIDs::BEDROCK);
                }
            }

            // Subsurface cover
            int32_t stone_top = std::max(bed, surface_y - params.subsurface_cover_depth);
            int32_t cover_start = std::max(stone_top, world_y_start);
            int32_t cover_end = std::min(surface_y, world_y_end);
            if (cover_start < cover_end) {
                for (int32_t local_y = cover_start - world_y_start; local_y < cover_end - world_y_start; local_y++) {
                    chunk.set_block(x, local_y, z, subsurface_block);
                }
            }

            // Stone layer
            int32_t stone_start = std::max(bed, world_y_start);
            int32_t stone_end = std::min(stone_top, world_y_end);
            if (stone_start < stone_end) {
                for (int32_t local_y = stone_start - world_y_start; local_y < stone_end - world_y_start; local_y++) {
                    chunk.set_block(x, local_y, z, BlockIDs::STONE);
                }
            }

            // Surface block
            if (surface_y >= world_y_start && surface_y < world_y_end) {
                int32_t local_y = surface_y - world_y_start;
                chunk.set_block(x, local_y, z, surface_block);
            }

            // Water
            if (water_top > surface_y) {
                int32_t water_start = std::max(surface_y + 1, world_y_start);
                int32_t water_end   = std::min(water_top + 1, world_y_end);
                if (water_start < water_end) {
                    for (int32_t local_y = water_start - world_y_start; local_y < water_end - world_y_start; local_y++) {
                        int32_t world_y = local_y + world_y_start;
                        BlockID water_block = (world_y == water_top) ? BlockIDs::SURFACE_WATER : BlockIDs::WATER;
                        chunk.set_block(x, local_y, z, water_block);
                    }
                }
            }
        }
    }

    chunk.compute_section_flags();
}

// -------------------------------------------------------------------------
// Debug: render continentalness PGM
// -------------------------------------------------------------------------
void ChunkGenerator::render_continentalness_pgm(const char* filename, int img_w, int img_h,
                                float world_x_start, float world_z_start,
                                float step) const {
    FILE* f = fopen(filename, "wb");
    if (!f) return;
    fprintf(f, "P5\n%d %d\n255\n", img_w, img_h);
    for (int py = 0; py < img_h; py++) {
        for (int px = 0; px < img_w; px++) {
            float wx = world_x_start + static_cast<float>(px) * step;
            float wz = world_z_start + static_cast<float>(py) * step;
            float cont = sample_continentalness(wx, wz);
            uint8_t byte = static_cast<uint8_t>(std::round(cont * 255.0f));
            fwrite(&byte, 1, 1, f);
        }
    }
    fclose(f);
}

// -------------------------------------------------------------------------
// Debug: render biome PGM with distinct grayscale values per biome
// -------------------------------------------------------------------------
void ChunkGenerator::render_biome_pgm(const char* filename, int img_w, int img_h,
                      float world_x_start, float world_z_start,
                      float step) const {
    FILE* f = fopen(filename, "wb");
    if (!f) return;
    fprintf(f, "P5\n%d %d\n255\n", img_w, img_h);
    for (int py = 0; py < img_h; py++) {
        for (int px = 0; px < img_w; px++) {
            float wx = world_x_start + static_cast<float>(px) * step;
            float wz = world_z_start + static_cast<float>(py) * step;
            ColumnSample col = sample_column(static_cast<int32_t>(wx),
                                               static_cast<int32_t>(wz));
            uint8_t byte;
            switch (col.biome) {
                case BiomeType::Ocean:     byte = 30;   break;
                case BiomeType::Beach:     byte = 220;  break;
                case BiomeType::Plains:    byte = 180;  break;
                case BiomeType::Forest:    byte = 140;  break;
                case BiomeType::Desert:    byte = 240;  break;
                case BiomeType::Taiga:     byte = 120;  break;
                case BiomeType::Tundra:    byte = 250;  break;
                case BiomeType::Savanna:   byte = 200;  break;
                case BiomeType::Jungle:    byte = 100;  break;
                case BiomeType::Swamp:     byte = 80;   break;
                case BiomeType::Mountains: byte = 60;   break;
                default:                   byte = 255;  break;
            }
            fwrite(&byte, 1, 1, f);
        }
    }
    fclose(f);
}

} // namespace VoxelEngine
