#ifndef FUK_MINECRAFT_CHUNK_DATA_HPP
#define FUK_MINECRAFT_CHUNK_DATA_HPP
#include "core/block_types.hpp"
#include "core/chunk_coords.hpp"
#include "core/light_packing.hpp"
#include <cstdint>
#include <memory>
#include <cstring>
#include <algorithm>

namespace VoxelEngine {

struct ChunkStorage {
    BlockID blocks[CHUNK_VOLUME];
    uint16_t light_packed[CHUNK_VOLUME];
};

class ChunkData;
void propagate_chunk_block_light_additive(ChunkData& chunk); // Defined in block_light_region.cpp

// -----------------------------------------------------------------------------
// Chunk Data Storage
// -----------------------------------------------------------------------------
class ChunkData {
private:
    std::unique_ptr<ChunkStorage> storage;

    bool     is_empty = true;
    bool     is_fully_solid = false;
    uint32_t block_count = 0;
    uint32_t emissive_count = 0;

    // Per-section non-air block count. Zero == all air.
    uint32_t section_block_count[CHUNK_SECTIONS];

    [[nodiscard]] static inline bool is_emissive_block(BlockID id) noexcept {
        return HasProperty(BlockRegistry::get_instance().get_block_fast(id).properties, BlockProperty::Emissive);
    }

    [[nodiscard]] static inline constexpr std::size_t index(int32_t x, int32_t y, int32_t z) noexcept {
        return static_cast<std::size_t>(x + y * CHUNK_WIDTH + z * CHUNK_WIDTH * CHUNK_HEIGHT);
    }

public:
    [[nodiscard]] BlockID* blocks() noexcept { return storage->blocks; }
    [[nodiscard]] const BlockID* blocks() const noexcept { return storage->blocks; }
    [[nodiscard]] uint16_t* light_packed() noexcept { return storage->light_packed; }
    [[nodiscard]] const uint16_t* light_packed() const noexcept { return storage->light_packed; }

    ChunkData();
    ChunkData(const ChunkData& other);
    ChunkData& operator=(const ChunkData& other);
    ChunkData(ChunkData&& other) noexcept;
    ChunkData& operator=(ChunkData&& other) noexcept;

    // -------------------------------------------------------------------------
    // Bulk Clear
    // -------------------------------------------------------------------------
    void clear_block_light() noexcept;
    void clear_sky_light() noexcept;
    void clear_light() noexcept;
    void clear() noexcept;

    // -------------------------------------------------------------------------
    // Block Access
    // -------------------------------------------------------------------------
    [[nodiscard]] BlockID get_block(int32_t x, int32_t y, int32_t z) const noexcept {
        if (x < 0 || x >= CHUNK_WIDTH ||
            y < 0 || y >= CHUNK_HEIGHT ||
            z < 0 || z >= CHUNK_DEPTH) {
            return BlockIDs::AIR;
        }
        return blocks()[index(x, y, z)];
    }

    [[nodiscard]] BlockID get_block(const BlockPos& pos) const noexcept {
        return get_block(pos.x, pos.y, pos.z);
    }

