## Goal
Implement frustum-prioritized chunk loading across generation, meshing, unload, LOD, and budget pipelines, with sharded chunk map locking and palette-compressed chunk storage.

## Constraints & Preferences
- Bias toward minimal, high-impact changes that reuse existing infrastructure.
- Camera3D child named "Camera3D" on the player node is the frustum source.
- Chunk size is 32×32×32, world height 1024 blocks.

## Progress
### Done
- Created `core/frustum.hpp` — header-only Frustum with 6-plane extraction and AABB test.
- Modified `DirtyChunkEntry` priority: `urgent > in_frustum > dist_sq`.
- Modified `MeshQueue::reprioritize()` and `MeshManager::reprioritize()` to accept optional `Frustum*`.
- Added Frustum to `WorldUpdater` with `set_frustum()` and two-phase generation (visible chunks get dedicated budget before distance-sorted sweep).
- Modified `WorldUpdater::update_unload()` to defer unloading of visible chunks beyond render distance.
- Added `VoxelEngineController::update_frustum()` that forwards to `WorldUpdater`.
- Added camera resolution and `Camera3D::get_frustum()` call in `ChunkManager::_process()`.
- Frustum-aware LOD: `effective_horizontal_dist_sq()` scales distance by 0.6× for visible chunks; `classify_target_lod()` and `apply_hysteresis()` both use it.
- Frustum threaded: `MeshManager::set_frustum()` → `LodController::set_frustum()`; called in `WorldUpdater::update()` before `update_lod()`.
- Dynamic mesh budget: `visible_chunk_ratio_` counted during frustum pass scales `mesh_rebuild_budget` and `upload_budget` from 0.5× (sparse) to 1.0× (full).
- Collision: binary-search approach (3D DDA reverted after bug reports).
- Updated `PERFORMANCE_REVIEW.md`, `README.md`.
- **Sharded chunk map**: replaced single global `shared_mutex` + `unordered_map` with 64 shards, each owning its own `shared_mutex` + `unordered_map`.
  - `acquire_shared_lock()` removed; added `ShardLock` RAII class, `lock_chunk()`, `lock_keys()`, `lock_all()`.
  - All single-chunk/block methods lock only their shard internally.
  - Batch methods lock all relevant shards in ascending order via `lock_keys`.
  - Updated caller hot paths to batch-lock (light propagation uses `lock_all()` + fast methods; dirty neighbor checks use `lock_keys` for 7 keys).
- **PaletteStorage (blocks)**: replaced flat `ChunkStorage` (128KB per chunk) with 8 paletted 16³ sections.
  - Uniform sections (all air, all stone): ~bytes (palette entry only, no index array).
  - Non-uniform sections: 4-bit (~2KB), 8-bit (~4KB), or 16-bit (~8KB) indices.
  - `blocks()` and `materialize_dense_blocks()` removed; greedy mesher uses `get_block_unsafe()` + `get_light_packed_word_unsafe()` directly (thread-safe).
  - Block memory at ~9645 chunks dropped from ~630MB to ~10–100MB.
- **PaletteStorage (light)**: same section-based compression applied to light.
  - `light_sections[8]` replaces the dense `uint16_t[32768]` (65KB per chunk).
  - Clear methods (`clear_block_light`, `clear_sky_light`, `clear_light`) operate directly on paletted sections.
  - `light_packed()` / `get_light_packed_data()` removed — all callers migrated to section-based accessors.
  - Total per-chunk memory: from ~130KB (65KB blocks + 65KB light) to ~1–20KB.
  - At ~9645 loaded chunks: total from ~1.25GB to ~10–200MB.
- Pushed sharded locking + PaletteStorage (blocks + light) to `main`.
- Renamed Mountains→StonePlateau biome across enum, block selection, PGM renderer, debug viewer.
- **Removed `has_no_boundary_faces_produced`** — the interior-chunk optimization incorrectly culled meshes for fully-solid chunks adjacent to non-fully-solid neighbors (e.g. stone chunks 1 block below the plateau surface). Now only the `buried` check (all 6 neighbors fully_solid) skips mesh building; all other chunks go through the greedy mesher which correctly handles per-face culling.
- **Fixed `count_section_blocks` Y-index formula** — the palette-section to chunk-section Y mapping used `si / 4` (z-part) instead of `(si / SECS_PER_DIM) % SECS_PER_DIM` (y-part), causing `is_section_all_air` to report wrong false for chunk section 1 (y=16..31) in surface chunks with blocks only in z>16.
- **Translucent water**: split water faces (SURFACE_WATER=5, WATER=6) into separate mesh surface 1 with `blend_mix` shader.
  - Created `shaders/voxel_shader_water.gdshader` with edge fade, water tint, shimmer/glint.
  - Created `materials/voxel_material_water.tres` referencing water shader.
  - Added `get_water_material()` to `MaterialManager`.
  - Updated `mesh_types.hpp`, `chunk_types.hpp`, `mesh_builder.hpp/cpp`, `mesh_builder_faces.cpp` for water vertex routing.
  - Updated `mesh_manager.cpp`, `mesh_manager_lod.cpp` for two-surface upload (surface 0 opaque, surface 1 water).
  - Updated `world_updater.cpp` to pass water material.
  - Beer-Lambert depth absorption via `depth_tex` + non-linear depth difference; tuned absorption=4.0, floor=0.35.
