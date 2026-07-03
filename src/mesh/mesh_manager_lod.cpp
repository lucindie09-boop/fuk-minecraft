#include "mesh/mesh_manager.hpp"

#include "mesh/merged_mesh_builder.hpp"
#include "mesh/lod_mesh_builder.hpp"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>

#include <chrono>
#include "core/hash_utils.hpp"
#include <cstring>
#include <unordered_set>

namespace VoxelEngine {

using namespace godot;

namespace {

PackedBuiltMeshData pack_vertex_array_lod(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    PackedBuiltMeshData packed;
    if (vertices.empty() || indices.empty()) {
        packed.empty = true;
        return packed;
    }
    packed.empty = false;
    const size_t n = vertices.size();
    packed.vertices.resize(n);
    packed.normals.resize(n);
    packed.custom0.resize(n * 4);
    packed.uvs.resize(n);
    packed.custom1.resize(n * 4);
    packed.indices.resize(indices.size());

    Vector3* v_ptr = packed.vertices.ptrw();
    Vector3* n_ptr = packed.normals.ptrw();
    uint8_t* c0_ptr = packed.custom0.ptrw();
    Vector2* uv_ptr = packed.uvs.ptrw();
    int32_t* idx_ptr = packed.indices.ptrw();

    constexpr float kInv127 = 1.0f / 127.0f;
    constexpr float kInv255 = 1.0f / 255.0f;

    for (size_t i = 0; i < n; i++) {
        const Vertex& v = vertices[i];
        v_ptr[i] = Vector3(v.x, v.y, v.z);
        n_ptr[i] = Vector3(v.nx * kInv127, v.ny * kInv127, v.nz * kInv127);
        c0_ptr[i * 4 + 0] = v.light_r;
        c0_ptr[i * 4 + 1] = v.light_g;
        c0_ptr[i * 4 + 2] = v.light_b;
        c0_ptr[i * 4 + 3] = v.sky_light;
        uv_ptr[i] = Vector2(v.u, v.v);
        packed.custom1.encode_half(static_cast<int64_t>(i * 4), static_cast<double>(v.texture_index));
        packed.custom1.encode_half(static_cast<int64_t>(i * 4 + 2), static_cast<double>(v.ao * kInv255));
    }
    std::memcpy(idx_ptr, indices.data(), indices.size() * sizeof(int32_t));
    return packed;
}

int32_t axis_distance_to_range(int32_t value, int32_t min_value, int32_t max_value) {
    if (value < min_value) {
        return min_value - value;
    }
    if (value > max_value) {
        return value - max_value;
    }
    return 0;
}

bool is_group_visible_in_range(int32_t anchor_cx, int32_t anchor_cy, int32_t anchor_cz,
                               int32_t merge_size, int32_t render_distance, int32_t vertical_buffer,
                               int32_t player_cx, int32_t player_cy, int32_t player_cz) {
    if (render_distance <= 0 || player_cx == INT32_MIN) {
        return true;
    }

    const int32_t max_cx = anchor_cx + merge_size - 1;
    const int32_t max_cy = anchor_cy + merge_size - 1;
    const int32_t max_cz = anchor_cz + merge_size - 1;
    const int32_t dx = axis_distance_to_range(player_cx, anchor_cx, max_cx);
    const int32_t dz = axis_distance_to_range(player_cz, anchor_cz, max_cz);

    if ((dx * dx + dz * dz) > (render_distance * render_distance)) {
        return false;
    }

    return player_cy >= (anchor_cy - vertical_buffer) &&
           player_cy <= (max_cy + vertical_buffer);
}

struct GroupMeshBuildTask : Task {
    ChunkMap* chunk_map = nullptr;
    ChunkScheduler* chunk_scheduler = nullptr;
    std::atomic<uint64_t>* async_epoch = nullptr;
    LodGroupRenderData* render_group = nullptr;
    int32_t anchor_cx = 0;
    int32_t anchor_cy = 0;
    int32_t anchor_cz = 0;
    LodLevel level = LodLevel::MergedFull;
    int32_t merge_shift = 1;
    int32_t downsample_step = 2;
    uint64_t epoch = 0;
    uint64_t mesh_job_serial = 0;
    bool high_priority = false;
    std::array<uint64_t, kMaxLodGroupMembers> member_keys{};
    int32_t member_count = 0;

