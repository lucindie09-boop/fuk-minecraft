#include "core/chunk_data.hpp"
#include "core/block_types.hpp"
#include "mesh/mesh_builder.hpp"
#include <cstdint>
#include <cstddef>

using namespace VoxelEngine;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 4) return 0;

    BlockRegistry::get_instance().initialize_default_blocks();

    ChunkData chunk;
    chunk.clear();

    size_t pos = 0;
    // Use fuzzer input to place blocks in the chunk
    // Each operation: [x:1][y:1][z:1][block_id:1]
    while (pos + 4 <= size) {
        uint16_t x = data[pos++] % CHUNK_WIDTH;
        uint16_t y = data[pos++] % CHUNK_HEIGHT;
        uint16_t z = data[pos++] % CHUNK_DEPTH;
        
        // Use fuzzer byte to select block type (limit to valid range)
        BlockID id = static_cast<BlockID>(data[pos++] % 21);
        chunk.set_block(x, y, z, id);
    }

    // Create mesh builder and build mesh - this should not crash
    // even with arbitrary block layouts
    MeshBuilder builder;
    builder.set_greedy_enabled(true);
    
    // Build the mesh - this is the hot path we want to fuzz
    builder.build_mesh(chunk, 0, 0, 0);
    
    // Read back vertices and indices via getter methods
    const auto& vertices = builder.get_vertices();
    const auto& indices = builder.get_indices();
    
    // Verify vertex/index counts are reasonable
    // A chunk should not have an absurd number of vertices
    constexpr size_t MAX_REASONABLE_VERTICES = 1000000; // 1M vertices is already excessive
    if (vertices.size() > MAX_REASONABLE_VERTICES) {
        // Too many vertices - possible overflow bug
        abort();
    }
    
    if (indices.size() > MAX_REASONABLE_VERTICES * 3) {
        // Too many indices - possible overflow bug
        abort();
    }

    // Verify indices are within bounds of vertices array
    for (uint32_t idx : indices) {
        if (idx >= vertices.size()) {
            // Index out of bounds - this is a bug
            abort();
        }
    }

    return 0;
}
