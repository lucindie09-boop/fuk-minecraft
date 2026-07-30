#include "doctest.h"
#include "core/perlin_noise.hpp"
#include <cmath>
#include <limits>

using namespace VoxelEngine;

// ===========================================================================
// RandomSource
// ===========================================================================

TEST_CASE("RandomSource determinism") {
    RandomSource a(42), b(42);
    for (int i = 0; i < 100; ++i) {
        CHECK(a.next_long() == b.next_long());
    }
}

TEST_CASE("RandomSource different seeds produce different sequences") {
    RandomSource a(42), b(999);
    int same = 0;
    for (int i = 0; i < 100; ++i) {
        if (a.next_long() == b.next_long()) ++same;
    }
    CHECK(same < 10);
}

TEST_CASE("RandomSource next_int uniform distribution") {
    RandomSource rng(42);
    int counts[10] = {};
    for (int i = 0; i < 10000; ++i) {
        int v = rng.next_int(10);
        CHECK(v >= 0);
        CHECK(v < 10);
        ++counts[v];
    }
    for (int i = 0; i < 10; ++i) {
        CHECK(counts[i] > 500);
        CHECK(counts[i] < 1500);
    }
}

TEST_CASE("RandomSource next_double in [0, 1)") {
    RandomSource rng(42);
    for (int i = 0; i < 10000; ++i) {
        double v = rng.next_double();
        CHECK(v >= 0.0);
        CHECK(v < 1.0);
    }
}

// ===========================================================================
// ImprovedNoise
// ===========================================================================

TEST_CASE("ImprovedNoise deterministic per seed") {
    ImprovedNoise a(42), b(42);
    CHECK(a.sample(1.5, 2.3, -0.5) == b.sample(1.5, 2.3, -0.5));
}

TEST_CASE("ImprovedNoise same seed produces same offsets and perm") {
    ImprovedNoise a(42), b(42);
    CHECK(a.get_offset_a() == b.get_offset_a());
    CHECK(a.get_offset_b() == b.get_offset_b());
    CHECK(a.get_offset_c() == b.get_offset_c());
    for (int i = 0; i < 256; ++i) {
        CHECK(a.perm_hash(i) == b.perm_hash(i));
    }
}

TEST_CASE("ImprovedNoise different seeds produce different values") {
    ImprovedNoise a(42), b(999);
    CHECK(a.sample(1.5, 2.3, -0.5) != b.sample(1.5, 2.3, -0.5));
}

TEST_CASE("ImprovedNoise sample is continuous") {
    ImprovedNoise noise(42);
    double dx = 0.001;
    double v0 = noise.sample(10.0, 20.0, 30.0);
    double v1 = noise.sample(10.0 + dx, 20.0, 30.0);
    double v2 = noise.sample(10.0, 20.0 + dx, 30.0);
    double v3 = noise.sample(10.0, 20.0, 30.0 + dx);
    CHECK(std::abs(v1 - v0) < 0.1);
    CHECK(std::abs(v2 - v0) < 0.1);
    CHECK(std::abs(v3 - v0) < 0.1);
}

TEST_CASE("ImprovedNoise sample bounded") {
    ImprovedNoise noise(42);
    double min_v = 1.0, max_v = -1.0;
    for (double x = -50.0; x <= 50.0; x += 3.7) {
        for (double y = -50.0; y <= 50.0; y += 3.7) {
            for (double z = -50.0; z <= 50.0; z += 3.7) {
                double v = noise.sample(x, y, z);
                if (v < min_v) min_v = v;
                if (v > max_v) max_v = v;
            }
        }
    }
    CHECK(min_v >= -1.0);
    CHECK(max_v <= 1.0);
}

// ===========================================================================
// OctaveNoise
// ===========================================================================

TEST_CASE("OctaveNoise deterministic per seed") {
    double amps[] = {1.0, 0.5, 0.25};
    OctaveNoise a(42, -3, amps, 3);
    OctaveNoise b(42, -3, amps, 3);
    CHECK(a.sample(1.5, 2.3, -0.5) == b.sample(1.5, 2.3, -0.5));
}

TEST_CASE("OctaveNoise persistence_0 matches vanilla formula") {
    // persistence_0 = 2^(len-1) / (2^len - 1)
    // Cross-checked against Cubiomes' persist_ini[] lookup table.
    SUBCASE("len=2: 2/3") {
        CHECK(std::ldexp(1.0, 1) / (std::ldexp(1.0, 2) - 1.0) == doctest::Approx(2.0 / 3.0));
    }
    SUBCASE("len=3: 4/7") {
        CHECK(std::ldexp(1.0, 2) / (std::ldexp(1.0, 3) - 1.0) == doctest::Approx(4.0 / 7.0));
    }
    SUBCASE("len=4: 8/15 (shift)") {
        CHECK(std::ldexp(1.0, 3) / (std::ldexp(1.0, 4) - 1.0) == doctest::Approx(8.0 / 15.0));
    }
    SUBCASE("len=7: 64/127") {
        CHECK(std::ldexp(1.0, 6) / (std::ldexp(1.0, 7) - 1.0) == doctest::Approx(64.0 / 127.0));
    }
    SUBCASE("len=9: 256/511 (continentalness)") {
        CHECK(std::ldexp(1.0, 8) / (std::ldexp(1.0, 9) - 1.0) == doctest::Approx(256.0 / 511.0));
    }
    SUBCASE("len=256: converges toward 0.5") {
        double p = std::ldexp(1.0, 255) / (std::ldexp(1.0, 256) - 1.0);
        CHECK(p == doctest::Approx(0.5).epsilon(1e-15));
    }
}

