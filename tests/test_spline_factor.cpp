#include "doctest.h"
#include "worldgen/terrain_spline.hpp"

TEST_CASE("Factor spline produces expected range") {
    // Factor values should be in roughly [0.625, 6.3] (vanilla range)
    // Test various (cont, erosion, weirdness) combos

    float min_f = 999, max_f = -999;
    float tests[][3] = {
        {0.6f, 0.0f, 0.0f},
        {0.6f, -0.5f, 0.0f},
        {0.6f, 0.5f, 0.0f},
        {0.6f, -0.5f, 0.3f},
        {0.6f, -0.5f, -0.3f},
        {0.3f, 0.0f, 0.0f},
        {0.3f, -0.5f, 0.0f},
        {0.4f, -0.5f, 0.0f},
        {0.03f, 0.0f, 0.0f},
        {-0.15f, 0.0f, 0.0f},
        {-0.15f, -0.5f, 0.0f},
        {0.03f, -0.6f, -0.2f},
        {0.03f, -0.6f, 0.2f},
        {0.06f, -0.6f, 0.2f},
        {0.06f, 0.05f, 0.45f},
        {0.06f, 0.05f, 0.7f},
    };
    for (auto& t : tests) {
        float f = VoxelEngine::compute_factor(t[0], t[1], t[2]);
        if (f < min_f) min_f = f;
        if (f > max_f) max_f = f;
        CHECK(f >= 0.0f);
        CHECK(f <= 10.0f);
    }
    CHECK(min_f >= 0.0f);
    CHECK(max_f <= 10.0f);
    // At neutral weirdness on land, factor should be a few
    float land_f = VoxelEngine::compute_factor(0.4f, 0.0f, 0.0f);
    CHECK(land_f >= 2.0f);
    CHECK(land_f <= 8.0f);
}

TEST_CASE("Jaggedness spline produces expected range") {
    float tests[][3] = {
        {0.5f, 0.0f, 0.0f},
        {0.5f, -0.8f, 0.0f},
        {0.03f, -1.0f, 0.0f},
        {0.65f, -1.0f, 0.0f},
        {0.65f, -0.78f, 0.0f},
    };
    for (auto& t : tests) {
        float j = VoxelEngine::compute_jaggedness(t[0], t[1], t[2]);
        CHECK(j >= 0.0f);
        CHECK(j <= 5.0f);
    }
}

TEST_CASE("Offset spline unchanged") {
    float o = VoxelEngine::compute_terrain_offset(0.5f, 0.0f, 0.0f);
    CHECK(o >= -0.22f);
    CHECK(o <= 0.15f);
}
