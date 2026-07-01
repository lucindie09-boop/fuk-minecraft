#!/usr/bin/env python
import os, sys

env = SConscript("godot-cpp/SConstruct")
env.Append(CPPPATH=["src/"])

# Collect all .cpp files in src/ and subdirectories
sources = Glob("src/*.cpp") + Glob("src/*/*.cpp")

# Exclude standalone tools from the main library
lib_sources = [s for s in sources if os.path.basename(str(s)) not in ("terrain_debug.cpp", "benchmark.cpp")]

library = env.SharedLibrary("bin/libgdextension{}{}".format(env["suffix"], env["SHLIBSUFFIX"]), source=lib_sources)
Default(library)

# Debug terrain renderer (standalone executable)
debug_env = env.Clone()
debug_env.Append(LIBS=[])
debug_env.Append(CPPPATH=["src/"])
debug_prog = debug_env.Program("bin/terrain_debug", ["tools/terrain_debug.cpp", "src/worldgen/chunk_generator.cpp", "src/core/chunk_data.cpp", "src/core/block_types.cpp"])
Alias("debug", debug_prog)

# Performance benchmark (standalone executable)
bench_env = env.Clone()
bench_env.Append(CPPPATH=["src/"])
bench_env.Append(LIBS=[])
bench_sources = ["tools/benchmark.cpp", "src/worldgen/chunk_generator.cpp", "src/core/chunk_data.cpp", "src/core/block_types.cpp"]
bench_prog = bench_env.Program("bin/benchmark", bench_sources)
Alias("bench", bench_prog)

# Unit tests (standalone executable, no Godot runtime needed)
test_env = env.Clone()
test_env.Append(CPPPATH=["src/", "tests/"])
test_env.Append(LIBS=[])
# Use separate build dir to avoid .obj collisions with the shared library
VariantDir("build/tests", "src", duplicate=1)
VariantDir("build/test_src", "tests", duplicate=1)
test_sources = Glob("build/test_src/*.cpp") + [
    "build/tests/core/chunk_data.cpp",
    "build/tests/core/block_types.cpp",
    "build/tests/mesh/mesh_builder.cpp",
    "build/tests/mesh/mesh_builder_faces.cpp",
    "build/tests/mesh/mesh_builder_greedy.cpp",
    "build/tests/mesh/chunk_neighbor_accessor.cpp",
    "build/tests/mesh/ambient_occlusion.cpp",
    "build/tests/mesh/smooth_lighting.cpp",
    "build/tests/lighting/block_light_region.cpp",
]
test_prog = test_env.Program("bin/run_tests", test_sources)
Alias("test", test_prog)
