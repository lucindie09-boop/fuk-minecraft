#include "mesh/mesh_manager.hpp"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
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

namespace VoxelEngine {

using namespace godot;

namespace {
constexpr int32_t kGreedyDisableBlockRadius = 16;

PackedBuiltMeshData pack_vertex_array(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
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

}

struct MeshBuildTask : Task {
    ChunkMap* chunk_map;
    ChunkScheduler* chunk_scheduler;
    std::atomic<uint64_t>* async_epoch;
    ChunkRenderData* render_data;
    int32_t chunk_x, chunk_y, chunk_z;
    uint64_t epoch;
    uint64_t mesh_job_serial;
    int32_t player_bx, player_by, player_bz;
    bool smooth_lighting;
    uint8_t dirty_subchunks = 0xFF;

    void execute() override {
        thread_local MeshBuilder builder;
        builder.set_smooth_lighting(smooth_lighting);

        // Narrow work to only dirty sub-chunks
        if (dirty_subchunks != 0xFF) {
            int32_t x_min = CHUNK_WIDTH, x_max = 0;
            int32_t y_min = CHUNK_HEIGHT, y_max = 0;
            int32_t z_min = CHUNK_DEPTH, z_max = 0;
            for (int32_t sx = 0; sx < SUBCHUNK_DIM; ++sx) {
                for (int32_t sy = 0; sy < SUBCHUNK_DIM; ++sy) {
                    for (int32_t sz = 0; sz < SUBCHUNK_DIM; ++sz) {
                        const int32_t idx = sx + sy * SUBCHUNK_DIM + sz * SUBCHUNK_DIM * SUBCHUNK_DIM;
                        if (dirty_subchunks & (1 << idx)) {
                            if (sx * SUBCHUNK_SIZE < x_min) x_min = sx * SUBCHUNK_SIZE;
                            if ((sx + 1) * SUBCHUNK_SIZE > x_max) x_max = (sx + 1) * SUBCHUNK_SIZE;
                            if (sy * SUBCHUNK_SIZE < y_min) y_min = sy * SUBCHUNK_SIZE;
                            if ((sy + 1) * SUBCHUNK_SIZE > y_max) y_max = (sy + 1) * SUBCHUNK_SIZE;
                            if (sz * SUBCHUNK_SIZE < z_min) z_min = sz * SUBCHUNK_SIZE;
                            if ((sz + 1) * SUBCHUNK_SIZE > z_max) z_max = (sz + 1) * SUBCHUNK_SIZE;
                        }
                    }
                }
            }
            if (x_min < x_max && y_min < y_max && z_min < z_max) {
                MeshBuilder::SubChunkBounds bounds;
                bounds.x_min = x_min; bounds.x_max = x_max;
                bounds.y_min = y_min; bounds.y_max = y_max;
                bounds.z_min = z_min; bounds.z_max = z_max;
                builder.set_subchunk_bounds(bounds);
            }
        }

        constexpr int32_t CW = CHUNK_WIDTH;
        constexpr int32_t CH = CHUNK_HEIGHT;
        constexpr int32_t CD = CHUNK_DEPTH;
        int32_t chunk_min_x = chunk_x * CW;
        int32_t chunk_max_x = chunk_x * CW + CW - 1;
        int32_t chunk_min_y = chunk_y * CH;
        int32_t chunk_max_y = chunk_y * CH + CH - 1;
        int32_t chunk_min_z = chunk_z * CD;
        int32_t chunk_max_z = chunk_z * CD + CD - 1;
        int32_t dx = player_bx < chunk_min_x ? chunk_min_x - player_bx :
                     player_bx > chunk_max_x ? player_bx - chunk_max_x : 0;
        int32_t dy = player_by < chunk_min_y ? chunk_min_y - player_by :
                     player_by > chunk_max_y ? player_by - chunk_max_y : 0;
        int32_t dz = player_bz < chunk_min_z ? chunk_min_z - player_bz :
                     player_bz > chunk_max_z ? player_bz - chunk_max_z : 0;
        if (player_bx != INT32_MIN && player_by != INT32_MIN && player_bz != INT32_MIN &&
            std::max(dx, std::max(dy, dz)) <= kGreedyDisableBlockRadius) {
            builder.set_greedy_enabled(false);
        } else {
            builder.set_greedy_enabled(true);
        }

        ChunkRenderData* all_neighbors[26] = {};
        if (chunk_map) {
            chunk_map->get_all_neighbors(chunk_x, chunk_y, chunk_z, all_neighbors);
        }
        auto data_or_null = [](ChunkRenderData* rd) -> const ChunkData* {
            return (rd && rd->data) ? rd->data.get() : nullptr;
        };

        builder.build_mesh(
            *render_data->data,
            data_or_null(all_neighbors[0]),  // neg_x
            data_or_null(all_neighbors[1]),  // pos_x
            data_or_null(all_neighbors[2]),  // neg_y
            data_or_null(all_neighbors[3]),  // pos_y
            data_or_null(all_neighbors[4]),  // neg_z
            data_or_null(all_neighbors[5]),  // pos_z
            data_or_null(all_neighbors[6]),  // neg_x_neg_z
            data_or_null(all_neighbors[7]),  // neg_x_pos_z
            data_or_null(all_neighbors[8]),  // pos_x_neg_z
            data_or_null(all_neighbors[9]),  // pos_x_pos_z
            data_or_null(all_neighbors[10]), // neg_x_neg_y
            data_or_null(all_neighbors[11]), // pos_x_neg_y
            data_or_null(all_neighbors[12]), // neg_x_pos_y
            data_or_null(all_neighbors[13]), // pos_x_pos_y
            data_or_null(all_neighbors[14]), // neg_y_neg_z
            data_or_null(all_neighbors[15]), // neg_y_pos_z
            data_or_null(all_neighbors[16]), // pos_y_neg_z
            data_or_null(all_neighbors[17]), // pos_y_pos_z
            data_or_null(all_neighbors[18]), // neg_x_neg_y_neg_z
            data_or_null(all_neighbors[19]), // pos_x_neg_y_neg_z
            data_or_null(all_neighbors[20]), // neg_x_pos_y_neg_z
            data_or_null(all_neighbors[21]), // pos_x_pos_y_neg_z
            data_or_null(all_neighbors[22]), // neg_x_neg_y_pos_z
            data_or_null(all_neighbors[23]), // pos_x_neg_y_pos_z
            data_or_null(all_neighbors[24]), // neg_x_pos_y_pos_z
            data_or_null(all_neighbors[25])  // pos_x_pos_y_pos_z
        );

        PackedBuiltMeshData packed_mesh = pack_vertex_array(builder.get_vertices(), builder.get_indices());
        PackedBuiltMeshData water_mesh = pack_vertex_array(builder.get_water_vertices(), builder.get_water_indices());

        // Content hash for upload deduplication (opaque only; water always uploaded)
        uint64_t content_hash;
        if (builder.get_vertices().empty() || builder.get_indices().empty()) {
            content_hash = 0;
        } else {
            content_hash = fnv1a_hash_bytes(builder.get_vertices().data(), builder.get_vertices().size() * sizeof(Vertex));
            content_hash = fnv1a_hash_bytes(builder.get_indices().data(), builder.get_indices().size() * sizeof(uint32_t), content_hash);
        }

        if (async_epoch && epoch != async_epoch->load(std::memory_order_acquire)) {
            render_data->pending_mesh_builds.fetch_sub(1, std::memory_order_relaxed);
            return;
        }

        CompletedMesh completed;
        completed.chunk_x = chunk_x;
        completed.chunk_y = chunk_y;
        completed.chunk_z = chunk_z;
        completed.epoch = epoch;
        completed.mesh_job_serial = mesh_job_serial;
        completed.source_chunk = render_data;
        completed.mesh_data = std::move(packed_mesh);
        completed.water_mesh_data = std::move(water_mesh);
        completed.mesh_content_hash = content_hash;

        chunk_scheduler->push_completed_mesh(std::move(completed), high_priority);

        render_data->pending_mesh_builds.fetch_sub(1, std::memory_order_relaxed);
    }