TEST_CASE("OctaveNoise skips zero-amplitude octaves") {
    double amps[] = {0.0, 0.0, 0.0};
    OctaveNoise noise(42, -3, amps, 3);
    CHECK(noise.sample(1.5, 2.3, -0.5) == doctest::Approx(0.0).epsilon(1e-15));
}

TEST_CASE("OctaveNoise different seeds produce different values") {
    double amps[] = {1.0, 0.5, 0.25};
    OctaveNoise a(42, -3, amps, 3);
    OctaveNoise b(999, -3, amps, 3);
    CHECK(a.sample(1.5, 2.3, -0.5) != b.sample(1.5, 2.3, -0.5));
}

TEST_CASE("OctaveNoise with continentalness amplitudes is deterministic") {
    double amps[] = {1.0, 1.0, 2.0, 2.0, 2.0, 1.0, 1.0, 1.0, 1.0};
    OctaveNoise a(42, -9, amps, 9);
    OctaveNoise b(42, -9, amps, 9);
    CHECK(a.sample(1.5, 2.3, -0.5) == b.sample(1.5, 2.3, -0.5));
    CHECK(a.sample(100.0, 200.0, 0.0) == b.sample(100.0, 200.0, 0.0));
}

// ===========================================================================
// DoublePerlinNoise
// ===========================================================================

TEST_CASE("DoublePerlinNoise is deterministic per seed") {
    double amps[] = {1.0, 1.0, 2.0, 2.0, 2.0, 1.0, 1.0, 1.0, 1.0};
    DoublePerlinNoise a(42, -9, amps, 9);
    DoublePerlinNoise b(42, -9, amps, 9);
    CHECK(a.sample(1.5, 2.3, -0.5) == b.sample(1.5, 2.3, -0.5));
    CHECK(a.sample(100.0, 200.0, 0.0) == b.sample(100.0, 200.0, 0.0));
}

TEST_CASE("DoublePerlinNoise different seeds produce different values") {
    double amps[] = {1.0, 1.0, 2.0, 2.0, 2.0, 1.0, 1.0, 1.0, 1.0};
    DoublePerlinNoise a(42, -9, amps, 9);
    DoublePerlinNoise b(999, -9, amps, 9);
    CHECK(a.sample(1.5, 2.3, -0.5) != b.sample(1.5, 2.3, -0.5));
}

TEST_CASE("DoublePerlinNoise amplitude trimming matches vanilla") {
    SUBCASE("Continentalness: {1,1,2,2,2,1,1,1,1} trimmed_len=9") {
        double amps[] = {1.0, 1.0, 2.0, 2.0, 2.0, 1.0, 1.0, 1.0, 1.0};
        double expected = (5.0 / 3.0) * 9.0 / 10.0;
        CHECK(DoublePerlinNoise::compute_amplitude(amps, 9) == doctest::Approx(expected));
    }

    SUBCASE("Erosion: {1,1,0,1,1} trimmed_len=5 (zero in middle not trimmed)") {
        double amps[] = {1.0, 1.0, 0.0, 1.0, 1.0};
        double expected = (5.0 / 3.0) * 5.0 / 6.0;
        CHECK(DoublePerlinNoise::compute_amplitude(amps, 5) == doctest::Approx(expected));
    }

    SUBCASE("Synthetic trailing zeros: {1,1,0,0} trimmed_len=2") {
        double amps[] = {1.0, 1.0, 0.0, 0.0};
        double expected = (5.0 / 3.0) * 2.0 / 3.0;
        CHECK(DoublePerlinNoise::compute_amplitude(amps, 4) == doctest::Approx(expected));
    }

    SUBCASE("All zeros: amplitude = 0") {
        double amps[] = {0.0, 0.0, 0.0};
        CHECK(DoublePerlinNoise::compute_amplitude(amps, 3) == doctest::Approx(0.0));
    }

    SUBCASE("Shift: {1,1,1,0} trimmed_len=3") {
        double amps[] = {1.0, 1.0, 1.0, 0.0};
        double expected = (5.0 / 3.0) * 3.0 / 4.0;
        CHECK(DoublePerlinNoise::compute_amplitude(amps, 4) == doctest::Approx(expected));
    }
}

TEST_CASE("DoublePerlinNoise non-zero sample") {
    double amps[] = {1.0, 1.0, 2.0, 2.0, 2.0, 1.0, 1.0, 1.0, 1.0};
    DoublePerlinNoise noise(42, -9, amps, 9);
    double v = noise.sample(1.5, 2.3, -0.5);
    CHECK(v != 0.0);
    CHECK(std::isfinite(v));
}


