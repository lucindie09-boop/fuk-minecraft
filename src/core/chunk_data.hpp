#ifndef FUK_MINECRAFT_CHUNK_DATA_HPP
#define FUK_MINECRAFT_CHUNK_DATA_HPP
#include "core/block_types.hpp"
#include "core/chunk_coords.hpp"
#include "core/light_packing.hpp"
#include <cstdint>
#include <memory>
#include <cstring>
#include <algorithm>
#include <vector>

namespace VoxelEngine {

class ChunkData;
void propagate_chunk_block_light_additive(ChunkData& chunk);

// -------------------------------------------------------------------------
// Paletted section storage: 16^3 = 4096 voxels per section, 8 sections per chunk.
// Uniform sections (single block type) cost just the palette entry.
// Non-uniform sections use bit-width-adaptive palette indexing.
// -------------------------------------------------------------------------
class PaletteStorage {
public:
    static constexpr int SEC_SIZE = 16;
    static constexpr int SECS_PER_DIM = CHUNK_WIDTH / SEC_SIZE; // 2
    static constexpr int NUM_SECTIONS = SECS_PER_DIM * SECS_PER_DIM * SECS_PER_DIM; // 8
    static constexpr int SEC_VOLUME = SEC_SIZE * SEC_SIZE * SEC_SIZE; // 4096

    struct Section {
        std::vector<uint16_t> palette;
        uint8_t bpi = 0; // 0=uniform, 4, 8, or 16
        std::vector<uint8_t> indices; // packed index data

        BlockID get_uniform() const {
            return palette.empty() ? BlockIDs::AIR : static_cast<BlockID>(palette[0]);
        }

        bool is_uniform() const { return palette.size() <= 1; }
    };

    Section sections[NUM_SECTIONS];
    uint16_t light[CHUNK_VOLUME];

    PaletteStorage() {
        for (auto& s : sections) {
            s.palette = {0}; // AIR
        }
        std::memset(light, 0, sizeof(light));
    }

    static int sec_index(int x, int y, int z) {
        return (x / SEC_SIZE) + (y / SEC_SIZE) * SECS_PER_DIM + (z / SEC_SIZE) * SECS_PER_DIM * SECS_PER_DIM;
    }

    static int sec_local(int x, int y, int z) {
        return (x & (SEC_SIZE - 1)) + ((y & (SEC_SIZE - 1)) * SEC_SIZE) + ((z & (SEC_SIZE - 1)) * SEC_SIZE * SEC_SIZE);
    }

    BlockID get_block(int x, int y, int z) const {
        const auto& s = sections[sec_index(x, y, z)];
        if (s.is_uniform()) return s.get_uniform();
        int idx = read_index(s.indices.data(), sec_local(x, y, z), s.bpi);
        return static_cast<BlockID>(s.palette[static_cast<size_t>(idx)]);
    }

    void set_block(int x, int y, int z, BlockID id) {
        auto& s = sections[sec_index(x, y, z)];
        int local = sec_local(x, y, z);
        if (s.is_uniform()) {
            BlockID cur = s.get_uniform();
            if (cur == id) return;
            if (cur == BlockIDs::AIR || cur == id) {
                // Becoming uniform with new id, or was uniform and now becoming more complex
            }
            // uniform -> 2-entry palette
            s.palette = {static_cast<uint16_t>(cur), static_cast<uint16_t>(id)};
            s.bpi = 4;
            s.indices.assign(SEC_VOLUME * 4 / 8, 0);
            write_index(s.indices.data(), local, 1, 4);
            return;
        }
        // Non-uniform: find or add to palette
        auto it = std::find(s.palette.begin(), s.palette.end(), static_cast<uint16_t>(id));
        int pal_idx;
        if (it != s.palette.end()) {
            pal_idx = static_cast<int>(it - s.palette.begin());
        } else {
            pal_idx = static_cast<int>(s.palette.size());
            s.palette.push_back(static_cast<uint16_t>(id));
            if (static_cast<size_t>(pal_idx) >= (size_t{1} << s.bpi)) {
                upgrade(s);
            }
        }
        write_index(s.indices.data(), local, pal_idx, s.bpi);
    }