    void set_block(int32_t x, int32_t y, int32_t z, BlockID block_id) noexcept {
        if (x < 0 || x >= CHUNK_WIDTH ||
            y < 0 || y >= CHUNK_HEIGHT ||
            z < 0 || z >= CHUNK_DEPTH) {
            return;
        }

        const std::size_t idx = index(x, y, z);
        const BlockID old_block = blocks()[idx];

        if (old_block == block_id) {
            return;
        }

        blocks()[idx] = block_id;

        // Update emissive tracking
        if (is_emissive_block(old_block) && emissive_count > 0) {
            --emissive_count;
        }
        if (is_emissive_block(block_id)) {
            ++emissive_count;
        }

        // Update global counts
        if (old_block == BlockIDs::AIR && block_id != BlockIDs::AIR) {
            ++block_count;
        } else if (old_block != BlockIDs::AIR && block_id == BlockIDs::AIR) {
            --block_count;
        }
        is_empty = (block_count == 0);

        // O(1) section update
        const int32_t section = y / SECTION_HEIGHT;
        if (old_block == BlockIDs::AIR && block_id != BlockIDs::AIR) {
            ++section_block_count[section];
        } else if (old_block != BlockIDs::AIR && block_id == BlockIDs::AIR) {
            if (section_block_count[section] > 0) {
                --section_block_count[section];
            }
        }

        // Invalidate fully_solid cache if the change breaks it
        if (is_fully_solid) {
            if (block_id == BlockIDs::AIR) {
                is_fully_solid = false;
            } else {
                const auto& registry = BlockRegistry::get_instance();
                const auto& block_type = registry.get_block(block_id);
                if (!HasProperty(block_type.properties, BlockProperty::Solid) ||
                    !HasProperty(block_type.properties, BlockProperty::Opaque)) {
                    is_fully_solid = false;
                }
            }
        }
    }

    void set_block(const BlockPos& pos, BlockID block_id) noexcept {
        set_block(pos.x, pos.y, pos.z, block_id);
    }

//Hot path write for bulk generation
inline void set_block_raw(int32_t x, int32_t y, int32_t z, BlockID block_id) noexcept {
    blocks()[index(x, y, z)] = block_id;
}

inline void finalize_bulk_write(uint32_t total_block_count, uint32_t total_emissive_count,
                                const uint32_t* section_counts) noexcept {
block_count = total_block_count;
emissive_count = total_emissive_count;
is_empty = (block_count == 0);
is_fully_solid = false;
std::memcpy(section_block_count, section_counts, sizeof(section_block_count));
}

    // Fast bulk fill ??? bypasses per-block overhead. Used by fast-path generation.
    void fill_blocks(BlockID block_id) noexcept;

    [[nodiscard]] bool is_all_air() const noexcept {
        return is_empty;
    }

    [[nodiscard]] bool fully_solid() const noexcept {
        return is_fully_solid;
    }

    [[nodiscard]] uint32_t get_block_count() const noexcept {
        return block_count;
    }

    [[nodiscard]] uint32_t get_emissive_count() const noexcept {
        return emissive_count;
    }

    [[nodiscard]] const BlockID* get_data() const noexcept {
        return blocks();
    }

    // -------------------------------------------------------------------------
    // Light Access
    // -------------------------------------------------------------------------
    [[nodiscard]] uint8_t get_light(int32_t x, int32_t y, int32_t z) const noexcept {
        if (x < 0 || x >= CHUNK_WIDTH ||
            y < 0 || y >= CHUNK_HEIGHT ||
            z < 0 || z >= CHUNK_DEPTH) {
            return 0;
        }
        const uint16_t v = light_packed()[index(x, y, z)];
        const uint8_t r = unpack_r(v);
        const uint8_t g = unpack_g(v);
        const uint8_t b = unpack_b(v);
        return r > g ? (r > b ? r : b) : (g > b ? g : b);
    }

    void set_light(int32_t x, int32_t y, int32_t z, uint8_t light_level) noexcept {
        if (x < 0 || x >= CHUNK_WIDTH ||
            y < 0 || y >= CHUNK_HEIGHT ||
            z < 0 || z >= CHUNK_DEPTH) {
            return;
        }
        const std::size_t idx = index(x, y, z);
        const uint8_t sky = unpack_sky(light_packed()[idx]);
        light_packed()[idx] = pack_light(sky, light_level, light_level, light_level);
    }

    [[nodiscard]] uint8_t get_sky_light(int32_t x, int32_t y, int32_t z) const noexcept {
        if (x < 0 || x >= CHUNK_WIDTH ||
            y < 0 || y >= CHUNK_HEIGHT ||
            z < 0 || z >= CHUNK_DEPTH) {
            return 0;
        }
        return unpack_sky(light_packed()[index(x, y, z)]);
    }

