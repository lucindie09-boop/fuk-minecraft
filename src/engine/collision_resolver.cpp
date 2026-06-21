#include "engine/collision_resolver.hpp"
#include "core/chunk_map.hpp"
#include <cmath>
#include <algorithm>

namespace VoxelEngine {

using namespace godot;

template<typename Pred>
static void resolve_axis(const godot::Vector3& position,
                         const godot::Vector3& motion,
                         const godot::Vector3& size,
                         int axis,
                         godot::Vector3& result,
                         bool& collided,
                         Pred is_solid) {
    if (motion[axis] == 0.0f) {
        collided = false;
        return;
    }
    float direction = motion[axis] > 0.0f ? 1.0f : -1.0f;
    float remaining = std::abs(motion[axis]);
    const float step = 1.0f;

    while (remaining > 0.001f) {
        float current_step = std::min(step, remaining);
        Vector3 test_pos = result;
        test_pos[axis] += direction * current_step;
        AABB test_aabb(test_pos, size);
        if (is_solid(test_aabb)) {
            collided = true;
            float low = result[axis];
            float high = test_pos[axis];
            float best = result[axis];
            for (int i = 0; i < 10; ++i) {
                float mid = (low + high) * 0.5f;
                Vector3 mid_pos = result;
                mid_pos[axis] = mid;
                AABB mid_aabb(mid_pos, size);
                if (is_solid(mid_aabb)) {
                    high = mid;
                } else {
                    best = mid;
                    low = mid;
                }
            }
            result[axis] = best;
            break;
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

    for (int axis = 0; axis < 3; ++axis) {
        bool collided = false;
        resolve_axis(position, motion, size, axis, result, collided,
            [this](const AABB& aabb) { return is_aabb_solid(aabb); });
        if (axis == 0) out.collided_x = collided;
        else if (axis == 1) out.collided_y = collided;
        else out.collided_z = collided;
    }

    out.position = result;

    AABB floor_aabb(result, size);
    floor_aabb.position.y -= 0.05f;
    out.on_floor = is_aabb_solid(floor_aabb);

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
