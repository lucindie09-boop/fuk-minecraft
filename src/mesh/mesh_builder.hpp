#ifndef FUK_MINECRAFT_MESH_BUILDER_HPP
#define FUK_MINECRAFT_MESH_BUILDER_HPP
#include <atomic>

#include "core/chunk_data.hpp"
#include "core/block_types.hpp"
#include "mesh/mesh_types.hpp"
#include "mesh/chunk_neighbor_accessor.hpp"
#include "core/performance_timer.hpp"
#include <vector>
#include <array>
#include <cstdint>
#include <chrono>
#include <algorithm>
#include <cmath>

namespace VoxelEngine {

// Face data for greedy meshing
struct Face {
    int32_t x, y, z;         // Position
    FaceDirection direction; // Face direction
    BlockID block_id;        // Block type
    int32_t u_max;           // Extension in u direction
    int32_t v_max;           // Extension in v direction
};

// Per-face data for mesh extraction (used by raycasting / face queries)
struct FaceData {
    FaceDirection direction;
    BlockPos      position;
    BlockID       block_id;
    bool          visible;

    FaceData() noexcept
        : direction(FaceDirection::Top),
          block_id(BlockIDs::AIR),
          visible(false) {}
};

// Pre-computed block-light brightness curve for levels 0–15.
// Matches the GPU function: level<=2 → 0.0025 + level*0.01875,
// level>2 → 0.04 + (level-2)^1.5 * 0.008
inline const std::array<float, 16> kBlockBrightness = []() {
    std::array<float, 16> arr{};
    for (int i = 0; i < 16; i++) {
        if (i <= 2) {
            arr[i] = 0.0025f + i * 0.01875f;
        } else {
            float x = static_cast<float>(i - 2);
            arr[i] = 0.04f + x * std::sqrt(x) * 0.008f;
        }
    }
    return arr;
}();

class MeshBuilder {
public:
    MeshBuilder() {
        vertices.reserve(kVertexReserve);
        indices.reserve(kIndexReserve);
    }

    BuiltMeshData build_mesh_data(
        const ChunkData& chunk_data,
        const ChunkData* neg_x,
        const ChunkData* pos_x,
        const ChunkData* neg_y,
        const ChunkData* pos_y,
        const ChunkData* neg_z,
        const ChunkData* pos_z,
        const ChunkData* neg_x_neg_z = nullptr,
        const ChunkData* neg_x_pos_z = nullptr,
        const ChunkData* pos_x_neg_z = nullptr,
        const ChunkData* pos_x_pos_z = nullptr
    );

    void clear();

    void build_mesh(const ChunkData& chunk,
                    const ChunkData* neighbor_x_neg = nullptr,
                    const ChunkData* neighbor_x_pos = nullptr,
                    const ChunkData* neighbor_y_neg = nullptr,
                    const ChunkData* neighbor_y_pos = nullptr,
                    const ChunkData* neighbor_z_neg = nullptr,
                    const ChunkData* neighbor_z_pos = nullptr,
                    const ChunkData* neg_x_neg_z = nullptr,
                    const ChunkData* neg_x_pos_z = nullptr,
                    const ChunkData* pos_x_neg_z = nullptr,
                    const ChunkData* pos_x_pos_z = nullptr);

    const std::vector<Vertex>& get_vertices() const { return vertices; }
    const std::vector<uint32_t>& get_indices() const { return indices; }
    size_t get_vertex_count() const { return vertices.size(); }
    size_t get_index_count() const { return indices.size(); }
    size_t get_triangle_count() const { return indices.size() / 3; }

    static PerformanceTimer& get_perf_timer() { return perf_timer; }
    static double get_avg_vertices_per_chunk() {
        uint64_t tv = total_vertices.load(std::memory_order_relaxed);
        uint64_t tc = total_chunks.load(std::memory_order_relaxed);
        return tc > 0 ? static_cast<double>(tv) / tc : 0.0;
    }
    static void reset_vertex_tracking() {
        total_vertices.store(0, std::memory_order_relaxed);
        total_chunks.store(0, std::memory_order_relaxed);
    }

    void set_greedy_enabled(bool enabled) { passive_greedy_enabled = enabled; }
    bool is_greedy_enabled() const { return passive_greedy_enabled; }

private:
    // -------------------------------------------------------------------------
    // Constants
    // -------------------------------------------------------------------------
    static constexpr int SC_W = CHUNK_WIDTH + 2;  // 34 with 32-wide chunks
    static constexpr int SC_D = CHUNK_DEPTH + 2;  // 34 with 32-deep chunks

