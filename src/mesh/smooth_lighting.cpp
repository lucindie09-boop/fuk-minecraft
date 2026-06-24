#include "mesh/smooth_lighting.hpp"
#include "core/light_packing.hpp"

namespace VoxelEngine {

// Average 4 packed light values component-wise (rounded)
static uint16_t average_light(const uint16_t lights[4]) {
    int sky = 0, r = 0, g = 0, b = 0;
    for (int i = 0; i < 4; i++) {
        sky += unpack_sky(lights[i]);
        r   += unpack_r(lights[i]);
        g   += unpack_g(lights[i]);
        b   += unpack_b(lights[i]);
    }
    return pack_light(
        static_cast<uint8_t>((sky + 2) / 4),
        static_cast<uint8_t>((r   + 2) / 4),
        static_cast<uint8_t>((g   + 2) / 4),
        static_cast<uint8_t>((b   + 2) / 4)
    );
}

void compute_smooth_light(
    const ChunkNeighborAccessor& accessor,
    int32_t x, int32_t y, int32_t z,
    FaceDirection direction,
    uint16_t light_keys_out[4]
) {
    // Face-adjacent block (center)
    int32_t cx, cy, cz;
    switch (direction) {
        case FaceDirection::Top:    cx = x; cy = y + 1; cz = z; break;
        case FaceDirection::Bottom: cx = x; cy = y - 1; cz = z; break;
        case FaceDirection::Right:  cx = x + 1; cy = y; cz = z; break;
        case FaceDirection::Left:   cx = x - 1; cy = y; cz = z; break;
        case FaceDirection::Front:  cx = x; cy = y; cz = z + 1; break;
        case FaceDirection::Back:   cx = x; cy = y; cz = z - 1; break;
    }
    uint16_t center_light = accessor.get_light_packed(cx, cy, cz);

    switch (direction) {
        case FaceDirection::Top: {
            static constexpr int sx[4] = {-1,  1,  1, -1};
            static constexpr int sz[4] = {-1, -1,  1,  1};
            for (int i = 0; i < 4; i++) {
                uint16_t lights[4] = {
                    center_light,
                    accessor.get_light_packed(cx + sx[i], cy, cz),
                    accessor.get_light_packed(cx, cy, cz + sz[i]),
                    accessor.get_light_packed(cx + sx[i], cy, cz + sz[i])
                };
                light_keys_out[i] = average_light(lights);
            }
            break;
        }
        case FaceDirection::Bottom: {
            static constexpr int sx[4] = {-1,  1,  1, -1};
            static constexpr int sz[4] = {-1, -1,  1,  1};
            for (int i = 0; i < 4; i++) {
                uint16_t lights[4] = {
                    center_light,
                    accessor.get_light_packed(cx + sx[i], cy, cz),
                    accessor.get_light_packed(cx, cy, cz + sz[i]),
                    accessor.get_light_packed(cx + sx[i], cy, cz + sz[i])
                };
                light_keys_out[i] = average_light(lights);
            }
            break;
        }
        case FaceDirection::Right: {
            static constexpr int sy[4] = {-1, -1,  1,  1};
            static constexpr int sz[4] = {-1,  1,  1, -1};
            for (int i = 0; i < 4; i++) {
                uint16_t lights[4] = {
                    center_light,
                    accessor.get_light_packed(cx, cy + sy[i], cz),
                    accessor.get_light_packed(cx, cy, cz + sz[i]),
                    accessor.get_light_packed(cx, cy + sy[i], cz + sz[i])
                };
                light_keys_out[i] = average_light(lights);
            }
            break;
        }
        case FaceDirection::Left: {
            static constexpr int sy[4] = {-1, -1,  1,  1};
            static constexpr int sz[4] = { 1, -1, -1,  1};
            for (int i = 0; i < 4; i++) {
                uint16_t lights[4] = {
                    center_light,
                    accessor.get_light_packed(cx, cy + sy[i], cz),
                    accessor.get_light_packed(cx, cy, cz + sz[i]),
                    accessor.get_light_packed(cx, cy + sy[i], cz + sz[i])
                };
                light_keys_out[i] = average_light(lights);
            }
            break;
        }
        case FaceDirection::Front: {
            static constexpr int sx[4] = { 1, -1, -1,  1};
            static constexpr int sy[4] = {-1, -1,  1,  1};
            for (int i = 0; i < 4; i++) {
                uint16_t lights[4] = {
                    center_light,
                    accessor.get_light_packed(cx + sx[i], cy, cz),
                    accessor.get_light_packed(cx, cy + sy[i], cz),
                    accessor.get_light_packed(cx + sx[i], cy + sy[i], cz)
                };
                light_keys_out[i] = average_light(lights);
            }
            break;
        }
        case FaceDirection::Back: {
            static constexpr int sx[4] = {-1,  1,  1, -1};
            static constexpr int sy[4] = {-1, -1,  1,  1};
            for (int i = 0; i < 4; i++) {
                uint16_t lights[4] = {
                    center_light,
                    accessor.get_light_packed(cx + sx[i], cy, cz),
                    accessor.get_light_packed(cx, cy + sy[i], cz),
                    accessor.get_light_packed(cx + sx[i], cy + sy[i], cz)
                };
                light_keys_out[i] = average_light(lights);
            }
            break;
        }
    }
}

} // namespace VoxelEngine
