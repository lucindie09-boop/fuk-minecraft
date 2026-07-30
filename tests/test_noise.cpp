#include "doctest.h"
#include "core/noise.hpp"
#include <cmath>

using namespace VoxelEngine;

TEST_CASE("deterministic 2D same seed") {
    FastNoise a(42), b(42);
    float va = a.noise_2d(1.5f, 2.3f);
    float vb = b.noise_2d(1.5f, 2.3f);
    CHECK(va == vb);
}

TEST_CASE("different seed different output") {
    FastNoise a(42), b(999);
    float va = a.noise_2d(1.5f, 2.3f);
    float vb = b.noise_2d(1.5f, 2.3f);
    CHECK(va != vb);
}

TEST_CASE("deterministic 3D same seed") {
    FastNoise a(42), b(42);
    float va = a.noise_3d(1.5f, 2.3f, -0.5f);
    float vb = b.noise_3d(1.5f, 2.3f, -0.5f);
    CHECK(va == vb);
}

TEST_CASE("2D noise range within [-1, 1]") {
    FastNoise fn(0);
    float min_v = 1.0f, max_v = -1.0f;
    for (float x = -100.0f; x <= 100.0f; x += 3.7f) {
        for (float y = -100.0f; y <= 100.0f; y += 3.7f) {
            float v = fn.noise_2d(x, y);
            if (v < min_v) min_v = v;
            if (v > max_v) max_v = v;
        }
    }
    CHECK(min_v >= -1.0f);
    CHECK(max_v <= 1.0f);
}

TEST_CASE("3D noise range within [-1, 1]") {
    FastNoise fn(0);
    float min_v = 1.0f, max_v = -1.0f;
    for (float x = -50.0f; x <= 50.0f; x += 5.0f) {
        for (float y = -50.0f; y <= 50.0f; y += 5.0f) {
            for (float z = -50.0f; z <= 50.0f; z += 5.0f) {
                float v = fn.noise_3d(x, y, z);
                if (v < min_v) min_v = v;
                if (v > max_v) max_v = v;
            }
        }
    }
    CHECK(min_v >= -1.0f);
    CHECK(max_v <= 1.0f);
}

TEST_CASE("fbm range within [-1, 1]") {
    FastNoise fn(0);
    float min_v = 1.0f, max_v = -1.0f;
    for (float x = -50.0f; x <= 50.0f; x += 4.0f) {
        for (float y = -50.0f; y <= 50.0f; y += 4.0f) {
            float v = fn.fbm(x, y, 4, 0.5f, 0.01f);
            if (v < min_v) min_v = v;
            if (v > max_v) max_v = v;
        }
    }
    CHECK(min_v >= -1.0f);
    CHECK(max_v <= 1.0f);
}

// Quick measurement of 3-octave FBM distribution used by the ruggedness gate
// in sample_land_shape. Results printed via MESSAGE (visible with -s flag).
TEST_CASE("ruggedness-style 3-octave fbm distribution") {
    FastNoise fn(12345);
    float min_v = 1.0f, max_v = -1.0f;
    double sum = 0.0, sum_sq = 0.0;
    int count = 0;
    for (float x = -5000.0f; x <= 5000.0f; x += 20.0f) {
        for (float z = -5000.0f; z <= 5000.0f; z += 20.0f) {
            float v = fn.fbm(x, z, 3, 0.50f, 0.0015f);
            if (v < min_v) min_v = v;
            if (v > max_v) max_v = v;
            sum += v;
            sum_sq += v * v;
            count++;
        }
    }
    double mean = sum / count;
    double variance = (sum_sq / count) - (mean * mean);
    double stddev = std::sqrt(variance > 0.0 ? variance : 0.0);
    double pct_below_04 = 100.0 * (std::erf((-0.4 - mean) / (stddev * 1.41421356)) + 1.0) / 2.0;
    double pct_above_04 = 100.0 - 100.0 * (std::erf((0.4 - mean) / (stddev * 1.41421356)) + 1.0) / 2.0;
    MESSAGE("3-oct fbm: min=" << min_v << " max=" << max_v
            << " mean=" << mean << " stddev=" << stddev
            << " pct_below_-0.4=" << pct_below_04
            << " pct_above_0.4=" << pct_above_04);
    CHECK(max_v - min_v > 0.6f);
}
