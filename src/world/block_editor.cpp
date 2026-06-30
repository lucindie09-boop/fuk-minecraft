#include "world/block_editor.hpp"
#include "world/chunk_world.hpp"
#include "mesh/mesh_manager.hpp"
#include "lighting/light_propagator.hpp"
#include "core/block_types.hpp"
#include <godot_cpp/core/class_db.hpp>

namespace VoxelEngine {
using namespace godot;

BlockEditor::BlockEditor(ChunkWorld* cw, MeshManager* mm, LightPropagator* lp)
    : chunk_world(cw), mesh_manager(mm), light_propagator(lp) {}

int BlockEditor::query_block(int32_t world_x, int32_t world_y, int32_t world_z) const {
    return chunk_world->get_block_world(world_x, world_y, world_z);
}

String BlockEditor::get_block_name(int block_id) const {
    const auto& block = BlockRegistry::get_instance().get_block(static_cast<BlockID>(block_id));
    return String(block.name);
}

void BlockEditor::place_block(int32_t world_x, int32_t world_y, int32_t world_z, BlockID new_block) {
    int32_t chunk_x, chunk_y, chunk_z, local_x, local_y, local_z;
    world_to_chunk_local(world_x, world_y, world_z, chunk_x, chunk_y, chunk_z, local_x, local_y, local_z);

    ChunkData* chunk_data = chunk_world->get_chunk_data(chunk_x, chunk_y, chunk_z);
    if (!chunk_data) {
        chunk_world->queue_pending_placement(world_x, world_y, world_z, static_cast<int>(new_block));
        return;
    }

    if (!is_local_in_bounds(local_x, local_y, local_z)) {
        return;
    }

    const BlockID old_block = chunk_data->get_block(local_x, local_y, local_z);
    if (old_block == new_block) return;

    const BlockRegistry& registry = BlockRegistry::get_instance();
    const BlockType& old_type = registry.get_block(old_block);
    const BlockType& new_type = registry.get_block(new_block);
    const bool old_opaque = HasProperty(old_type.properties, BlockProperty::Opaque);
    const bool new_opaque = HasProperty(new_type.properties, BlockProperty::Opaque);

    const uint8_t old_r = chunk_data->get_light_r(local_x, local_y, local_z);
    const uint8_t old_g = chunk_data->get_light_g(local_x, local_y, local_z);
    const uint8_t old_b = chunk_data->get_light_b(local_x, local_y, local_z);

    chunk_data->set_block(local_x, local_y, local_z, new_block);

    if (old_opaque != new_opaque) {
        chunk_data->propagate_sky_light_column(local_x, local_z, chunk_world->get_chunk_data(chunk_x, chunk_y + 1, chunk_z));
    }

    light_propagator->update_block_light_incremental(chunk_x, chunk_y, chunk_z, chunk_x, chunk_y, chunk_z,
                               local_x, local_y, local_z,
                               old_block, new_block, old_r, old_g, old_b);

    chunk_world->mark_chunk_dirty(chunk_x, chunk_y, chunk_z);

    ChunkRenderData* render_data = chunk_world->get_chunk_render_data(chunk_x, chunk_y, chunk_z);
    if (render_data) {
        render_data->is_mesh_dirty = true;
        render_data->mesh_version++;
        render_data->dirty_subchunks |= static_cast<uint8_t>(1 << subchunk_index(local_x, local_y, local_z));
    }

    queue_player_edit_chunk_refresh(chunk_x, chunk_y, chunk_z);

    post_block_change(world_x, world_y, world_z, new_block);
}

Dictionary BlockEditor::raycast(godot::Node* chunk_manager, const NodePath& player_path, double max_distance) const {
    Dictionary result;
    result["success"] = false;

    if (player_path.is_empty()) return result;
    Node* player_node = chunk_manager->get_node_or_null(player_path);
    if (!player_node) return result;
    Node3D* player = Object::cast_to<Node3D>(player_node);
    if (!player) return result;

    Camera3D* camera = nullptr;
    if (!player_path.is_empty()) {
        if (player_path != last_camera_path) {
            last_camera_path = player_path;
            cached_camera = Object::cast_to<Camera3D>(player->get_node_or_null(NodePath("Camera3D")));
        }
        camera = cached_camera;
    }
    if (!camera) return result;

    Vector3 ray_origin = camera->get_global_position();
    Vector3 ray_dir = -camera->get_global_transform().basis.get_column(2).normalized();

    int32_t current_x = static_cast<int32_t>(std::floor(ray_origin.x));
    int32_t current_y = static_cast<int32_t>(std::floor(ray_origin.y));
    int32_t current_z = static_cast<int32_t>(std::floor(ray_origin.z));

    int32_t step_x = ray_dir.x > 0 ? 1 : (ray_dir.x < 0 ? -1 : 0);
    int32_t step_y = ray_dir.y > 0 ? 1 : (ray_dir.y < 0 ? -1 : 0);
    int32_t step_z = ray_dir.z > 0 ? 1 : (ray_dir.z < 0 ? -1 : 0);

    double t_max_x = step_x != 0 ?
        ((step_x > 0 ? (current_x + 1) : current_x) - ray_origin.x) / ray_dir.x : max_distance;
    double t_max_y = step_y != 0 ?
        ((step_y > 0 ? (current_y + 1) : current_y) - ray_origin.y) / ray_dir.y : max_distance;
    double t_max_z = step_z != 0 ?
        ((step_z > 0 ? (current_z + 1) : current_z) - ray_origin.z) / ray_dir.z : max_distance;

    double t_delta_x = step_x != 0 ? std::abs(1.0 / ray_dir.x) : max_distance;
    double t_delta_y = step_y != 0 ? std::abs(1.0 / ray_dir.y) : max_distance;
    double t_delta_z = step_z != 0 ? std::abs(1.0 / ray_dir.z) : max_distance;

    int32_t prev_x = current_x;
    int32_t prev_y = current_y;
    int32_t prev_z = current_z;

    const int32_t max_steps = static_cast<int32_t>(max_distance) * 3;
    int32_t steps = 0;

constexpr int32_t kLockReacquireInterval = 8;
    auto map_lock = chunk_world->get_chunk_map().lock_all();
    while (steps < max_steps) {
if (steps > 0 && steps % kLockReacquireInterval == 0) {
map_lock = chunk_world->get_chunk_map().lock_all();
}
        int block = chunk_world->get_chunk_map().get_block_world_fast(current_x, current_y, current_z);
        if (block != 0) {
            result["success"] = true;
            result["position"] = Vector3(current_x, current_y, current_z);
            result["place_position"] = Vector3(prev_x, prev_y, prev_z);
            return result;
        }

        prev_x = current_x;
        prev_y = current_y;
        prev_z = current_z;

        if (t_max_x < t_max_y) {
            if (t_max_x < t_max_z) {
                current_x += step_x;
                t_max_x += t_delta_x;
            } else {
                current_z += step_z;
                t_max_z += t_delta_z;
            }
        } else {
            if (t_max_y < t_max_z) {
                current_y += step_y;
                t_max_y += t_delta_y;
            } else {
                current_z += step_z;
                t_max_z += t_delta_z;
            }
        }
        steps++;
    }
    return result;
}

// -------------------------------------------------------------------------
// Internal helpers
// -------------------------------------------------------------------------

void BlockEditor::mark_chunk_refresh_urgent(int32_t center_x, int32_t center_y, int32_t center_z) {
    for_each_chunk_in_refresh_radius(center_x, center_y, center_z, [this](int32_t cx, int32_t cy, int32_t cz) {
        mesh_manager->mark_chunk_urgent(cx, cy, cz);
    });
}

void BlockEditor::queue_immediate_dirty_chunk(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z) {
    if (!chunk_world->has_loaded_chunk(chunk_x, chunk_y, chunk_z)) {
        return;
    }
    mesh_manager->queue_immediate_dirty_chunk(chunk_x, chunk_y, chunk_z);
}

void BlockEditor::queue_player_edit_chunk_refresh(int32_t center_x, int32_t center_y, int32_t center_z) {
    mark_chunk_refresh_urgent(center_x, center_y, center_z);
    for_each_chunk_in_refresh_radius(center_x, center_y, center_z, [this](int32_t cx, int32_t cy, int32_t cz) {
        queue_immediate_dirty_chunk(cx, cy, cz);
    });
}

void BlockEditor::update_mud_variants(int32_t world_x, int32_t world_y, int32_t world_z, BlockID new_block) {
    if (world_y <= 0) return;
    const BlockID below = static_cast<BlockID>(chunk_world->get_block_world(world_x, world_y - 1, world_z));
    if (new_block != BlockIDs::AIR) {
        if (below == BlockIDs::MUD) {
            set_block_variant(world_x, world_y - 1, world_z, BlockIDs::MUD_FULL);
        } else if (below == BlockIDs::WET_SAND) {
            set_block_variant(world_x, world_y - 1, world_z, BlockIDs::WET_SAND_FULL);
        }
    } else {
        if (below == BlockIDs::MUD_FULL) {
            set_block_variant(world_x, world_y - 1, world_z, BlockIDs::MUD);
        } else if (below == BlockIDs::WET_SAND_FULL) {
            set_block_variant(world_x, world_y - 1, world_z, BlockIDs::WET_SAND);
        }
    }
}

void BlockEditor::post_block_change(int32_t world_x, int32_t world_y, int32_t world_z, BlockID new_block) {
    update_mud_variants(world_x, world_y, world_z, new_block);
}

void BlockEditor::set_block_variant(int32_t world_x, int32_t world_y, int32_t world_z, BlockID block_id) {
    int32_t chunk_x, chunk_y, chunk_z, local_x, local_y, local_z;
    world_to_chunk_local(world_x, world_y, world_z, chunk_x, chunk_y, chunk_z, local_x, local_y, local_z);
    ChunkData* chunk_data = chunk_world->get_chunk_data(chunk_x, chunk_y, chunk_z);
    if (!chunk_data) return;
    if (!is_local_in_bounds(local_x, local_y, local_z)) return;
    const BlockID old_block = chunk_data->get_block(local_x, local_y, local_z);
    if (old_block == block_id) return;
    chunk_data->set_block(local_x, local_y, local_z, block_id);
    chunk_world->mark_chunk_dirty(chunk_x, chunk_y, chunk_z);
    ChunkRenderData* render_data = chunk_world->get_chunk_render_data(chunk_x, chunk_y, chunk_z);
    if (render_data) {
        render_data->is_mesh_dirty = true;
        render_data->mesh_version++;
        render_data->dirty_subchunks |= static_cast<uint8_t>(1 << subchunk_index(local_x, local_y, local_z));
    }
    mesh_manager->queue_dirty_chunk(chunk_x, chunk_y, chunk_z);
}

} // namespace VoxelEngine
