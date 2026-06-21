# fuk-minecraft

A Minecraft-style voxel engine built in Godot 4 with a custom C++ GDExtension. Procedural terrain generation, chunked world streaming, greedy meshing, colored block lighting, and a day/night cycle.

## Architecture

- **Godot 4** — renderer, input, audio, and UI
- **C++ GDExtension** — voxel engine core (chunking, meshing, lighting, terrain gen, collision)
- **ThreadPool** — async chunk generation and mesh building
- **RenderingServer** — direct GPU mesh upload for zero SceneTree overhead per chunk

## Key Systems

| System | File(s) | Notes |
|--------|---------|-------|
| Chunk data | `src/core/chunk_data.hpp` | 32×32×32 chunks, packed light (4 bits per channel) |
| Block types | `src/core/block_types.hpp` | Registry singleton, property flags, per-face textures |
| Chunk map | `src/core/chunk_map.hpp` | `shared_mutex` reader/writer lock |
| World updater | `src/world/world_updater.hpp/cpp` | Per-frame budgeted scheduling (generate → light → mesh → upload) |
| Mesh builder | `src/mesh/mesh_builder.hpp/cpp` | Greedy + standard face culling, neighbor-aware |
| Lighting | `src/lighting/light_propagator.cpp` | Incremental block-light propagation, sky-light columns |
| Terrain gen | `src/worldgen/chunk_generator.hpp/cpp` | Multi-octave FBM noise, biomes, lakes, caves |
| Collision | `src/engine/voxel_engine_controller.cpp` | `resolve_voxel_collision` custom AABB grid query (no Godot physics nodes) |
| Day/night | `src/world/day_night_cycle.hpp` | Shader-driven sky-light intensity + color blending |

## Build

Requires:
- Godot 4.2+
- Python 3 + SCons
- C++17 compiler (MSVC on Windows, GCC/Clang on Linux/macOS)
- `godot-cpp` submodule (already present in `godot-cpp/`)

```bash
# Build the extension DLL
scons

# Build standalone debug tools
scons debug    # terrain_debug.exe
scons bench    # benchmark.exe
```

## Running

Open the project root in Godot 4 and press Play. The main scene is `Main.tscn`. The C++ extension loads automatically from `bin/libgdextension.windows.template_debug.x86_64.dll` (or platform-specific equivalent).

## Controls

| Key | Action |
|-----|--------|
| W/A/S/D | Move |
| Space | Jump |
| Mouse | Look |
| Left click | Break block |
| Right click | Place block |
| 1–0, – | Select block type |
| Esc | Release mouse |

## Performance Tuning

The `ChunkManager` node exposes several editor properties for tuning:

- **render_distance** — how many chunks to load around the player
- **editor_render_distance** — reduced distance in the Godot editor
- **max_loaded_chunks** (in `FrameBudgets`) — hard memory ceiling
- **FrameBudgets** (in `src/world/world_updater.hpp`) — per-frame generation/mesh/upload caps

## Notes

- The player script (`player.gd`) is a temporary GDScript placeholder. A C++ player controller is planned.
- Modified chunks are saved to `user://chunks/` via RLE-compressed `.chunk` files.
- The `analyze.py` script analyzes biome maps from the `terrain_debug` tool.
