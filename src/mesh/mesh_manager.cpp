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
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include "mesh/chunk_visibility.hpp"
#include <chrono>
#include <cstring>

namespace VoxelEngine {

using namespace godot;

namespace {
constexpr int32_t kGreedyDisableBlockRadius = 16;
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

    void execute() override {
        thread_local MeshBuilder builder;
builder.set_smooth_lighting(smooth_lighting);
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

        ChunkRenderData* neighbors[6] = {};
        ChunkRenderData* diag[4] = {};
        if (chunk_map) {
            chunk_map->get_extended_neighbors(chunk_x, chunk_y, chunk_z, neighbors, diag);
        }
        ChunkRenderData* d_x_neg = neighbors[0];
        ChunkRenderData* d_x_pos = neighbors[1];
        ChunkRenderData* d_y_neg = neighbors[2];
        ChunkRenderData* d_y_pos = neighbors[3];
        ChunkRenderData* d_z_neg = neighbors[4];
        ChunkRenderData* d_z_pos = neighbors[5];

        ChunkRenderData* c_neg_x_neg_z = diag[0];
        ChunkRenderData* c_neg_x_pos_z = diag[1];
        ChunkRenderData* c_pos_x_neg_z = diag[2];
        ChunkRenderData* c_pos_x_pos_z = diag[3];

        builder.build_mesh(
            *render_data->data,
            d_x_neg && d_x_neg->data ? d_x_neg->data.get() : nullptr,
            d_x_pos && d_x_pos->data ? d_x_pos->data.get() : nullptr,
            d_y_neg && d_y_neg->data ? d_y_neg->data.get() : nullptr,
            d_y_pos && d_y_pos->data ? d_y_pos->data.get() : nullptr,
            d_z_neg && d_z_neg->data ? d_z_neg->data.get() : nullptr,
            d_z_pos && d_z_pos->data ? d_z_pos->data.get() : nullptr,
            c_neg_x_neg_z && c_neg_x_neg_z->data ? c_neg_x_neg_z->data.get() : nullptr,
            c_neg_x_pos_z && c_neg_x_pos_z->data ? c_neg_x_pos_z->data.get() : nullptr,
            c_pos_x_neg_z && c_pos_x_neg_z->data ? c_pos_x_neg_z->data.get() : nullptr,
            c_pos_x_pos_z && c_pos_x_pos_z->data ? c_pos_x_pos_z->data.get() : nullptr
        );

        PackedBuiltMeshData packed_mesh;
        const auto& vertices = builder.get_vertices();
        const auto& indices = builder.get_indices();

        if (vertices.empty() || indices.empty()) {
            packed_mesh.empty = true;
        } else {
            packed_mesh.empty = false;
            packed_mesh.vertices.resize(vertices.size());
            packed_mesh.normals.resize(vertices.size());
            packed_mesh.uvs.resize(vertices.size());
            packed_mesh.uv2s.resize(vertices.size());
            packed_mesh.colors.resize(vertices.size());
            packed_mesh.indices.resize(indices.size());

            Vector3* v_ptr = packed_mesh.vertices.ptrw();
            Vector3* n_ptr = packed_mesh.normals.ptrw();
            Vector2* uv_ptr = packed_mesh.uvs.ptrw();
            Vector2* uv2_ptr = packed_mesh.uv2s.ptrw();
            Color* c_ptr = packed_mesh.colors.ptrw();
            int32_t* idx_ptr = packed_mesh.indices.ptrw();

            constexpr float kInv127 = 1.0f / 127.0f;
            constexpr float kInv255 = 1.0f / 255.0f;

            for (size_t i = 0; i < vertices.size(); i++) {
                const Vertex& v = vertices[i];
                v_ptr[i] = Vector3(v.x, v.y, v.z);
                n_ptr[i] = Vector3(v.nx * kInv127, v.ny * kInv127, v.nz * kInv127);
                uv_ptr[i] = Vector2(v.u, v.v);
                uv2_ptr[i] = Vector2(static_cast<float>(v.texture_index), v.ao * kInv255);
                c_ptr[i] = Color(v.light_r * kInv255, v.light_g * kInv255, v.light_b * kInv255, v.sky_light * kInv255);
            }
            std::memcpy(idx_ptr, indices.data(), indices.size() * sizeof(int32_t));
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

        chunk_scheduler->push_completed_mesh(std::move(completed), high_priority);

        render_data->pending_mesh_builds.fetch_sub(1, std::memory_order_relaxed);
    }

    bool high_priority;
};

void MeshManager::process_completed_meshes(uint64_t epoch, double budget_ms, int32_t max_uploads, const Ref<ShaderMaterial>& material) {
    if (!chunk_scheduler || !chunk_map) return;
    // Fast path: skip the loop entirely if no completed meshes are waiting.
    if (chunk_scheduler->completed_mesh_count() == 0) return;
    RenderingServer* rs = RenderingServer::get_singleton();
    int32_t dynamic_max_uploads = max_uploads;

    auto start_time = std::chrono::high_resolution_clock::now();
    int32_t uploads_this_frame = 0;

    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);

