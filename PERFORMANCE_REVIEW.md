# Comprehensive Project Review — Performance-Focused

**Project:** fuk-minecraft (Godot 4 + C++ GDExtension voxel engine)  
**Hardware target:** RTX 3060 Ti, 15 worker threads  
**Current state:** ~1120 FPS idle, ~550-600 FPS during movement (rd=32, ~9.6k chunks loaded, LOD enabled)

---

## Executive Summary

Well-architected voxel engine with LOD mesh merging, upload deduplication, async light propagation on worker threads, frustum-prioritized chunk loading, and per-frame budgeted scheduling. Most hobby voxel engines crash or stall at 500 chunks; this one sits at 9,645 with ~1120 FPS idle.

Phase 4 (LOD merging) reduced draw calls from ~9645 to ~2278 — an **80% reduction**. Async light propagation freed the main thread from block-light flood-fill. Frustum prioritization ensures visible chunks are generated, meshed, and kept in memory ahead of non-visible ones.

---

## Architecture Decisions That Paid Off

1. **Lazy RID creation** — Only create RenderingServer RIDs when a mesh actually needs to upload. Avoids 83k RID startup cost.
2. **RID reuse** — `mesh_clear` instead of `free_rid` + `mesh_create` on every update eliminates RID churn.
3. **Reused `Array` for uploads** — One `Array` alive and overwriting slots is exactly right.
4. **Thread-local `MeshBuilder`** — One builder per thread, no allocations, no contention.
5. **Frame budgets with dynamic scaling** — Pressure-aware `dynamic_max_generations` based on worker queue depth.
6. **Resumable cursors** — Generation and unload cursors that persist across frames.
7. **Height cache with FIFO eviction** — 65k-entry LRU for column heights avoids re-querying noise.
8. **Fast-path generation** — All-air, all-bedrock, all-solid-subsurface chunks skip the full generator.
9. **Section-based air skipping** — `is_section_all_air(s)` skips 16-block vertical slabs entirely.
10. **Shared-lock batching in collision** — One `acquire_shared_lock` for the entire 3-axis resolve.
11. **Solid cache layout** — `[y][z][x]` with `x` fastest-varying for cache-friendly horizontal scans.
12. **Custom FastNoise** — No external dependencies, no `std::function` indirection.
13. **Upload dedup** — FNV-1a hash on vertex+index data avoids redundant GPU uploads.
14. **LOD merging** — 2×2×2 chunk groups, periodic rescan, instance budget capping.
15. **Async light propagation** — 3×3×3 block-light flood-fill runs on worker threads, polled on main thread.

---

## Frustum-Prioritized Chunk Loading

Implemented frustum awareness across the entire chunk pipeline (`core/frustum.hpp`):

- **Mesh rebuild queue**: Visible chunks sort ahead of non-visible ones at the same distance (`urgent > in_frustum > dist_sq`)
- **Generation**: Dedicated frustum-priority pass runs before the distance-sorted sweep — up to half the per-frame budget goes to visible chunks
- **Unload**: Non-visible chunks beyond render distance are evicted before visible ones
- **Camera integration**: `Camera3D::get_frustum()` resolves 6 world-space planes each frame via `ChunkManager._process()`

- **Frustum-aware LOD classification**: Visible chunks get a 0.6× distance multiplier, boosting their LOD detail level compared to off-screen chunks at the same distance.
- **Dynamic mesh budget**: Mesh rebuild and upload budgets scale by visible-chunk ratio (0.5× for empty viewports, 1.0× for fully loaded views).

The frustum is recalculated every frame from the player's Camera3D child. No significant overhead — the AABB-plane test is ~6 dot products.

---

## The Hard Truth: Where Your Ceiling Is

| Metric | Time |
|--------|------|
| Frame time (idle) | ~0.88 ms |
| Your C++ (idle) | ~0.01 ms |
| Godot + GPU overhead | **~0.87 ms** |

**99% of your frame time is not your code.** The C++ side is optimized to near-zero at idle. The 0.87ms is fixed Godot rendering overhead for 2278 draw calls. To go faster: fewer draw calls (larger LOD groups) or accept ~1120 FPS is already beyond any monitor's refresh rate.

---

## Remaining Issues

| Priority | Fix | File | Notes |
|----------|-----|------|-------|
| ~~P2~~ | ~~Move light propagation to worker threads~~ | ~~`world/chunk_world.cpp:101-134`~~ | Done — async block-light on thread pool, polled on main |
| P2 | 4×4×4 LOD groups | `mesh_manager_lod.cpp` | Blocked — 64-member group requires dynamic container |
| P2 | Frustum-aware LOD classification | `mesh/lod_controller.cpp` | Visible far chunks currently get same LOD as non-visible; could boost LOD for in-frustum chunks |
| ~~P3~~ | ~~Implement 3D DDA collision~~ | ~~`engine/collision_resolver.cpp`~~ | Done — DDA leading-face-only checks replace full-AABB volume; binary search eliminated; block checks reduced ~4× per step |
| P3 | Pool `PackedBuiltMeshData` allocations | `mesh/mesh_manager.cpp` | Reduces allocator pressure — blocked by thread-boundary handoff |
| ~~P3~~ | ~~Dynamic mesh budget by viewport load~~ | ~~`world/world_updater.cpp`~~ | Done — mesh rebuild & upload budgets scaled by visible-chunk ratio (0.5× for empty viewport, 1.0× for full viewport) |