    void fill_uniform(BlockID id) {
        for (auto& s : sections) {
            s.palette = {static_cast<uint16_t>(id)};
            s.indices.clear();
            s.bpi = 0;
        }
    }

    void fill_from_dense(const BlockID* dense) {
        for (int i = 0; i < NUM_SECTIONS; ++i) {
            build_section(i, dense);
        }
    }

    void build_section(int si, const BlockID* dense) {
        auto& s = sections[si];
        int sx = (si % SECS_PER_DIM) * SEC_SIZE;
        int sy = ((si / SECS_PER_DIM) % SECS_PER_DIM) * SEC_SIZE;
        int sz = (si / (SECS_PER_DIM * SECS_PER_DIM)) * SEC_SIZE;

        // Count unique blocks
        uint16_t seen[256];
        int n_seen = 0;
        for (int dz = 0; dz < SEC_SIZE; ++dz) {
            for (int dy = 0; dy < SEC_SIZE; ++dy) {
                for (int dx = 0; dx < SEC_SIZE; ++dx) {
                    int wx = sx + dx, wy = sy + dy, wz = sz + dz;
                    BlockID id = dense[wx + wy * CHUNK_WIDTH + wz * CHUNK_WIDTH * CHUNK_HEIGHT];
                    bool found = false;
                    for (int k = 0; k < n_seen; ++k) {
                        if (seen[k] == static_cast<uint16_t>(id)) { found = true; break; }
                    }
                    if (!found && n_seen < 256) seen[n_seen++] = static_cast<uint16_t>(id);
                }
            }
        }

        if (n_seen == 0 || (n_seen == 1 && seen[0] == 0)) {
            s.palette = {0};
            s.indices.clear();
            s.bpi = 0;
            return;
        }

        s.palette.assign(seen, seen + n_seen);
        s.bpi = n_seen <= 16 ? 4 : (n_seen <= 256 ? 8 : 16);
        s.indices.assign(SEC_VOLUME * s.bpi / 8, 0);

        for (int dz = 0; dz < SEC_SIZE; ++dz) {
            for (int dy = 0; dy < SEC_SIZE; ++dy) {
                for (int dx = 0; dx < SEC_SIZE; ++dx) {
                    int wx = sx + dx, wy = sy + dy, wz = sz + dz;
                    int local = dx + dy * SEC_SIZE + dz * SEC_SIZE * SEC_SIZE;
                    BlockID id = dense[wx + wy * CHUNK_WIDTH + wz * CHUNK_WIDTH * CHUNK_HEIGHT];
                    int idx = 0;
                    for (int k = 0; k < n_seen; ++k) {
                        if (seen[k] == static_cast<uint16_t>(id)) { idx = k; break; }
                    }
                    write_index(s.indices.data(), local, idx, s.bpi);
                }
            }
        }
    }

    bool all_air() const {
        for (auto& s : sections) {
            if (!s.is_uniform()) return false;
            if (s.get_uniform() != BlockIDs::AIR) return false;
        }
        return true;
    }

    int count_non_air() const {
        int count = 0;
        for (auto& s : sections) {
            if (s.is_uniform()) {
                if (s.get_uniform() != BlockIDs::AIR) count += SEC_VOLUME;
            } else {
                int bpi = s.bpi;
                for (int i = 0; i < SEC_VOLUME; ++i) {
                    int idx = read_index(s.indices.data(), i, bpi);
                    if (idx != 0 || s.palette[0] != 0) {
                        if (static_cast<BlockID>(s.palette[static_cast<size_t>(idx)]) != BlockIDs::AIR)
                            ++count;
                    }
                }
            }
        }
        return count;
    }

    int count_emissive() const {
        int count = 0;
        for (auto& s : sections) {
            if (s.is_uniform()) {
                BlockID id = s.get_uniform();
                if (id != BlockIDs::AIR && is_emissive_fast(id)) count += SEC_VOLUME;
            } else {
                for (size_t pi = 0; pi < s.palette.size(); ++pi) {
                    if (static_cast<BlockID>(s.palette[pi]) != BlockIDs::AIR && is_emissive_fast(static_cast<BlockID>(s.palette[pi]))) {
                        // Count occurrences of this palette entry
                        int bpi = s.bpi;
                        int c = 0;
                        for (int i = 0; i < SEC_VOLUME; ++i) {
                            if (read_index(s.indices.data(), i, bpi) == static_cast<int>(pi)) ++c;
                        }
                        count += c;
                    }
                }
            }
        }
        return count;
    }