    bool high_priority;
};

void MeshManager::process_completed_meshes(uint64_t epoch, double budget_ms, int32_t max_uploads,
                                           const Ref<ShaderMaterial>& material,
                                           const Ref<ShaderMaterial>& water_material) {
    if (!chunk_scheduler || !chunk_map) return;

    int32_t dynamic_max_uploads = max_uploads;
    int32_t uploads_this_frame = 0;

    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);

    if (chunk_scheduler->completed_mesh_count() > 0) {
    RenderingServer* rs = RenderingServer::get_singleton();

    while (uploads_this_frame < dynamic_max_uploads) {
        bool high_priority = false;
        CompletedMesh completed;
        if (!chunk_scheduler->poll_completed_mesh(completed, high_priority)) {
            break;
        }

        if (completed.epoch != epoch || !completed.source_chunk) {
            continue;
        }

        ChunkRenderData* render_data = completed.source_chunk;
        render_data->pending_mesh_uploads.fetch_sub(1, std::memory_order_relaxed);
        if (render_data->mesh_job_serial.load(std::memory_order_acquire) != completed.mesh_job_serial) {
            continue;
        }

        if (completed.mesh_data.empty && completed.water_mesh_data.empty) {
            if (render_data->mesh_rid.is_valid()) {
                rs->free_rid(render_data->mesh_rid);
                render_data->mesh_rid = RID();
            }
            if (render_data->instance_rid.is_valid()) {
                rs->free_rid(render_data->instance_rid);
                render_data->instance_rid = RID();
            }
            uploads_this_frame++;
            continue;
        }

        // 3.1 Upload deduplication: skip GPU upload if content hash unchanged (first upload always goes through)
        const bool content_unchanged = render_data->mesh_content_hash != 0 &&
                                       render_data->mesh_content_hash == completed.mesh_content_hash;

        if (!content_unchanged) {
            // Reuse mesh RID instead of creating a new one every frame
            if (!render_data->mesh_rid.is_valid()) {
                render_data->mesh_rid = rs->mesh_create();
                render_data->material_set = false;
                AABB chunk_aabb(Vector3(0, 0, 0), Vector3(CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH));
                rs->mesh_set_custom_aabb(render_data->mesh_rid, chunk_aabb);
            } else {
                rs->mesh_clear(render_data->mesh_rid);
                render_data->material_set = false;
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
                    ScopedTimer t(*perf_timer, TimerID::MeshUploadGpu);
                    rs->mesh_add_surface_from_arrays(render_data->mesh_rid, RenderingServer::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), BitField<RenderingServer::ArrayFormat>(fmt));
                } else {
                    rs->mesh_add_surface_from_arrays(render_data->mesh_rid, RenderingServer::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), BitField<RenderingServer::ArrayFormat>(fmt));
                }

                if (material.is_valid()) {
                    rs->mesh_surface_set_material(render_data->mesh_rid, surface_index, material->get_rid());
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
                    ScopedTimer t(*perf_timer, TimerID::MeshUploadGpu);
                    rs->mesh_add_surface_from_arrays(render_data->mesh_rid, RenderingServer::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), BitField<RenderingServer::ArrayFormat>(fmt));
                } else {
                    rs->mesh_add_surface_from_arrays(render_data->mesh_rid, RenderingServer::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), BitField<RenderingServer::ArrayFormat>(fmt));
                }

