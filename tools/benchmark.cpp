#include "worldgen/chunk_generator.hpp"
#include "mesh/mesh_builder.hpp"
#include "core/chunk_data.hpp"
#include "core/block_types.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

struct BenchResult {
    const char* name;
    double avg_ms;
};

static BenchResult bench_generation(int n) {
    VoxelEngine::TerrainParams params;
    VoxelEngine::ChunkGenerator gen(params);
    VoxelEngine::ChunkData chunk;

    for (int i = 0; i < 50; i++)
        gen.generate_chunk(chunk, i * 10, 0, i * 10);

    VoxelEngine::PerformanceTimer perf;
    for (int i = 0; i < n; i++) {
        VoxelEngine::ScopedTimer timer(perf, VoxelEngine::TimerID::GenerateChunk);
        gen.generate_chunk(chunk, i * 10, 0, i * 10);
    }

    double avg = perf.get_avg(VoxelEngine::TimerID::GenerateChunk);
    printf("  generate_chunk: avg=%.3f ms  min=%.3f ms  max=%.3f ms  (n=%d)\n",
           avg, perf.get_min(VoxelEngine::TimerID::GenerateChunk),
           perf.get_max(VoxelEngine::TimerID::GenerateChunk), n);
    return {"generate_chunk_avg_ms", avg};
}

static BenchResult bench_meshing(int n) {
    VoxelEngine::TerrainParams params;
    VoxelEngine::ChunkGenerator gen(params);
    VoxelEngine::ChunkData chunk;

    gen.generate_chunk(chunk, 0, 0, 0);

    VoxelEngine::MeshBuilder mb;
    VoxelEngine::PerformanceTimer perf;

    for (int i = 0; i < 50; i++) {
        mb.clear();
        mb.build_mesh(chunk);
    }

    for (int i = 0; i < n; i++) {
        mb.clear();
        VoxelEngine::ScopedTimer timer(perf, VoxelEngine::TimerID::BuildMesh);
        mb.build_mesh(chunk);
    }

    double avg = perf.get_avg(VoxelEngine::TimerID::BuildMesh);
    printf("  build_mesh:     avg=%.3f ms  min=%.3f ms  max=%.3f ms  (n=%d)\n",
           avg, perf.get_min(VoxelEngine::TimerID::BuildMesh),
           perf.get_max(VoxelEngine::TimerID::BuildMesh), n);
    return {"build_mesh_avg_ms", avg};
}

static BenchResult bench_palette_ops(int n) {
    VoxelEngine::ChunkData chunk;
    chunk.clear();

    VoxelEngine::PerformanceTimer perf;

    for (int i = 0; i < 50; i++) {
        for (int x = 0; x < VoxelEngine::CHUNK_WIDTH; x++)
            for (int z = 0; z < VoxelEngine::CHUNK_DEPTH; z++)
                for (int y = 0; y < VoxelEngine::CHUNK_HEIGHT; y++)
                    chunk.set_block(x, y, z, VoxelEngine::BlockIDs::AIR);
    }

    for (int i = 0; i < n; i++) {
        VoxelEngine::ScopedTimer timer(perf, VoxelEngine::TimerID::PaletteWrite);
        for (int x = 0; x < VoxelEngine::CHUNK_WIDTH; x++)
            for (int z = 0; z < VoxelEngine::CHUNK_DEPTH; z++)
                for (int y = 0; y < VoxelEngine::CHUNK_HEIGHT; y++)
                    chunk.set_block(x, y, z, VoxelEngine::BlockIDs::STONE);
    }

    double avg = perf.get_avg(VoxelEngine::TimerID::PaletteWrite);
    printf("  palette_write:  avg=%.3f ms  min=%.3f ms  max=%.3f ms  (n=%d full chunk fills)\n",
           avg, perf.get_min(VoxelEngine::TimerID::PaletteWrite),
           perf.get_max(VoxelEngine::TimerID::PaletteWrite), n);
    return {"palette_write_avg_ms", avg};
}

struct BaselineEntry {
    std::string name;
    double max_avg_ms;
};

static std::vector<BaselineEntry> load_baseline(const char* path) {
    std::vector<BaselineEntry> entries;
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Warning: could not open baseline file %s\n", path);
        return entries;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') continue;
        char name[128];
        double val;
        if (sscanf(line, "%127s %lf", name, &val) == 2)
            entries.push_back({name, val});
    }
    fclose(f);
    return entries;
}

int main(int argc, char** argv) {
    bool check_mode = false;
    const char* baseline_path = nullptr;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--check") == 0 && i + 1 < argc) {
            check_mode = true;
            baseline_path = argv[++i];
        }
    }

    printf("=== VoxelEngine Benchmark ===\n");
    std::vector<BenchResult> results;
    results.push_back(bench_generation(1000));
    results.push_back(bench_meshing(1000));
    results.push_back(bench_palette_ops(100));

    if (!check_mode || !baseline_path) return 0;

    auto baseline = load_baseline(baseline_path);
    if (baseline.empty()) {
        fprintf(stderr, "No baseline entries loaded, skipping check.\n");
        return 0;
    }

    printf("\n--- Baseline check (%s) ---\n", baseline_path);
    int regressions = 0;

    for (auto& r : results) {
        for (auto& b : baseline) {
            if (r.name == b.name) {
                if (r.avg_ms > b.max_avg_ms) {
                    printf("  REGRESSION %s: %.3f ms > baseline %.3f ms\n",
                           r.name, r.avg_ms, b.max_avg_ms);
                    regressions++;
                } else {
                    printf("  OK %s: %.3f ms <= baseline %.3f ms\n",
                           r.name, r.avg_ms, b.max_avg_ms);
                }
            }
        }
    }

    if (regressions > 0) {
        printf("\nFAILED: %d metric(s) regressed beyond baseline.\n", regressions);
        return 1;
    }
    printf("\nAll metrics within baseline.\n");
    return 0;
}
