#ifndef FUK_MINECRAFT_MESH_TYPES_HPP
#define FUK_MINECRAFT_MESH_TYPES_HPP
#include <cstdint>
#include <vector>
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#endif

namespace VoxelEngine {

// Face direction used by meshing
enum class FaceDirection : uint8_t {
    Top    = 0, // +Y
    Bottom = 1, // -Y
    Right  = 2, // +X
    Left   = 3, // -X
    Front  = 4, // +Z
    Back   = 5  // -Z
};

// Vertex layout for chunk meshes
struct Vertex {
    float x, y, z;        // 12 bytes
    int8_t nx, ny, nz;    // 3 bytes
    uint8_t normal_pad;   // 1 byte padding
    float u, v;           // 8 bytes
    uint16_t texture_index; // 2 bytes
    uint8_t ao;           // 1 byte
    uint8_t emissive_index; // 1 byte (emissive texture layer, 0 = none)
    uint8_t light_r;      // 1 byte
    uint8_t light_g;      // 1 byte
    uint8_t light_b;      // 1 byte
    uint8_t sky_light;    // 1 byte
};

// Built mesh data ready for upload to GPU
struct BuiltMeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Vertex> water_vertices;
    std::vector<uint32_t> water_indices;
    bool empty = true;
};

#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
struct PackedBuiltMeshData {
    godot::PackedVector3Array vertices;
    godot::PackedByteArray custom0;  // RGBA8_UNORM: light_r, light_g, light_b, sky_light
    godot::PackedByteArray custom1;  // RGBA8_UNORM: R=texture_index, G=ao, B=normal_encoded, A=emissive_index
    godot::PackedByteArray custom2;  // RG_HALF: u, v
    godot::PackedInt32Array indices;
    bool empty = true;
};
#endif

struct WorldRenderStats {
    int32_t visible_instances = 0;
    int32_t mesh_rids = 0;
    int32_t chunk_instances = 0;
    int32_t far_region_instances = 0;
    int32_t chunk_mesh_rids = 0;
    int32_t far_region_mesh_rids = 0;
    int32_t eligible_far_chunks = 0;
    int32_t cached_far_chunks = 0;
    int32_t active_region_member_chunks = 0;
    int32_t regions_partial_missing_cache = 0;
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_MESH_TYPES_HPP