    void count_section_blocks(uint32_t out[CHUNK_SECTIONS]) const {
        std::memset(out, 0, CHUNK_SECTIONS * sizeof(uint32_t));
        // CHUNK_SECTIONS = 2 (vertical sections of 16 blocks each)
        // Our 8 sections are different: 2 vertical * 2 x * 2 z
        // Each pair of our sections (same y-level, different x/z) maps to one CHUNK_SECTION
        for (int si = 0; si < NUM_SECTIONS; ++si) {
            int sy = si / (SECS_PER_DIM * SECS_PER_DIM); // 0 or 1
            const auto& s = sections[si];
            if (s.is_uniform()) {
                if (s.get_uniform() != BlockIDs::AIR) out[sy] += SEC_VOLUME;
            } else {
                for (int i = 0; i < SEC_VOLUME; ++i) {
                    int idx = read_index(s.indices.data(), i, s.bpi);
                    if (static_cast<BlockID>(s.palette[static_cast<size_t>(idx)]) != BlockIDs::AIR)
                        out[sy]++;
                }
            }
        }
    }

    // Iterate all non-air blocks in a section
    template<typename F>
    void for_each_block(int si, F&& f) const {
        const auto& s = sections[si];
        if (s.is_uniform()) {
            BlockID id = s.get_uniform();
            if (id == BlockIDs::AIR) return;
            int sx = (si % SECS_PER_DIM) * SEC_SIZE;
            int sy = ((si / SECS_PER_DIM) % SECS_PER_DIM) * SEC_SIZE;
            int sz = (si / (SECS_PER_DIM * SECS_PER_DIM)) * SEC_SIZE;
            for (int dz = 0; dz < SEC_SIZE; ++dz)
                for (int dy = 0; dy < SEC_SIZE; ++dy)
                    for (int dx = 0; dx < SEC_SIZE; ++dx)
                        f(sx + dx, sy + dy, sz + dz, id);
            return;
        }
        int sx = (si % SECS_PER_DIM) * SEC_SIZE;
        int sy = ((si / SECS_PER_DIM) % SECS_PER_DIM) * SEC_SIZE;
        int sz = (si / (SECS_PER_DIM * SECS_PER_DIM)) * SEC_SIZE;
        for (int dz = 0; dz < SEC_SIZE; ++dz) {
            for (int dy = 0; dy < SEC_SIZE; ++dy) {
                for (int dx = 0; dx < SEC_SIZE; ++dx) {
                    int local = dx + dy * SEC_SIZE + dz * SEC_SIZE * SEC_SIZE;
                    int idx = read_index(s.indices.data(), local, s.bpi);
                    BlockID id = static_cast<BlockID>(s.palette[static_cast<size_t>(idx)]);
                    if (id != BlockIDs::AIR) f(sx + dx, sy + dy, sz + dz, id);
                }
            }
        }
    }

    template<typename F>
    void for_each_block_all(int si, F&& f) const {
        const auto& s = sections[si];
        int sx = (si % SECS_PER_DIM) * SEC_SIZE;
        int sy = ((si / SECS_PER_DIM) % SECS_PER_DIM) * SEC_SIZE;
        int sz = (si / (SECS_PER_DIM * SECS_PER_DIM)) * SEC_SIZE;
        if (s.is_uniform()) {
            BlockID id = s.get_uniform();
            for (int dz = 0; dz < SEC_SIZE; ++dz)
                for (int dy = 0; dy < SEC_SIZE; ++dy)
                    for (int dx = 0; dx < SEC_SIZE; ++dx)
                        f(sx + dx, sy + dy, sz + dz, id);
            return;
        }
        for (int dz = 0; dz < SEC_SIZE; ++dz) {
            for (int dy = 0; dy < SEC_SIZE; ++dy) {
                for (int dx = 0; dx < SEC_SIZE; ++dx) {
                    int local = dx + dy * SEC_SIZE + dz * SEC_SIZE * SEC_SIZE;
                    int idx = read_index(s.indices.data(), local, s.bpi);
                    f(sx + dx, sy + dy, sz + dz, static_cast<BlockID>(s.palette[static_cast<size_t>(idx)]));
                }
            }
        }
    }

