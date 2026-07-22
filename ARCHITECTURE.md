# Architecture

This document describes the current, stable architecture of the voxel engine. For historical context, progress tracking, and resolved issues, see [AGENTS.md](AGENTS.md).

## Core Constants

- **Chunk size**: 32×32×32 blocks (`CHUNK_WIDTH`, `CHUNK_HEIGHT`, `CHUNK_DEPTH`)
- **World height**: 1024 blocks (`WORLD_HEIGHT_Y`)
- **Section size**: 16³ blocks (8 sections per chunk)
- **Shards**: 64 shards for chunk map locking

## Threading Model

### Thread Pool
- Single shared worker pool with `hardware_concurrency() - 1` threads
- High-priority queue for critical operations
- No split generation/mesh pools (split approach was tried and reverted due to throughput starvation)

### Locking Hierarchy

**ChunkMap Sharded Locking:**
- 64 shards, each with its own `shared_mutex` + `unordered_map`
- Hash: `key % 64`
- Lock types:
  - `ShardLock` (shared, `std::shared_lock`)
  - `ExclusiveShardLock` (exclusive, `std::unique_lock`)
  - `lock_chunk()` - single shard
  - `lock_keys()` / `lock_keys_exclusive()` - multiple shards in ascending order (deadlock-safe)
  - `lock_all()` / `lock_all_exclusive()` - all 64 shards

**Locking Rules:**
1. Single-chunk/block accessors lock only their own shard
2. Batch methods lock all relevant shards in ascending order to avoid deadlock
3. Hot paths with many sequential reads (light propagation, dirty-neighbor checks) batch-lock instead of per-call locking
4. **ChunkData writes**: Must hold `lock_all_exclusive()` or `lock_keys_exclusive()` for targeted shards
5. **`_locked` methods**: Caller MUST already hold exclusive lock, uses `_fast` accessors only, MUST NOT call auto-locking methods
6. **Auto-locking methods** (`get_chunk_data`, `get_chunk_render_data`, `mark_chunks_dirty_for_light`, `queue_dirty_chunk`): Acquire their own shared locks — MUST NOT be called under exclusive lock
7. **Public wrappers**: Acquire exclusive lock → call `_locked` → release lock → call auto-locking accessors for dirty-marking

**Targeted Shard Locking:**
- `lock_keys_exclusive<N>()` locks only shards whose keys appear in input, in ascending shard order
- Used in all hot paths to reduce contention:
  - `set_block_variant()` — 1 chunk key
  - `propagate_block_light_region()` — 27 keys (3×3×3 neighborhood)
  - `place_block` — 27 keys (3×3×3 center)
  - `light_propagate_add` / `light_propagate_remove` — origin 3×3×3 + each seed node's 3×3×3 (deduplicated)
  - `update_block_light_incremental` — 54 keys (origin + center 3×3×3)
  - `PlayerLight::update` — vector of up to 54 keys (old+new chunk 3×3×3)

**BFS Bounded Reach:**
- Max light level 15 < chunk size 32
- 3×3×3 neighborhood (27 keys) covers all BFS paths from a single seed chunk

## Chunk Lifecycle

### Generation
1. `ChunkWorld::generate_chunk()` checks if chunk exists in map
2. If not found, attempts `load_chunk_from_disk()`
3. If load fails, generates via `ChunkGenerator`
4. Inserts into `ChunkMap` with `ChunkRenderData` wrapper
5. Queues for mesh build via `ChunkScheduler`

### Mesh Building
1. `MeshBuilder::build_mesh()` creates mesh data from `ChunkData`
2. Uses `ChunkNeighborAccessor` for 26 neighbor chunks
3. Greedy meshing with stride/detail reduction for LOD (controlled by `lod_distance`/`lod_detail_level`)
4. Neighbor chunks are pinned via `pending_mesh_builds` atomic during build to prevent unload

### Unloading
1. `try_unload_chunk()` checks if chunk can be unloaded (no pending mesh builds, not in frustum)
2. Calls `save_chunk_to_disk()` with atomic write pattern
3. Removes from `ChunkMap`