                if (water_material.is_valid()) {
                    rs->mesh_surface_set_material(render_data->mesh_rid, surface_index, water_material->get_rid());
                }
            }

            render_data->material_set = true;
            render_data->mesh_content_hash = completed.mesh_content_hash;
        }

        // 4.4 Instance budget cap: don't create instances for chunks beyond render distance
        // Uses same 2D horizontal distance + vertical buffer logic as the LOD classifier
        bool within_render_distance = true;
        if (mesh_render_distance > 0 && last_player_chunk_x != INT32_MIN) {
            const int32_t dx = completed.chunk_x - last_player_chunk_x;
            const int32_t dz = completed.chunk_z - last_player_chunk_z;
            const int32_t dy = std::abs(completed.chunk_y - last_player_chunk_y);
            within_render_distance = (dx*dx + dz*dz) <= (mesh_render_distance * mesh_render_distance) && dy <= 10;
        }
        const bool show_instance = render_data->render_lod != ChunkRenderLod::HiddenInGroup && within_render_distance;
        if (render_data->instance_rid.is_valid()) {
            rs->instance_set_visible(render_data->instance_rid, show_instance);
        } else if (show_instance) {
            render_data->instance_rid = rs->instance_create();
            rs->instance_set_base(render_data->instance_rid, render_data->mesh_rid);
            AABB chunk_aabb(Vector3(0, 0, 0), Vector3(CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH));
            rs->instance_set_custom_aabb(render_data->instance_rid, chunk_aabb);
            Transform3D transform;
            transform.origin = Vector3(completed.chunk_x * CHUNK_WIDTH, completed.chunk_y * CHUNK_HEIGHT, completed.chunk_z * CHUNK_DEPTH);
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
            rs->instance_set_visible(render_data->instance_rid, true);
        }

        if (completed.chunk_y > 0) {
            queue_dirty_chunk(completed.chunk_x, completed.chunk_y - 1, completed.chunk_z);
        }

        uploads_this_frame++;
    }
    }

    process_completed_group_meshes(epoch, budget_ms, dynamic_max_uploads, material, water_material, uploads_this_frame, 0.0);
}