    while (uploads_this_frame < dynamic_max_uploads) {
        auto current_time = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(current_time - start_time).count();
        if (elapsed_ms >= budget_ms) break;

        bool high_priority = false;
        CompletedMesh completed;
        if (!chunk_scheduler->poll_completed_mesh(completed, high_priority)) {
            break;
        }

        if (completed.epoch != epoch || !completed.source_chunk) {
            continue;  // do NOT count skipped stale meshes against budget
        }

        ChunkRenderData* render_data = completed.source_chunk;
        render_data->pending_mesh_uploads.fetch_sub(1, std::memory_order_relaxed);
        if (render_data->mesh_job_serial.load(std::memory_order_acquire) != completed.mesh_job_serial) {
            continue;  // do NOT count skipped stale meshes against budget
        }

        if (completed.mesh_data.empty) {
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

        arrays[Mesh::ARRAY_VERTEX] = completed.mesh_data.vertices;
        arrays[Mesh::ARRAY_NORMAL] = completed.mesh_data.normals;
        arrays[Mesh::ARRAY_TEX_UV] = completed.mesh_data.uvs;
        arrays[Mesh::ARRAY_TEX_UV2] = completed.mesh_data.uv2s;
        arrays[Mesh::ARRAY_COLOR] = completed.mesh_data.colors;
        arrays[Mesh::ARRAY_INDEX] = completed.mesh_data.indices;

        // Reuse mesh RID instead of creating a new one every frame — eliminates RID churn.
        if (!render_data->mesh_rid.is_valid()) {
            render_data->mesh_rid = rs->mesh_create();
            AABB chunk_aabb(Vector3(0, 0, 0), Vector3(CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH));
            rs->mesh_set_custom_aabb(render_data->mesh_rid, chunk_aabb);
        } else {
            rs->mesh_clear(render_data->mesh_rid);
        }
        if (perf_timer) {
            ScopedTimer t(*perf_timer, TimerID::MeshUploadGpu);
            rs->mesh_add_surface_from_arrays(render_data->mesh_rid, RenderingServer::PRIMITIVE_TRIANGLES, arrays);
        } else {
            rs->mesh_add_surface_from_arrays(render_data->mesh_rid, RenderingServer::PRIMITIVE_TRIANGLES, arrays);
        }

        if (material.is_valid()) {
            rs->mesh_surface_set_material(render_data->mesh_rid, 0, material->get_rid());
        }

        if (render_data->instance_rid.is_valid()) {
            // Instance already exists, mesh base is already set
        } else {
            // Lazy instance creation: only create when chunk first becomes visible
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
                        RID scenario = world->get_scenario();
                        rs->instance_set_scenario(render_data->instance_rid, scenario);
                    }
                    rs->instance_set_visible(render_data->instance_rid, true);
                }
            }