    static constexpr float kWaterSurfaceDrop = 0.12f;
    static constexpr float kLoweredBlockOffset = 0.0625f;  // 1/16
    static constexpr int   kMaxGreedyMergeDistance = 16;
    static constexpr size_t kVertexReserve = CHUNK_VOLUME * 2 + 4096;
    static constexpr size_t kIndexReserve  = kVertexReserve * 3 / 2;

    // -------------------------------------------------------------------------
    // Lookup tables
    // -------------------------------------------------------------------------
    static constexpr float kFaceVertices[6][4][3] = {
        // Top (+Y)
        {{0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}},
        // Bottom (-Y)
        {{0, 0, 1}, {1, 0, 1}, {1, 0, 0}, {0, 0, 0}},
        // Right (+X)
        {{1, 0, 0}, {1, 0, 1}, {1, 1, 1}, {1, 1, 0}},
        // Left (-X)
        {{0, 0, 1}, {0, 0, 0}, {0, 1, 0}, {0, 1, 1}},
        // Front (+Z)
        {{1, 0, 1}, {0, 0, 1}, {0, 1, 1}, {1, 1, 1}},
        // Back (-Z)
        {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}}
    };

    static constexpr int32_t kDirectionOffsets[6][3] = {
        {0, 1, 0},   // Top
        {0, -1, 0},  // Bottom
        {1, 0, 0},   // Right
        {-1, 0, 0},  // Left
        {0, 0, 1},   // Front
        {0, 0, -1}   // Back
    };

    static constexpr float kFaceNormals[6][3] = {
        {0, 1, 0},   // Top
        {0, -1, 0},  // Bottom
        {1, 0, 0},   // Right
        {-1, 0, 0},  // Left
        {0, 0, 1},   // Front
        {0, 0, -1}   // Back
    };

    static constexpr float kFaceUVs[4][2] = {
        {0, 1}, // Bottom-Left
        {1, 1}, // Bottom-Right
        {1, 0}, // Top-Right
        {0, 0}  // Top-Left
    };

    static constexpr std::array<FaceDirection, 6> kAllDirections = {
        FaceDirection::Top, FaceDirection::Bottom,
        FaceDirection::Right, FaceDirection::Left,
        FaceDirection::Front, FaceDirection::Back
    };

    static constexpr std::array<FaceDirection, 4> kSideDirections = {
        FaceDirection::Right, FaceDirection::Left,
        FaceDirection::Front, FaceDirection::Back
    };

    // Maps FaceDirection enum (Top=0, Bottom=1, Right=2, Left=3, Front=4, Back=5)
    // to texture_indices convention (+X=0, -X=1, +Y=2, -Y=3, +Z=4, -Z=5).
    static constexpr int kDirToTexIdx[6] = {2, 3, 0, 1, 4, 5};

    // -------------------------------------------------------------------------
    // Member data
    // -------------------------------------------------------------------------
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    // Layout is [x][z][y] (not [y][x][z]) so that scans with y as the inner
    // loop variable — passive_greedy_mesh_vertical and the non-greedy fallback
    // path — read sequential memory instead of striding by SC_W * SC_D each step.
    std::array<std::array<std::array<uint8_t, CHUNK_HEIGHT>, SC_D>, SC_W> solid_cache{};

    // Neighbor chunk accessor for cross-chunk block/light access.
    ChunkNeighborAccessor accessor;

    static PerformanceTimer perf_timer;
    static std::atomic<uint64_t> total_vertices;
    static std::atomic<uint64_t> total_chunks;

    bool passive_greedy_enabled = true;

    // -------------------------------------------------------------------------
    // Static helpers (small, keep inline)
    // -------------------------------------------------------------------------
    static bool is_side_face(FaceDirection direction) {
        return direction == FaceDirection::Right ||
               direction == FaceDirection::Left ||
               direction == FaceDirection::Front ||
               direction == FaceDirection::Back;
    }

    static void apply_special_block_offsets(float corners[4][3], BlockID block_id, FaceDirection dir);

    static int compute_face_rotation(int32_t x, int32_t y, int32_t z, FaceDirection dir) {
        int32_t wx = x + static_cast<int32_t>(dir) * 7;
        int32_t wz = z + y * 3;
        return ((wx * 13 + wz * 17) & 3);
    }