    void set_sky_light(int32_t x, int32_t y, int32_t z, uint8_t light_level) noexcept {
        if (x < 0 || x >= CHUNK_WIDTH ||
            y < 0 || y >= CHUNK_HEIGHT ||
            z < 0 || z >= CHUNK_DEPTH) {
            return;
        }
        const std::size_t idx = index(x, y, z);
        const uint16_t v = light_packed()[idx];
        light_packed()[idx] = pack_light(light_level, unpack_r(v), unpack_g(v), unpack_b(v));
    }

    [[nodiscard]] uint8_t get_light_r(int32_t x, int32_t y, int32_t z) const noexcept {
        if (x < 0 || x >= CHUNK_WIDTH ||
            y < 0 || y >= CHUNK_HEIGHT ||
            z < 0 || z >= CHUNK_DEPTH) {
            return 0;
        }
        return unpack_r(light_packed()[index(x, y, z)]);
    }

    [[nodiscard]] uint8_t get_light_g(int32_t x, int32_t y, int32_t z) const noexcept {
        if (x < 0 || x >= CHUNK_WIDTH ||
            y < 0 || y >= CHUNK_HEIGHT ||
            z < 0 || z >= CHUNK_DEPTH) {
            return 0;
        }
        return unpack_g(light_packed()[index(x, y, z)]);
    }

    [[nodiscard]] uint8_t get_light_b(int32_t x, int32_t y, int32_t z) const noexcept {
        if (x < 0 || x >= CHUNK_WIDTH ||
            y < 0 || y >= CHUNK_HEIGHT ||
            z < 0 || z >= CHUNK_DEPTH) {
            return 0;
        }
        return unpack_b(light_packed()[index(x, y, z)]);
    }

    void set_light_rgb(int32_t x, int32_t y, int32_t z, uint8_t r, uint8_t g, uint8_t b) noexcept {
        if (x < 0 || x >= CHUNK_WIDTH ||
            y < 0 || y >= CHUNK_HEIGHT ||
            z < 0 || z >= CHUNK_DEPTH) {
            return;
        }
        const std::size_t idx = index(x, y, z);
        const uint8_t sky = unpack_sky(light_packed()[idx]);
        light_packed()[idx] = pack_light(sky, r, g, b);
    }

    [[nodiscard]] const uint16_t* get_light_packed_data() const noexcept {
        return light_packed();
    }

    // Unchecked accessors for hot paths (meshing, light propagation).
    [[nodiscard]] inline BlockID get_block_unsafe(int32_t x, int32_t y, int32_t z) const noexcept {
        return blocks()[index(x, y, z)];
    }

    [[nodiscard]] inline uint8_t get_light_unsafe(int32_t x, int32_t y, int32_t z) const noexcept {
        const uint16_t v = light_packed()[index(x, y, z)];
        const uint8_t r = unpack_r(v);
        const uint8_t g = unpack_g(v);
        const uint8_t b = unpack_b(v);
        return r > g ? (r > b ? r : b) : (g > b ? g : b);
    }

    inline void set_light_unsafe(int32_t x, int32_t y, int32_t z, uint8_t light_level) noexcept {
        const std::size_t idx = index(x, y, z);
        const uint8_t sky = unpack_sky(light_packed()[idx]);
        light_packed()[idx] = pack_light(sky, light_level, light_level, light_level);
    }

    [[nodiscard]] inline uint8_t get_sky_light_unsafe(int32_t x, int32_t y, int32_t z) const noexcept {
        return unpack_sky(light_packed()[index(x, y, z)]);
    }

    inline void set_sky_light_unsafe(int32_t x, int32_t y, int32_t z, uint8_t light_level) noexcept {
        const std::size_t idx = index(x, y, z);
        const uint16_t v = light_packed()[idx];
        light_packed()[idx] = pack_light(light_level, unpack_r(v), unpack_g(v), unpack_b(v));
    }

    [[nodiscard]] inline uint8_t get_light_r_unsafe(int32_t x, int32_t y, int32_t z) const noexcept {
        return unpack_r(light_packed()[index(x, y, z)]);
    }

