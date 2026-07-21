## Goal
Ongoing: a Minecraft-style voxel engine (Godot 4 + C++ GDExtension) with chunked streaming, greedy meshing, colored lighting, biome-based terrain, and frustum/LOD-prioritized rendering. The frustum-prioritized loading + sharded-locking + palette-storage rework described below is complete and on `main`; this file now also tracks the rendering/content work and repo hygiene that followed it.

## Constraints & Preferences
- Bias toward minimal, high-impact changes that reuse existing infrastructure.
- Camera3D child named "Camera3D" on the player node is the frustum source.
- Chunk size is 32×32×32, world height 1024 blocks (`WORLD_HEIGHT_Y` in `chunk_coords.hpp`).
- Block properties live in `data/block_definitions.json`, not scattered across C++ — that's the single source of truth now (see below).

## Progress
### Done
**Frustum-prioritized loading, sharded locking, palette storage (foundational rework):**
- Created `core/frustum.hpp` — header-only Frustum with 6-plane extraction and AABB test.
- `DirtyChunkEntry` priority: `urgent > in_frustum > dist_sq`; `MeshQueue`/`MeshManager` reprioritize accept an optional `Frustum*`.
- `WorldUpdater` two-phase generation: dedicated frustum-priority pass first, then distance-sorted sweep. `update_unload()` defers unloading visible chunks beyond render distance.
- Dynamic mesh budget: visible-chunk ratio scales `mesh_rebuild_budget`/`upload_budget` from 0.5× (sparse) to 1.0× (full).
- Collision: binary-search AABB approach (a 3D DDA variant was tried and reverted after side-face ghosting bugs).
- **Sharded chunk map**: 64 shards, each with its own `shared_mutex` + `unordered_map`. `ShardLock`/`ExclusiveShardLock` RAII types; `lock_chunk()`, `lock_keys()`, `lock_all()` for ordered multi-shard locking. Single-chunk/block accessors lock only their own shard; batch methods lock all relevant shards in ascending order to avoid deadlock.
- **PaletteStorage** (blocks and light): 8 × 16³ paletted sections per chunk replace the old dense arrays. Uniform sections (all air, all stone) cost only a palette entry; non-uniform sections use 4/8/16-bit indices as needed. Per-chunk memory dropped from ~130KB to ~1–20KB on typical terrain (was ~1.25GB → ~10–200MB at ~9,645 loaded chunks in earlier measurements; not re-benchmarked since).
- Renamed Mountains→StonePlateau biome across enum, block selection, and debug renderers.
- Fixed several correctness bugs found via play-testing: incorrect interior-chunk mesh culling next to non-fully-solid neighbors; a Y-index formula bug in `count_section_blocks`; stale `solid_cache` entries causing missing faces (fixed by zeroing at build start); an editor-viewport `_process` bug.
- Added translucent water as a second mesh surface with its own shader (`voxel_shader_water.gdshader`, `voxel_material_water.tres`) — edge fade, tint, shimmer, Beer-Lambert depth absorption — and wired sky/fog/player-light parameter updates to both terrain and water materials every frame.
- Micro-optimizations across the greedy mesher (single-pass vertical sweep, solid-block fast paths, `BlockID`-typed `solid_cache`, incremental AO tracking) — measured greedy_v cost dropping roughly 2.68ms → ~0.73–0.98ms and greedy_h ~0.82ms → ~0.52ms in earlier profiling on this project's dev machine.
- **Removed the old LOD system entirely**: deleted `lod_controller`, `lod_types`, `lod_mesh_builder`, `merged_mesh_builder`, `mesh_manager_lod.*` (2,435 lines) — the 2×2×2 group-mesh-merging approach, along with `CompletedGroupMesh`, `ChunkRenderLod`/`LodLevel`, and the `lod0_radius`/`lod1_radius` inspector properties.

