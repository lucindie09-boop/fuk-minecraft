#include "worldgen/chunk_generator.hpp"
#include "core/chunk_data.hpp"

namespace VoxelEngine {

PerformanceTimer ChunkGenerator::perf_timer;

ChunkGenerator::ColumnSample ChunkGenerator::sample_column(int32_t world_x, int32_t world_z) const {
    float x = static_cast<float>(world_x);
    float z = static_cast<float>(world_z);

    float cont = sample_continentalness(x, z);
    bool is_land = cont >= params.land_threshold;
    float cached_lake_raw = 0.0f;
    float cached_lake_water_level = -1.0f;
    float coast_width = std::max(0.0001f, params.shelf_width * 0.4f);

    float height = 0.0f;
    float water_level = -1.0f;
    BiomeType biome = BiomeType::Land;
    float saved_land_height = 0.0f;

    if (is_land) {
        float land_height = sample_land_shape(x, z);
        float coast_t = smoothstep(params.ocean_threshold, params.ocean_threshold + coast_width, cont);
        land_height = lerp(params.sea_level, land_height, coast_t);
        saved_land_height = land_height;
        height = land_height;

        // Lake evaluation (noise-based placement)
        float lake_raw = lake_noise.noise_2d(x * params.lake_noise_scale, z * params.lake_noise_scale);
        lake_raw = clamp01((lake_raw + 1.0f) * 0.5f);
        float lake_detail = lake_noise.noise_2d(x * params.lake_detail_scale + 500.0f,
                                                z * params.lake_detail_scale - 500.0f);
        lake_detail = clamp01((lake_detail + 1.0f) * 0.5f);
        float lake_third = lake_noise.noise_2d(x * params.lake_detail_scale * 3.0f + 1000.0f,
                                               z * params.lake_detail_scale * 3.0f - 1000.0f);
        lake_third = clamp01((lake_third + 1.0f) * 0.5f);
        lake_raw = lake_raw * 0.85f + lake_detail * 0.12f + lake_third * 0.03f;
        cached_lake_raw = lake_raw;

        float lake_mask = smoothstep(params.lake_threshold, 0.88f, lake_raw);
        lake_mask *= smoothstep(0.55f, 0.65f, cont);

        if (lake_mask > 0.01f) {
            float height_noise = terrain_noise.noise_2d(x * params.lake_height_variation_scale,
                                                        z * params.lake_height_variation_scale);
            height_noise = clamp01((height_noise + 1.0f) * 0.5f);
            float lake_water_level = params.sea_level + params.lake_min_height_above_sea +
                                   height_noise * (params.lake_max_height_above_sea - params.lake_min_height_above_sea);
            cached_lake_water_level = lake_water_level;

            float lake_bed = lake_water_level - 4.0f;
            float bed_noise = terrain_noise.noise_2d(x * 0.012f, z * 0.012f) * 1.5f;
            lake_bed += bed_noise;
            lake_bed = std::min(lake_bed, land_height - 1.0f);
            lake_bed = std::max(lake_bed, lake_water_level - params.lake_depth);

            height = lake_bed;
            water_level = lake_water_level;
            biome = BiomeType::Lake;
        } else {
            // Beach
            float beach_t = smoothstep(params.land_threshold, params.land_threshold + params.beach_width, cont);
            if (beach_t < 0.9f && cont >= params.land_threshold - 0.05f) {
                biome = BiomeType::Beach;
            }
        }
    } else {
        // Simple ocean: shelf→slope→deep
        float cont_from_coast = params.land_threshold - cont;
        float depth = cont_from_coast <= 0.05f
            ? lerp(1.0f, params.shelf_depth, cont_from_coast / 0.05f)
            : lerp(params.shelf_depth, params.deep_ocean_depth, (cont_from_coast - 0.05f) / 0.12f);
        depth = std::min(params.deep_ocean_depth, depth);
        height = params.sea_level - depth;

        float bed_noise = terrain_noise.noise_2d(x * 0.002f, z * 0.002f) * 2.0f;
        height += bed_noise;
        height = std::min(height, params.sea_level - 1.0f);
        water_level = params.sea_level;

        float beach_t = smoothstep(params.land_threshold, params.land_threshold + params.beach_width, cont);
        biome = (beach_t < 0.9f && cont >= params.land_threshold - 0.05f) ? BiomeType::Beach : BiomeType::Ocean;
    }

    height = std::max(params.bedrock_height + 1.0f, height);
    if (water_level >= 0.0f) {
        if (biome == BiomeType::Lake) {
            water_level = std::max(0.0f, water_level);
        } else {
            water_level = std::max(params.sea_level, water_level);
        }
    }

    return ColumnSample{biome, height, water_level, false, saved_land_height, cached_lake_raw, cont, cached_lake_water_level};
}

BlockID ChunkGenerator::get_surface_block(BiomeType biome, int32_t y, bool has_surface_water, bool near_water) const {
    if (has_surface_water) {
        if (biome == BiomeType::Ocean) return BlockIDs::SAND;
        if (biome == BiomeType::Lake) return BlockIDs::SAND;
        return BlockIDs::DIRT;
    }
    switch (biome) {
        case BiomeType::Ocean:  return BlockIDs::SAND;
        case BiomeType::Lake:   return BlockIDs::SAND;
        case BiomeType::Beach:  return near_water ? BlockIDs::WET_SAND : BlockIDs::SAND;
        default:                return near_water ? BlockIDs::MUD : (y > params.sea_level ? BlockIDs::GRASS : BlockIDs::DIRT);
    }
}

BlockID ChunkGenerator::get_subsurface_block(BiomeType biome, bool near_water) const {
    switch (biome) {
        case BiomeType::Ocean:
        case BiomeType::Lake:   return BlockIDs::SAND;
        case BiomeType::Beach:  return near_water ? BlockIDs::WET_SAND_FULL : BlockIDs::SAND;
        default:                return BlockIDs::DIRT;
    }
}

// -------------------------------------------------------------------------
// Fast chunk content estimation (for surface-aware generation)
// -------------------------------------------------------------------------
ChunkGenerator::HeightRange ChunkGenerator::get_chunk_height_range(int32_t chunk_x, int32_t chunk_z) const {
    int32_t wx_start = chunk_x * CHUNK_WIDTH;
    int32_t wz_start = chunk_z * CHUNK_DEPTH;
    float min_h = 10000.0f;
    float max_h = -10000.0f;
    // Sample corners and center for a good estimate
    for (int32_t x : {0, CHUNK_WIDTH - 1}) {
        for (int32_t z : {0, CHUNK_DEPTH - 1}) {
            float h = get_terrain_height(wx_start + x, wz_start + z);
            min_h = std::min(min_h, h);
            max_h = std::max(max_h, h);
        }
    }
    // Center sample
    float center_h = get_terrain_height(wx_start + CHUNK_WIDTH / 2, wz_start + CHUNK_DEPTH / 2);
    min_h = std::min(min_h, center_h);
    max_h = std::max(max_h, center_h);
    return HeightRange{min_h, max_h};
}

BlockID ChunkGenerator::get_chunk_subsurface_block(int32_t chunk_x, int32_t chunk_z) const {
    int32_t wx = chunk_x * CHUNK_WIDTH;
    int32_t wz = chunk_z * CHUNK_DEPTH;
    // Sample center column for biome
    ColumnSample col = sample_column(wx + CHUNK_WIDTH / 2, wz + CHUNK_DEPTH / 2);
    return get_subsurface_block(col.biome, false);
}

void ChunkGenerator::generate_chunk(ChunkData& chunk, int32_t chunk_x, int32_t chunk_y, int32_t chunk_z) {
    ScopedTimer timer(perf_timer, TimerID::GenerateChunk);
    chunk.clear();

    int32_t world_x_start = chunk_x * CHUNK_WIDTH;
    int32_t world_y_start = chunk_y * CHUNK_HEIGHT;
    int32_t world_z_start = chunk_z * CHUNK_DEPTH;

    // Single struct-of-arrays for all per-column data (replaces 7 separate stack arrays).
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

    // 2-pass scanline Manhattan distance transform for near_water detection.
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

    // Combined lake fixup + bank blend pass
    for (int32_t x = 0; x < CHUNK_WIDTH; x++) {
        for (int32_t z = 0; z < CHUNK_DEPTH; z++) {
            float world_x = static_cast<float>(world_x_start + x);
            float world_z = static_cast<float>(world_z_start + z);

            // Lake fixup
            if (columns[x][z].biome == BiomeType::Lake) {
                float height_noise = terrain_noise.noise_2d(world_x * params.lake_height_variation_scale * 0.02f,
                                                            world_z * params.lake_height_variation_scale * 0.02f);
                height_noise = clamp01((height_noise + 1.0f) * 0.5f);
                float lake_water_level = params.sea_level + params.lake_min_height_above_sea +
                                   height_noise * (params.lake_max_height_above_sea - params.lake_min_height_above_sea);
                columns[x][z].water_level = static_cast<int32_t>(std::round(lake_water_level));
                float lake_bed = static_cast<float>(columns[x][z].water_level) - 4.0f;
                float bed_noise = terrain_noise.noise_2d(world_x * 0.012f, world_z * 0.012f) * 1.5f;
                lake_bed += bed_noise;
                lake_bed = std::min(lake_bed, static_cast<float>(columns[x][z].height) - 1.0f);
                lake_bed = std::max(lake_bed, static_cast<float>(columns[x][z].water_level) - params.lake_depth);
                columns[x][z].height = static_cast<int32_t>(std::round(lake_bed));
            }

            // Bank blend (only if not a lake)
            if (columns[x][z].biome != BiomeType::Lake) {
                bool is_water = columns[x][z].water_level > columns[x][z].height;
                if (!is_water) {
                    int32_t h = columns[x][z].height;
                    float height_noise = terrain_noise.noise_2d(world_x * params.lake_height_variation_scale * 0.02f,
                                                                 world_z * params.lake_height_variation_scale * 0.02f);
                    height_noise = clamp01((height_noise + 1.0f) * 0.5f);
                    float lake_water_level = params.sea_level + params.lake_min_height_above_sea +
                                       height_noise * (params.lake_max_height_above_sea - params.lake_min_height_above_sea);
                    int32_t local_bank_target = static_cast<int32_t>(std::round(lake_water_level));
                    float lr = lake_noise.noise_2d(world_x * params.lake_noise_scale, world_z * params.lake_noise_scale);
                    lr = clamp01((lr + 1.0f) * 0.5f);
                    float ld = lake_noise.noise_2d(world_x * params.lake_detail_scale + 500.0f, world_z * params.lake_detail_scale - 500.0f);
                    ld = clamp01((ld + 1.0f) * 0.5f);
                    float lt = lake_noise.noise_2d(world_x * params.lake_detail_scale * 3.0f + 1000.0f, world_z * params.lake_detail_scale * 3.0f - 1000.0f);
                    lt = clamp01((lt + 1.0f) * 0.5f);
                    lr = lr * 0.85f + ld * 0.12f + lt * 0.03f;
                    float cont = sample_continentalness(world_x, world_z);
                    float water_t = 0.0589f;
                    float water_start_lr = params.lake_threshold + water_t * (0.88f - params.lake_threshold);
                    float noise_gradient = 31.0f * params.lake_noise_scale;
                    float lake_bank = smoothstep(water_start_lr - noise_gradient, water_start_lr, lr);
                    lake_bank *= smoothstep(0.55f, 0.56f, cont);
                    if (lake_bank > 0.01f) {
                        float hf = static_cast<float>(h);
                        columns[x][z].height = static_cast<int32_t>(std::round(hf + (static_cast<float>(local_bank_target) - hf) * lake_bank));
                    }
                }
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

            // Bedrock occupies world y in [0, bed)
            int32_t bedrock_overlap_start = std::max(0, world_y_start);
            int32_t bedrock_overlap_end   = std::min(bed, world_y_end);
            if (bedrock_overlap_start < bedrock_overlap_end) {
                for (int32_t local_y = bedrock_overlap_start - world_y_start; local_y < bedrock_overlap_end - world_y_start; local_y++) {
                    chunk.set_block(x, local_y, z, BlockIDs::BEDROCK);
                }
            }

            // Subsurface
            int32_t subsurface_start = std::max(bed, world_y_start);
            int32_t subsurface_end   = std::min(surface_y, world_y_end);
            if (subsurface_start < subsurface_end) {
                for (int32_t local_y = subsurface_start - world_y_start; local_y < subsurface_end - world_y_start; local_y++) {
                    chunk.set_block(x, local_y, z, subsurface_block);
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
                case BiomeType::Ocean: byte = 30;   break;
                case BiomeType::Land:  byte = 200; break;
                case BiomeType::Lake:  byte = 120; break;
                case BiomeType::Beach: byte = 220; break;
                default:              byte = 255; break;
            }
            fwrite(&byte, 1, 1, f);
        }
    }
    fclose(f);
}

} // namespace VoxelEngine