    void unpin_members() {
        if (!chunk_map) return;
        for (int32_t i = 0; i < member_count; ++i) {
            int32_t mcx, mcy, mcz;
            ChunkMap::decode_chunk_key(member_keys[i], mcx, mcy, mcz);
            ChunkRenderData* member_data = chunk_map->get_chunk_render_data(mcx, mcy, mcz);
            if (member_data) {
                member_data->pending_mesh_builds.fetch_sub(1, std::memory_order_relaxed);
            }
        }
    }

    // Used only when execute() bails before ever pushing a CompletedGroupMesh (epoch
    // mismatch), since in that case the upload-side pin has no message to ride back on.
    void unpin_members_uploads_too() {
        if (!chunk_map) return;
        for (int32_t i = 0; i < member_count; ++i) {
            int32_t mcx, mcy, mcz;
            ChunkMap::decode_chunk_key(member_keys[i], mcx, mcy, mcz);
            ChunkRenderData* member_data = chunk_map->get_chunk_render_data(mcx, mcy, mcz);
            if (member_data) {
                member_data->pending_mesh_uploads.fetch_sub(1, std::memory_order_relaxed);
            }
        }
    }

    void execute() override {
        if (!chunk_map || !chunk_scheduler || !render_group) {
            return;
        }

        thread_local MeshBuilder builder;
        BuiltMeshData built;
        if (level == LodLevel::MergedDownsampled) {
            built = LodMeshBuilder::build_downsampled(
                *chunk_map, anchor_cx, anchor_cy, anchor_cz, merge_shift, downsample_step, builder);
        } else {
            built = MergedMeshBuilder::build_merged(
                *chunk_map, anchor_cx, anchor_cy, anchor_cz, merge_shift, builder);
        }

        PackedBuiltMeshData packed_mesh = pack_vertex_array_lod(built.vertices, built.indices);
        PackedBuiltMeshData water_mesh = pack_vertex_array_lod(built.water_vertices, built.water_indices);

        // 3.1 Content hash for upload deduplication
        uint64_t content_hash;
        if (built.vertices.empty() || built.indices.empty()) {
            content_hash = 0;
        } else {
            content_hash = fnv1a_hash_bytes(built.vertices.data(), built.vertices.size() * sizeof(Vertex));
            content_hash = fnv1a_hash_bytes(built.indices.data(), built.indices.size() * sizeof(uint32_t), content_hash);
        }

        if (async_epoch && epoch != async_epoch->load(std::memory_order_acquire)) {
            render_group->pending_mesh_builds.fetch_sub(1, std::memory_order_relaxed);
            render_group->pending_mesh_uploads.fetch_sub(1, std::memory_order_relaxed);
            unpin_members();
            unpin_members_uploads_too();
            return;
        }

        CompletedGroupMesh completed;
        completed.anchor_cx = anchor_cx;
        completed.anchor_cy = anchor_cy;
        completed.anchor_cz = anchor_cz;
        completed.epoch = epoch;
        completed.mesh_job_serial = mesh_job_serial;
        completed.source_group = render_group;
        completed.mesh_data = std::move(packed_mesh);
        completed.water_mesh_data = std::move(water_mesh);
        completed.mesh_content_hash = content_hash;
        completed.member_count = member_count;
        completed.member_keys = member_keys;

        chunk_scheduler->push_completed_group_mesh(std::move(completed), high_priority);
        render_group->pending_mesh_builds.fetch_sub(1, std::memory_order_relaxed);
        unpin_members();
    }
};

} // namespace

PackedBuiltMeshData MeshManager::pack_built_mesh(const BuiltMeshData& built_mesh) {
    PackedBuiltMeshData packed_mesh;
    if (built_mesh.empty) {
        packed_mesh.empty = true;
        return packed_mesh;
    }

    packed_mesh.empty = false;
    const size_t n = built_mesh.vertices.size();
    packed_mesh.vertices.resize(n);
    packed_mesh.normals.resize(n);
    packed_mesh.custom0.resize(n * 4);
    packed_mesh.uvs.resize(n);
    packed_mesh.custom1.resize(n * 4);
    packed_mesh.indices.resize(built_mesh.indices.size());

    Vector3* v_ptr = packed_mesh.vertices.ptrw();
    Vector3* n_ptr = packed_mesh.normals.ptrw();
    uint8_t* c0_ptr = packed_mesh.custom0.ptrw();
    Vector2* uv_ptr = packed_mesh.uvs.ptrw();
    int32_t* idx_ptr = packed_mesh.indices.ptrw();

    constexpr float kInv127 = 1.0f / 127.0f;
    constexpr float kInv255 = 1.0f / 255.0f;

    for (size_t i = 0; i < n; ++i) {
        const Vertex& v = built_mesh.vertices[i];
        v_ptr[i] = Vector3(v.x, v.y, v.z);
        n_ptr[i] = Vector3(v.nx * kInv127, v.ny * kInv127, v.nz * kInv127);
        c0_ptr[i * 4 + 0] = v.light_r;
        c0_ptr[i * 4 + 1] = v.light_g;
        c0_ptr[i * 4 + 2] = v.light_b;
        c0_ptr[i * 4 + 3] = v.sky_light;
        uv_ptr[i] = Vector2(v.u, v.v);
        packed_mesh.custom1.encode_half(static_cast<int64_t>(i * 4), static_cast<double>(v.texture_index));
        packed_mesh.custom1.encode_half(static_cast<int64_t>(i * 4 + 2), static_cast<double>(v.ao * kInv255));
    }
    std::memcpy(idx_ptr, built_mesh.indices.data(), built_mesh.indices.size() * sizeof(int32_t));
    return packed_mesh;
}

void MeshManager::hide_chunk_instance(ChunkRenderData* render_data) {
    if (!render_data || !render_data->instance_rid.is_valid()) {
        return;
    }
    RenderingServer* rs = RenderingServer::get_singleton();
    rs->instance_set_visible(render_data->instance_rid, false);
}

void MeshManager::show_chunk_instance(ChunkRenderData* render_data, int32_t cx, int32_t cy, int32_t cz) {
    if (!render_data) {
        return;
    }
    RenderingServer* rs = RenderingServer::get_singleton();
    if (!render_data->instance_rid.is_valid()) {
        if (!render_data->mesh_rid.is_valid()) {
            return;
        }
        render_data->instance_rid = rs->instance_create();
        rs->instance_set_base(render_data->instance_rid, render_data->mesh_rid);
        AABB chunk_aabb(Vector3(0, 0, 0), Vector3(CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH));
        rs->instance_set_custom_aabb(render_data->instance_rid, chunk_aabb);
        Transform3D transform;
        transform.origin = Vector3(cx * CHUNK_WIDTH, cy * CHUNK_HEIGHT, cz * CHUNK_DEPTH);
        rs->instance_set_transform(render_data->instance_rid, transform);
        if (owner) {
            Node3D* owner3d = Object::cast_to<Node3D>(owner);
            if (owner3d) {
                Ref<World3D> world = owner3d->get_world_3d();
                if (world.is_valid()) {
                    rs->instance_set_scenario(render_data->instance_rid, world->get_scenario());
                }
            }
        }
    }
    rs->instance_set_visible(render_data->instance_rid, true);
}

void MeshManager::free_group_render_data(LodGroupRenderData& group) {
    RenderingServer* rs = RenderingServer::get_singleton();
    if (group.mesh_rid.is_valid()) {
        rs->free_rid(group.mesh_rid);
        group.mesh_rid = RID();
    }
    if (group.instance_rid.is_valid()) {
        rs->free_rid(group.instance_rid);
        group.instance_rid = RID();
    }
    group.mesh_content_hash = 0;
    group.material_set = false;
}

void MeshManager::release_group_to_individual(LodGroupRenderData* group) {
    if (!group || !chunk_map) {
        return;
    }
    needs_stuck_recovery_ = true;
    RenderingServer* rs = RenderingServer::get_singleton();
    if (group->instance_rid.is_valid()) {
        rs->instance_set_visible(group->instance_rid, false);
    }
    free_group_render_data(*group);

    for (int32_t i = 0; i < group->member_count; ++i) {
        int32_t cx, cy, cz;
        ChunkMap::decode_chunk_key(group->member_keys[i], cx, cy, cz);
        ChunkRenderData* render_data = chunk_map->get_chunk_render_data(cx, cy, cz);
        if (!render_data) {
            continue;
        }
        render_data->render_lod = ChunkRenderLod::Individual;
        render_data->lod_group_key = 0;
        render_data->current_lod = LodLevel::Individual;
        render_data->effective_lod = LodLevel::Individual;
        render_data->is_mesh_dirty = true;
        render_data->pending_mesh_uploads.fetch_sub(1, std::memory_order_relaxed);
        show_chunk_instance(render_data, cx, cy, cz);
        queue_immediate_dirty_chunk(cx, cy, cz);
    }

    lod_controller.remove_group(group->group_key);
}

void MeshManager::set_lod_settings(const LodSettings& settings) {
    LodSettings sanitized = settings;
    if (!lod_merge_shift_supported(sanitized.lod1_merge_shift)) {
        if (sanitized.lod1_merge_shift < 0) {
            sanitized.lod1_merge_shift = 0;
        } else {
            sanitized.lod1_merge_shift = kMaxSupportedLodMergeShift;
        }
    }
    if (!lod_merge_shift_supported(sanitized.lod2_merge_shift)) {
        if (sanitized.lod2_merge_shift < 0) {
            sanitized.lod2_merge_shift = 0;
        } else {
            sanitized.lod2_merge_shift = kMaxSupportedLodMergeShift;
        }
    }
    const int32_t lod2_merge_size = lod_merge_size(sanitized.lod2_merge_shift);
    if (sanitized.lod2_downsample_step < lod2_merge_size) {
        sanitized.lod2_downsample_step = lod2_merge_size;
    }

    const bool was_enabled = lod_controller.get_settings().enabled;
    lod_controller.set_settings(sanitized);
    if (was_enabled && !sanitized.enabled) {
        disable_lod_and_split_all_groups();
    }
}

void MeshManager::disable_lod_and_split_all_groups() {
    if (!chunk_map) {
        lod_controller.clear();
        return;
    }

    std::vector<LodTransition> transitions;
    lod_controller.collect_all_group_splits(transitions);
    const uint64_t epoch = async_epoch ? async_epoch->load(std::memory_order_acquire) : 0;
    for (const LodTransition& transition : transitions) {
        apply_split_transition(transition, epoch);
    }

    chunk_map->for_each([&](uint64_t /*key*/, const std::unique_ptr<ChunkRenderData>& render_data) {
        render_data->render_lod = ChunkRenderLod::Individual;
        render_data->lod_group_key = 0;
        render_data->current_lod = LodLevel::Individual;
        render_data->effective_lod = LodLevel::Individual;
        if (render_data->instance_rid.is_valid()) {
            RenderingServer* rs = RenderingServer::get_singleton();
            rs->instance_set_visible(render_data->instance_rid, true);
        }
    });

    lod_controller.clear();
}

void MeshManager::recover_stuck_lod_chunks() {
    if (!chunk_map || !needs_stuck_recovery_) {
        return;
    }
    needs_stuck_recovery_ = false;
    chunk_map->for_each([&](uint64_t key, const std::unique_ptr<ChunkRenderData>& render_data) {
        if (render_data->render_lod != ChunkRenderLod::HiddenInGroup || render_data->lod_group_key == 0) {
            return;
        }
        LodGroupRenderData* group = lod_controller.get_group(render_data->lod_group_key);
        if (!group) {
            int32_t cx, cy, cz;
            ChunkMap::decode_chunk_key(key, cx, cy, cz);
            render_data->render_lod = ChunkRenderLod::Individual;
            render_data->lod_group_key = 0;
            render_data->current_lod = LodLevel::Individual;
            render_data->effective_lod = LodLevel::Individual;
            render_data->is_mesh_dirty = true;
            RenderingServer* rs = RenderingServer::get_singleton();
            if (render_data->instance_rid.is_valid()) {
                rs->instance_set_visible(render_data->instance_rid, true);
            }
            queue_immediate_dirty_chunk(cx, cy, cz);
            return;
        }
        if (group->pending_mesh_builds.load(std::memory_order_acquire) > 0 ||
            group->pending_mesh_uploads.load(std::memory_order_acquire) > 0) {
            return;
        }
        if (group->instance_rid.is_valid()) {
            return;
        }
        int32_t cx, cy, cz;
        ChunkMap::decode_chunk_key(key, cx, cy, cz);
        render_data->render_lod = ChunkRenderLod::Individual;
        render_data->lod_group_key = 0;
        render_data->current_lod = LodLevel::Individual;
        render_data->effective_lod = LodLevel::Individual;
        render_data->is_mesh_dirty = true;
        RenderingServer* rs = RenderingServer::get_singleton();
        if (render_data->instance_rid.is_valid()) {
            rs->instance_set_visible(render_data->instance_rid, true);
        }
        queue_immediate_dirty_chunk(cx, cy, cz);
    });
}

void MeshManager::update_lod(int32_t render_distance, bool force_rescan) {
    if (last_player_chunk_x == INT32_MIN) {
        return;
    }

    if (!lod_controller.get_settings().enabled) {
        return;
    }

    // Periodic rescan to discover groups whose member chunks arrived after the initial scan
    if (!force_rescan) {
        if (--lod_periodic_rescan_counter <= 0) {
            force_rescan = true;
            lod_periodic_rescan_counter = 300;
        }
    }

    const bool rescanned =
        lod_controller.update(last_player_chunk_x, last_player_chunk_y, last_player_chunk_z,
                              render_distance, force_rescan);

    if (!rescanned && lod_controller.has_incomplete_groups()) {
        lod_controller.queue_incomplete_group_merges();
    }
    if (rescanned || !lod_controller.get_pending_transitions().empty()) {
        process_lod_transitions(async_epoch ? async_epoch->load(std::memory_order_acquire) : 0);
    }

    recover_stuck_lod_chunks();
}

void MeshManager::split_lod_group_for_edit(uint64_t group_key) {
    std::vector<LodTransition> transitions;
    lod_controller.split_group_on_edit(group_key, transitions);
    for (const LodTransition& transition : transitions) {
        apply_split_transition(transition, async_epoch ? async_epoch->load(std::memory_order_acquire) : 0);
    }
}

void MeshManager::apply_split_transition(const LodTransition& transition, uint64_t epoch) {
    if (!chunk_map) {
        return;
    }

    needs_stuck_recovery_ = true;
    LodGroupRenderData* group = lod_controller.get_group(transition.group_key);
    if (!group) {
        return;
    }

    RenderingServer* rs = RenderingServer::get_singleton();
    if (group->instance_rid.is_valid()) {
        rs->instance_set_visible(group->instance_rid, false);
    }
    free_group_render_data(*group);

    for (int32_t i = 0; i < group->member_count; ++i) {
        int32_t cx, cy, cz;
        ChunkMap::decode_chunk_key(group->member_keys[i], cx, cy, cz);
        ChunkRenderData* render_data = chunk_map->get_chunk_render_data(cx, cy, cz);
        if (!render_data) {
            continue;
        }
        render_data->render_lod = ChunkRenderLod::Individual;
        render_data->lod_group_key = 0;
        render_data->current_lod = LodLevel::Individual;
        render_data->effective_lod = LodLevel::Individual;
        render_data->is_mesh_dirty = true;
        show_chunk_instance(render_data, cx, cy, cz);
        queue_immediate_dirty_chunk(cx, cy, cz);
    }

    lod_controller.remove_group(transition.group_key);
}

void MeshManager::apply_merge_transition(const LodTransition& transition, uint64_t epoch) {
    if (!chunk_map || !thread_pool || !chunk_scheduler) {
        return;
    }

    std::array<uint64_t, kMaxLodGroupMembers> member_keys{};
    int32_t member_count = 0;
    if (!lod_controller.collect_group_members(transition.anchor_cx, transition.anchor_cy, transition.anchor_cz,
                                              transition.merge_shift, member_keys, member_count)) {
        return;
    }

    LodGroupRenderData* group = lod_controller.get_or_create_group(
        transition.group_key, transition.anchor_cx, transition.anchor_cy, transition.anchor_cz);
    group->group_key = transition.group_key;
    group->anchor_cx = transition.anchor_cx;
    group->anchor_cy = transition.anchor_cy;
    group->anchor_cz = transition.anchor_cz;
    group->level = transition.target_level;
    group->merge_shift = transition.merge_shift;
    group->downsample_step = transition.downsample_step;
    group->member_count = member_count;
    group->member_keys = member_keys;

    for (int32_t i = 0; i < member_count; ++i) {
        int32_t cx, cy, cz;
        ChunkMap::decode_chunk_key(member_keys[i], cx, cy, cz);
        ChunkRenderData* render_data = chunk_map->get_chunk_render_data(cx, cy, cz);
        if (!render_data) {
            continue;
        }
        render_data->lod_group_key = transition.group_key;
        render_data->current_lod = transition.target_level;
        render_data->effective_lod = transition.target_level;
    }

    if (group->pending_mesh_builds.load(std::memory_order_acquire) > 0 ||
        group->pending_mesh_uploads.load(std::memory_order_acquire) > 0) {
        return;
    }

    group->is_dirty = false;
    group->pending_mesh_builds.fetch_add(1, std::memory_order_relaxed);
    group->pending_mesh_uploads.fetch_add(1, std::memory_order_relaxed);

    // Pin every member chunk so try_unload_chunk() can't evict it out from under the
    // worker thread, which re-resolves each member by coordinate at execute() time
    // (not at queue time). Without this, a chunk that falls out of load radius between
    // queueing and execution silently drops out of the merged mesh as a "hole".
    for (int32_t i = 0; i < member_count; ++i) {
        int32_t mcx, mcy, mcz;
        ChunkMap::decode_chunk_key(member_keys[i], mcx, mcy, mcz);
        ChunkRenderData* member_data = chunk_map->get_chunk_render_data(mcx, mcy, mcz);
        if (member_data) {
            member_data->pending_mesh_builds.fetch_add(1, std::memory_order_relaxed);
            member_data->pending_mesh_uploads.fetch_add(1, std::memory_order_relaxed);
        }
    }

    const uint64_t mesh_job_serial = group->mesh_job_serial.fetch_add(1, std::memory_order_acq_rel) + 1;

    auto task = std::make_unique<GroupMeshBuildTask>();
    task->chunk_map = chunk_map;
    task->chunk_scheduler = chunk_scheduler;
    task->async_epoch = async_epoch;
    task->render_group = group;
    task->anchor_cx = transition.anchor_cx;
    task->anchor_cy = transition.anchor_cy;
    task->anchor_cz = transition.anchor_cz;
    task->level = transition.target_level;
    task->merge_shift = transition.merge_shift;
    task->downsample_step = transition.downsample_step;
    task->epoch = epoch;
    task->mesh_job_serial = mesh_job_serial;
    task->high_priority = false;
    task->member_count = member_count;
    task->member_keys = member_keys;

    thread_pool->enqueue_task(std::move(task), false);
}

void MeshManager::apply_rebuild_transition(const LodTransition& transition, uint64_t epoch) {
    LodGroupRenderData* group = lod_controller.get_group(transition.group_key);
    if (!group) {
        apply_merge_transition(transition, epoch);
        return;
    }

    // Keep the current group mesh/instance alive until the replacement upload lands.
    // Freeing it here leaves every member hidden while the async rebuild runs, which
    // shows up in-game as LOD groups disappearing or partially popping out.
    group->level = transition.target_level;
    group->merge_shift = transition.merge_shift;
    group->downsample_step = transition.downsample_step;
    group->is_dirty = true;
    apply_merge_transition(transition, epoch);
}

void MeshManager::process_lod_transitions(uint64_t epoch) {
    std::unordered_set<uint64_t> split_groups;
    for (const LodTransition& transition : lod_controller.get_pending_transitions()) {
        switch (transition.kind) {
            case LodTransitionKind::SplitToIndividual:
                if (split_groups.insert(transition.group_key).second) {
                    apply_split_transition(transition, epoch);
                }
                break;
            case LodTransitionKind::MergeGroup:
                apply_merge_transition(transition, epoch);
                break;
            case LodTransitionKind::RebuildGroup:
                apply_rebuild_transition(transition, epoch);
                break;
        }
    }
    lod_controller.clear_pending_transitions();
}

void MeshManager::process_completed_group_meshes_standalone(uint64_t epoch, double budget_ms, int32_t max_uploads,
                                                            const Ref<ShaderMaterial>& material,
                                                            const Ref<ShaderMaterial>& water_material) {
    int32_t uploads_this_frame = 0;
    process_completed_group_meshes(epoch, budget_ms, max_uploads, material, water_material, uploads_this_frame, 0.0);
}

void MeshManager::process_completed_group_meshes(uint64_t epoch, double budget_ms, int32_t max_uploads,
                                               const Ref<ShaderMaterial>& material,
                                               const Ref<ShaderMaterial>& water_material,
                                               int32_t& uploads_this_frame,
                                               double elapsed_budget_ms) {
    if (!chunk_scheduler || !chunk_map) {
        return;
    }
    if (chunk_scheduler->completed_group_mesh_count() == 0) {
        return;
    }

    RenderingServer* rs = RenderingServer::get_singleton();
    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);

