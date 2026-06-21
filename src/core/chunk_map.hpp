#ifndef FUK_MINECRAFT_CHUNK_MAP_HPP
#define FUK_MINECRAFT_CHUNK_MAP_HPP
#include "core/chunk_types.hpp"
#include "core/chunk_data.hpp"
#include "core/block_types.hpp"
#include <godot_cpp/variant/vector3.hpp>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <functional>

namespace VoxelEngine {

// -------------------------------------------------------------------------
// Chunk map — owns the chunk storage and provides thread-safe accessors.
// This is the single source of truth for which chunks are loaded.
//
// Chunks are stored as unique_ptr because the map has exclusive ownership.
// Callers receive raw pointers/references for non-owning access.
// -------------------------------------------------------------------------
class ChunkMap {
public:
    [[nodiscard]] inline uint64_t get_chunk_key(int32_t x, int32_t y, int32_t z) const noexcept {
        // Encode 3 signed 21-bit coordinates into 63 bits of a uint64_t.
        // Range: [-1,048,576, +1,048,575] per axis — plenty for any voxel world.
        constexpr uint32_t OFFSET = 1u << 20;
        constexpr uint32_t MASK = 0x1FFFFF;
        uint64_t ux = static_cast<uint64_t>((static_cast<uint32_t>(x) + OFFSET) & MASK);
        uint64_t uy = static_cast<uint64_t>((static_cast<uint32_t>(y) + OFFSET) & MASK);
        uint64_t uz = static_cast<uint64_t>((static_cast<uint32_t>(z) + OFFSET) & MASK);
        return (ux << 42) | (uy << 21) | uz;
    }

    static inline void decode_chunk_key(uint64_t key, int32_t& x, int32_t& y, int32_t& z) noexcept {
        constexpr uint32_t OFFSET = 1u << 20;
        constexpr uint32_t MASK = 0x1FFFFF;
        x = static_cast<int32_t>((static_cast<uint32_t>((key >> 42) & MASK)) - OFFSET);
        y = static_cast<int32_t>((static_cast<uint32_t>((key >> 21) & MASK)) - OFFSET);
        z = static_cast<int32_t>((static_cast<uint32_t>(key & MASK)) - OFFSET);
    }

    void get_chunk_coords(const godot::Vector3& world_pos, int32_t& chunk_x, int32_t& chunk_y, int32_t& chunk_z) const noexcept {
        int32_t wx = static_cast<int32_t>(world_pos.x);
        int32_t wy = static_cast<int32_t>(world_pos.y);
        int32_t wz = static_cast<int32_t>(world_pos.z);
        int32_t dummy_x, dummy_y, dummy_z;
        world_to_chunk_local(wx, wy, wz, chunk_x, chunk_y, chunk_z, dummy_x, dummy_y, dummy_z);
    }

    [[nodiscard]] ChunkData* get_chunk_data(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z) const {
        std::shared_lock lock(chunks_mutex);
        auto it = chunks.find(get_chunk_key(chunk_x, chunk_y, chunk_z));
        if (it != chunks.end()) {
            return it->second->data.get();
        }
        return nullptr;
    }

