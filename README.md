# fuk-minecraft

A Minecraft-style voxel engine built in Godot 4 with a custom C++ GDExtension. Procedural terrain generation, chunked world streaming, greedy meshing, colored block lighting, day/night cycle, distance-based mesh LOD, and frustum-prioritized chunk loading.

## Architecture

- **Godot 4** — renderer, input, audio, and UI
- **C++ GDExtension** — voxel engine core (chunking, meshing, lighting, terrain gen, collision)
- **ThreadPool** — async chunk generation, mesh building, and light propagation, sized to `hardware_concurrency() - 1` workers
- **RenderingServer** — direct GPU mesh upload for zero SceneTree overhead per chunk
- **Sharded chunk map** — 64 independently-locked shards (`shared_mutex` each), so a write on one shard never blocks readers on another
- **Palette-compressed storage** — block and light data stored as 8 paletted 16³ sections per chunk instead of dense arrays, cutting per-chunk memory from ~130KB to as little as ~1–20KB on uniform terrain
- **Frustum prioritization** — camera frustum extracted each frame; visible chunks get priority for generation, meshing, retention, and LOD detail

## Key Systems

| System | File(s) | Notes |
|--------|---------|-------|
| Chunk data | `src/core/chunk_data.hpp/cpp` | 32×32×32 chunks, palette-compressed blocks + light (8 × 16³ sections each) |
| Block types | `src/core/block_types.hpp/cpp` | Registry singleton loaded from `data/block_definitions.json` — the single source of truth for block properties, per-face textures, and emissive maps |
| Chunk map | `src/core/chunk_map.hpp` | 64-shard `shared_mutex` map, ordered multi-shard locking (`lock_keys`/`lock_all`), resumable bucket-cursor iteration |
| Frustum utility | `src/core/frustum.hpp` | AABB-in-frustum test, used by generation, mesh, unload, and LOD priority |
| World updater | `src/world/world_updater.hpp/cpp` | Per-frame budgeted scheduling (generate → light → mesh → upload) |
| Generation scheduler | `src/world/generation_scheduler.hpp/cpp` | Standalone generation loop |
| Mesh queue | `src/mesh/mesh_queue.hpp` | Priority queue sorted by urgent > in-frustum > distance |
| Mesh builder | `src/mesh/mesh_builder.hpp/cpp` (+ `mesh_builder_faces.cpp`, `mesh_builder_greedy.cpp`) | Greedy + standard face culling, neighbor-aware, thread-local instances, solid-block fast path |
| Mesh manager | `src/mesh/mesh_manager.hpp/cpp` | Upload dedup, lazy RID creation, instance budget capping, distance-based LOD stride/detail scaling |
| Lighting | `src/lighting/light_propagator.cpp` | Async block-light propagation on worker threads, sky-light columns |
| Terrain gen | `src/worldgen/chunk_generator.hpp/cpp` | Multi-octave FBM noise, continentalness/temperature/humidity biomes, coastlines, oceans, near-water surface variants |
| Vegetation | `src/worldgen/vegetation_generator.hpp/cpp` | Tree placement with minimum spacing, deferred cross-chunk writes |
| Collision | `src/engine/collision_resolver.cpp` | Custom binary-search AABB voxel grid query (no Godot physics nodes) |
| Day/night | `src/world/day_night_cycle.hpp` | Shader-driven sky-light intensity + color blending |
| LOD | `lod_distance` / `lod_detail_level` properties (`mesh_manager.cpp`) | Distance-based stride/detail reduction for far chunks, capped remesh-per-frame |
| Frame budgets | `src/core/frame_budgets.hpp` | Tiered budgets for generate/light/mesh/upload (idle/active/loading) |
| Performance timers | `src/core/performance_timer.hpp` | Scoped frame-by-frame profiling |

## Rendering Notes

- Opaque and water are separate mesh surfaces; water uses its own shader (`shaders/voxel_shader_water.gdshader`) with edge fade, tint, shimmer, and Beer-Lambert depth absorption.
- Blocks can carry an emissive texture (second `Texture2DArray`) for glow, driven by `data/block_definitions.json`.
- The directional sun light has shadows disabled — both terrain shaders are unshaded, so the shadow pass was pure overhead with no visual effect.

## Build

Requires:
- Godot 4.1+ (GDExtension `compatibility_minimum`); developed against 4.7
- Python 3 + SCons
- C++17 compiler (MSVC on Windows, GCC/Clang on Linux/macOS)
- `godot-cpp` submodule (run `git submodule update --init --recursive` after cloning)

```bash
# Build the extension library
scons

# Build standalone tools
scons debug    # terrain_debug executable
scons bench    # benchmark executable
scons test     # builds and can run the doctest suite (see tests/)
```

CI (`.github/workflows/build.yml`) builds on `windows-latest` and runs `scons test` + the resulting `run_tests.exe` on every push and pull request. It does not currently cover Linux/macOS builds.

## Running

Open the project root in Godot 4 and press Play. The main scene is `Main.tscn`. The C++ extension loads automatically from the platform-specific library declared in `voxel_engine.gdextension` (e.g. `bin/libgdextension.windows.template_debug.x86_64.dll` on Windows).

## Controls

| Key | Action |
|-----|--------|
| W/A/S/D | Move |
| Space | Jump |
| Shift (in flight) | Descend |
| Mouse | Look |
| Left click | Break block |
| Right click | Place block |
| 1–0, – | Select block type |
| F | Toggle flight |
| T | Toggle day/night cycle |
| G | Step time forward |
| Esc | Release mouse |

## Performance Tuning

The `ChunkManager` node exposes these editor properties (see `src/godot_bindings/chunk_manager.cpp`):

- **seed**, **render_distance**, **editor_render_distance**, **editor_enabled**
- **player_path**, **player_position**, **auto_update**
- **sea_level**, **base_height**, **height_scale**, **mountain_scale**, **biome_size** — terrain shape
- **smooth_lighting** — toggle smooth vertex lighting
- **lod_distance**, **lod_detail_level** — distance-based mesh LOD (stride/detail reduction; see `mesh_manager.cpp`)
- **player_light_enabled** / **player_light_level** — player-following dynamic light
- **day_time**, **day_night_cycle_enabled**, **day_duration**, **day_sky_intensity**/**night_sky_intensity**, **day_sky_color**/**night_sky_color**
- **fog_density** — exponential fog distance
- **vegetation_enabled** — toggle tree/vegetation generation
- **debug_enabled**, **debug_print_interval** — performance report logging
- **FrameBudgets** (in `src/core/frame_budgets.hpp`) — per-frame generation/mesh/upload caps

## Notes

- The player script (`player.gd`) is an explicit temporary GDScript placeholder — a C++ player controller is planned but not yet implemented.
- Modified chunks are saved to `user://chunks/` via RLE-compressed `.chunk` files.
- `analyze.py` analyzes biome maps produced by the `terrain_debug` tool; it requires `Pillow`, `numpy`, and `scipy`, which aren't otherwise part of the build.
- The old 2×2×2 merged-mesh LOD system (`MergedMeshBuilder`, `mesh_manager_lod.*`) was removed; the current LOD approach is per-chunk distance-based stride reduction inside the greedy mesher (`lod_distance`/`lod_detail_level`), not chunk group-merging.
- Frustum prioritization requires a `Camera3D` child on the player node (named `Camera3D`).
- There is no gameplay layer yet beyond block break/place — no inventory, crafting, mobs, or multiplayer.

## License

GPL-3.0 — see `LICENSE`.
