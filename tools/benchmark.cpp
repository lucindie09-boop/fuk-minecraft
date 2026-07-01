#include "worldgen/chunk_generator.hpp"
#include <cstdio>
#include <cstdint>

int main() {
    VoxelEngine::TerrainParams params;
    VoxelEngine::ChunkGenerator gen(params);

    const int NUM_CHUNKS = 1000;
    VoxelEngine::ChunkData chunk;

    // Warmup — generate chunks at surface level (chunk_y=6 covers y=192..223)
    for (int i = 0; i < 50; i++) {
        gen.generate_chunk(chunk, i * 10, 6, i * 10);
    }

    VoxelEngine::PerformanceTimer perf;
    for (int i = 0; i < NUM_CHUNKS; i++) {
        VoxelEngine::ScopedTimer timer(perf, VoxelEngine::TimerID::GenerateChunk);
        gen.generate_chunk(chunk, i * 10, 6, i * 10);
    }

    double avg = perf.get_avg(VoxelEngine::TimerID::GenerateChunk);
    printf("Generated %d chunks\n", NUM_CHUNKS);
    printf("Average chunk generation time: %.3f ms\n", avg);
    printf("Min: %.3f ms, Max: %.3f ms\n", perf.get_min(VoxelEngine::TimerID::GenerateChunk),
           perf.get_max(VoxelEngine::TimerID::GenerateChunk));
    return 0;
}
