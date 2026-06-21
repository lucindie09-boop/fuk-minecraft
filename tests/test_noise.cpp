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
