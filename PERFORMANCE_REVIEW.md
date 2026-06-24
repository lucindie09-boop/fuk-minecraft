# Comprehensive Project Review — Performance-Focused

**Project:** fuk-minecraft (Godot 4 + C++ GDExtension voxel engine)  
**Hardware target:** RTX 3060 Ti, 15 worker threads  
**Current state:** ~940 FPS idle, ~250 FPS during heavy loading (rd=32, ~9.6k chunks loaded)

---

## Executive Summary

You have built an **extremely well-architected** voxel engine. The separation of concerns, threading model, frame budgets, and lazy resource management are all professional-grade. Most hobby voxel engines crash or stall at 500 chunks; you're sitting at 9,645.

**However, you are now hitting the wall of diminishing returns.** Your C++ code is only consuming ~0.26ms/frame when idle. Godot's engine overhead is eating **0.74ms/frame** — 70% of your frame budget. No amount of C++ micro-optimization will fix that. To get materially faster, you need to change *what* you render, not *how fast* you render it.

That said, there are real bugs, cache misses, and scaling issues in your hot loops that are costing you milliseconds during loading. Fix these first before touching Godot's renderer.

---

## What I Like (Keep Doing This)

1. **Lazy RID creation** (`chunk_world.cpp:186-191`) — Creating 83k `RenderingServer` RIDs for invisible chunks would destroy startup. You only create them when a mesh actually needs to upload. Smart.
2. **RID reuse** (`mesh_manager.cpp:210-215`) — `mesh_clear` instead of `free_rid` + `mesh_create` on every update eliminates RID churn. This matters a lot at scale.
3. **Reused `Array` for uploads** (`mesh_manager.cpp:165-166`) — You caught the heap-alloc-per-upload bug. Keeping one `Array` alive and overwriting slots is exactly right.
4. **Thread-local `MeshBuilder`** (`mesh_manager.cpp:38`) — One builder per thread, no allocations, no contention. Good.
5. **Frame budgets with dynamic scaling** (`world_updater.cpp:59-70`) — Pressure-aware `dynamic_max_generations` based on worker queue depth is elegant. This prevents runaway queue growth.
6. **Resumable cursors** (`world_updater.hpp:127-140`) — Generation and unload cursors that persist across frames mean you don't re-scan the same chunks every frame. This is a non-trivial optimization many engines miss.
7. **Height cache with FIFO eviction** (`world_updater.cpp:250-269`) — 65k-entry LRU for column heights avoids recomputing biome/noise during generation scanning. The `unordered_map` + `deque` combo is clean.
8. **Fast-path generation** (`chunk_world.cpp:29-54`) — All-air, all-bedrock, and all-solid-subsurface chunks skip the full generator. This is a massive win for the vertical column case.
9. **Section-based air skipping** (`mesh_builder.cpp:124-127`) — `is_section_all_air(s)` lets the mesh builder skip 16-block vertical slabs entirely. This is why your per-chunk vertex count is only ~6.6k instead of 200k+.
10. **Shared-lock batching in collision** (`collision_resolver.cpp:69-74`) — One `acquire_shared_lock` for the entire 3-axis resolve instead of one per step. This was a real bottleneck you fixed correctly.
11. **Solid cache layout** (`mesh_builder.hpp:191`) — `[x][z][y]` with `y` as the fastest-varying index is correct for the vertical scan loops. The init loop (`mesh_builder.cpp:75-81`) walks it perfectly.
12. **Custom FastNoise** (`noise.hpp`) — No external dependencies, no `std::function` indirection, inline everything. This is exactly what a hot-path noise function should look like.
13. **Good test/build hygiene** (`SConstruct:30-48`) — Separate `bench`, `test`, and `debug` targets. Using `VariantDir` for test objects to avoid collision. Professional.

---

## What I Don't Like (Performance Issues & Bugs)

### 1. **CRITICAL BUG: Chunk visibility logic is completely broken**
**File:** `src/mesh/chunk_visibility.cpp` (lines 17-28)

```cpp
if (neighbor_above && neighbor_above->data && !neighbor_above->data->is_all_air()) {
    is_visible = false;
}
```

