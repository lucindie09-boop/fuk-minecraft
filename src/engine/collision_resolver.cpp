#include "engine/collision_resolver.hpp"
#include "core/chunk_map.hpp"
#include <cmath>
#include <algorithm>

namespace VoxelEngine {

using namespace godot;

// Return the block coordinate that the leading face is entering.
// For +dir: the leading face at boundary N enters block N.
// For -dir: the leading face at boundary N enters block N-1.
static int32_t face_block_coord(float new_leading, float direction) {
    return static_cast<int32_t>(direction > 0.0f ? new_leading : new_leading - 1.0f);
}

// Check only the blocks on the leading face of the AABB in the movement direction,
// instead of the full AABB volume. This cuts block checks from O(volume) to O(face).
template<typename Pred>
static bool is_leading_face_solid(const Vector3& position,
                                   const Vector3& size,
                                   int axis, float direction,
                                   float new_leading,
                                   int32_t& out_face_coord,
                                   Pred is_solid_block) {
    out_face_coord = face_block_coord(new_leading, direction);
    if (axis == 0) {
        int32_t min_y = static_cast<int32_t>(std::floor(position.y));
        int32_t max_y = static_cast<int32_t>(std::floor(position.y + size.y));
        int32_t min_z = static_cast<int32_t>(std::floor(position.z));
        int32_t max_z = static_cast<int32_t>(std::floor(position.z + size.z));
        for (int32_t y = min_y; y <= max_y; ++y)
            for (int32_t z = min_z; z <= max_z; ++z)
                if (is_solid_block(out_face_coord, y, z)) return true;
    } else if (axis == 1) {
        int32_t min_x = static_cast<int32_t>(std::floor(position.x));
        int32_t max_x = static_cast<int32_t>(std::floor(position.x + size.x));
        int32_t min_z = static_cast<int32_t>(std::floor(position.z));
        int32_t max_z = static_cast<int32_t>(std::floor(position.z + size.z));
        for (int32_t x = min_x; x <= max_x; ++x)
            for (int32_t z = min_z; z <= max_z; ++z)
                if (is_solid_block(x, out_face_coord, z)) return true;
    } else {
        int32_t min_x = static_cast<int32_t>(std::floor(position.x));
        int32_t max_x = static_cast<int32_t>(std::floor(position.x + size.x));
        int32_t min_y = static_cast<int32_t>(std::floor(position.y));
        int32_t max_y = static_cast<int32_t>(std::floor(position.y + size.y));
        for (int32_t x = min_x; x <= max_x; ++x)
            for (int32_t y = min_y; y <= max_y; ++y)
                if (is_solid_block(x, y, out_face_coord)) return true;
    }
    return false;
}

template<typename Pred>
static void resolve_axis(const Vector3& position,
                         const Vector3& motion,
                         const Vector3& size,
                         int axis,
                         Vector3& result,
                         bool& collided,
                         Pred is_solid_block) {
    if (motion[axis] == 0.0f) {
        collided = false;
        return;
    }
    const float direction = motion[axis] > 0.0f ? 1.0f : -1.0f;
    float remaining = std::abs(motion[axis]);

    while (remaining > 0.001f) {
        const float leading_edge = result[axis] + (direction > 0.0f ? size[axis] : 0.0f);

        // Next grid boundary in the movement direction
        float next_boundary = direction > 0.0f
            ? std::floor(leading_edge) + 1.0f
            : std::floor(leading_edge);

        // If already at boundary, advance one full grid cell
        float dist = std::abs(next_boundary - leading_edge);
        if (dist < 0.001f) {
            next_boundary = leading_edge + direction * 1.0f;
            dist = 1.0f;
        }

        const float current_step = std::min(dist, remaining);
        const float new_leading = leading_edge + direction * current_step;

        // DDA: only check the leading face blocks instead of the full AABB volume.
        // On collision the exact position is computed from the solid block's face —
        // no binary search needed.
        int32_t face_coord = 0;
        if (is_leading_face_solid(result, size, axis, direction, new_leading, face_coord, is_solid_block)) {
            collided = true;
            result[axis] = direction > 0.0f
                ? (static_cast<float>(face_coord) - size[axis])
                : (static_cast<float>(face_coord) + 1.0f);
            return;
        }

        result[axis] += direction * current_step;
        remaining -= current_step;
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
        resolve_axis(position, motion, size, axis, result, collided,
            [this](int32_t x, int32_t y, int32_t z) { return chunk_map_->is_block_solid_fast(x, y, z); });
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