static inline bool should_cull_neighbor(BlockID current, BlockID neighbor, FaceDirection direction, const BlockRegistry& registry) {
    if (neighbor == BlockIDs::AIR) {
         return false;
    }
    const BlockType& neighbor_type = registry.get_block(neighbor);
    if (HasProperty(neighbor_type.properties, BlockProperty::Transparent)) {
        if (current != neighbor) return false;
    }
    const BlockType& current_type = registry.get_block(current);
    if (current == neighbor && current_type.cull_against_same) return true;
    if (direction == FaceDirection::Right || direction == FaceDirection::Left ||
        direction == FaceDirection::Front || direction == FaceDirection::Back) {
        float current_height = 1.0f - current_type.top_face_offset;
        float neighbor_height = 1.0f - neighbor_type.top_face_offset;
        if (neighbor_height < current_height) return false;
        if (neighbor_height > current_height) return true;
    }
    return true;
}

void MeshManager::rebuild_rendering_server_mesh(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z, uint64_t epoch,
                                                   ChunkRenderData* render_data,
                                                   ChunkRenderData* d_x_neg,
                                                   ChunkRenderData* d_x_pos,
                                                   ChunkRenderData* d_y_neg,
                                                   ChunkRenderData* d_y_pos,
                                                   ChunkRenderData* d_z_neg,
                                                   ChunkRenderData* d_z_pos) {
    if (!render_data || !render_data->is_mesh_dirty) return;
    if (!thread_pool || !chunk_scheduler || !chunk_map) return;

    // 1.5 Version check: skip enqueuing if versions match
    bool needs_enqueue = false;
    if (render_data->mesh_version != render_data->last_built_version) {
        needs_enqueue = true;
    } else {
        ChunkRenderData* neighbors[6] = { d_x_neg, d_x_pos, d_y_neg, d_y_pos, d_z_neg, d_z_pos };
        for (int i = 0; i < 6; ++i) {
            uint32_t n_ver = neighbors[i] ? neighbors[i]->mesh_version : 0;
            if (n_ver != render_data->last_built_neighbor_versions[i]) {
                needs_enqueue = true;
                break;
            }
        }
    }

    if (!needs_enqueue) {
        render_data->is_mesh_dirty = false;
        render_data->dirty_subchunks = 0;
        return;
    }

    // Occlusion culling: skip mesh generation for completely invisible chunks
    if (!render_data->data || render_data->data->is_all_air()) {
        if (render_data->mesh_rid.is_valid()) {
            RenderingServer* rs = RenderingServer::get_singleton();
            RID old_mesh = render_data->mesh_rid;
            rs->free_rid(old_mesh);
            render_data->mesh_rid = RID();
        }
        if (render_data->instance_rid.is_valid()) {
            RenderingServer* rs = RenderingServer::get_singleton();
            rs->free_rid(render_data->instance_rid);
            render_data->instance_rid = RID();
        }
        render_data->is_mesh_dirty = false;
        render_data->dirty_subchunks = 0;

        render_data->last_built_version = render_data->mesh_version;
        ChunkRenderData* neighbors[6] = { d_x_neg, d_x_pos, d_y_neg, d_y_pos, d_z_neg, d_z_pos };
        for (int i = 0; i < 6; ++i) {
            render_data->last_built_neighbor_versions[i] = neighbors[i] ? neighbors[i]->mesh_version : 0;
        }
        return;
    }

    if (render_data->data && render_data->data->fully_solid()) {
        const auto neighbor_fully_solid = [](ChunkRenderData* n) {
            return n && n->data && n->data->fully_solid();
        };
        const bool buried =
            neighbor_fully_solid(d_x_neg) &&
            neighbor_fully_solid(d_x_pos) &&
            neighbor_fully_solid(d_y_pos) &&
            neighbor_fully_solid(d_z_neg) &&
            neighbor_fully_solid(d_z_pos) &&
            (neighbor_fully_solid(d_y_neg) || chunk_y == 0);
        if (buried) {
            if (render_data->mesh_rid.is_valid()) {
                RenderingServer* rs = RenderingServer::get_singleton();
                rs->free_rid(render_data->mesh_rid);
                render_data->mesh_rid = RID();
            }
            if (render_data->instance_rid.is_valid()) {
                RenderingServer* rs = RenderingServer::get_singleton();
                rs->free_rid(render_data->instance_rid);
                render_data->instance_rid = RID();
            }
            render_data->is_mesh_dirty = false;
            render_data->dirty_subchunks = 0;

            render_data->last_built_version = render_data->mesh_version;
            ChunkRenderData* neighbors[6] = { d_x_neg, d_x_pos, d_y_neg, d_y_pos, d_z_neg, d_z_pos };
            for (int i = 0; i < 6; ++i) {
                render_data->last_built_neighbor_versions[i] = neighbors[i] ? neighbors[i]->mesh_version : 0;
            }
            return;
        }
    }

    if (render_data->pending_mesh_builds.load(std::memory_order_acquire) > 0) {
        queue_immediate_dirty_chunk(chunk_x, chunk_y, chunk_z);
        return;
    }
    render_data->is_mesh_dirty = false;
    render_data->pending_mesh_builds.fetch_add(1, std::memory_order_relaxed);
    render_data->pending_mesh_uploads.fetch_add(1, std::memory_order_relaxed);
    const uint64_t mesh_job_serial = render_data->mesh_job_serial.fetch_add(1, std::memory_order_acq_rel) + 1;
    uint64_t key = chunk_map->get_chunk_key(chunk_x, chunk_y, chunk_z);
    const bool high_priority = mesh_queue.erase_urgent(key);

    // Update last built versions
    render_data->last_built_version = render_data->mesh_version;
    ChunkRenderData* neighbors[6] = { d_x_neg, d_x_pos, d_y_neg, d_y_pos, d_z_neg, d_z_pos };
    for (int i = 0; i < 6; ++i) {
        render_data->last_built_neighbor_versions[i] = neighbors[i] ? neighbors[i]->mesh_version : 0;
    }

    const int32_t player_bx = last_player_block_x;
    const int32_t player_by = last_player_block_y;
    const int32_t player_bz = last_player_block_z;

    // Do NOT capture raw neighbor pointers. The worker looks up neighbors by coordinate
    // at build time. If a neighbor was unloaded, the mesh builder sees nullptr (air).
    // Only the center chunk is pinned via pending_mesh_builds.
    auto mesh_task = std::make_unique<MeshBuildTask>();
    mesh_task->chunk_map = chunk_map;
    mesh_task->chunk_scheduler = chunk_scheduler;
    mesh_task->async_epoch = async_epoch;
    mesh_task->render_data = render_data;
    mesh_task->chunk_x = chunk_x;
    mesh_task->chunk_y = chunk_y;
    mesh_task->chunk_z = chunk_z;
    mesh_task->epoch = epoch;
    mesh_task->mesh_job_serial = mesh_job_serial;
    mesh_task->player_bx = player_bx;
    mesh_task->player_by = player_by;
    mesh_task->player_bz = player_bz;
    mesh_task->high_priority = high_priority;
    mesh_task->smooth_lighting = smooth_lighting_enabled;
    mesh_task->dirty_subchunks = render_data->dirty_subchunks;
    render_data->dirty_subchunks = 0;

    thread_pool->enqueue_task(std::move(mesh_task), high_priority);
}