    [[nodiscard]] inline uint8_t get_light_g_unsafe(int32_t x, int32_t y, int32_t z) const noexcept {
        return unpack_g(light_packed()[index(x, y, z)]);
    }

    [[nodiscard]] inline uint8_t get_light_b_unsafe(int32_t x, int32_t y, int32_t z) const noexcept {
        return unpack_b(light_packed()[index(x, y, z)]);
    }

    // Returns the raw packed uint16_t for a voxel (sky|r|g|b in 4-bit nibbles).
    // No bounds checking ??? caller must guarantee x,y,z are in [0,CHUNK_*) range.
    [[nodiscard]] inline uint16_t get_light_packed_word_unsafe(int32_t x, int32_t y, int32_t z) const noexcept {
        return light_packed()[index(x, y, z)];
    }

    inline void set_light_rgb_unsafe(int32_t x, int32_t y, int32_t z, uint8_t r, uint8_t g, uint8_t b) noexcept {
        const std::size_t idx = index(x, y, z);
        const uint8_t sky = unpack_sky(light_packed()[idx]);
        light_packed()[idx] = pack_light(sky, r, g, b);
    }

    // -------------------------------------------------------------------------
    // Light Propagation (Flood-fill)
    // -------------------------------------------------------------------------
    void propagate_light() {
        clear_block_light();
        if (is_empty || emissive_count == 0) {
            return;
        }
        propagate_chunk_block_light_additive(*this);
    }

    // -------------------------------------------------------------------------
    // Sky Light Propagation (Column tracing from top)
    // -------------------------------------------------------------------------
    void propagate_sky_light(const ChunkData* chunk_above = nullptr);
    void propagate_sky_light_column(int32_t x, int32_t z, const ChunkData* chunk_above = nullptr);

    // -------------------------------------------------------------------------
    // Raw Data / Serialization
    // -------------------------------------------------------------------------
    void set_data(const BlockID* data, uint32_t /*count*/);

    // -------------------------------------------------------------------------
    // Section Queries
    // -------------------------------------------------------------------------
    void compute_section_flags();
    void compute_fully_solid();

    [[nodiscard]] bool is_section_all_air(int32_t section_index) const noexcept {
        if (section_index < 0 || section_index >= CHUNK_SECTIONS) {
            return false;
        }
        return section_block_count[section_index] == 0;
    }

    // -------------------------------------------------------------------------
    // Iteration
    // -------------------------------------------------------------------------
    template<typename Func>
    [[nodiscard]] bool iterate_blocks(Func&& callback) const {
        if (is_empty) {
            return true;
        }

        for (int32_t s = 0; s < CHUNK_SECTIONS; ++s) {
            if (section_block_count[s] == 0) {
                continue;
            }
            const int32_t y0 = s * SECTION_HEIGHT;
            const int32_t y1 = y0 + SECTION_HEIGHT;
            for (int32_t y = y0; y < y1; ++y) {
                for (int32_t z = 0; z < CHUNK_DEPTH; ++z) {
                    for (int32_t x = 0; x < CHUNK_WIDTH; ++x) {
                        const BlockID block_id = get_block_unsafe(x, y, z);
                        if (block_id != BlockIDs::AIR) {
                            if (!callback(BlockPos{x, y, z}, block_id)) {
                                return false;
                            }
                        }
                    }
                }
            }
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // Neighbor Access
    // -------------------------------------------------------------------------
    [[nodiscard]] BlockID get_neighbor_block(int32_t x, int32_t y, int32_t z,
                                             int32_t dx, int32_t dy, int32_t dz) const noexcept {
        const int32_t nx = x + dx;
        const int32_t ny = y + dy;
        const int32_t nz = z + dz;

        if (nx < 0 || nx >= CHUNK_WIDTH ||
            ny < 0 || ny >= CHUNK_HEIGHT ||
            nz < 0 || nz >= CHUNK_DEPTH) {
            return BlockIDs::AIR; // neighbor chunk responsibility
        }

        return get_block_unsafe(nx, ny, nz);
    }
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_CHUNK_DATA_HPP