This says: *"If the chunk directly above this one contains a single non-air block, this chunk is invisible."* That is wrong. A chunk is invisible only if **all 6 of its faces are fully occluded by opaque neighbors**. A chunk buried at y=-10 with air chunks on its sides is still visible from those sides. A chunk at the surface with a dirt block above it is still visible from the side where the neighbor is air.

**Impact:** You are likely culling chunks that should be visible, causing holes or missing geometry, especially at chunk edges and overhangs. Your occlusion culling is *too aggressive*.

**Fix:** Replace with proper 6-face occlusion checks, or at minimum check the 4 side neighbors for air before declaring invisible.

---

### 2. **CRITICAL: `solid_cache` access pattern in horizontal greedy meshing is cache-terrible**
**File:** `src/mesh/mesh_builder_greedy.cpp` (lines 48-120)

```cpp
for (int32_t y = y0; y < y1; y++) {      // outer
    for (int32_t z = 0; z < CHUNK_DEPTH; z++) {  // middle
        for (int32_t x = 0; x < CHUNK_WIDTH; x++) { // INNERMOST
            // ... solid_cache[x][z][y] ...
        }
    }
}
```

Your `solid_cache` is declared as:
```cpp
std::array<std::array<std::array<uint8_t, CHUNK_HEIGHT>, SC_D>, SC_W> solid_cache{};
```

`x` is the **outermost** array dimension. When `x` varies innermost in the loop, every iteration jumps to a different `std::array` object — a different cache line. You are loading **34 cache lines** for 34 x-steps instead of reading sequentially. This is the worst possible access pattern for this array layout.

By contrast, your **vertical** greedy meshing (`mesh_builder_greedy.cpp:165-242`) uses `for x, for z, for y`, which is cache-friendly because `y` is the innermost dimension.

**Impact:** Horizontal greedy meshing (Top/Bottom faces) is probably 2-4x slower than it should be due to cache misses. For chunks with lots of flat terrain (most chunks), this is a huge fraction of mesh build time.