**Rendering, terrain content, and a second LOD approach (later work — not previously logged here):**
- **Replaced the removed LOD system with a simpler, per-chunk distance-based approach**: `lod_distance` and `lod_detail_level` properties on `ChunkManager` drive stride/detail reduction directly inside the greedy mesher for far chunks, instead of merging chunks into groups. Iterated through several correctness passes: boundary footprint aliasing, corner-sampling aliasing in `solid_cache`, majority-vote (instead of first-hit) macro-cell occupancy, a stride-1 "skirt" ring at the LOD transition to prevent T-junction cracks, and a cap of 128 LOD remeshes/frame.
- Fixed a longstanding null-neighbor handling bug across the mesher: missing neighbor chunks are now treated as air for culling purposes rather than force-culling faces (this had been reintroduced/reverted at least once before landing correctly).
- Fixed a race in `cross_writer` (vegetation cross-chunk block writes): now holds an exclusive shard lock across the read-check-write instead of two separate locked operations.
- Fixed a use-after-free: neighbor chunks are now pinned (via `pending_mesh_builds` atomic) during mesh build so they can't be unloaded mid-build.
- **Unified block definitions into a single JSON source of truth** (`data/block_definitions.json`): `BlockRegistry::load_from_json` replaces scattered per-block C++ definitions; texture/emissive arrays are generated from the same file.
- **Added emissive texture support**: a second `Texture2DArray` for per-face glow maps, sourced from `data/block_definitions.json`, threaded through `mesh_builder_faces.cpp` and `material_manager.cpp`.
- **Added vegetation generation**: tree placement with minimum spacing between trunks, deferred cross-chunk writes to the main thread to avoid shard-lock contention, exposed as a `vegetation_enabled` inspector toggle. Fixed a greedy-mesh vertex winding bug (CW vs CCW) and an AO flip condition discovered while re-enabling it.
- **Disabled the sun's shadow map**: both terrain shaders are unshaded, so the shadow pass was pure GPU cost with no visible effect.
- Fixed godray shader white-tint and crosshair rendering artifacts; removed unused textures.
- Added far-region chunk merging, deferred auto-update, and unload notification to reduce redundant work when regions are only partially cached.

**Repo hygiene / process (most recent):**
- Removed a committed Windows `.lnk` shortcut (`src/fuk-minecraft.lnk`) that leaked a local filesystem path; scrubbed it from git history (not just HEAD) via a history rewrite, then force-pushed. Added `*.lnk` to `.gitignore`.
- Removed a stray empty file (`s`) and three orphaned Godot `.obj.import` files (`RealSkeleton.obj.import` and two `tools/*.obj.import` files pointing at build binaries, not real assets). Added `/s` and `*.import` to `.gitignore`.
- Added a `LICENSE` file (GPL-3.0) — the repo had none before, meaning it was technically all-rights-reserved despite being public.
- Added CI (`.github/workflows/build.yml`): builds via SCons on `windows-latest` for every push/PR, then runs `scons test` and the resulting `run_tests.exe`. Not yet extended to Linux/macOS builds.

### In Progress
- (none)

### Blocked
- (none)

## Key Decisions
- Use `Camera3D::get_frustum()` for direct 6-plane extraction in world space; frustum recalculated every frame.
- Two-phase generation: dedicated frustum-priority pass first, then distance-sorted sweep.
- Collision uses the original binary-search approach (a DDA approach was tried and reverted).
- Sharded locking: 64 shards, per-shard `unordered_map` + `shared_mutex`, hash is `key % 64`. Hot paths doing many sequential reads (light propagation, dirty-neighbor checks) batch-lock instead of locking per call.
- Palette sections are 16³ (8 per chunk), matching `dirty_subchunks`; block and light storage share the same generic `PalSection`/`section_get`/`section_set` machinery.
- Greedy mesher reads through `get_block_unsafe()`/`get_light_packed_word_unsafe()` rather than raw pointers or thread-local shared buffers.
- Thread pool stays a single shared pool (not split into separate gen/mesh pools) — a split was tried and reverted after it starved generation throughput ~7×; a high-priority queue path handles contention instead.
- **LOD approach**: after removing the group-merge system, the project settled on per-chunk distance-based stride/detail reduction (`lod_distance`/`lod_detail_level`) rather than re-introducing chunk merging — simpler to reason about and to keep correct at boundaries, at the cost of not reducing draw-call count the way merged groups did.
- **Block definitions are data, not code**: `data/block_definitions.json` is the only place block properties/textures/emissive maps are defined; C++ and the texture-array generator both read from it.
- Solid-block fast paths in the mesher require zeroing `solid_cache` at the start of every build — stale `BlockID`s from a reused thread-local builder previously caused wrong registry lookups in all-air sections.

## Next Steps
- Expand automated test coverage further: the `tests/` suite now has ~82 test cases / 463 assertions across palette storage, mesh culling, greedy mesher, neighbor accessor, and face emission, but concurrency (shard locking, cross-chunk writer races), LOD edge cases, and light propagation remove paths still lack regression tests.
- Extend CI to Linux and/or macOS builds — it currently only builds/tests on `windows-latest`.
- No gameplay layer exists yet beyond block break/place (no inventory, crafting, mobs, multiplayer); `player.gd` remains an explicit temporary placeholder pending a C++ player controller.
- Re-benchmark and refresh any performance numbers before quoting them externally (e.g. in the README) — the last detailed profiling pass predates the water surface, emissive maps, shadow-map removal, and the new stride-based LOD system.