### Persistence
- **Save format v3**: `[width:u32][height:u32][depth:u32][version:u32=3][crc32:u32][RLE body...]`
- **Atomic writes**: Write to `.tmp` file → create `.bak` backup of existing → atomic rename to target
- **CRC recovery**: On CRC mismatch, attempt to load from `.bak` backup; delete corrupted files if no valid backup
- Supports v3 (CRC32), v2 (legacy RLE), v1 (flat legacy) transparently

## Memory Layout

### PaletteStorage
- 8 sections per chunk, each 16³ blocks
- Uniform sections (all air, all stone) cost only a palette entry
- Non-uniform sections use 4/8/16-bit indices as needed
- Per-chunk memory: ~1–20KB on typical terrain (vs ~130KB with dense layout)
- Block and light storage share generic `PalSection`/`section_get`/`section_set` machinery

### Block Definitions
- **Single source of truth**: `data/block_definitions.json`
- C++ `BlockRegistry::load_from_json()` reads from JSON
- Texture/emissive arrays generated from same file
- Do not hardcode block properties in C++

## Rendering

### Frustum-Prioritized Loading
- `Camera3D::get_frustum()` provides 6 world-space planes
- `DirtyChunkEntry` priority: `urgent > in_frustum > dist_sq`
- Two-phase generation: dedicated frustum-priority pass first, then distance-sorted sweep
- Dynamic mesh budget: visible-chunk ratio scales budget from 0.5× (sparse) to 1.0× (full)

### LOD System
- Per-chunk distance-based stride/detail reduction (not chunk merging)
- Controlled by `lod_distance`/`lod_detail_level` properties
- Stride-1 "skirt" ring at LOD transition prevents T-junction cracks
- Cap of 128 LOD remeshes/frame

### Mesh Surfaces
- Primary surface: opaque terrain with greedy meshing
- Secondary surface: translucent water with edge fade, tint, shimmer, Beer-Lambert depth absorption
- Emissive textures: second `Texture2DArray` for per-face glow maps

## Collision

- Binary-search AABB approach (3D DDA variant was tried and reverted)
- Custom voxel collision queries chunk map directly instead of Godot physics nodes
- Player collision via `ChunkManager::resolve_voxel_collision()`

## Key Files

### Core
- `src/core/chunk_data.hpp/cpp` — `PaletteStorage`, `PalSection`, section-based accessors
- `src/core/chunk_map.hpp` — Sharded locking, `lock_keys_exclusive`, auto-locking methods
- `src/core/chunk_coords.hpp` — Constants (`CHUNK_WIDTH`, `SECTION_HEIGHT`, `WORLD_HEIGHT_Y`)
- `src/core/frustum.hpp` — Frustum utility (AABB test, chunk visibility)
- `src/core/block_types.hpp/cpp` — `BlockRegistry`, `load_from_json`
- `src/core/crc32.hpp` — IEEE 802.3 CRC32 for chunk save checksum
- `src/core/thread_pool.hpp` — Shared worker pool, high-priority queue

### Mesh
- `src/mesh/mesh_manager.hpp/cpp` — Per-chunk mesh builds, upload, instance management
- `src/mesh/mesh_builder.cpp` / `mesh_builder_greedy.cpp` / `mesh_builder_faces.cpp` — Greedy meshing
- `src/mesh/chunk_neighbor_accessor.hpp/cpp` — 26 neighbor pointers for mesh building

### World
- `src/world/chunk_world.cpp` — Save/load, all hot paths use `lock_keys_exclusive()`
- `src/world/block_editor.cpp` — `place_block` with targeted locking
- `src/world/player_light.hpp` — Player light with targeted locking
- `src/world/world_updater.hpp/cpp` — Frustum integration, budgets

### Lighting
- `src/lighting/light_propagator.hpp/cpp` — Public wrappers + `_locked` variants
- `src/lighting/block_light_region.hpp/cpp` — Single-chunk additive-only propagation

### Data
- `data/block_definitions.json` — Single source of truth for block properties

### Testing
- `tests/test_concurrency.cpp` — 16 tests for shard locking, deadlock prevention, PaletteStorage
- `tests/test_light_propagation.cpp` — Cross-chunk BFS edge case tests
- `tools/fuzz_light_propagation.cpp` — Fuzz harness for light propagation
- `tools/fuzz_mesh_builder.cpp` — Fuzz harness for mesh building