    bool check_fully_solid(const BlockRegistry& registry) const {
        for (auto& s : sections) {
            if (s.is_uniform()) {
                BlockID id = s.get_uniform();
                if (id == BlockIDs::AIR) return false;
                const auto& bt = registry.get_block(id);
                if (!HasProperty(bt.properties, BlockProperty::Solid) || !HasProperty(bt.properties, BlockProperty::Opaque))
                    return false;
            } else {
                for (int i = 0; i < SEC_VOLUME; ++i) {
                    int idx = read_index(s.indices.data(), i, s.bpi);
                    BlockID id = static_cast<BlockID>(s.palette[static_cast<size_t>(idx)]);
                    if (id == BlockIDs::AIR) return false;
                    const auto& bt = registry.get_block(id);
                    if (!HasProperty(bt.properties, BlockProperty::Solid) || !HasProperty(bt.properties, BlockProperty::Opaque))
                        return false;
                }
            }
        }
        return true;
    }

private:
    static bool is_emissive_fast(BlockID id) {
        return HasProperty(BlockRegistry::get_instance().get_block_fast(id).properties, BlockProperty::Emissive);
    }

    static int read_index(const uint8_t* data, int local, int bpi) {
        switch (bpi) {
            case 4: {
                int byte_idx = local / 2;
                int nibble = local & 1;
                return (data[byte_idx] >> (nibble * 4)) & 0xF;
            }
            case 8: return data[local];
            case 16: return data[local * 2] | (data[local * 2 + 1] << 8);
            default: return 0;
        }
    }

    static void write_index(uint8_t* data, int local, int val, int bpi) {
        switch (bpi) {
            case 4: {
                int byte_idx = local / 2;
                int nibble = local & 1;
                data[byte_idx] = (data[byte_idx] & (0xF0 >> (nibble * 4))) | static_cast<uint8_t>((val & 0xF) << (nibble * 4));
                break;
            }
            case 8: data[local] = static_cast<uint8_t>(val); break;
            case 16: data[local * 2] = static_cast<uint8_t>(val & 0xFF); data[local * 2 + 1] = static_cast<uint8_t>((val >> 8) & 0xFF); break;
        }
    }

    void upgrade(Section& s) {
        int new_bpi = s.bpi == 4 ? 8 : 16;
        std::vector<uint8_t> new_indices(SEC_VOLUME * new_bpi / 8, 0);
        for (int i = 0; i < SEC_VOLUME; ++i) {
            int old_val = read_index(s.indices.data(), i, s.bpi);
            write_index(new_indices.data(), i, old_val, new_bpi);
        }
        s.indices = std::move(new_indices);
        s.bpi = static_cast<uint8_t>(new_bpi);
    }
};

// -------------------------------------------------------------------------
// Chunk Data Storage
// -------------------------------------------------------------------------
class ChunkData {
private:
    std::unique_ptr<PaletteStorage> storage;

    bool     is_empty = true;
    bool     is_fully_solid = false;
    uint32_t block_count = 0;
    uint32_t emissive_count = 0;

    // Per-section non-air block count (2 sections of 16 vertical blocks each).
    uint32_t section_block_count[CHUNK_SECTIONS];

    [[nodiscard]] static inline bool is_emissive_block(BlockID id) noexcept {
        return HasProperty(BlockRegistry::get_instance().get_block_fast(id).properties, BlockProperty::Emissive);
    }

    [[nodiscard]] static inline constexpr std::size_t index(int32_t x, int32_t y, int32_t z) noexcept {
        return static_cast<std::size_t>(x + y * CHUNK_WIDTH + z * CHUNK_WIDTH * CHUNK_HEIGHT);
    }

public:
    // Raw pointer accessors removed — use per-voxel API instead.
    // For the one caller that still needs bulk reads (mesh_builder_greedy.cpp),
    // materialize_dense_blocks() provides a temporary dense buffer.

    uint16_t* light_packed() noexcept { return storage->light; }
    const uint16_t* light_packed() const noexcept { return storage->light; }

    ChunkData();
    ChunkData(const ChunkData& other);
    ChunkData& operator=(const ChunkData& other);
    ChunkData(ChunkData&& other) noexcept;
    ChunkData& operator=(ChunkData&& other) noexcept;