- **Fixed LOD mesh builders dropping water data**: `lod_mesh_builder.cpp` and `merged_mesh_builder.cpp` now copy `water_vertices`/`water_indices` from the builder into the merged/downsampled output (fixes water missing at LOD group distance).
- **Fixed `side_lowered_offset` in `mesh_builder_faces.cpp`**: was checking the HORIZONTAL neighbor for `top_face_offset` instead of the block BELOW (y-1). When water (offset=0.12) sat adjacent to wet sand (offset=0.0625), the water's offset collapsed the wet sand side face to ~0.12 units tall (2 pixels).
- **Fixed LOD lighting (day/night cycle)**: replaced hardcoded `sky_light=15`/`light=15` in `fill_downsampled_chunk` and `fill_face_neighbor` with actual sampled values from the macro-cell center block via new helper `sample_macro_cell_light`.
- **`pipelines_busy` no longer checks gen queue**: removed `worker_queue_size > 0` from `process_mesh_budgets` — the shared pool always has some backlog during initial loading, which previously forced reduced mesh budgets even when the pool was otherwise able to handle mesh work.
- **Greedy_v single-pass refactor**: 4 separate vertical direction passes → one Y-sweep per column. Greedy_v: 2.684ms → 1.962ms.
- **Solid cache fast path for fully-culled columns**: checks all 4 side cache entries before the 4-direction inner loop; interior columns with all-solid neighbors skip the loop entirely. Greedy_v further → 0.98ms (63% total reduction from baseline).
- **Solid cache bypass for air neighbors**: uses cache to skip paletted-section `get_block_unsafe` when neighbor is air.
- **Micro-optimizations**: `get_block` → `get_block_unsafe`; moved `light_key` fetch after cull check.
- **Greedy_h top-face fast path**: for interior columns with non-air opaque neighbor above + Solid offset=0 block, skip neighbor lookup, cull check, light read, rotation lookup. Greedy_h: 0.817ms → 0.691ms (initial load).
- **solid_cache stores BlockID (uint16_t) instead of bool**: center-block and neighbor reads bypass paletted-section accessor entirely. Eliminated boundary-switch for block neighbor lookup in greedy_v — all 4 neighbor reads are a single cache line. Greedy_h: 0.691ms → 0.519ms; greedy_v: 0.918ms → 0.816ms (initial).
- **Removed `!has_occlusion` merge guard**: was preventing all vertical merges (AO < 1.0 on every side face). Now uniform-AO runs become merged quads. Greedy_v: 0.816ms → 0.727ms (initial). Merge_attempts=0 on flat terrain (all side faces culled by solid neighbors); visible at cliffs/chunk boundaries.
- **Fixed `_process` for editor viewport**: `ChunkManager::_process()` now works in editor by resolving camera from editor viewport.

### In Progress
- (none)

### Blocked
- (none)

## Key Decisions
- Use `Camera3D::get_frustum()` for direct 6-plane extraction in world space.
- Two-phase generation: dedicated frustum-priority pass first, then distance-sorted sweep.
- LOD boost via distance scaling (0.6×) rather than ad-hoc level promotion, for hysteresis compatibility.
- Collision reverted to original binary-search approach (DDA approach caused side-face ghosting).
- Sharded locking uses 64 shards with per-shard `unordered_map` and `shared_mutex`. Hash is `key % 64` (maps to z chunk coordinate).
- Hot paths that issue many sequential reads (light propagation, dirty neighbor checks) batch-lock to avoid per-call mutex overhead.
- Palette sections are 16³ (8 per chunk), matching existing `dirty_subchunks` bitmask.
- Light reuses the same `PalSection` struct (palette + bit-width-adaptive indices) for uniform-light optimization.
- Greedy mesher uses `get_block_unsafe()` + `get_light_packed_word_unsafe()` per-voxel access instead of raw pointers, avoiding thread-local shared buffers.
- `PalStorage::section_get`/`section_set` are generic static helpers used by both block and light accessors, reducing code duplication.
- **Single-pass vertical mesher**: all 4 side directions per voxel in one sweep; shares center block reads and section air-skip checks.
- **Solid cache fast path**: interior columns only (not chunk boundaries); requires all 4 side neighbors non-air + block Solid with offset=0.
- **Thread pool split REVERTED** (discussion only): splitting into gen (2) + mesh (13) starved gen throughput by 7×; single shared pool with high-priority mesh path handles contention correctly.
- **solid_cache stores BlockID**: enables all center + neighbor block reads to bypass paletted section accessor; eliminated boundary-switch for block neighbor lookup in greedy_v; increased memory from 37KB to 74KB per builder (temporary, per-thread).
- **Vertical merge no longer requires `!has_occlusion`**: uniform AO across a run produces correct merged quads regardless of occlusion value; was blocking EVERY interior-column side-face merge previously because AO < 1.0 on nearly all faces due to adjacent solid blocks. On flat terrain, merge_attempts=0 is expected (all side faces culled by neighbors); visible at terrain edges.

