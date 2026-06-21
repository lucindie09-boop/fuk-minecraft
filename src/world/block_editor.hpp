#ifndef FUK_MINECRAFT_BLOCK_EDITOR_HPP
#define FUK_MINECRAFT_BLOCK_EDITOR_HPP
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/object.hpp>
#include <cstdint>
#include <cmath>

#include "core/chunk_types.hpp"

namespace VoxelEngine {

class ChunkWorld;
class MeshManager;
class LightPropagator;

class BlockEditor {
public:
    BlockEditor(ChunkWorld* cw, MeshManager* mm, LightPropagator* lp);

    void place_block(int32_t world_x, int32_t world_y, int32_t world_z, BlockID block_id);
    int query_block(int32_t world_x, int32_t world_y, int32_t world_z) const;
    godot::String get_block_name(int block_id) const;
    godot::Dictionary raycast(godot::Node* owner, const godot::NodePath& player_path, double max_distance) const;

private:
    ChunkWorld* chunk_world;
    MeshManager* mesh_manager;
    LightPropagator* light_propagator;
    mutable godot::Camera3D* cached_camera = nullptr;
    mutable godot::NodePath last_camera_path;

    void set_block_variant(int32_t world_x, int32_t world_y, int32_t world_z, BlockID block_id);
    void update_mud_variants(int32_t world_x, int32_t world_y, int32_t world_z, BlockID new_block);
    void post_block_change(int32_t world_x, int32_t world_y, int32_t world_z, BlockID new_block);
    void queue_player_edit_chunk_refresh(int32_t center_x, int32_t center_y, int32_t center_z);
    void mark_chunk_refresh_urgent(int32_t center_x, int32_t center_y, int32_t center_z);
    void queue_immediate_dirty_chunk(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z);

    template <typename Callback>
    void for_each_chunk_in_refresh_radius(int32_t center_x, int32_t center_y, int32_t center_z, Callback&& callback) {
        for (int32_t dy = -1; dy <= 1; dy++) {
            for (int32_t dz = -1; dz <= 1; dz++) {
                for (int32_t dx = -1; dx <= 1; dx++) {
                    callback(center_x + dx, center_y + dy, center_z + dz);
                }
            }
        }
    }

    bool is_local_in_bounds(int32_t local_x, int32_t local_y, int32_t local_z) const {
        return local_x >= 0 && local_x < CHUNK_WIDTH &&
               local_y >= 0 && local_y < CHUNK_HEIGHT &&
               local_z >= 0 && local_z < CHUNK_DEPTH;
    }
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_BLOCK_EDITOR_HPP