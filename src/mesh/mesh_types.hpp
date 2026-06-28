#ifndef FUK_MINECRAFT_MESH_TYPES_HPP
#define FUK_MINECRAFT_MESH_TYPES_HPP
#include <cstdint>
#include <vector>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>

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
    uint8_t texture_ao_pad; // 1 byte padding
    uint8_t light_r;      // 1 byte
    uint8_t light_g;      // 1 byte
    uint8_t light_b;      // 1 byte
    uint8_t sky_light;    // 1 byte
};

// Built mesh data ready for upload to GPU
struct BuiltMeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    bool empty = true;
};

struct PackedBuiltMeshData {
    godot::PackedVector3Array vertices;
    godot::PackedVector3Array normals;
    godot::PackedByteArray custom0;  // RGBA8_UNORM: light_r, light_g, light_b, sky_light
    godot::PackedVector2Array uvs;
    godot::PackedByteArray custom1;  // RG_HALF: texture_index, ao
    godot::PackedInt32Array indices;
    bool empty = true;
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_MESH_TYPES_HPP