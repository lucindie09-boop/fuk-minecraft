#include "engine/collision_resolver.hpp"
#include "core/chunk_map.hpp"
#include <cmath>
#include <algorithm>

namespace VoxelEngine {

using namespace godot;

// DDA sweep: walk leading edge one cell at a time.
// At each step only the newly entered cell in the sweep axis
// is checked (the YZ projection is constant for this axis).
template<typename Pred>
static void dda_sweep_axis(const Vector3& position,
                           const Vector3& motion,
                           const Vector3& size,
                           int axis,
                           Vector3& result,
                           bool& collided,
                           Pred column_solid) {
    collided = false;
    float mag = motion[axis];
    if (std::abs(mag) < 0.001f) return;

    float dir = mag > 0.0f ? 1.0f : -1.0f;
    float leading = result[axis] + (dir > 0.0f ? size[axis] : 0.0f);
    float end_leading = leading + mag;

    // YZ projection of the AABB (constant during this axis sweep)
    int y1 = static_cast<int>(std::floor(result.y));
    int y2 = static_cast<int>(std::floor(result.y + size.y));
    int z1 = static_cast<int>(std::floor(result.z));
    int z2 = static_cast<int>(std::floor(result.z + size.z));

    // DDA along direction axis
    int cell = static_cast<int>(std::floor(leading));
    float next_boundary;
    if (dir > 0.0f) {
        next_boundary = static_cast<float>(cell + 1);
        if (leading >= next_boundary - 1e-6f) {
            cell += 1;
            next_boundary = static_cast<float>(cell + 1);
        }
    } else {
        next_boundary = static_cast<float>(cell);
        if (leading <= next_boundary + 1e-6f) {
            cell -= 1;
            next_boundary = static_cast<float>(cell);
        }
    }

    while ((dir > 0.0f && next_boundary < end_leading) ||
           (dir < 0.0f && next_boundary > end_leading)) {
        // The DDA enters a new cell -- check its YZ column
        int cx = cell;
        int cy = y1;
        int cz = z1;
        // Only need to check the cell column at the leading face.
        // For positive dir the new cell entered is at ceil(leading)-1 = cell
        // For negative dir the new cell is at cell (floor(leading)-1 after crossing)
        // Actually cell already represents the new cell after crossing.
        if (column_solid(cx, cy, cz, y2, z2)) {
            collided = true;
            // Snap to the boundary: the AABB's leading face sits at the boundary
            if (dir > 0.0f)
                result[axis] = next_boundary - size[axis];
            else
                result[axis] = next_boundary;
            return;
        }

        cell += static_cast<int>(dir);
        next_boundary = dir > 0.0f
            ? static_cast<float>(cell + 1)
            : static_cast<float>(cell);
    }

    // No collision in the sweep -- move the full distance
    result[axis] += mag;
}

CollisionResolver::CollisionResult CollisionResolver::resolve(
    const Vector3& position,
    const Vector3& motion,
    const Vector3& size
) const {
    Vector3 result = position;
    CollisionResult out;

    auto lock = chunk_map_->lock_all();

    for (int axis = 0; axis < 3; ++axis) {
        bool collided = false;
        dda_sweep_axis(position, motion, size, axis, result, collided,
            [this](int cx, int cy, int cz, int y2, int z2) {
                for (int y = cy; y <= y2; ++y)
                    for (int z = cz; z <= z2; ++z)
                        if (chunk_map_->is_block_solid_fast(cx, y, z))
                            return true;
                return false;
            });
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

    for (int32_t y = min_y; y <= max_y; ++y)
        for (int32_t z = min_z; z <= max_z; ++z)
            for (int32_t x = min_x; x <= max_x; ++x)
                if (chunk_map_->is_block_solid_fast(x, y, z))
                    return true;
    return false;
}

bool CollisionResolver::is_aabb_solid(const AABB& aabb) const {
    auto lock = chunk_map_->lock_all();
    return is_aabb_solid_fast(aabb);
}

} // namespace VoxelEngine