        uploads_this_frame++;
    }
    }
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
        return;
    }

    if (render_data->data && render_data->data->fully_solid()) {
        // Per-face occlusion: check if every axis-aligned face is covered by solid blocks
        // in the adjacent neighbor (or absent at world boundaries).
        bool occ = true;
        constexpr int W = CHUNK_WIDTH, H = CHUNK_HEIGHT, D = CHUNK_DEPTH;
        // X- face -> d_x_neg's X+ face (x=31)
        if (d_x_neg && d_x_neg->data) {
            if (!d_x_neg->data->fully_solid() && !d_x_neg->data->is_all_air()) {
                const BlockID* b = d_x_neg->data->blocks();
                for (int y = 0; y < H && occ; ++y)
                    for (int z = 0; z < D; ++z)
                        if (b[y * W * H + z * W + 31] == BlockIDs::AIR) { occ = false; break; }
            }
        } else occ = false;
        // X+ face -> d_x_pos's X- face (x=0)
        if (occ && d_x_pos && d_x_pos->data) {
            if (!d_x_pos->data->fully_solid() && !d_x_pos->data->is_all_air()) {
                const BlockID* b = d_x_pos->data->blocks();
                for (int y = 0; y < H && occ; ++y)
                    for (int z = 0; z < D; ++z)
                        if (b[y * W * H + z * W] == BlockIDs::AIR) { occ = false; break; }
            }
        } else occ = false;
        // Y- face -> d_y_neg's Y+ face (y=31)
        if (occ && d_y_neg && d_y_neg->data) {
            if (!d_y_neg->data->fully_solid() && !d_y_neg->data->is_all_air()) {
                const BlockID* b = d_y_neg->data->blocks();
                for (int x = 0; x < W && occ; ++x)
                    for (int z = 0; z < D; ++z)
                        if (b[x + 31 * W + z * W * H] == BlockIDs::AIR) { occ = false; break; }
            }
        } else if (!d_y_neg && chunk_y != 0) occ = false;
        // Y+ face -> d_y_pos's Y- face (y=0)
        if (occ && d_y_pos && d_y_pos->data) {
            if (!d_y_pos->data->fully_solid() && !d_y_pos->data->is_all_air()) {
                const BlockID* b = d_y_pos->data->blocks();
                for (int x = 0; x < W && occ; ++x)
                    for (int z = 0; z < D; ++z)
                        if (b[x + z * W * H] == BlockIDs::AIR) { occ = false; break; }
            }
        } else occ = false;
        // Z- face -> d_z_neg's Z+ face (z=31)
        if (occ && d_z_neg && d_z_neg->data) {
            if (!d_z_neg->data->fully_solid() && !d_z_neg->data->is_all_air()) {
                const BlockID* b = d_z_neg->data->blocks();
                for (int x = 0; x < W && occ; ++x)
                    for (int y = 0; y < H; ++y)
                        if (b[x + y * W + 31 * W * H] == BlockIDs::AIR) { occ = false; break; }
            }
        } else occ = false;
        // Z+ face -> d_z_pos's Z- face (z=0)
        if (occ && d_z_pos && d_z_pos->data) {
            if (!d_z_pos->data->fully_solid() && !d_z_pos->data->is_all_air()) {
                const BlockID* b = d_z_pos->data->blocks();
                for (int x = 0; x < W && occ; ++x)
                    for (int y = 0; y < H; ++y)
                        if (b[x + y * W] == BlockIDs::AIR) { occ = false; break; }
            }
        } else occ = false;
        if (occ) {
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

    thread_pool->enqueue_task(std::move(mesh_task), high_priority);
}

void MeshManager::rebuild_chunk_mesh(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z, uint64_t epoch) {
    if (!chunk_map) return;
    ChunkRenderData* chunk_render_data = chunk_map->get_chunk_render_data(chunk_x, chunk_y, chunk_z);
    if (!chunk_render_data) return;
    chunk_render_data->is_mesh_dirty = true;

    ChunkRenderData* neighbors[6] = {};
    chunk_map->get_neighbors(chunk_x, chunk_y, chunk_z, neighbors);

    auto visibility = ChunkVisibility::evaluate(
        chunk_render_data, neighbors, chunk_map,
        chunk_x, chunk_y, chunk_z
    );

    if (!visibility.is_visible) {
        chunk_render_data->is_mesh_dirty = false;
        chunk_render_data->mesh_job_serial.fetch_add(1, std::memory_order_acq_rel);
        RenderingServer* rs = RenderingServer::get_singleton();
        if (chunk_render_data->mesh_rid.is_valid()) {
            rs->free_rid(chunk_render_data->mesh_rid);
            chunk_render_data->mesh_rid = RID();
        }
        if (chunk_render_data->instance_rid.is_valid()) {
            rs->free_rid(chunk_render_data->instance_rid);
            chunk_render_data->instance_rid = RID();
        }
        if (visibility.should_dirty_below) {
            queue_dirty_chunk(chunk_x, chunk_y - 1, chunk_z);
        }
        return;
    }

    if (visibility.should_dirty_below) {
        queue_dirty_chunk(chunk_x, chunk_y - 1, chunk_z);
    }

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

void MeshManager::reprioritize(int32_t player_cx, int32_t player_cy, int32_t player_cz) {
    mesh_queue.reprioritize(player_cx, player_cy, player_cz);
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
            if (dx*dx + dy*dy + dz*dz > mesh_rd_sq) {
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

} // namespace VoxelEngine