    const LodSettings& settings = lod_controller.get_settings();

    while (uploads_this_frame < max_uploads && elapsed_budget_ms < budget_ms) {
        bool high_priority = false;
        CompletedGroupMesh completed;
        if (!chunk_scheduler->poll_completed_group_mesh(completed, high_priority)) {
            break;
        }

        if (completed.epoch != epoch || !completed.source_group) {
            for (int32_t i = 0; i < completed.member_count; ++i) {
                int32_t mcx, mcy, mcz;
                ChunkMap::decode_chunk_key(completed.member_keys[i], mcx, mcy, mcz);
                ChunkRenderData* member_data = chunk_map->get_chunk_render_data(mcx, mcy, mcz);
                if (member_data) {
                    member_data->pending_mesh_uploads.fetch_sub(1, std::memory_order_relaxed);
                }
            }
            continue;
        }

        LodGroupRenderData* group = completed.source_group;
        group->pending_mesh_uploads.fetch_sub(1, std::memory_order_relaxed);
        if (group->mesh_job_serial.load(std::memory_order_acquire) != completed.mesh_job_serial) {
            // Superseded by a newer rebuild of the same group - this stale result is
            // discarded, but the members were pinned for this job specifically, so
            // release that pin here rather than leaving it stuck until the chunk map
            // happens to be cleared.
            for (int32_t i = 0; i < completed.member_count; ++i) {
                int32_t mcx, mcy, mcz;
                ChunkMap::decode_chunk_key(completed.member_keys[i], mcx, mcy, mcz);
                ChunkRenderData* member_data = chunk_map->get_chunk_render_data(mcx, mcy, mcz);
                if (member_data) {
                    member_data->pending_mesh_uploads.fetch_sub(1, std::memory_order_relaxed);
                }
            }
            continue;
        }

        const int32_t group_merge_size = lod_merge_size(group->merge_shift);
        const float group_width = static_cast<float>(CHUNK_WIDTH * group_merge_size);

        if (completed.mesh_data.empty && completed.water_mesh_data.empty) {
            release_group_to_individual(group);
            ++uploads_this_frame;
            continue;
        }

        // 3.1 Upload deduplication: skip GPU upload if content hash unchanged
        const bool content_unchanged = group->mesh_content_hash != 0 &&
                                       group->mesh_content_hash == completed.mesh_content_hash;

        if (!content_unchanged) {
            if (!group->mesh_rid.is_valid()) {
                group->mesh_rid = rs->mesh_create();
                group->material_set = false;
                AABB group_aabb(Vector3(0, 0, 0), Vector3(group_width, group_width, group_width));
                rs->mesh_set_custom_aabb(group->mesh_rid, group_aabb);
            } else {
                rs->mesh_clear(group->mesh_rid);
                group->material_set = false;
            }

            int64_t fmt = 0;
            fmt |= RenderingServer::ARRAY_FORMAT_VERTEX;
            fmt |= RenderingServer::ARRAY_FORMAT_NORMAL;
            fmt |= RenderingServer::ARRAY_FORMAT_TEX_UV;
            fmt |= RenderingServer::ARRAY_FORMAT_INDEX;
            fmt |= RenderingServer::ARRAY_FORMAT_CUSTOM0;
            fmt |= static_cast<int64_t>(RenderingServer::ARRAY_CUSTOM_RGBA8_UNORM) << RenderingServer::ARRAY_FORMAT_CUSTOM0_SHIFT;
            fmt |= RenderingServer::ARRAY_FORMAT_CUSTOM1;
            fmt |= static_cast<int64_t>(RenderingServer::ARRAY_CUSTOM_RG_HALF) << RenderingServer::ARRAY_FORMAT_CUSTOM1_SHIFT;
            fmt |= RenderingServer::ARRAY_FLAG_COMPRESS_ATTRIBUTES;

            // Surface 0: opaque
            int surface_index = 0;
            if (!completed.mesh_data.empty) {
                arrays[Mesh::ARRAY_VERTEX] = completed.mesh_data.vertices;
                arrays[Mesh::ARRAY_TEX_UV] = completed.mesh_data.uvs;
                arrays[Mesh::ARRAY_NORMAL] = completed.mesh_data.normals;
                arrays[Mesh::ARRAY_INDEX] = completed.mesh_data.indices;
                arrays[Mesh::ARRAY_CUSTOM0] = completed.mesh_data.custom0;
                arrays[Mesh::ARRAY_CUSTOM1] = completed.mesh_data.custom1;

                if (perf_timer) {
                    ScopedTimer t(*perf_timer, TimerID::GroupMeshUpload);
                    rs->mesh_add_surface_from_arrays(group->mesh_rid, RenderingServer::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), BitField<RenderingServer::ArrayFormat>(fmt));
                } else {
                    rs->mesh_add_surface_from_arrays(group->mesh_rid, RenderingServer::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), BitField<RenderingServer::ArrayFormat>(fmt));
                }

                if (material.is_valid()) {
                    rs->mesh_surface_set_material(group->mesh_rid, surface_index, material->get_rid());
                }
                surface_index++;
            }

