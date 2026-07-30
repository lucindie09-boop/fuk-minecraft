#include "worldgen/chunk_generator.hpp"
#include "core/chunk_data.hpp"
#include "core/java_rng.hpp"
#include "worldgen/vegetation_generator.hpp"

extern "C" {
#include "layers.h"
}

namespace VoxelEngine {

PerformanceTimer ChunkGenerator::perf_timer;

// -------------------------------------------------------------------------
// Biome blending
// -------------------------------------------------------------------------
void ChunkGenerator::compute_biome_blend(int node_x, int node_z,
                                         const int terrain_biomes[10][10],
                                         float& out_depth, float& out_scale) const {
    float depth = 0.0f, scale = 0.0f;
    for (const auto& bw : biome_weights_) {
        int ti = node_x + 2 + bw.di;
        int tj = node_z + 2 + bw.dj;
        int biome_id = terrain_biomes[ti][tj];
        const auto& bt = get_biome_terrain(biome_id);
        depth += bw.w * bt.base_height;
        scale += bw.w * bt.height_scale;
    }
    out_depth = depth;
    out_scale = scale;
}

// -------------------------------------------------------------------------
// Column sampling for quick estimation
// -------------------------------------------------------------------------
ChunkGenerator::ColumnSample ChunkGenerator::sample_column(int32_t world_x, int32_t world_z) const {
    // Use surface biomes for the chunk containing this column.
    int chunk_x = world_x >> 5;  // CHUNK_WIDTH=32
    int chunk_z = world_z >> 5;
    int surface_biomes[16][16] = {}; // Zero-initialize
    biome_layer_.get_surface_biomes(chunk_x, chunk_z, surface_biomes);
    int lx = world_x & 31;
    int lz = world_z & 31;
    int biome_id = surface_biomes[lx][lz];

    const auto& bt = get_biome_terrain(biome_id);
    float depth = bt.base_height;
    float scale = bt.height_scale;

    // Rough height estimate: sea_level + depth * 8.5 + noise component.
    float height = params.sea_level + depth * 8.5f;
    if (scale > 0.001f) {
        height += scale * 32.0f;
    }

    float water_level = -1.0f;
    if (biome_id == Biomes::ocean || biome_id == Biomes::deep_ocean ||
        biome_id == Biomes::frozen_ocean || biome_id == Biomes::river || biome_id == Biomes::frozen_river) {
        water_level = params.sea_level;
        if (height > params.sea_level) height = params.sea_level - 1.0f;
    }

    return {biome_id, height, water_level};
}

// -------------------------------------------------------------------------
// Fast chunk content estimation
// -------------------------------------------------------------------------
ChunkGenerator::HeightRange ChunkGenerator::get_chunk_height_range(int32_t chunk_x, int32_t chunk_z) const {
    int32_t wx_start = chunk_x * CHUNK_WIDTH;
    int32_t wz_start = chunk_z * CHUNK_DEPTH;
    float min_h = 10000.0f, max_h = -10000.0f, max_water_h = -1.0f;
    for (int32_t x : {0, CHUNK_WIDTH - 1}) {
        for (int32_t z : {0, CHUNK_DEPTH - 1}) {
            auto col = sample_column(wx_start + x, wz_start + z);
            min_h = std::min(min_h, col.height);
            max_h = std::max(max_h, col.height);
            if (col.water_level > max_water_h) max_water_h = col.water_level;
        }
    }
    auto center = sample_column(wx_start + CHUNK_WIDTH / 2, wz_start + CHUNK_DEPTH / 2);
    min_h = std::min(min_h, center.height);
    max_h = std::max(max_h, center.height);
    if (center.water_level > max_water_h) max_water_h = center.water_level;
    return {min_h, max_h, max_water_h};
}

BlockID ChunkGenerator::get_chunk_subsurface_block(int32_t chunk_x, int32_t chunk_z) const {
    int32_t wx = chunk_x * CHUNK_WIDTH;
    int32_t wz = chunk_z * CHUNK_DEPTH;
    auto col = sample_column(wx + CHUNK_WIDTH / 2, wz + CHUNK_DEPTH / 2);
    return get_subsurface_block(col.biome_id, false);
}

// -------------------------------------------------------------------------
// Surface block selection (per-biome rules)
// -------------------------------------------------------------------------
BlockID ChunkGenerator::get_surface_block(int biome_id, int32_t y, bool has_surface_water) const {
    if (has_surface_water) return BlockIDs::SAND;

    if (biome_id == Biomes::ocean || biome_id == Biomes::deep_ocean || biome_id == Biomes::frozen_ocean ||
        biome_id == Biomes::river || biome_id == Biomes::frozen_river) {
        return BlockIDs::SAND;
    }

    switch (biome_id) {
        case Biomes::desert:        return BlockIDs::SAND;
        case Biomes::beach:         return BlockIDs::SAND;
        case Biomes::stoneBeach:    return BlockIDs::STONE;
        case Biomes::mesa:
        case Biomes::mesaPlateau_F:
        case Biomes::mesaPlateau:   return BlockIDs::SAND;  // red sand ideally
        case Biomes::mushroomIsland:
        case Biomes::mushroomIslandShore: return BlockIDs::GRASS;  // mycelium ideally
        case Biomes::icePlains:
        case Biomes::iceMountains:  return BlockIDs::GRASS;  // snow on top in surface builder
        default:            return BlockIDs::GRASS;
    }
}

BlockID ChunkGenerator::get_subsurface_block(int biome_id, bool aquatic) const {
    if (aquatic) return BlockIDs::SAND;
    switch (biome_id) {
        case Biomes::desert:        return BlockIDs::SAND;
        case Biomes::beach:         return BlockIDs::SAND;
        case Biomes::mesa:
        case Biomes::mesaPlateau_F:
        case Biomes::mesaPlateau:   return BlockIDs::SAND;
        case Biomes::extremeHills:
        case Biomes::extremeHillsPlus:
        case Biomes::extremeHillsEdge: return BlockIDs::STONE;
        default:            return BlockIDs::DIRT;
    }
}

// -------------------------------------------------------------------------
// Main chunk generation
// -------------------------------------------------------------------------
void ChunkGenerator::generate_chunk(ChunkData& chunk, int32_t chunk_x, int32_t chunk_y, int32_t chunk_z,
                                    const CrossChunkWriter& cross_writer, bool vegetation_enabled) {
    ScopedTimer timer(perf_timer, TimerID::GenerateChunk);
    chunk.clear();

    const int32_t world_x_start = chunk_x * CHUNK_WIDTH;
    const int32_t world_y_start = chunk_y * CHUNK_HEIGHT;
    const int32_t world_z_start = chunk_z * CHUNK_DEPTH;
    const int32_t world_y_end = world_y_start + CHUNK_HEIGHT;

    // Air chunk check: skip entirely if above reasonable terrain height.
    if (world_y_start > params.sea_level + 180.0f) {
        chunk.compute_section_flags();
        return;
    }

    // =====================================================================
    // Phase 1: Biome maps
    // =====================================================================
    int terrain_biomes[10][10];
    int surface_biomes[16][16];
    biome_layer_.get_terrain_biomes(chunk_x, chunk_z, terrain_biomes);
    biome_layer_.get_surface_biomes(chunk_x, chunk_z, surface_biomes);

    // =====================================================================
    // Phase 2: 5×5×5 density grid
    // =====================================================================
    constexpr int DENSITY_NODES = 5;
    constexpr int Y_CELLS = CHUNK_HEIGHT / 8;  // 4
    constexpr int Y_NODES = Y_CELLS + 1;       // 5
    float height_map[DENSITY_NODES][DENSITY_NODES][Y_NODES];

    // Temporary buffers for noise generation
    // scaleNoise and depthNoise: 5×5 values at density node positions
    double scale_buf[25], depth_buf[25];
    {
        // scaleNoise sampled at (x*8, z*8) within chunk, scaled by 0.2
        double sb[5][5][1];
        scaleNoise.generate_noise_3d(&sb[0][0][0],
            world_x_start / 8, 0, world_z_start / 8,
            5, 1, 5, 1.0, 1.0, 1.0);
        for (int i = 0; i < 25; ++i) scale_buf[i] = sb[i / 5][0][i % 5];

        double db[5][5][1];
        depthNoise.generate_noise_3d(&db[0][0][0],
            world_x_start / 8, 0, world_z_start / 8,
            5, 1, 5, 1.0, 1.0, 1.0);
        for (int i = 0; i < 25; ++i) depth_buf[i] = db[i / 5][0][i % 5];
    }

    // minLimit, maxLimit, main noise for all 5×5×5 nodes
    // Use generate_noise_3d which samples each octave with the correct frequencies
    double min_buf[125], max_buf[125], main_buf[125];
    {
        // Noise sampled at positions relative to chunk origin, with scales matching vanilla.
        // Vanilla signature: generateNoiseOctaves(buf, xOff, yOff, zOff, xSz, ySz, zSz, xScale, yScale, zScale)
        // where xOff = chunk_x*4, yOff = 0, zOff = chunk_z*4, xSz=5, ySz=33, zSz=5, xScale=4, yScale=8, zScale=4
        // For our 32-block chunks, we use:
        // xOff = world_x_start/8, yOff = world_y_start/8, zOff = world_z_start/8, xSz=5, ySz=5, zSz=5, xScale=8, yScale=8, zScale=8
        // Note: world_x_start/8 = chunk_x*32/8 = chunk_x*4, which advances by 4 nodes per chunk (correct for 8-block spacing)
        minLimitNoise.generate_noise_3d(min_buf,
            world_x_start / 8, world_y_start / 8, world_z_start / 8,
            5, Y_NODES, 5, 8.0, 8.0, 8.0);
        maxLimitNoise.generate_noise_3d(max_buf,
            world_x_start / 8, world_y_start / 8, world_z_start / 8,
            5, Y_NODES, 5, 8.0, 8.0, 8.0);
        mainNoise.generate_noise_3d(main_buf,
            world_x_start / 8, world_y_start / 8, world_z_start / 8,
            5, Y_NODES, 5, 8.0, 8.0, 8.0);
    }

    // Build density grid
    const float sea_level = params.sea_level;
    for (int nx = 0; nx < DENSITY_NODES; ++nx) {
        for (int nz = 0; nz < DENSITY_NODES; ++nz) {
            int ni = nx * DENSITY_NODES + nz;
            double scale_mod = scale_buf[ni] * 0.2;
            double depth_mod = depth_buf[ni] * 0.1;

            float avg_depth, avg_scale;
            compute_biome_blend(nx, nz, terrain_biomes, avg_depth, avg_scale);
            avg_depth += static_cast<float>(depth_mod);
            avg_scale += static_cast<float>(scale_mod);
            if (avg_scale < 0.001f) avg_scale = 0.001f;

            for (int ny = 0; ny < Y_NODES; ++ny) {
                int noise_idx = ny * DENSITY_NODES * DENSITY_NODES + nz * DENSITY_NODES + nx;
                double min_v = min_buf[noise_idx];
                double max_v = max_buf[noise_idx];
                double main_v = main_buf[noise_idx];

                double density = clamped_lerp(min_v / 512.0, max_v / 512.0, main_v / 10.0 + 0.5);
                double world_y = static_cast<double>(world_y_start + ny * 8);
                double height_taper = (world_y - (sea_level + avg_depth * 8.5)) * 12.0
                                    / (avg_scale * 512.0 + 0.001);
                density -= height_taper;

                height_map[nx][nz][ny] = static_cast<float>(density);
            }
        }
    }

    // =====================================================================
    // Phase 3: Trilinear interpolation → fill blocks
    // =====================================================================
    // Interpolate the 5×5×5 density grid into 32×32×32 blocks.
    // Each 8×8×8 cell between adjacent density nodes is trilinearly interpolated.
    for (int nx = 0; nx < DENSITY_NODES - 1; ++nx) {
        for (int nz = 0; nz < DENSITY_NODES - 1; ++nz) {
            for (int ny = 0; ny < Y_NODES - 1; ++ny) {
                // 8 corner density values (2 per axis)
                float d000 = height_map[nx][nz][ny];
                float d100 = height_map[nx + 1][nz][ny];
                float d010 = height_map[nx][nz + 1][ny];
                float d110 = height_map[nx + 1][nz + 1][ny];
                float d001 = height_map[nx][nz][ny + 1];
                float d101 = height_map[nx + 1][nz][ny + 1];
                float d011 = height_map[nx][nz + 1][ny + 1];
                float d111 = height_map[nx + 1][nz + 1][ny + 1];

                int bx = nx * 8, bz = nz * 8, base_y = world_y_start + ny * 8;

                for (int dx = 0; dx < 8; ++dx) {
                    float fx = static_cast<float>(dx) * (1.0f / 7.0f);
                    for (int dz = 0; dz < 8; ++dz) {
                        float fz = static_cast<float>(dz) * (1.0f / 7.0f);
                        int world_x = world_x_start + bx + dx;
                        int world_z = world_z_start + bz + dz;
                        int biome_id = surface_biomes[bx + dx][bz + dz];

                        for (int dy = 0; dy < 8; ++dy) {
                            float fy = static_cast<float>(dy) * (1.0f / 7.0f);

                            // Trilinear interpolation
                            float d0 = lerp(lerp(d000, d100, fx), lerp(d010, d110, fx), fz);
                            float d1 = lerp(lerp(d001, d101, fx), lerp(d011, d111, fx), fz);
                            float d = lerp(d0, d1, fy);

                            int world_y = base_y + dy;
                            int ly = world_y - world_y_start;

                            if (d > 0.0f) {
                                // Solid block — will be replaced by surface builder later
                                // For now, place stone (or appropriate base block)
                                if (world_y < params.bedrock_height) {
                                    chunk.set_block(bx + dx, ly, bz + dz, BlockIDs::BEDROCK);
                                } else {
                                    chunk.set_block(bx + dx, ly, bz + dz, BlockIDs::STONE);
                                }
                            } else if (world_y < sea_level) {
                                chunk.set_block(bx + dx, ly, bz + dz, BlockIDs::WATER);
                            }
                        }
                    }
                }
            }
        }
    }

    // =====================================================================
    // Phase 3b: Surface builder — replace top blocks per biome
    // =====================================================================
    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int z = 0; z < CHUNK_DEPTH; ++z) {
            int biome_id = surface_biomes[x][z];
            bool aquatic = (biome_id == Biomes::ocean || biome_id == Biomes::deep_ocean ||
                            biome_id == Biomes::frozen_ocean || biome_id == Biomes::river ||
                            biome_id == Biomes::frozen_river);

            // Find topmost solid block in this column
            int top_solid_y = -1;
            for (int wy = world_y_end - 1; wy >= world_y_start; --wy) {
                int ly = wy - world_y_start;
                BlockID b = chunk.get_block(x, ly, z);
                if (b != BlockIDs::AIR && b != BlockIDs::WATER) {
                    top_solid_y = wy;
                    break;
                }
            }

            if (top_solid_y < 0) continue;

            int top_ly = top_solid_y - world_y_start;
            int biome_for_surface = biome_id;

            // Check for snow in cold biomes
            bool snow_cover = (biome_id == Biomes::icePlains || biome_id == Biomes::iceMountains ||
                               biome_id == Biomes::frozen_ocean || biome_id == Biomes::frozen_river ||
                               biome_id == Biomes::coldTaiga || biome_id == Biomes::coldTaigaHills ||
                               biome_id == Biomes::snowy_tundra);
            // Check if underwater surface
            bool has_surface_water = (static_cast<float>(top_solid_y) < sea_level && aquatic);

            // Replace top block
            BlockID surface = get_surface_block(biome_for_surface, top_solid_y, has_surface_water);
            if (snow_cover && surface == BlockIDs::GRASS) {
                chunk.set_block(x, top_ly, z, BlockIDs::SNOW);
                // Place grass underneath snow
                if (top_ly > 0) {
                    chunk.set_block(x, top_ly - 1, z, BlockIDs::GRASS);
                }
            } else if (aquatic && top_solid_y < static_cast<int>(sea_level)) {
                // Aquatic biome floor: sand/gravel
                chunk.set_block(x, top_ly, z, BlockIDs::SAND);
                if (top_ly > 0) chunk.set_block(x, top_ly - 1, z, BlockIDs::SAND);
            } else {
                chunk.set_block(x, top_ly, z, surface);

                // Subsurface blocks
                BlockID subsurface = get_subsurface_block(biome_for_surface, aquatic);
                int cover_depth = (biome_id == Biomes::mesa || biome_id == Biomes::mesaPlateau_F ||
                                   biome_id == Biomes::mesaPlateau) ? 4 : 3;
                for (int dy = 1; dy <= cover_depth; ++dy) {
                    int sy = top_ly - dy;
                    if (sy >= 0) {
                        BlockID existing = chunk.get_block(x, sy, z);
                        if (existing == BlockIDs::STONE) {
                            chunk.set_block(x, sy, z, subsurface);
                        }
                    }
                }
            }
        }
    }

    // =====================================================================
    // Phase 4: Carve caves
    // =====================================================================
    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int z = 0; z < CHUNK_DEPTH; ++z) {
            for (int wy = world_y_start; wy < world_y_end; ++wy) {
                if (wy < params.bedrock_height + 4 || wy > static_cast<int>(sea_level + 10.0f))
                    continue;
                int ly = wy - world_y_start;
                BlockID id = chunk.get_block(x, ly, z);
                if (id == BlockIDs::AIR || id == BlockIDs::WATER) continue;
                if (is_cave(world_x_start + x, wy, world_z_start + z)) {
                    chunk.set_block(x, ly, z, BlockIDs::AIR);
                }
            }
        }
    }

    // Place vegetation
    if (vegetation_enabled) {
        VegetationGenerator veg;
        // Build ChunkColumn array from the generated chunk
        ChunkColumn columns[CHUNK_WIDTH][CHUNK_DEPTH];
        for (int x = 0; x < CHUNK_WIDTH; ++x) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {
                int top_y = world_y_start - 1;
                for (int wy = world_y_end - 1; wy >= world_y_start; --wy) {
                    int ly = wy - world_y_start;
                    BlockID b = chunk.get_block(x, ly, z);
                    if (b != BlockIDs::AIR && b != BlockIDs::WATER) {
                        top_y = wy;
                        break;
                    }
                }
                columns[x][z].top_solid_y = top_y;
                columns[x][z].biome_id = surface_biomes[x][z];
            }
        }
        veg.generate_vegetation(chunk, columns, chunk_x, chunk_z,
                                world_y_start, world_y_end, cross_writer);
    }

    chunk.compute_section_flags();
}

// -------------------------------------------------------------------------
// Debug render: biome PGM
// -------------------------------------------------------------------------
void ChunkGenerator::render_biome_pgm(const char* filename, int img_w, int img_h,
                                      float world_x_start, float world_z_start,
                                      float step) const {
    FILE* f = fopen(filename, "wb");
    if (!f) return;
    fprintf(f, "P5\n%d %d\n255\n", img_w, img_h);
    for (int py = 0; py < img_h; ++py) {
        for (int px = 0; px < img_w; ++px) {
            float wx = world_x_start + static_cast<float>(px) * step;
            float wz = world_z_start + static_cast<float>(py) * step;
            auto col = sample_column(static_cast<int32_t>(wx), static_cast<int32_t>(wz));
            uint8_t byte = static_cast<uint8_t>((col.biome_id * 37) & 255);
            fwrite(&byte, 1, 1, f);
        }
    }
    fclose(f);
}

} // namespace VoxelEngine
