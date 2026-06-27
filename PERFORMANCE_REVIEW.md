# Comprehensive Project Review — Performance-Focused

**Project:** fuk-minecraft (Godot 4 + C++ GDExtension voxel engine)  
**Hardware target:** RTX 3060 Ti, 15 worker threads  
**Current state:** ~1120 FPS idle, ~550-600 FPS during movement (rd=32, ~9.6k chunks loaded, LOD enabled)

---

## Executive Summary

You have built an **extremely well-architected** voxel engine with LOD mesh merging, upload deduplication, and per-frame budgeted scheduling. Most hobby voxel engines crash or stall at 500 chunks; you're sitting at 9,645 with ~1120 FPS idle.

Phase 4 (LOD merging) reduced draw calls from ~9645 to ~2278 — an **80% reduction**. The remaining 0.87ms/frame is Godot+GPU overhead. No amount of C++ micro-optimization will fix that — rendering fewer things is the only path forward.

---

## What I Like (Keep Doing This)

1. **Lazy RID creation** (`chunk_world.cpp:186-191`) — Creating 83k `RenderingServer` RIDs for invisible chunks would destroy startup. You only create them when a mesh actually needs to upload. Smart.
2. **RID reuse** (`mesh_manager.cpp:210-215`) — `mesh_clear` instead of `free_rid` + `mesh_create` on every update eliminates RID churn.
3. **Reused `Array` for uploads** (`mesh_manager.cpp:165-166`) — One `Array` alive and overwriting slots is exactly right.
4. **Thread-local `MeshBuilder`** (`mesh_manager.cpp:38`) — One builder per thread, no allocations, no contention.
5. **Frame budgets with dynamic scaling** (`world_updater.cpp:59-70`) — Pressure-aware `dynamic_max_generations` based on worker queue depth.
6. **Resumable cursors** (`world_updater.hpp:127-140`) — Generation and unload cursors that persist across frames.
7. **Height cache with FIFO eviction** (`world_updater.cpp:250-269`) — 65k-entry LRU for column heights.
8. **Fast-path generation** (`chunk_world.cpp:29-54`) — All-air, all-bedrock, all-solid-subsurface chunks skip the full generator.
9. **Section-based air skipping** (`mesh_builder.cpp:124-127`) — `is_section_all_air(s)` skips 16-block vertical slabs entirely.
10. **Shared-lock batching in collision** (`collision_resolver.cpp:69-74`) — One `acquire_shared_lock` for the entire 3-axis resolve.
11. **Solid cache layout** (`mesh_builder.hpp:191`) — `[x][z][y]` with `y` fastest-varying is correct for vertical scan loops.
12. **Custom FastNoise** (`noise.hpp`) — No external dependencies, no `std::function` indirection.
13. **Good test/build hygiene** (`SConstruct:30-48`) — Separate `bench`, `test`, and `debug` targets.
14. **Upload dedup** (`hash_utils.hpp`) — FNV-1a hash on vertex+index data avoids redundant GPU uploads.
15. **LOD merging** (`mesh_manager_lod.cpp`) — 2×2×2 chunk groups, 300-frame periodic rescan, instance budget capping.

---

## Phase 4 — LOD Mesh Merging

Implemented 2×2×2 group merging to reduce draw calls:

- **Pre-LOD:** ~9645 draw calls (one per chunk)
- **Post-LOD:** ~2278 draw calls (940 individual + 1338 group instances)
- **80% reduction** in draw calls
- FPS improved from ~891 to ~1120-1156
- Groups discovered over ~30s via periodic 300-frame rescans
- Instance budget cap uses 2D horizontal distance + vertical buffer ≤ 2 (matching LOD classifier)
- `recover_stuck_lod_chunks` throttled via `needs_stuck_recovery_` flag — only runs after group release/split (cut `world_update` from 0.27ms to 0.01ms at idle)
- `material_set` flag avoids redundant `mesh_surface_set_material` RS calls; reset in both `mesh_create` and `mesh_clear` paths

### LOD Settings (`voxel_engine_controller.cpp:59-63`)
- `lod0_radius = 8` — merge near-player chunks
- `lod1_radius = 24` — merge mid-range chunks
- `merge_shift = 1` — 2×2×2 groups

---

## The Hard Truth: Where Your Ceiling Is

| Metric | Time |
|--------|------|
| Frame time (idle) | ~0.88 ms |
| Your C++ (idle) | ~0.01 ms |
| Godot + GPU overhead | **~0.87 ms** |

**99% of your frame time is not your code.** You have optimized the C++ side to near-zero at idle. The 0.87ms is fixed Godot rendering overhead for 2278 draw calls. To go faster, you need fewer draw calls (larger LOD groups or custom rendering pipeline) or accept that ~1120 FPS is already beyond any monitor's refresh rate.

---

## Remaining Issues

| Priority | Fix | File | Notes |
|----------|-----|------|-------|
| P2 | Fix chunk visibility bug | `mesh/chunk_visibility.cpp` | Broken 6-face occlusion check |
| P2 | Transpose `solid_cache` to `[y][z][x]` | `mesh/mesh_builder.hpp:191` | 2-4× mesh build speedup potential |
| P2 | Cache `BlockRegistry*` in `build_mesh` | `mesh/mesh_builder.cpp` | ~10% mesh build speedup |
| P2 | Add missing `ScopedTimer` to `build_mesh` | `mesh/mesh_builder.cpp:49` | Enables real profiling |
| P2 | Cap `scaled_mesh/upload_budget` at 32 | `world/world_updater.cpp:181-182` | Prevents 2.8ms spikes |
| P2 | 4×4×4 LOD groups | `mesh_manager_lod.cpp` | Blocked — 64-member group requires dynamic container |
| P2 | Deduplicate `FrameBudgets` | `world/world_updater.hpp`, `core/frame_budgets.hpp` | Code hygiene |
| P3 | Implement 3D DDA collision | `engine/collision_resolver.cpp` | 10× fewer collision checks |
| P3 | Move light propagation to worker threads | `world/chunk_world.cpp:92-115` | Frees main thread budget |
| P3 | Pool `PackedBuiltMeshData` allocations | `mesh/mesh_manager.cpp` | Reduces allocator pressure |
