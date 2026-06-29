#include "engine/collision_resolver.hpp"
#include "core/chunk_map.hpp"
#include <cmath>
#include <algorithm>

namespace VoxelEngine {

using namespace godot;

// Check if any block in a 2D slab (voxel plane) is solid.
// `axis` is the normal axis of the slab, `slab_coord` is the voxel coordinate on that axis.
// Used by swept-AABB DDA: when an AABB face crosses a voxel boundary, we only need to check
// the newly entered slab rather than the full AABB volume.
static bool slab_is_solid(const ChunkMap* chunk_map, int axis, int32_t slab_coord,
                          const Vector3& pos, const Vector3& size) {
    int32_t min_y = static_cast<int32_t>(std::floor(pos.y));
    int32_t max_y = static_cast<int32_t>(std::floor(pos.y + size.y));
    int32_t min_z = static_cast<int32_t>(std::floor(pos.z));
    int32_t max_z = static_cast<int32_t>(std::floor(pos.z + size.z));
    int32_t min_x = static_cast<int32_t>(std::floor(pos.x));
    int32_t max_x = static_cast<int32_t>(std::floor(pos.x + size.x));

    switch (axis) {
    case 0:
        for (int32_t y = min_y; y <= max_y; ++y)
            for (int32_t z = min_z; z <= max_z; ++z)
                if (chunk_map->is_block_solid_fast(slab_coord, y, z))
                    return true;
        break;
    case 1:
        for (int32_t x = min_x; x <= max_x; ++x)
            for (int32_t z = min_z; z <= max_z; ++z)
                if (chunk_map->is_block_solid_fast(x, slab_coord, z))
                    return true;
        break;
    case 2:
        for (int32_t x = min_x; x <= max_x; ++x)
            for (int32_t y = min_y; y <= max_y; ++y)
                if (chunk_map->is_block_solid_fast(x, y, slab_coord))
                    return true;
        break;
    }
    return false;
}

// Resolve a single axis using slab-DDA.
//
// Instead of discrete-stepping the full AABB and binary-searching on contact,
// we step the leading face to the next voxel boundary and check only the newly
// entered slab. If it's solid, the exact collision point is known directly
// (boundary minus AABB extent) — no bisection needed.
static void resolve_axis(const Vector3& /*position*/,
                         const Vector3& motion,
                         const Vector3& size,
                         int axis,
                         Vector3& result,
                         bool& collided,
                         const ChunkMap* chunk_map) {
    if (std::abs(motion[axis]) < 0.0001f) {
        collided = false;
        return;
    }

    float dir = motion[axis] > 0.0f ? 1.0f : -1.0f;
    float remaining = std::abs(motion[axis]);

    while (remaining > 0.001f) {
        float leading = result[axis] + (dir > 0.0f ? size[axis] : 0.0f);
        int32_t old_slab = static_cast<int32_t>(std::floor(leading));

        // Distance to the next voxel boundary in the direction of travel
        float dist;
        if (dir > 0.0f) {
            float next_boundary = std::floor(leading) + 1.0f;
            dist = next_boundary - leading;
        } else {
            float next_boundary = std::ceil(leading) - 1.0f;
            dist = leading - next_boundary;
        }

        // Sitting exactly on a boundary — step a full voxel
        if (dist < 0.0001f) {
            dist = 1.0f;
        }

        float step = std::min(dist, remaining);
        float new_leading = leading + dir * step;
        int32_t new_slab = static_cast<int32_t>(std::floor(new_leading));

        // Check if the leading face crossed into a new voxel slab
        bool crossed = (dir > 0.0f) ? (new_slab > old_slab) : (new_slab < old_slab);
        if (crossed) {
            // Only check the newly entered slab (1-voxel-thick plane), not the full AABB volume.
            // The other axes' positions are unchanged at this point.
            Vector3 check_pos = result;
            check_pos[axis] += dir * step;

            if (slab_is_solid(chunk_map, axis, new_slab, check_pos, size)) {
                collided = true;
                // Exact collision point: push the trailing edge flush against the block face
                if (dir > 0.0f) {
                    result[axis] = static_cast<float>(old_slab + 1) - size[axis];
                } else {
                    result[axis] = static_cast<float>(old_slab);
                }
                break;
            }
        }

        result[axis] += dir * step;
        remaining -= step;
    }
}

CollisionResolver::CollisionResult CollisionResolver::resolve(
    const Vector3& position,
    const Vector3& motion,
    const Vector3& size
) const {
    Vector3 result = position;
    CollisionResult out;

    auto lock = chunk_map_->acquire_shared_lock();

    for (int axis = 0; axis < 3; ++axis) {
        bool collided = false;
        resolve_axis(position, motion, size, axis, result, collided, chunk_map_);
        if (axis == 0) out.collided_x = collided;
        else if (axis == 1) out.collided_y = collided;
        else out.collided_z = collided;
    }

    out.position = result;

    AABB floor_aabb(result, size);
    floor_aabb.position.y -= 0.05f;
    out.on_floor = is_aabb_solid_fast(floor_aabb);

    return out;
}

bool CollisionResolver::is_aabb_solid_fast(const AABB& aabb) const {
    if (!chunk_map_) return false;
    int32_t min_x = static_cast<int32_t>(std::floor(aabb.position.x));
    int32_t min_y = static_cast<int32_t>(std::floor(aabb.position.y));
    int32_t min_z = static_cast<int32_t>(std::floor(aabb.position.z));
    int32_t max_x = static_cast<int32_t>(std::floor(aabb.position.x + aabb.size.x));
    int32_t max_y = static_cast<int32_t>(std::floor(aabb.position.y + aabb.size.y));
    int32_t max_z = static_cast<int32_t>(std::floor(aabb.position.z + aabb.size.z));

    for (int32_t y = min_y; y <= max_y; ++y) {
        for (int32_t z = min_z; z <= max_z; ++z) {
            for (int32_t x = min_x; x <= max_x; ++x) {
                if (chunk_map_->is_block_solid_fast(x, y, z)) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool CollisionResolver::is_aabb_solid(const AABB& aabb) const {
    auto lock = chunk_map_->acquire_shared_lock();
    return is_aabb_solid_fast(aabb);
}

} // namespace VoxelEngine