            // Surface 1: water (if present)
            if (!completed.water_mesh_data.empty) {
                arrays[Mesh::ARRAY_VERTEX] = completed.water_mesh_data.vertices;
                arrays[Mesh::ARRAY_TEX_UV] = completed.water_mesh_data.uvs;
                arrays[Mesh::ARRAY_NORMAL] = completed.water_mesh_data.normals;
                arrays[Mesh::ARRAY_INDEX] = completed.water_mesh_data.indices;
                arrays[Mesh::ARRAY_CUSTOM0] = completed.water_mesh_data.custom0;
                arrays[Mesh::ARRAY_CUSTOM1] = completed.water_mesh_data.custom1;

                if (perf_timer) {
                    ScopedTimer t(*perf_timer, TimerID::GroupMeshUpload);
                    rs->mesh_add_surface_from_arrays(group->mesh_rid, RenderingServer::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), BitField<RenderingServer::ArrayFormat>(fmt));
                } else {
                    rs->mesh_add_surface_from_arrays(group->mesh_rid, RenderingServer::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), BitField<RenderingServer::ArrayFormat>(fmt));
                }

                if (water_material.is_valid()) {
                    rs->mesh_surface_set_material(group->mesh_rid, surface_index, water_material->get_rid());
                }
            }

            group->material_set = true;
            group->mesh_content_hash = completed.mesh_content_hash;
        }

        // 4.4 Instance budget cap: skip group instance if anchor beyond render distance
        // Uses same 2D horizontal distance + vertical buffer logic as the LOD classifier
        const bool group_within_range = is_group_visible_in_range(
            completed.anchor_cx,
            completed.anchor_cy,
            completed.anchor_cz,
            group_merge_size,
            mesh_render_distance,
            settings.vertical_buffer,
            last_player_chunk_x,
            last_player_chunk_y,
            last_player_chunk_z);

        if (!group_within_range) {
            if (group->instance_rid.is_valid()) {
                rs->instance_set_visible(group->instance_rid, false);
            }
        } else if (!group->instance_rid.is_valid()) {
            group->instance_rid = rs->instance_create();
            rs->instance_set_base(group->instance_rid, group->mesh_rid);
            AABB group_aabb(Vector3(0, 0, 0), Vector3(group_width, group_width, group_width));
            rs->instance_set_custom_aabb(group->instance_rid, group_aabb);
            Transform3D transform;
            transform.origin = Vector3(
                completed.anchor_cx * CHUNK_WIDTH,
                completed.anchor_cy * CHUNK_HEIGHT,
                completed.anchor_cz * CHUNK_DEPTH);
            rs->instance_set_transform(group->instance_rid, transform);
            if (owner) {
                Node3D* owner3d = Object::cast_to<Node3D>(owner);
                if (owner3d) {
                    Ref<World3D> world = owner3d->get_world_3d();
                    if (world.is_valid()) {
                        rs->instance_set_scenario(group->instance_rid, world->get_scenario());
                    }
                }
            }
        }

        const bool group_instance_ready = group_within_range &&
                                          group->instance_rid.is_valid() &&
                                          group->mesh_rid.is_valid();
        const bool keep_individual_members = group_within_range && !group_instance_ready;
        if (group_instance_ready) {
            rs->instance_set_visible(group->instance_rid, true);
        }

        for (int32_t i = 0; i < group->member_count; ++i) {
            int32_t mcx, mcy, mcz;
            ChunkMap::decode_chunk_key(group->member_keys[i], mcx, mcy, mcz);
            ChunkRenderData* member_data = chunk_map->get_chunk_render_data(mcx, mcy, mcz);
            if (!member_data) {
                continue;
            }
            if (group_instance_ready) {
                member_data->render_lod = ChunkRenderLod::HiddenInGroup;
                hide_chunk_instance(member_data);
            } else if (keep_individual_members) {
                member_data->render_lod = ChunkRenderLod::Individual;
                member_data->lod_group_key = 0;
                member_data->current_lod = LodLevel::Individual;
                member_data->effective_lod = LodLevel::Individual;
                show_chunk_instance(member_data, mcx, mcy, mcz);
                queue_immediate_dirty_chunk(mcx, mcy, mcz);
            } else {
                member_data->render_lod = ChunkRenderLod::HiddenInGroup;
                hide_chunk_instance(member_data);
            }
            member_data->pending_mesh_uploads.fetch_sub(1, std::memory_order_relaxed);
        }

        ++uploads_this_frame;
    }
}

} // namespace VoxelEngine