    static void apply_uv_rotation(float& u, float& v, int rotation) {
        if (rotation == 0) return;
        float uc = u - 0.5f;
        float vc = v - 0.5f;
        switch (rotation) {
            case 1: u = 0.5f - vc; v = 0.5f + uc; break; //  90°
            case 2: u = 0.5f - uc; v = 0.5f - vc; break; // 180°
            case 3: u = 0.5f + vc; v = 0.5f - uc; break; // 270°
        }
    }

    static bool block_face_rotates(BlockID id, int tex_idx) noexcept {
        switch (id) {
            case BlockIDs::AIR:
            case BlockIDs::SURFACE_WATER:
            case BlockIDs::WATER:
            case BlockIDs::LEAVES:
            case BlockIDs::LIGHT_BLOCK:
            case BlockIDs::LIGHT_RED:
            case BlockIDs::LIGHT_GREEN:
            case BlockIDs::LIGHT_BLUE:
                return false;
            case BlockIDs::GRASS:
                return tex_idx == 2 || tex_idx == 3;
            default:
                return true;
        }
    }

    static int get_face_rotation(BlockID block_id, int32_t x, int32_t y, int32_t z,
                                  FaceDirection dir, int dir_index) {
        if (!block_face_rotates(block_id, kDirToTexIdx[dir_index])) return 0;
        return compute_face_rotation(x, y, z, dir);
    }

    static float get_face_shade(FaceDirection direction) {
        switch (direction) {
            case FaceDirection::Top:    return 1.00f;
            case FaceDirection::Bottom: return 0.50f;
            case FaceDirection::Right:  return 0.75f;
            case FaceDirection::Left:   return 0.75f;
            case FaceDirection::Front:  return 0.60f;
            case FaceDirection::Back:   return 0.60f;
        }
        return 1.0f;
    }

    bool should_cull_against_neighbor(const ChunkData& chunk, BlockID current, BlockID neighbor,
                                       FaceDirection direction, int32_t x, int32_t y, int32_t z) const;

    static int compute_ao(bool side1, bool side2, bool corner) {
        if (side1 && side2) return 3;
        return static_cast<int>(side1) + static_cast<int>(side2) + static_cast<int>(corner);
    }

    static float ao_to_brightness(int level) {
        static constexpr float table[4] = {1.0f, 0.8f, 0.6f, 0.4f};
        return table[level < 4 ? level : 3];
    }

    int vertex_ao_level(const ChunkNeighborAccessor& accessor,
                        int32_t s1x, int32_t s1y, int32_t s1z,
                        int32_t s2x, int32_t s2y, int32_t s2z,
                        int32_t cx, int32_t cy, int32_t cz) const;

    void compute_face_ao(const ChunkNeighborAccessor& accessor, int32_t x, int32_t y, int32_t z,
                        FaceDirection direction, float ao_out[4]) const;


    // -------------------------------------------------------------------------
    // Face emission (heavy — defined in .cpp)
    // -------------------------------------------------------------------------
    void add_face(const ChunkData& chunk, const ChunkNeighborAccessor& accessor,
                  int32_t x, int32_t y, int32_t z,
                  FaceDirection direction, BlockID block_id);

    void add_greedy_face(const ChunkData& chunk, const ChunkNeighborAccessor& accessor,
                         const Face& face, uint16_t face_light_key, int rotation);

    // -------------------------------------------------------------------------
    // Passive greedy meshing (heavy — defined in .cpp)
    // -------------------------------------------------------------------------
    void flush_horizontal_merge(const ChunkData& chunk, const ChunkNeighborAccessor& accessor,
                                int32_t x_start, int32_t x_end,
                                int32_t y, int32_t z, FaceDirection direction,
                                BlockID block_id, uint16_t light_key, int rotation);

    void passive_greedy_mesh_horizontal(const ChunkData& chunk, const ChunkNeighborAccessor& accessor,
                                        FaceDirection direction);

    // -------------------------------------------------------------------------
    // Passive vertical greedy meshing (1D Y-axis merge for side faces)
    // -------------------------------------------------------------------------
    void flush_vertical_merge(const ChunkData& chunk, const ChunkNeighborAccessor& accessor,
                                int32_t y_start, int32_t y_end,
                                int32_t x, int32_t z, FaceDirection direction,
                                BlockID block_id, uint16_t light_key, int rotation);

    void passive_greedy_mesh_vertical(const ChunkData& chunk, const ChunkNeighborAccessor& accessor,
                                        FaceDirection direction);

};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_MESH_BUILDER_HPP