**Fix:** Swap the horizontal loop order to `for x, for y, for z` (or `for x, for z, for y` — but x must be outer, z middle, y inner... wait, your solid_cache is [x][z][y]. To make it cache-friendly, you want the rightmost index to vary fastest. So you need `for x, for z, for y`. But your horizontal greedy is scanning top/bottom faces, where you need to vary x and z. For a fixed y, the cache-friendly order is `for x { for z { ... } }` because `solid_cache[x][z][y]` — for fixed x, varying z moves to the next inner array. Actually, for fixed x and y, varying z is sequential: `solid_cache[x][0][y]`, `solid_cache[x][1][y]`, ... which are adjacent in the middle array. But varying x jumps between outer arrays.

So the fix is: in `passive_greedy_mesh_horizontal`, change the loop order so `x` is **outermost** and `z` is middle:
```cpp
for (int32_t x = 0; x < CHUNK_WIDTH; x++) {
    for (int32_t z = 0; z < CHUNK_DEPTH; z++) {
        for (int32_t y = y0; y < y1; y++) {
            // ... solid_cache[x][z][y] now has x outer, z middle, y inner
            // But for top/bottom faces, you need to scan x and z together for merging...
        }
    }
}
```

Wait, that breaks the greedy merge logic because the merge is along x for each fixed (y,z) row. You can't trivially swap. **Alternative fix:** Transpose `solid_cache` to `[z][y][x]` or `[y][z][x]` so that `x` is the fastest-varying index. Then the current loop order (`for y, for z, for x`) becomes cache-friendly.

Actually, the cleanest fix: change `solid_cache` layout to `std::array<std::array<std::array<uint8_t, SC_W>, SC_D>, CHUNK_HEIGHT>` so it's `[y][z][x]`. Then `y` varies outermost, `z` middle, `x` innermost — matching the horizontal greedy loop order. But this breaks the vertical greedy loops which currently rely on `[x][z][y]`.

**Better fix:** Keep two caches, or better yet, just transpose the array. Since `solid_cache` is only 34*34*32 = ~37KB, you can afford to lay it out however you want. The vertical greedy loops (`for x, for z, for y`) would also be cache-friendly with `[y][z][x]` because y is innermost. Wait, no: `[y][z][x]` with `for x, for z, for y` means y varies fastest, which IS sequential. Yes! `[y][z][x]` makes BOTH loops cache-friendly:
- Horizontal: `for y, for z, for x` → x varies fastest, sequential ✓
- Vertical: `for x, for z, for y` → y varies fastest, sequential ✓
- Init: `for x, for z, for y` → y varies fastest, sequential ✓

**This is the single biggest meshing performance win available.** Change `solid_cache` to `[y][z][x]` and update all indexing.

---

### 3. **BlockRegistry singleton calls are a hot-loop tax**
**Files:** `src/mesh/mesh_builder.cpp`, `src/mesh/mesh_builder_faces.cpp`, `src/mesh/mesh_builder_greedy.cpp`

Every face of every block triggers `BlockRegistry::get_instance()`:
- `should_cull_against_neighbor` (`mesh_builder.cpp:183-184`) — 2 calls per face
- `compute_face_ao` (`mesh_builder.cpp:210`) — 1 call per face
- `add_face` (`mesh_builder_faces.cpp:14-15`) — 1 call per face
- `apply_special_block_offsets` (`mesh_builder_faces.cpp:249`) — 1 call per face

With ~6,600 vertices per chunk (avg from your report), that's roughly 1,100 faces per chunk × 7 registry calls = **7,700 singleton lookups per chunk**. At 120 mesh rebuilds per frame during loading, that's **924,000 registry lookups per frame**. `get_instance()` is just a `static` local variable access (usually a thread-safe `call_once` or relaxed atomic), but it's still a function call and memory barrier that the compiler can't inline across translation units in some builds.

**Fix:** Cache the registry pointer at the top of `build_mesh`:
```cpp
thread_local BlockRegistry* tl_registry = nullptr;
if (!tl_registry) tl_registry = &BlockRegistry::get_instance();
// Pass tl_registry to should_cull_against_neighbor, compute_face_ao, add_face, etc.
```

Or simply store `const BlockRegistry& registry = BlockRegistry::get_instance();` at the top of `build_mesh` and pass it down by reference. Since `build_mesh` is the entry point, this eliminates all subsequent lookups.

---

### 4. **Missing `ScopedTimer` in `build_mesh`**
**File:** `src/mesh/mesh_builder.cpp` (line 49)

```cpp
void MeshBuilder::build_mesh(...) {
    auto build_start = std::chrono::high_resolution_clock::now(); // Dead code
    clear();
    // ... no timer ...
}
```

You have a `BuildMesh` timer ID defined (`TimerID::BuildMesh`) and reported in `perf_report.cpp`, but **nobody ever samples it**. Your perf report shows `build_mesh` count = 0, so you have no idea how long mesh generation actually takes. You only know `process_completed_meshes` (main thread upload) and `generate_chunk` (terrain generation).

**Fix:** Add:
```cpp
ScopedTimer timer(perf_timer, TimerID::BuildMesh);
```
at the top of `build_mesh`. This is a one-line fix that will reveal whether your mesh building is the bottleneck during loading.

---

### 5. **Frame budget scaling is too aggressive at high render distances**
**File:** `src/world/world_updater.cpp` (lines 181-182)

```cpp
int32_t scaled_mesh_budget   = std::max(budgets.mesh_rebuilds_gameplay, active_render_distance * 2);
int32_t scaled_upload_budget = std::max(budgets.mesh_uploads_gameplay,  active_render_distance * 2);
```

At `render_distance = 32`, this yields **64 mesh rebuilds and 64 uploads per frame**. At 60 FPS, that's 3,840 mesh operations per second. Each mesh upload involves Godot `RenderingServer` calls, mutexes, and Variant copies. Your data shows `process_completed_meshes` averaging **2.8ms** during loading — and that's *with* the 2.5ms budget. You're overshooting your own budget because the loop count is too high.

**Fix:** Cap these budgets. Linear scaling with render distance is wrong — the number of *visible* chunks scales with distance², but the number of *updates* per frame should not. Use a fixed cap or scale with the square root:
```cpp
int32_t scaled_mesh_budget   = std::min(32, std::max(budgets.mesh_rebuilds_gameplay, active_render_distance));
int32_t scaled_upload_budget = std::min(32, std::max(budgets.mesh_uploads_gameplay, active_render_distance));
```

Or even better: make it dynamic based on actual frame time. If `process_completed_meshes` consistently exceeds `budget_ms`, halve the budget next frame.

---

### 6. **Collision resolver uses naive stepping instead of voxel traversal**
**File:** `src/engine/collision_resolver.cpp` (lines 10-54)

```cpp
const float step = 1.0f;
while (remaining > 0.001f) {
    // ... check AABB, binary search if hit ...
    remaining -= step;
}
```

With `SPEED = 100.0` and 60 FPS, motion is ~1.67 units/frame. That's 2 steps. But for falling (GRAVITY = 20), terminal velocity is high. With JUMP = 40, a fall from a height could involve motion of 10+ units, requiring 10+ steps. Each step does an AABB check over `size × size` blocks, and a collision triggers a **10-iteration binary search** — each iteration also doing an AABB check. A single fall could require **100+ block lookups**.

**Fix:** Replace with a **3D Digital Differential Analyzer (DDA)** — step through the voxel grid along the ray/motion vector, checking only the blocks the AABB actually intersects. This is O(distance) in the number of crossed voxels, not O(distance / step_size × block_volume). This is standard in Minecraft clones and reduces collision checks by an order of magnitude for diagonal motion.

---

### 7. **Duplicate `FrameBudgets` definitions**
**Files:** `src/core/frame_budgets.hpp` and `src/world/world_updater.hpp` (lines 30-56)

You have two identical `FrameBudgets` structs in different headers. `world_updater.hpp` defines its own instead of including `frame_budgets.hpp`. This is a maintenance hazard — if you change one, the other is stale. Since `voxel_engine_controller.hpp` includes `frame_budgets.hpp` but `WorldUpdater` uses its own copy, the `frame_budgets.hpp` file is dead weight.

**Fix:** Remove the definition from `world_updater.hpp` and `#include "core/frame_budgets.hpp"` instead.

---

### 8. **Lighting propagation happens on the main thread, synchronously**
**File:** `src/world/chunk_world.cpp` (lines 92-115)

```cpp
if (light_propagator && light_propagated_chunks.find(key) == light_propagated_chunks.end()) {
    ChunkData* chunk = ...;
    if (chunk && chunk->get_emissive_count() > 0) {
        light_propagator->propagate_from_existing_light(...);
    }
    light_propagated_chunks.insert(key);
}
```

Block light propagation is a BFS flood-fill. If a chunk has many light sources (e.g., a city), this can take milliseconds. It runs inside `process_completed_chunks`, which is on the main thread, under the `budget_ms` timer. But it competes with mesh uploads and chunk installs for the same 2.5ms budget.

**Fix:** Move light propagation to a worker thread. The `LightPropagator` only needs read access to the chunk map (shared_lock) and write access to the chunk's light data. You can enqueue a light propagation task to the thread pool and poll the result next frame, just like you do for mesh generation.

---

### 9. **Mesh data is heap-allocated on every worker task**
**File:** `src/mesh/mesh_manager.cpp` (lines 92-127)

```cpp
PackedBuiltMeshData packed_mesh;
packed_mesh.vertices.resize(vertices.size());
// ... 5 more resize calls ...
```

Every mesh rebuild allocates 6 `Packed*Array` objects from Godot's memory pool on the worker thread. At 120 mesh rebuilds per frame during loading, that's **720 PackedArray allocations per frame**. Godot's memory pool is thread-safe but not free. This is a non-trivial allocator tax.

**Fix:** Use an object pool for `PackedBuiltMeshData`, or better yet, write the `Vertex` data directly into a `std::vector` and only convert to Godot arrays on the main thread inside `process_completed_meshes`. The main thread already has the `Array arrays` reuse — extend this to reuse the `PackedVector3Array` objects too. Zero-allocate on workers, allocate once on main thread.

---

### 10. **Render distance = 32 in `Main.tscn` is extremely high for a CPU-side engine**
**File:** `Main.tscn` (line 44)

```
render_distance = 32
```

At rd=32, you have ~9,600 chunk instances. Godot's `RenderingServer` has to cull, sort, and submit 9,600 draw calls every frame. Your idle frame time is 1.06ms, of which **0.74ms is Godot overhead**. That's 70% of your frame time spent in engine code, not your code. You cannot optimize your C++ to make Godot's culling faster.

**Impact:** Even if your C++ code took 0ms, you'd still be capped at ~1,350 FPS. In practice, with a real game (entities, UI, physics), you'll hit 500 FPS easily. But the user wants *more*.

**Fix:**
- **LOD / Chunk merging:** Group distant chunks into larger meshes (e.g., 2×2×2 chunks = 1 mesh). This reduces instance count by 8×. At rd=32, instead of ~9,600 instances, you'd have ~1,200. Godot's overhead drops proportionally.
- **Aggressive culling:** Your current visibility logic is broken (Issue #1), but even fixed, you could do frustum culling in C++ before Godot sees the chunks. `rs->instance_set_visible(instance_rid, false)` is cheaper than letting Godot cull.
- **Lower render distance:** Unless you have a specific need for rd=32, drop it to 16-20. The visual difference is small; the performance gain is massive.

---

## The Hard Truth: Where Your Ceiling Is

You are getting **940 FPS** with 9,645 chunk instances on an RTX 3060 Ti. That is genuinely impressive. But look at your own numbers:

| Metric | Time |
|--------|------|
| Frame time (idle) | 1.06 ms |
| Your C++ (`process_total`) | 0.26 ms |
| Godot + GPU overhead | **0.74 ms** |

**70% of your frame time is not your code.** You have optimized the C++ side extremely well. The remaining 0.26ms is a mix of mutex locks, hash lookups, and a few cache misses. You *can* squeeze another 0.05–0.1ms out of it by fixing the issues above, but you will not double your FPS without attacking the Godot side.

**To get to 2,000+ FPS, you need:**
1. Fewer instances (LOD / merging) — this is the biggest lever
2. A custom rendering pipeline (RenderingDevice API, compute shaders) — massive undertaking
3. Or accept that 940 FPS is more than enough for any monitor and move on to gameplay

---

## Prioritized Action Items

| Priority | Fix | File | Est. Impact |
|----------|-----|------|-------------|
| **P0** | Fix chunk visibility bug | `mesh/chunk_visibility.cpp` | Fixes rendering holes |
| **P0** | Transpose `solid_cache` to `[y][z][x]` | `mesh/mesh_builder.hpp:191` | 2-4× mesh build speedup |
| **P0** | Cache `BlockRegistry*` in `build_mesh` | `mesh/mesh_builder.cpp` | ~10% mesh build speedup |
| **P1** | Add missing `ScopedTimer` to `build_mesh` | `mesh/mesh_builder.cpp:49` | Enables real profiling |
| **P1** | Cap `scaled_mesh/upload_budget` at 32 | `world/world_updater.cpp:181-182` | Prevents 2.8ms spikes |
| **P1** | Deduplicate `FrameBudgets` | `world/world_updater.hpp`, `core/frame_budgets.hpp` | Code hygiene |
| **P2** | Implement 3D DDA collision | `engine/collision_resolver.cpp` | 10× fewer collision checks |
| **P2** | Move light propagation to worker threads | `world/chunk_world.cpp:92-115` | Frees main thread budget |
| **P2** | Implement chunk LOD / merging | New system | Halves Godot overhead |
| **P3** | Pool `PackedBuiltMeshData` allocations | `mesh/mesh_manager.cpp` | Reduces allocator pressure |

---

## One-Liner Challenge

> You spent effort optimizing `frame_budgets`, `chunk_scheduler`, and `thread_pool` — all excellent. But your **cache layout in `solid_cache` is backwards** and your **visibility culling is mathematically wrong**. Fix those two before touching anything else. They are O(n) fixes with O(n) speedups. Everything else is gravy.

