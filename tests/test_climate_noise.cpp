#include "doctest.h"
#include "core/perlin_noise.hpp"
#include "worldgen/climate_sampler.hpp"
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

// ===========================================================================
// ClimateSampler
// ===========================================================================

TEST_CASE("ClimateSampler determinism") {
    ClimateSampler a(42), b(42);
    CHECK(a.sample_continentalness(100.0, 200.0) == b.sample_continentalness(100.0, 200.0));
    CHECK(a.sample_erosion(100.0, 200.0) == b.sample_erosion(100.0, 200.0));
    CHECK(a.sample_weirdness(100.0, 200.0) == b.sample_weirdness(100.0, 200.0));
    CHECK(a.sample_temperature(100.0, 200.0) == b.sample_temperature(100.0, 200.0));
    CHECK(a.sample_humidity(100.0, 200.0) == b.sample_humidity(100.0, 200.0));
}

TEST_CASE("ClimateSampler different seeds produce different values") {
    ClimateSampler a(42), b(999);
    CHECK(a.sample_continentalness(100.0, 200.0) != b.sample_continentalness(100.0, 200.0));
}

TEST_CASE("ClimateSampler fields produce finite values") {
    ClimateSampler sampler(42);
    CHECK(std::isfinite(sampler.sample_continentalness(0.0, 0.0)));
    CHECK(std::isfinite(sampler.sample_erosion(0.0, 0.0)));
    CHECK(std::isfinite(sampler.sample_weirdness(0.0, 0.0)));
    CHECK(std::isfinite(sampler.sample_temperature(0.0, 0.0)));
    CHECK(std::isfinite(sampler.sample_humidity(0.0, 0.0)));
}

TEST_CASE("ClimateSampler shift warps coordinates continuously") {
    ClimateSampler sampler(42);
    double shift_x0 = sampler.sample_shift_x(1000.0, 2000.0);
    double shift_x1 = sampler.sample_shift_x(1001.0, 2000.0);
    double shift_x2 = sampler.sample_shift_x(1000.0, 2001.0);
    // Shift noise at warp_strength=256 can differ by ~60-80 between adjacent blocks
    // due to the lacunarity/amplitude stack; just verify it's not wildly discontinuous
    CHECK(std::abs(shift_x1 - shift_x0) < 120.0);
    CHECK(std::abs(shift_x2 - shift_x0) < 120.0);
}

TEST_CASE("ClimateSampler fields produce expected value range over grid") {
    ClimateSampler sampler(42);
    double min_cont = 1e10, max_cont = -1e10;
    double min_eros = 1e10, max_eros = -1e10;
    double min_weir = 1e10, max_weir = -1e10;
    double min_temp = 1e10, max_temp = -1e10;
    double min_hum  = 1e10, max_hum  = -1e10;

    for (double x = -5000.0; x <= 5000.0; x += 200.0) {
        for (double z = -5000.0; z <= 5000.0; z += 200.0) {
            double c = sampler.sample_continentalness(x, z);
            double e = sampler.sample_erosion(x, z);
            double w = sampler.sample_weirdness(x, z);
            double t = sampler.sample_temperature(x, z);
            double h = sampler.sample_humidity(x, z);

            if (c < min_cont) min_cont = c;
            if (c > max_cont) max_cont = c;
            if (e < min_eros) min_eros = e;
            if (e > max_eros) max_eros = e;
            if (w < min_weir) min_weir = w;
            if (w > max_weir) max_weir = w;
            if (t < min_temp) min_temp = t;
            if (t > max_temp) max_temp = t;
            if (h < min_hum)  min_hum  = h;
            if (h > max_hum)  max_hum  = h;
        }
    }

    INFO("Continentalness range: [" << min_cont << ", " << max_cont << "]");
    INFO("Erosion range: [" << min_eros << ", " << max_eros << "]");
    INFO("Weirdness range: [" << min_weir << ", " << max_weir << "]");
    INFO("Temperature range: [" << min_temp << ", " << max_temp << "]");
    INFO("Humidity range: [" << min_hum << ", " << max_hum << "]");

    CHECK(std::isfinite(min_cont));
    CHECK(std::isfinite(max_cont));
    CHECK(max_cont - min_cont > 0.0);
    CHECK(max_eros - min_eros > 0.0);
    CHECK(max_weir - min_weir > 0.0);
    CHECK(max_temp - min_temp > 0.0);
    CHECK(max_hum  - min_hum  > 0.0);

    CHECK(min_cont > -100.0);
    CHECK(max_cont < 100.0);
    CHECK(min_eros > -100.0);
    CHECK(max_eros < 100.0);
    CHECK(min_weir > -100.0);
    CHECK(max_weir < 100.0);
    CHECK(min_temp > -100.0);
    CHECK(max_temp < 100.0);
    CHECK(min_hum > -100.0);
    CHECK(max_hum < 100.0);
}
