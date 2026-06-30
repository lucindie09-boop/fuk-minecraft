# fuk-minecraft

A Minecraft-style voxel engine built in Godot 4 with a custom C++ GDExtension. Procedural terrain generation, chunked world streaming, greedy meshing, colored block lighting, day/night cycle, LOD mesh merging, and frustum-prioritized chunk loading. Reaches ~1120 FPS idle at render distance 32.

## Architecture

- **Godot 4** — renderer, input, audio, and UI
- **C++ GDExtension** — voxel engine core (chunking, meshing, lighting, terrain gen, collision)
- **ThreadPool** — async chunk generation, mesh building, and light propagation (15 workers)
- **RenderingServer** — direct GPU mesh upload for zero SceneTree overhead per chunk
- **LOD mesh merging** — 2×2×2 chunk groups via `MergedMeshBuilder`, periodic group rescan
- **Frustum prioritization** — camera frustum extracted each frame; visible chunks prioritized for generation, meshing, retention, LOD detail boost, and dynamic mesh budgets

## Key Systems

| System | File(s) | Notes |
|--------|---------|-------|
| Chunk data | `src/core/chunk_data.hpp` | 32×32×32 chunks, packed light (4 bits per channel) |
| Block types | `src/core/block_types.hpp` | Registry singleton, property flags, per-face textures |
| Chunk map | `src/core/chunk_map.hpp` | `shared_mutex` reader/writer lock, resumable bucket-cursor iteration |
| Frustum utility | `src/core/frustum.hpp` | AABB-in-frustum test, used by generation, mesh, unload, LOD, and budget scaling |
| World updater | `src/world/world_updater.hpp/cpp` | Per-frame budgeted scheduling (generate → light → mesh → upload) |
| Generation scheduler | `src/world/generation_scheduler.hpp/cpp` | Standalone refactored generation loop (future replacement) |
| Mesh queue | `src/mesh/mesh_queue.hpp` | Priority queue sorted by urgent > in-frustum > distance |
| Mesh builder | `src/mesh/mesh_builder.hpp/cpp` | Greedy + standard face culling, neighbor-aware, thread-local instances |
| Mesh manager | `src/mesh/mesh_manager.hpp/cpp` | Upload deduplication (FNV-1a), lazy RID creation, instance budget capping |
| Lighting | `src/lighting/light_propagator.cpp` | Async block-light propagation on worker threads, sky-light columns |
| Terrain gen | `src/worldgen/chunk_generator.hpp/cpp` | Multi-octave FBM noise, biomes, lakes, caves, surface height cache |
| Collision | `src/engine/collision_resolver.cpp` | 3D DDA with leading-face-only block checks, no binary search (no Godot physics nodes) |
| Day/night | `src/world/day_night_cycle.hpp` | Shader-driven sky-light intensity + color blending, sunset bleed fix |
| LOD groups | `src/mesh/mesh_manager_lod.hpp/cpp` | 2×2×2 chunk merging, hysteresis, split-on-edit, periodic rescan |
| Frame budgets | `src/core/frame_budgets.hpp` | Tiered budgets for generate/light/mesh/upload (idle/active/loading) |
| Performance timers | `src/core/performance_timer.hpp` | Scoped frame-by-frame profiling |

## Performance

| Metric | Value |
|--------|-------|
| FPS (idle, rd=32) | ~1120-1156 |
| Frame time (idle) | ~0.88ms |
| Your C++ (idle) | ~0.01ms |
| Godot + GPU overhead | ~0.87ms |
| Draw calls (idle) | ~2278 (940 individual + 1338 group instances) |
| Chunks loaded | ~9645 |
| Worker threads | 15 (hw_concurrency - 1) |
| Async light propagation | 3×3×3 block-light flood-fill on thread pool |

## Build

Requires:
- Godot 4.2+
- Python 3 + SCons
- C++17 compiler (MSVC on Windows, GCC/Clang on Linux/macOS)
- `godot-cpp` submodule (already present in `godot-cpp/`)

```bash
# Build the extension DLL
scons

# Build standalone tools
scons debug    # terrain_debug.exe
scons bench    # benchmark.exe
scons test     # run_tests.exe
```

## Running

Open the project root in Godot 4 and press Play. The main scene is `Main.tscn`. The C++ extension loads automatically from `bin/libgdextension.windows.template_debug.x86_64.dll` (or platform-specific equivalent).

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

The `ChunkManager` node exposes several editor properties for tuning:

- **render_distance** — how many chunks to load around the player
- **editor_render_distance** — reduced distance in the Godot editor
- **smooth_lighting** — toggle smooth vertex lighting
- **player_light_enabled / player_light_level** — player-following dynamic light
- **day_night_cycle_enabled / day_duration** — day cycle control
- **fog_density** — exponential fog distance
- **lod0_radius / lod1_radius** — LOD group radii (`voxel_engine_controller.cpp`)
- **FrameBudgets** (in `src/core/frame_budgets.hpp`) — per-frame generation/mesh/upload caps

## Notes

- The player script (`player.gd`) is a temporary GDScript placeholder. A C++ player controller is planned.
- Modified chunks are saved to `user://chunks/` via RLE-compressed `.chunk` files.
- The `analyze.py` script analyzes biome maps from the `terrain_debug` tool.
- LOD groups use 2×2×2 chunks (8 members per group) with FNV-1a deduplication and lazy material binding.
- Frustum prioritization requires a `Camera3D` child on the player node (named `Camera3D`).