    // Bulk Clear
    void clear_block_light() noexcept;
    void clear_sky_light() noexcept;
    void clear_light() noexcept;
    void clear() noexcept;

    // Block Access
    [[nodiscard]] BlockID get_block(int32_t x, int32_t y, int32_t z) const noexcept {
        if (x < 0 || x >= CHUNK_WIDTH ||
            y < 0 || y >= CHUNK_HEIGHT ||
            z < 0 || z >= CHUNK_DEPTH) {
            return BlockIDs::AIR;
        }
        return storage->get_block(x, y, z);
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

        const BlockID old_block = storage->get_block(x, y, z);
        if (old_block == block_id) return;

        storage->set_block(x, y, z, block_id);

        if (is_emissive_block(old_block) && emissive_count > 0) --emissive_count;
        if (is_emissive_block(block_id)) ++emissive_count;

        if (old_block == BlockIDs::AIR && block_id != BlockIDs::AIR) ++block_count;
        else if (old_block != BlockIDs::AIR && block_id == BlockIDs::AIR) --block_count;
        is_empty = (block_count == 0);

        const int32_t section = y / SECTION_HEIGHT;
        if (old_block == BlockIDs::AIR && block_id != BlockIDs::AIR) ++section_block_count[section];
        else if (old_block != BlockIDs::AIR && block_id == BlockIDs::AIR) {
            if (section_block_count[section] > 0) --section_block_count[section];
        }

        if (is_fully_solid) {
            if (block_id == BlockIDs::AIR) is_fully_solid = false;
            else {
                const auto& registry = BlockRegistry::get_instance();
                const auto& bt = registry.get_block(block_id);
                if (!HasProperty(bt.properties, BlockProperty::Solid) ||
                    !HasProperty(bt.properties, BlockProperty::Opaque))
                    is_fully_solid = false;
            }
        }
    }

    void set_block(const BlockPos& pos, BlockID block_id) noexcept {
        set_block(pos.x, pos.y, pos.z, block_id);
    }

    // Bulk fast fill
    void fill_blocks(BlockID block_id) noexcept;

    [[nodiscard]] bool is_all_air() const noexcept { return is_empty; }
    [[nodiscard]] bool fully_solid() const noexcept { return is_fully_solid; }
    [[nodiscard]] uint32_t get_block_count() const noexcept { return block_count; }
    [[nodiscard]] uint32_t get_emissive_count() const noexcept { return emissive_count; }

    // Light Access
    [[nodiscard]] uint8_t get_light(int32_t x, int32_t y, int32_t z) const noexcept {
        if (x < 0 || x >= CHUNK_WIDTH || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) return 0;
        const uint16_t v = light_packed()[index(x, y, z)];
        const uint8_t r = unpack_r(v), g = unpack_g(v), b = unpack_b(v);
        return r > g ? (r > b ? r : b) : (g > b ? g : b);
    }

    void set_light(int32_t x, int32_t y, int32_t z, uint8_t light_level) noexcept {
        if (x < 0 || x >= CHUNK_WIDTH || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) return;
        const std::size_t idx = index(x, y, z);
        light_packed()[idx] = pack_light(unpack_sky(light_packed()[idx]), light_level, light_level, light_level);
    }

    [[nodiscard]] uint8_t get_sky_light(int32_t x, int32_t y, int32_t z) const noexcept {
        if (x < 0 || x >= CHUNK_WIDTH || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) return 0;
        return unpack_sky(light_packed()[index(x, y, z)]);
    }

    void set_sky_light(int32_t x, int32_t y, int32_t z, uint8_t light_level) noexcept {
        if (x < 0 || x >= CHUNK_WIDTH || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) return;
        const std::size_t idx = index(x, y, z);
        const uint16_t v = light_packed()[idx];
        light_packed()[idx] = pack_light(light_level, unpack_r(v), unpack_g(v), unpack_b(v));
    }

    [[nodiscard]] uint8_t get_light_r(int32_t x, int32_t y, int32_t z) const noexcept {
        if (x < 0 || x >= CHUNK_WIDTH || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) return 0;
        return unpack_r(light_packed()[index(x, y, z)]);
    }

