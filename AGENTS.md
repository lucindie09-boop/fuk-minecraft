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

## Next Steps
- Verify memory consumption in-game with ~10k loaded chunks.
- Confirm no regression in light propagation correctness.

## Critical Context
- Chunk size is 32×32×32, world height is 1024 blocks.
- LOD settings: `lod0_radius=8`, `lod1_radius=24`, vertical_buffer=2, hysteresis=2.
- `Camera3D::get_frustum()` returns `TypedArray<Plane>` (6 planes, world-space, normals pointing inward).
- Frustum is recalculated every frame.
- `acquire_shared_lock()` removed from `ChunkMap`. Callers use `lock_keys()`, `lock_all()`, or auto-locking methods.
- Collision resolver now uses the original binary-search approach.
- Generator `insert` locks only one shard — no longer blocks readers on other shards.
- Block + light memory per chunk: old 130KB → new ~1–20KB (uniform-heavy terrain). At ~9645 loaded chunks: total dropped from ~1.25GB to ~10–200MB.

## Relevant Files
- `src/core/chunk_data.hpp`: `PaletteStorage` class with 8 × 16³ paletted sections for blocks + light, `PalSection` struct, read/write_index helpers, `section_get`/`section_set` generics
- `src/core/chunk_data.cpp`: `ChunkData` methods adapted for paletted block + light storage
- `src/core/chunk_map.hpp`: Sharded locking (64 shards, `ShardLock`, `lock_keys`, `lock_all`, auto-locking methods)
- `src/core/chunk_coords.hpp`: Constants (`CHUNK_WIDTH`, `SECTION_HEIGHT`, `SUBCHUNK_SIZE`, `NUM_SUBCHUNKS`)
- `src/core/frustum.hpp`: Frustum utility (AABB test, chunk visibility)
- `src/mesh/lod_controller.hpp/cpp`: `effective_horizontal_dist_sq()`, `classify_target_lod()`, `apply_hysteresis()`
- `src/mesh/mesh_manager.hpp/cpp`: `set_frustum()`, `update_lod()`
- `src/mesh/mesh_builder_greedy.cpp`: Uses `get_block_unsafe()` + `get_light_packed_word_unsafe()` for neighbor lookups (thread-safe)
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