void MeshManager::rebuild_chunk_mesh(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z, uint64_t epoch) {
    if (!chunk_map) return;
    ChunkRenderData* chunk_render_data = chunk_map->get_chunk_render_data(chunk_x, chunk_y, chunk_z);
    if (!chunk_render_data) return;
    chunk_render_data->is_mesh_dirty = true;

    ChunkRenderData* neighbors[6] = {};
    chunk_map->get_neighbors(chunk_x, chunk_y, chunk_z, neighbors);

    rebuild_rendering_server_mesh(chunk_x, chunk_y, chunk_z, epoch, chunk_render_data,
                                  neighbors[0], neighbors[1],
                                  neighbors[2], neighbors[3],
                                  neighbors[4], neighbors[5]);
}

void MeshManager::rebuild_all_meshes_with_neighbors(uint64_t epoch) {
    if (!chunk_map) return;
    std::vector<std::tuple<int32_t, int32_t, int32_t>> dirty_chunks;
    chunk_map->for_each([&](uint64_t key, const std::unique_ptr<ChunkRenderData>& render_data) {
        if (render_data->is_mesh_dirty) {
            int32_t chunk_x, chunk_y, chunk_z;
            ChunkMap::decode_chunk_key(key, chunk_x, chunk_y, chunk_z);
            dirty_chunks.emplace_back(chunk_x, chunk_y, chunk_z);
        }
    });
    for (auto [chunk_x, chunk_y, chunk_z] : dirty_chunks) {
        ChunkRenderData* render_data = chunk_map->get_chunk_render_data(chunk_x, chunk_y, chunk_z);
        ChunkRenderData* d_x_neg = chunk_map->get_chunk_render_data(chunk_x - 1, chunk_y, chunk_z);
        ChunkRenderData* d_x_pos = chunk_map->get_chunk_render_data(chunk_x + 1, chunk_y, chunk_z);
        ChunkRenderData* d_y_neg = chunk_map->get_chunk_render_data(chunk_x, chunk_y - 1, chunk_z);
        ChunkRenderData* d_y_pos = chunk_map->get_chunk_render_data(chunk_x, chunk_y + 1, chunk_z);
        ChunkRenderData* d_z_neg = chunk_map->get_chunk_render_data(chunk_x, chunk_y, chunk_z - 1);
        ChunkRenderData* d_z_pos = chunk_map->get_chunk_render_data(chunk_x, chunk_y, chunk_z + 1);
        if (render_data) {
            rebuild_rendering_server_mesh(chunk_x, chunk_y, chunk_z, epoch, render_data,
                                          d_x_neg, d_x_pos, d_y_neg, d_y_pos, d_z_neg, d_z_pos);
        }
    }
}

