#!/usr/bin/env python
import os, sys

env = SConscript("godot-cpp/SConstruct")
env.Append(CPPPATH=["src/"])

# Optional TSan support (Linux/GCC/Clang only)
tsan = ARGUMENTS.get("TSAN", "0")
if tsan == "1" and sys.platform != "win32":
    env.Append(CCFLAGS=["-fsanitize=thread", "-g", "-O1"])
    env.Append(LINKFLAGS=["-fsanitize=thread"])

# Optional ASan+UBSan support (Linux/GCC/Clang only)
asan = ARGUMENTS.get("ASAN", "0")
if asan == "1" and sys.platform != "win32":
    env.Append(CCFLAGS=["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g", "-O1"])
    env.Append(LINKFLAGS=["-fsanitize=address,undefined"])

# Collect all .cpp files in src/ and subdirectories
sources = Glob("src/*.cpp") + Glob("src/*/*.cpp")

# Exclude standalone tools from the main library
lib_sources = [s for s in sources if os.path.basename(str(s)) not in ("terrain_debug.cpp", "benchmark.cpp")]

library = env.SharedLibrary("bin/libgdextension{}{}".format(env["suffix"], env["SHLIBSUFFIX"]), source=lib_sources)
Default(library)

# Debug terrain renderer (standalone executable)
debug_env = env.Clone()
debug_env.Append(LIBS=[])
debug_prog = debug_env.Program("bin/terrain_debug", ["tools/terrain_debug.cpp", "src/worldgen/chunk_generator.cpp", "src/worldgen/vegetation_generator.cpp", "src/core/chunk_data.cpp", "src/core/block_types.cpp"])
Alias("debug", debug_prog)

# Performance benchmark (standalone executable)
bench_env = env.Clone()
bench_env.Append(CPPPATH=["src/"])
bench_env.Append(LIBS=[])
bench_sources = ["tools/benchmark.cpp", "src/worldgen/chunk_generator.cpp", "src/worldgen/vegetation_generator.cpp", "src/core/chunk_data.cpp", "src/core/block_types.cpp", "src/mesh/mesh_builder.cpp", "src/mesh/mesh_builder_faces.cpp", "src/mesh/mesh_builder_greedy.cpp", "src/mesh/chunk_neighbor_accessor.cpp", "src/mesh/ambient_occlusion.cpp", "src/mesh/smooth_lighting.cpp", "src/lighting/block_light_region.cpp"]
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
    "build/tests/lighting/block_light_region.cpp",
    "build/tests/mesh/smooth_lighting.cpp",
]
test_prog = test_env.Program("bin/run_tests", test_sources)
Alias("test", test_prog)

# LibFuzzer harnesses (Clang-only, Linux/macOS)
# Build with: scons fuzz  (requires clang++)
if sys.platform != "win32":
    # Create fresh environment to avoid godot-cpp GCC-specific flags
    fuzz_env = Environment()
    fuzz_env["CC"] = "clang"
    fuzz_env["CXX"] = "clang++"
    fuzz_env.Append(CPPPATH=["src/", "godot-cpp/include/", "godot-cpp/gen/include/", "godot-cpp/"])
    fuzz_env.Append(CCFLAGS=["-std=c++17", "-fsanitize=fuzzer,address,undefined", "-fno-omit-frame-pointer", "-g", "-O1"])
    fuzz_env.Append(LINKFLAGS=["-fsanitize=fuzzer,address,undefined"])
    # Use separate build dir to avoid .obj collisions with test environment
    VariantDir("build/fuzz", "src", duplicate=1)
    fuzz_sources_common = ["build/fuzz/core/chunk_data.cpp", "build/fuzz/core/block_types.cpp"]
    fuzz_palette = fuzz_env.Program("bin/fuzz_palette", ["tools/fuzz_palette.cpp"] + fuzz_sources_common)
    fuzz_chunk = fuzz_env.Program("bin/fuzz_chunk_load", ["tools/fuzz_chunk_load.cpp"] + fuzz_sources_common)
    Alias("fuzz", [fuzz_palette, fuzz_chunk])