## Critical Context
- Chunk size is 32×32×32 (`CHUNK_WIDTH`/`CHUNK_HEIGHT`/`CHUNK_DEPTH` in `chunk_coords.hpp`), world height 1024 (`WORLD_HEIGHT_Y`).
- `Camera3D::get_frustum()` returns `TypedArray<Plane>` (6 planes, world-space, normals pointing inward), recalculated every frame.
- Generator `insert` locks only one shard — doesn't block readers on other shards.
- Block + light memory per chunk: dense layout was ~130KB; paletted layout is ~1–20KB on typical (uniform-heavy) terrain.
- `data/block_definitions.json` is the single source of truth for block properties, per-face textures, and emissive maps — do not hardcode block properties in C++.
- The old merged-mesh LOD system is gone; the current LOD system is stride/detail-based inside the greedy mesher, controlled by `lod_distance`/`lod_detail_level`.
- CI runs on GitHub Actions (`.github/workflows/build.yml`), Windows-only, build + test on every push/PR.
- Performance numbers quoted in earlier sessions (e.g. ~1120 FPS idle, ~9,645 chunks loaded) came from profiling before the water surface, emissive maps, shadow-map removal, and new LOD system landed — treat them as historical, not current, until re-measured.

## Relevant Files
- `src/core/chunk_data.hpp/cpp`: `PaletteStorage`, `PalSection`, section-based block + light accessors
- `src/core/chunk_map.hpp`: Sharded locking (64 shards, `ShardLock`, `lock_keys`, `lock_all`, auto-locking methods)
- `src/core/chunk_coords.hpp`: Constants (`CHUNK_WIDTH`, `SECTION_HEIGHT`, `WORLD_HEIGHT_Y`, etc.)
- `src/core/frustum.hpp`: Frustum utility (AABB test, chunk visibility)
- `src/core/block_types.hpp/cpp`: `BlockRegistry`, `load_from_json` — reads `data/block_definitions.json`
- `data/block_definitions.json`: Single source of truth for block properties, textures, emissive maps
- `src/mesh/mesh_manager.hpp/cpp`: Per-chunk mesh builds, upload, instance management, `lod_distance`/`lod_detail_level` stride logic
- `src/mesh/mesh_builder.cpp` / `mesh_builder_greedy.cpp` / `mesh_builder_faces.cpp`: Greedy meshing, solid-cache fast paths, water/emissive face routing
- `src/worldgen/chunk_generator.hpp/cpp`: Biome/terrain generation
- `src/worldgen/vegetation_generator.hpp/cpp`: Tree placement, cross-chunk deferred writes
- `src/worldgen/texture_array_generator.hpp`: Diffuse + emissive `Texture2DArray` generation from block definitions
- `src/world/world_updater.hpp/cpp`: `set_frustum()`, budgets, frustum pass
- `src/engine/collision_resolver.hpp/cpp`: Binary-search collision
- `src/core/frame_budgets.hpp`: Budget constants
- `src/godot_bindings/chunk_manager.cpp`: All inspector-exposed properties, block ID constants, camera/frustum entry point
- `src/engine/voxel_engine_controller.hpp/cpp`: Bridges frustum and other state from `ChunkManager` to `WorldUpdater`
- `src/lighting/light_propagator.cpp`: Async light propagation using `lock_all()` + fast methods
- `src/world/block_editor.cpp`: Block break/place using auto-locking `get_block_world`
- `shaders/voxel_shader_water.gdshader`, `materials/voxel_material_water.tres`: Water rendering
- `src/render/material_manager.hpp/cpp`: `get_water_material()`, per-frame parameter push to terrain + water
- `src/render/environment_controller.cpp`: Sun light setup (shadows disabled)
- `src/core/thread_pool.hpp`: Shared worker pool, `hardware_concurrency() - 1` sizing, high-priority queue
- `.github/workflows/build.yml`: CI — SCons build + `scons test` + `run_tests.exe` on Windows
- `LICENSE`: GPL-3.0
- `tests/*.cpp`: doctest suite — ~82 test cases, 463 assertions across palette storage, mesh culling, greedy mesher, neighbor accessor, face emission, noise, and light propagation