void MeshManager::queue_dirty_chunk(int32_t cx, int32_t cy, int32_t cz) {
    if (!chunk_map) return;
    ChunkRenderData* render_data = chunk_map->get_chunk_render_data(cx, cy, cz);
    if (render_data) {
        render_data->is_mesh_dirty = true;
    }
    int32_t dx = cx - last_player_chunk_x;
    int32_t dy = cy - last_player_chunk_y;
    int32_t dz = cz - last_player_chunk_z;
    int32_t dist_sq = dx * dx + dy * dy + dz * dz;
    mesh_queue.queue_dirty_chunk(chunk_map->get_chunk_key(cx, cy, cz), dist_sq, false);
}

void MeshManager::queue_immediate_dirty_chunk(int32_t cx, int32_t cy, int32_t cz) {
    if (!chunk_map) return;
    uint64_t key = chunk_map->get_chunk_key(cx, cy, cz);
    mesh_queue.queue_immediate_dirty_chunk(key, mesh_queue.is_pending(key));
}

void MeshManager::mark_chunk_urgent(int32_t cx, int32_t cy, int32_t cz) {
    if (!chunk_map) return;
    mesh_queue.mark_urgent(chunk_map->get_chunk_key(cx, cy, cz));
}

void MeshManager::reprioritize(int32_t player_cx, int32_t player_cy, int32_t player_cz, const Frustum* frustum) {
    mesh_queue.reprioritize(player_cx, player_cy, player_cz, frustum);
}

void MeshManager::mark_chunks_dirty_for_light(int32_t center_cx, int32_t center_cy, int32_t center_cz) {
    if (!chunk_map) return;
    for (int32_t dy = -1; dy <= 1; dy++) {
        for (int32_t dz = -1; dz <= 1; dz++) {
            for (int32_t dx = -1; dx <= 1; dx++) {
                const int32_t cx = center_cx + dx;
                const int32_t cy = center_cy + dy;
                const int32_t cz = center_cz + dz;
                ChunkRenderData* render_data = chunk_map->get_chunk_render_data(cx, cy, cz);
                if (render_data) {
                    render_data->is_mesh_dirty = true;
                    render_data->mesh_version++;
                }
                queue_dirty_chunk(cx, cy, cz);
            }
        }
    }
}