    [[nodiscard]] ChunkRenderData* get_chunk_render_data(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z) const {
        std::shared_lock lock(chunks_mutex);
        auto it = chunks.find(get_chunk_key(chunk_x, chunk_y, chunk_z));
        if (it != chunks.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    [[nodiscard]] bool has_loaded_chunk(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z) const {
        std::shared_lock lock(chunks_mutex);
        return chunks.find(get_chunk_key(chunk_x, chunk_y, chunk_z)) != chunks.end();
    }

    [[nodiscard]] bool is_block_solid(int32_t world_x, int32_t world_y, int32_t world_z) const {
        std::shared_lock lock(chunks_mutex);
        int32_t chunk_x, chunk_y, chunk_z, local_x, local_y, local_z;
        world_to_chunk_local(world_x, world_y, world_z, chunk_x, chunk_y, chunk_z, local_x, local_y, local_z);
        uint64_t key = get_chunk_key(chunk_x, chunk_y, chunk_z);

        auto it = chunks.find(key);
        if (it == chunks.end()) return false;

        BlockID block_id = static_cast<BlockID>(it->second->data->get_block(local_x, local_y, local_z));
        return block_id != BlockIDs::AIR;
    }

    // Fast path: assumes caller already holds shared_lock on chunks_mutex
    [[nodiscard]] bool is_block_solid_fast(int32_t world_x, int32_t world_y, int32_t world_z) const {
        int32_t chunk_x, chunk_y, chunk_z, local_x, local_y, local_z;
        world_to_chunk_local(world_x, world_y, world_z, chunk_x, chunk_y, chunk_z, local_x, local_y, local_z);
        uint64_t key = get_chunk_key(chunk_x, chunk_y, chunk_z);
        auto it = chunks.find(key);
        if (it == chunks.end()) return false;
        BlockID block_id = static_cast<BlockID>(it->second->data->get_block(local_x, local_y, local_z));
        return block_id != BlockIDs::AIR;
    }

    [[nodiscard]] int get_block_world(int32_t world_x, int32_t world_y, int32_t world_z) const {
        std::shared_lock lock(chunks_mutex);
        int32_t chunk_x, chunk_y, chunk_z, local_x, local_y, local_z;
        world_to_chunk_local(world_x, world_y, world_z, chunk_x, chunk_y, chunk_z, local_x, local_y, local_z);

        auto it = chunks.find(get_chunk_key(chunk_x, chunk_y, chunk_z));
        if (it == chunks.end()) {
            return static_cast<int>(BlockIDs::AIR);
        }

        return static_cast<int>(it->second->data->get_block(local_x, local_y, local_z));
    }

    // Fast path: assumes caller already holds shared_lock on chunks_mutex
    [[nodiscard]] int get_block_world_fast(int32_t world_x, int32_t world_y, int32_t world_z) const {
        int32_t chunk_x, chunk_y, chunk_z, local_x, local_y, local_z;
        world_to_chunk_local(world_x, world_y, world_z, chunk_x, chunk_y, chunk_z, local_x, local_y, local_z);
        auto it = chunks.find(get_chunk_key(chunk_x, chunk_y, chunk_z));
        if (it == chunks.end()) {
            return static_cast<int>(BlockIDs::AIR);
        }
        return static_cast<int>(it->second->data->get_block(local_x, local_y, local_z));
    }

    // Fast path: assumes caller already holds shared_lock on chunks_mutex
    [[nodiscard]] ChunkData* get_chunk_data_fast(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z) const {
        auto it = chunks.find(get_chunk_key(chunk_x, chunk_y, chunk_z));
        if (it != chunks.end()) {
            return it->second->data.get();
        }
        return nullptr;
    }

    [[nodiscard]] bool contains_fast(uint64_t key) const {
        return chunks.find(key) != chunks.end();
    }

    // Acquire a shared lock for batch reads (e.g. raycast, light propagation)
    [[nodiscard]] std::shared_lock<std::shared_mutex> acquire_shared_lock() const {
        return std::shared_lock<std::shared_mutex>(chunks_mutex);
    }

    [[nodiscard]] size_t size() const {
        std::shared_lock lock(chunks_mutex);
        return chunks.size();
    }

    void clear() {
        std::unique_lock lock(chunks_mutex);
        chunks.clear();
    }

    void erase(uint64_t key) {
        std::unique_lock lock(chunks_mutex);
        chunks.erase(key);
    }

    void insert(uint64_t key, std::unique_ptr<ChunkRenderData> render_data) {
        std::unique_lock lock(chunks_mutex);
        chunks[key] = std::move(render_data);
    }

    [[nodiscard]] bool contains(uint64_t key) const {
        std::shared_lock lock(chunks_mutex);
        return chunks.find(key) != chunks.end();
    }

    void reserve(size_t n) {
        std::unique_lock lock(chunks_mutex);
        chunks.reserve(n);
    }

    // Thread-safe iteration over a snapshot of keys/values.
    // Holds a shared lock for the entire duration of the callback.
    template<typename Callback>
    void for_each(Callback&& callback) {
        std::shared_lock lock(chunks_mutex);
        for (auto& pair : chunks) {
            callback(pair.first, pair.second);
        }
    }

    template<typename Callback>
    void for_each(Callback&& callback) const {
        std::shared_lock lock(chunks_mutex);
        for (auto& pair : chunks) {
            callback(pair.first, pair.second);
        }
    }

    // Iterate at most `max_count` entries. Returns true if all entries were visited.
    // NOTE: always starts at chunks.begin() — with an unordered_map larger than
    // max_count, this revisits the same leading bucket-order entries on every
    // call and never reaches the rest of the map. Prefer for_each_limited_resumable
    // for any periodic/incremental scan (e.g. unload checks).
    template<typename Callback>
    bool for_each_limited(Callback&& callback, size_t max_count) const {
        std::shared_lock lock(chunks_mutex);
        size_t count = 0;
        for (auto& pair : chunks) {
            if (count >= max_count) return false;
            callback(pair.first, pair.second);
            count++;
        }
        return true;
    }

    // Resumable bucket-walk: visits at most `max_count` entries starting from
    // `bucket_cursor` (caller-owned, persisted across calls) and advances the
    // cursor so the next call continues where this one left off. Guarantees
    // every entry gets visited periodically regardless of map size, unlike
    // for_each_limited which always restarts at the beginning.
    //
    // Safe across rehashes: if bucket_count() shrinks/grows between calls,
    // the cursor is simply clamped — worst case is a few entries skipped or
    // re-visited once, which is fine for a periodic eviction sweep.
    template<typename Callback>
    void for_each_limited_resumable(Callback&& callback, size_t max_count, size_t& bucket_cursor) const {
        std::shared_lock lock(chunks_mutex);
        const size_t n_buckets = chunks.bucket_count();
        if (n_buckets == 0) {
            bucket_cursor = 0;
            return;
        }
        if (bucket_cursor >= n_buckets) {
            bucket_cursor = 0;
        }

        size_t visited = 0;
        size_t buckets_scanned = 0;
        size_t b = bucket_cursor;

        while (buckets_scanned < n_buckets && visited < max_count) {
            for (auto it = chunks.begin(b); it != chunks.end(b); ++it) {
                callback(it->first, it->second);
                ++visited;
                if (visited >= max_count) {
                    bucket_cursor = b;
                    return;
                }
            }
            b = (b + 1) % n_buckets;
            ++buckets_scanned;
        }
        bucket_cursor = b;
    }

    // Batch neighbor lookup under a single shared lock.
    // out[0] = (-X, 0, 0), out[1] = (+X, 0, 0), out[2] = (0, -Y, 0),
    // out[3] = (0, +Y, 0), out[4] = (0, 0, -Z), out[5] = (0, 0, +Z)
    void get_neighbors(int32_t cx, int32_t cy, int32_t cz, ChunkRenderData* out[6]) const {
        std::shared_lock lock(chunks_mutex);
        auto it0 = chunks.find(get_chunk_key(cx - 1, cy,     cz    ));
        out[0] = (it0 != chunks.end()) ? it0->second.get() : nullptr;
        auto it1 = chunks.find(get_chunk_key(cx + 1, cy,     cz    ));
        out[1] = (it1 != chunks.end()) ? it1->second.get() : nullptr;
        auto it2 = chunks.find(get_chunk_key(cx,     cy - 1, cz    ));
        out[2] = (it2 != chunks.end()) ? it2->second.get() : nullptr;
        auto it3 = chunks.find(get_chunk_key(cx,     cy + 1, cz    ));
        out[3] = (it3 != chunks.end()) ? it3->second.get() : nullptr;
        auto it4 = chunks.find(get_chunk_key(cx,     cy,     cz - 1));
        out[4] = (it4 != chunks.end()) ? it4->second.get() : nullptr;
        auto it5 = chunks.find(get_chunk_key(cx,     cy,     cz + 1));
        out[5] = (it5 != chunks.end()) ? it5->second.get() : nullptr;
    }

    // Batch orthogonal + diagonal neighbor lookup under a single shared lock.
    // diag[0] = (-X, 0, -Z), diag[1] = (-X, 0, +Z), diag[2] = (+X, 0, -Z), diag[3] = (+X, 0, +Z)
    void get_extended_neighbors(int32_t cx, int32_t cy, int32_t cz,
                                ChunkRenderData* ortho[6],
                                ChunkRenderData* diag[4]) const {
        std::shared_lock lock(chunks_mutex);
        auto it0 = chunks.find(get_chunk_key(cx - 1, cy,     cz    ));
        ortho[0] = (it0 != chunks.end()) ? it0->second.get() : nullptr;
        auto it1 = chunks.find(get_chunk_key(cx + 1, cy,     cz    ));
        ortho[1] = (it1 != chunks.end()) ? it1->second.get() : nullptr;
        auto it2 = chunks.find(get_chunk_key(cx,     cy - 1, cz    ));
        ortho[2] = (it2 != chunks.end()) ? it2->second.get() : nullptr;
        auto it3 = chunks.find(get_chunk_key(cx,     cy + 1, cz    ));
        ortho[3] = (it3 != chunks.end()) ? it3->second.get() : nullptr;
        auto it4 = chunks.find(get_chunk_key(cx,     cy,     cz - 1));
        ortho[4] = (it4 != chunks.end()) ? it4->second.get() : nullptr;
        auto it5 = chunks.find(get_chunk_key(cx,     cy,     cz + 1));
        ortho[5] = (it5 != chunks.end()) ? it5->second.get() : nullptr;
        auto it6 = chunks.find(get_chunk_key(cx - 1, cy,     cz - 1));
        diag[0] = (it6 != chunks.end()) ? it6->second.get() : nullptr;
        auto it7 = chunks.find(get_chunk_key(cx - 1, cy,     cz + 1));
        diag[1] = (it7 != chunks.end()) ? it7->second.get() : nullptr;
        auto it8 = chunks.find(get_chunk_key(cx + 1, cy,     cz - 1));
        diag[2] = (it8 != chunks.end()) ? it8->second.get() : nullptr;
        auto it9 = chunks.find(get_chunk_key(cx + 1, cy,     cz + 1));
        diag[3] = (it9 != chunks.end()) ? it9->second.get() : nullptr;
    }

    // Batch ChunkData neighbor lookup under a single shared lock.
    void get_neighbor_data(int32_t cx, int32_t cy, int32_t cz, ChunkData* out[6]) const {
        std::shared_lock lock(chunks_mutex);
        auto it = chunks.find(get_chunk_key(cx - 1, cy,     cz    ));
        out[0] = (it != chunks.end()) ? it->second->data.get() : nullptr;
        it = chunks.find(get_chunk_key(cx + 1, cy,     cz    ));
        out[1] = (it != chunks.end()) ? it->second->data.get() : nullptr;
        it = chunks.find(get_chunk_key(cx,     cy - 1, cz    ));
        out[2] = (it != chunks.end()) ? it->second->data.get() : nullptr;
        it = chunks.find(get_chunk_key(cx,     cy + 1, cz    ));
        out[3] = (it != chunks.end()) ? it->second->data.get() : nullptr;
        it = chunks.find(get_chunk_key(cx,     cy,     cz - 1));
        out[4] = (it != chunks.end()) ? it->second->data.get() : nullptr;
        it = chunks.find(get_chunk_key(cx,     cy,     cz + 1));
        out[5] = (it != chunks.end()) ? it->second->data.get() : nullptr;
    }

    [[nodiscard]] bool has_any_solid_above(int32_t cx, int32_t cy_start, int32_t cz) const {
        std::shared_lock lock(chunks_mutex);
        constexpr int32_t MAX_COLUMN_SEARCH = 128;
        for (int32_t y = cy_start; y < cy_start + MAX_COLUMN_SEARCH; ++y) {
            auto it = chunks.find(get_chunk_key(cx, y, cz));
            if (it == chunks.end()) return false;
            if (!it->second->data->is_all_air()) return true;
        }
        return true;
    }

    // Find-and-erase under a single unique lock. Returns the erased
    // ChunkRenderData if the key existed, otherwise nullptr.
    [[nodiscard]] std::unique_ptr<ChunkRenderData> find_and_erase(uint64_t key) {
        std::unique_lock lock(chunks_mutex);
        auto it = chunks.find(key);
        if (it == chunks.end()) return nullptr;
        auto result = std::move(it->second);
        chunks.erase(it);
        return result;
    }

    // Conditional find-and-erase. Only erases if the predicate returns true.
    template<typename Pred>
    [[nodiscard]] std::unique_ptr<ChunkRenderData> find_and_erase_if(uint64_t key, Pred&& predicate) {
        std::unique_lock lock(chunks_mutex);
        auto it = chunks.find(key);
        if (it == chunks.end()) return nullptr;
        if (!predicate(*it->second)) return nullptr;
        auto result = std::move(it->second);
        chunks.erase(it);
        return result;
    }

private:
    std::unordered_map<uint64_t, std::unique_ptr<ChunkRenderData>> chunks;
    mutable std::shared_mutex chunks_mutex;
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_CHUNK_MAP_HPP