## Next Steps
- Verify no visual regression in side-face AO at cliff/chunk-boundary edges.
- Confirm merge_attempts > 0 when exploring terrain with varied elevation.

## Critical Context
- Chunk size is 32×32×32, world height is 1024 blocks.
- LOD settings: `lod0_radius=8`, `lod1_radius=24`, vertical_buffer=2, hysteresis=2.
- `Camera3D::get_frustum()` returns `TypedArray<Plane>` (6 planes, world-space, normals pointing inward).
- Frustum is recalculated every frame.
- `acquire_shared_lock()` removed from `ChunkMap`. Callers use `lock_keys()`, `lock_all()`, or auto-locking methods.
- Collision resolver now uses the original binary-search approach.
- Generator `insert` locks only one shard — no longer blocks readers on other shards.
- Block + light memory per chunk: old 130KB → new ~1–20KB (uniform-heavy terrain). At ~9645 loaded chunks: total dropped from ~1.25GB to ~10–200MB.
- **Performance numbers (current)**: 15 workers (RTX 3060 Ti, hw_concurrency - 1). Initial load ~9645 chunks in ~2s (4822/s). Steady-state: 0.012ms C++ overhead, ~1.5ms GPU, 630-680 FPS, ~1900 verts/chunk. Greedy_h: ~0.365-0.486ms. Greedy_v: ~0.517-0.727ms. Build_mesh: ~0.988-1.349ms.

## Relevant Files
- `src/core/chunk_data.hpp`: `PaletteStorage` class with 8 × 16³ paletted sections for blocks + light, `PalSection` struct, read/write_index helpers, `section_get`/`section_set` generics
- `src/core/chunk_data.cpp`: `ChunkData` methods adapted for paletted block + light storage
- `src/core/chunk_map.hpp`: Sharded locking (64 shards, `ShardLock`, `lock_keys`, `lock_all`, auto-locking methods)
- `src/core/chunk_coords.hpp`: Constants (`CHUNK_WIDTH`, `SECTION_HEIGHT`, `SUBCHUNK_SIZE`, `NUM_SUBCHUNKS`)
- `src/core/frustum.hpp`: Frustum utility (AABB test, chunk visibility)
- `src/mesh/lod_controller.hpp/cpp`: `effective_horizontal_dist_sq()`, `classify_target_lod()`, `apply_hysteresis()`
- `src/mesh/mesh_manager.hpp/cpp`: `set_frustum()`, `update_lod()`
- `src/mesh/mesh_builder_greedy.cpp`: greedy_h top-face fast path, greedy_v solid cache fast path, flush_vertical_merge merge condition
- `src/mesh/mesh_builder.cpp`: solid_cache population now stores BlockID; non-greedy neighbor lookup simplified
- `src/mesh/mesh_builder.hpp`: solid_cache type changed to uint16_t; GreedyVerticalStatsSnapshot struct
- `src/world/world_updater.hpp/cpp`: `set_frustum()`, `visible_chunk_ratio_`, budgets, frustum pass
- `src/engine/collision_resolver.hpp/cpp`: Binary-search collision (after DDA revert)
- `src/core/frame_budgets.hpp`: Budget constants
- `src/core/chunk_types.hpp`: `DirtyChunkEntry` with `in_frustum` field
- `src/mesh/mesh_queue.hpp`: Priority queue with `in_frustum`
- `src/mesh/mesh_types.hpp`: LOD types
- `src/godot_bindings/chunk_manager.cpp`: Camera frustum extraction entry point
- `src/engine/voxel_engine_controller.hpp/cpp`: Bridges frustum from ChunkManager to WorldUpdater
- `src/lighting/light_propagator.cpp`: Uses `lock_all()` + fast methods for propagation
- `src/world/block_editor.cpp`: Uses auto-locking `get_block_world`
- `src/world/chunk_world.cpp`: Uses `lock_keys` for dirty neighbor checks
- `src/world/generation_scheduler.cpp` / `world_updater.cpp`: Uses auto-locking `contains`
- `shaders/voxel_shader_water.gdshader`: Water shader variant (edge fade, shimmer, sun glint)
- `materials/voxel_material_water.tres`: Water ShaderMaterial resource
- `src/render/material_manager.hpp/cpp`: `get_water_material()` lazy loading
- `src/mesh/mesh_builder_faces.cpp`: Water face routing in `add_face`/`add_greedy_face`
- `src/mesh/ambient_occlusion.cpp`: AO computation per face, used by flush_vertical_merge uniformity check
- `src/debug/perf_report.cpp`: Merge stats display (conditional on merge_attempts > 0)
- `src/core/thread_pool.hpp`: Per-worker round-robin FIFO + high-priority queue (shared pool, no gen/mesh split)