void MeshManager::process_queue(int32_t max_immediate, int32_t max_rebuilds, double budget_ms) {
    int32_t mesh_rd_sq = mesh_render_distance ? mesh_render_distance * mesh_render_distance : INT32_MAX;
    int32_t pcx = last_player_chunk_x;
    int32_t pcy = last_player_chunk_y;
    int32_t pcz = last_player_chunk_z;
        mesh_queue.process(
            [this, mesh_rd_sq, pcx, pcy, pcz](int32_t cx, int32_t cy, int32_t cz) {
                int32_t dx = cx - pcx;
                int32_t dy = cy - pcy;
                int32_t dz = cz - pcz;
                if (dx*dx + dz*dz > mesh_rd_sq || std::abs(dy) > 10) {
                    return;
                }
                rebuild_chunk_mesh(cx, cy, cz, async_epoch ? async_epoch->load(std::memory_order_acquire) : 0);
            },
        max_immediate,
        max_rebuilds,
        budget_ms
    );
}

void MeshManager::clear() {
    mesh_queue.clear();
    last_player_chunk_x = INT32_MIN;
    last_player_chunk_y = INT32_MIN;
    last_player_chunk_z = INT32_MIN;
    last_player_block_x = INT32_MIN;
    last_player_block_y = INT32_MIN;
    last_player_block_z = INT32_MIN;
}


void MeshManager::mark_all_chunks_dirty() {
if (!chunk_map) return;
chunk_map->for_each([&](uint64_t key, const std::unique_ptr<ChunkRenderData>& render_data) {
if (render_data->data && !render_data->data->is_all_air()) {
}
render_data->is_mesh_dirty = true;
int32_t cx, cy, cz;
ChunkMap::decode_chunk_key(key, cx, cy, cz);
queue_dirty_chunk(cx, cy, cz);
});
}

bool MeshManager::has_pending_mesh_work() const {
    if (!chunk_scheduler) {
        return mesh_queue.size() > 0 || mesh_queue.immediate_size() > 0;
    }
    return chunk_scheduler->completed_mesh_count() > 0 ||
           chunk_scheduler->completed_group_mesh_count() > 0 ||
           mesh_queue.size() > 0 ||
           mesh_queue.immediate_size() > 0;
}

WorldRenderStats MeshManager::gather_render_stats() {
    WorldRenderStats stats;
    if (!chunk_map) {
        return stats;
    }

    chunk_map->for_each([&](uint64_t /*key*/, const std::unique_ptr<ChunkRenderData>& render_data) {
        if (render_data->mesh_rid.is_valid()) {
            ++stats.mesh_rids;
        }
        if (!render_data->instance_rid.is_valid()) {
            return;
        }
        if (render_data->render_lod == ChunkRenderLod::HiddenInGroup) {
            ++stats.hidden_instances;
        } else {
            ++stats.visible_instances;
        }
    });

    if (lod_controller.get_settings().enabled) {
        stats.lod = lod_controller.get_ring_stats();
        stats.lod.pending_group_retries = static_cast<int32_t>(lod_controller.pending_group_retry_count());
        stats.lod.pending_group_transitions = static_cast<int32_t>(lod_controller.pending_transition_count());
        if (chunk_scheduler) {
            stats.lod.completed_group_meshes = static_cast<int32_t>(chunk_scheduler->completed_group_mesh_count());
        }
        stats.lod.group_instances = 0;
        lod_controller.for_each_group([&](uint64_t /*key*/, const LodGroupRenderData& group) {
            ++stats.lod.live_groups;
            if (group.instance_rid.is_valid()) {
                ++stats.lod.group_instances;
            }
            if (group.is_dirty) {
                ++stats.lod.dirty_groups;
            }
            if (group.pending_mesh_builds.load(std::memory_order_acquire) > 0) {
                ++stats.lod.groups_building;
            }
            if (group.pending_mesh_uploads.load(std::memory_order_acquire) > 0) {
                ++stats.lod.groups_uploading;
            }
        });
    }

    return stats;
}

} // namespace VoxelEngine