    [[nodiscard]] uint8_t get_light_g(int32_t x, int32_t y, int32_t z) const noexcept {
        if (x < 0 || x >= CHUNK_WIDTH || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) return 0;
        return unpack_g(light_packed()[index(x, y, z)]);
    }

    [[nodiscard]] uint8_t get_light_b(int32_t x, int32_t y, int32_t z) const noexcept {
        if (x < 0 || x >= CHUNK_WIDTH || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) return 0;
        return unpack_b(light_packed()[index(x, y, z)]);
    }

    void set_light_rgb(int32_t x, int32_t y, int32_t z, uint8_t r, uint8_t g, uint8_t b) noexcept {
        if (x < 0 || x >= CHUNK_WIDTH || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) return;
        const std::size_t idx = index(x, y, z);
        light_packed()[idx] = pack_light(unpack_sky(light_packed()[idx]), r, g, b);
    }

    [[nodiscard]] const uint16_t* get_light_packed_data() const noexcept { return light_packed(); }

    // Unchecked accessors for hot paths
    [[nodiscard]] inline BlockID get_block_unsafe(int32_t x, int32_t y, int32_t z) const noexcept {
        return storage->get_block(x, y, z);
    }

    [[nodiscard]] inline uint8_t get_light_unsafe(int32_t x, int32_t y, int32_t z) const noexcept {
        const uint16_t v = light_packed()[index(x, y, z)];
        const uint8_t r = unpack_r(v), g = unpack_g(v), b = unpack_b(v);
        return r > g ? (r > b ? r : b) : (g > b ? g : b);
    }

    inline void set_light_unsafe(int32_t x, int32_t y, int32_t z, uint8_t light_level) noexcept {
        const std::size_t idx = index(x, y, z);
        light_packed()[idx] = pack_light(unpack_sky(light_packed()[idx]), light_level, light_level, light_level);
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

    [[nodiscard]] inline uint16_t get_light_packed_word_unsafe(int32_t x, int32_t y, int32_t z) const noexcept {
        return light_packed()[index(x, y, z)];
    }

    inline void set_light_rgb_unsafe(int32_t x, int32_t y, int32_t z, uint8_t r, uint8_t g, uint8_t b) noexcept {
        const std::size_t idx = index(x, y, z);
        light_packed()[idx] = pack_light(unpack_sky(light_packed()[idx]), r, g, b);
    }

    // Light Propagation
    void propagate_light() {
        clear_block_light();
        if (is_empty || emissive_count == 0) return;
        propagate_chunk_block_light_additive(*this);
    }

    void propagate_sky_light(const ChunkData* chunk_above = nullptr);
    void propagate_sky_light_column(int32_t x, int32_t z, const ChunkData* chunk_above = nullptr);

    // Bulk load from dense array
    void set_data(const BlockID* data, uint32_t count);

    // Section Queries
    void compute_section_flags();
    void compute_fully_solid();

    [[nodiscard]] bool is_section_all_air(int32_t section_index) const noexcept {
        if (section_index < 0 || section_index >= CHUNK_SECTIONS) return false;
        return section_block_count[section_index] == 0;
    }

    template<typename Func>
    [[nodiscard]] bool iterate_blocks(Func&& callback) const {
        if (is_empty) return true;
        for (int si = 0; si < PaletteStorage::NUM_SECTIONS; ++si) {
            // Skip check at section level if possible
            bool any = false;
            storage->for_each_block(si, [&](int x, int y, int z, BlockID id) {
                any = true;
                if (!callback(BlockPos{x, y, z}, id)) return false;
                return true;
            });
            if (!any) continue;
        }
        return true;
    }

    [[nodiscard]] BlockID get_neighbor_block(int32_t x, int32_t y, int32_t z,
                                             int32_t dx, int32_t dy, int32_t dz) const noexcept {
        const int32_t nx = x + dx, ny = y + dy, nz = z + dz;
        if (nx < 0 || nx >= CHUNK_WIDTH || ny < 0 || ny >= CHUNK_HEIGHT || nz < 0 || nz >= CHUNK_DEPTH)
            return BlockIDs::AIR;
        return get_block_unsafe(nx, ny, nz);
    }

};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_CHUNK_DATA_HPP
