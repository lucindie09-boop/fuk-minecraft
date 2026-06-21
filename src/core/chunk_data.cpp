#include "core/chunk_data.hpp"

namespace VoxelEngine {

ChunkData::ChunkData()
    : storage(std::make_unique<ChunkStorage>()) {
    clear();
}

ChunkData::ChunkData(const ChunkData& other)
    : storage(std::make_unique<ChunkStorage>(*other.storage)),
      is_empty(other.is_empty),
      is_fully_solid(other.is_fully_solid),
      block_count(other.block_count),
      emissive_count(other.emissive_count) {
    std::memcpy(section_block_count, other.section_block_count, sizeof(section_block_count));
}

ChunkData& ChunkData::operator=(const ChunkData& other) {
    if (this != &other) {
        *storage = *other.storage;
        is_empty       = other.is_empty;
        is_fully_solid = other.is_fully_solid;
        block_count    = other.block_count;
        emissive_count = other.emissive_count;
        std::memcpy(section_block_count, other.section_block_count, sizeof(section_block_count));
    }
    return *this;
}

ChunkData::ChunkData(ChunkData&& other) noexcept
    : storage(std::move(other.storage)),
      is_empty(other.is_empty),
      is_fully_solid(other.is_fully_solid),
      block_count(other.block_count),
      emissive_count(other.emissive_count) {
    std::memcpy(section_block_count, other.section_block_count, sizeof(section_block_count));
    other.is_empty = true;
    other.is_fully_solid = false;
    other.block_count = 0;
    other.emissive_count = 0;
    std::memset(other.section_block_count, 0, sizeof(other.section_block_count));
}

ChunkData& ChunkData::operator=(ChunkData&& other) noexcept {
    if (this != &other) {
        storage        = std::move(other.storage);
        is_empty       = other.is_empty;
        is_fully_solid = other.is_fully_solid;
        block_count    = other.block_count;
        emissive_count = other.emissive_count;
        std::memcpy(section_block_count, other.section_block_count, sizeof(section_block_count));
        other.is_empty = true;
        other.is_fully_solid = false;
        other.block_count = 0;
        other.emissive_count = 0;
        std::memset(other.section_block_count, 0, sizeof(other.section_block_count));
    }
    return *this;
}

void ChunkData::clear_block_light() noexcept {
    for (int32_t i = 0; i < CHUNK_VOLUME; ++i) {
        light_packed()[i] &= 0x000F;
    }
}

void ChunkData::clear_sky_light() noexcept {
    for (int32_t i = 0; i < CHUNK_VOLUME; ++i) {
        light_packed()[i] &= 0xFFF0;
    }
}

void ChunkData::clear_light() noexcept {
    std::memset(light_packed(), 0, CHUNK_VOLUME * sizeof(uint16_t));
}

void ChunkData::clear() noexcept {
    std::memset(blocks(), 0, CHUNK_VOLUME * sizeof(BlockID));
    clear_light();
    is_empty    = true;
    is_fully_solid = false;
    block_count = 0;
    emissive_count = 0;
    std::memset(section_block_count, 0, sizeof(section_block_count));
}

void ChunkData::fill_blocks(BlockID block_id) noexcept {
    std::fill(blocks(), blocks() + CHUNK_VOLUME, block_id);

    if (block_id == BlockIDs::AIR) {
        is_empty = true;
        is_fully_solid = false;
        block_count = 0;
        emissive_count = 0;
        std::memset(section_block_count, 0, sizeof(section_block_count));
        return;
    }

    is_empty = false;
    block_count = CHUNK_VOLUME;
    emissive_count = is_emissive_block(block_id) ? CHUNK_VOLUME : 0;

    const auto& registry = BlockRegistry::get_instance();
    const auto& block_type = registry.get_block(block_id);
    is_fully_solid = HasProperty(block_type.properties, BlockProperty::Solid) &&
                     HasProperty(block_type.properties, BlockProperty::Opaque);

    const uint32_t blocks_per_section = CHUNK_VOLUME / CHUNK_SECTIONS;
    for (int32_t s = 0; s < CHUNK_SECTIONS; ++s) {
        section_block_count[s] = blocks_per_section;
    }
}

void ChunkData::set_data(const BlockID* data, uint32_t /*count*/) {
    std::memcpy(blocks(), data, CHUNK_VOLUME * sizeof(BlockID));

    block_count    = 0;
    emissive_count = 0;
    std::memset(section_block_count, 0, sizeof(section_block_count));

    for (std::size_t i = 0; i < CHUNK_VOLUME; ++i) {
        const BlockID id = blocks()[i];
        if (id != BlockIDs::AIR) {
            ++block_count;
            if (is_emissive_block(id)) {
                ++emissive_count;
            }
            const int32_t y = static_cast<int32_t>(
                (i / static_cast<std::size_t>(CHUNK_WIDTH)) % static_cast<std::size_t>(CHUNK_HEIGHT)
            );
            ++section_block_count[y / SECTION_HEIGHT];
        }
    }

    is_empty = (block_count == 0);
    compute_fully_solid();
}

void ChunkData::compute_fully_solid() {
    if (is_empty) {
        is_fully_solid = false;
        return;
    }
    const auto& registry = BlockRegistry::get_instance();
    for (std::size_t i = 0; i < CHUNK_VOLUME; ++i) {
        const BlockID id = blocks()[i];
        if (id == BlockIDs::AIR) {
            is_fully_solid = false;
            return;
        }
        const auto& block = registry.get_block(id);
        if (!HasProperty(block.properties, BlockProperty::Solid) ||
            !HasProperty(block.properties, BlockProperty::Opaque)) {
            is_fully_solid = false;
            return;
        }
    }
    is_fully_solid = true;
}

void ChunkData::compute_section_flags() {
    std::memset(section_block_count, 0, sizeof(section_block_count));

    for (std::size_t i = 0; i < CHUNK_VOLUME; ++i) {
        if (blocks()[i] != BlockIDs::AIR) {
            const int32_t y = static_cast<int32_t>(
                (i / static_cast<std::size_t>(CHUNK_WIDTH)) % static_cast<std::size_t>(CHUNK_HEIGHT)
            );
            ++section_block_count[y / SECTION_HEIGHT];
        }
    }
}

void ChunkData::propagate_sky_light(const ChunkData* chunk_above) {
    const BlockRegistry& registry = BlockRegistry::get_instance();
    for (int32_t x = 0; x < CHUNK_WIDTH; ++x) {
        for (int32_t z = 0; z < CHUNK_DEPTH; ++z) {
            uint8_t current_sky_light = chunk_above ? chunk_above->get_sky_light(x, 0, z) : 15;
            for (int32_t y = CHUNK_HEIGHT - 1; y >= 0; --y) {
                const BlockID block_id = get_block_unsafe(x, y, z);
                if (block_id != BlockIDs::AIR) {
                    const BlockType& block_type = registry.get_block(block_id);
                    if (HasProperty(block_type.properties, BlockProperty::Opaque)) {
                        current_sky_light = 0;
                    }
                }
                set_sky_light_unsafe(x, y, z, current_sky_light);
            }
        }
    }
}

void ChunkData::propagate_sky_light_column(int32_t x, int32_t z, const ChunkData* chunk_above) {
    const BlockRegistry& registry = BlockRegistry::get_instance();
    uint8_t current_sky_light = chunk_above ? chunk_above->get_sky_light(x, 0, z) : 15;
    for (int32_t y = CHUNK_HEIGHT - 1; y >= 0; --y) {
        const BlockID block_id = get_block_unsafe(x, y, z);
        if (block_id != BlockIDs::AIR) {
            const BlockType& block_type = registry.get_block(block_id);
            if (HasProperty(block_type.properties, BlockProperty::Opaque)) {
                current_sky_light = 0;
            }
        }
        set_sky_light_unsafe(x, y, z, current_sky_light);
    }
}

} // namespace VoxelEngine
