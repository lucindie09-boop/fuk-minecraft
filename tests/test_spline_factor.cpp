#include "doctest.h"
#include "worldgen/terrain_spline.hpp"
#include <cmath>
#include <algorithm>

// ===========================================================================
// Helper: compute ridges and ridges_folded from raw weirdness
// ===========================================================================
static float ridges(float w) {
    return -3.0f * (std::fabs(std::fabs(w) - 0.6666667f) - 0.33333334f);
}
static float ridges_folded(float w) {
    return 1.0f - 2.0f * std::fabs(w);
}

// ===========================================================================
// Reference-value pinning: these tests capture the exact output of the spline
// DAG at well-known (continentalness, erosion, weirdness) triples. If a
// future edit changes the spline constants or topology, these tests will fail
// — forcing a conscious decision about whether the change was intentional.
// ===========================================================================

static float pin_offset(float c, float e, float w) {
    float wr = ridges(w);
    float rf = ridges_folded(w);
    float vals[5] = {c, e, wr, w, rf};
    // We build a fresh stack to test the 4-arg path (no static state)
    VoxelEngine::SplineStack stack;
    VoxelEngine::TerrainSpline* root = VoxelEngine::init_terrain_spline(stack);
    float result = VoxelEngine::get_spline(root, vals) + 0.015f;
    return std::clamp(result, -0.22f, 0.15f);
}

static float pin_factor(float c, float e, float w) {
    float wr = ridges(w);
    float rf = ridges_folded(w);
    float vals[5] = {c, e, wr, w, rf};
    VoxelEngine::SplineStack stack;
    VoxelEngine::TerrainSpline* root = VoxelEngine::init_factor_spline(stack);
    return VoxelEngine::get_spline(root, vals);
}

static float pin_jaggedness(float c, float e, float w) {
    float wr = ridges(w);
    float rf = ridges_folded(w);
    float vals[5] = {c, e, wr, w, rf};
    VoxelEngine::SplineStack stack;
    VoxelEngine::TerrainSpline* root = VoxelEngine::init_jaggedness_spline(stack);
    return VoxelEngine::get_spline(root, vals);
}

struct SplineTriple { float c, e, w; };

TEST_CASE("Reference: terrain_offset pinning") {
    SplineTriple points[] = {
        { 0.50f,  0.00f,  0.00f},
        { 0.80f,  0.50f,  0.00f},
        { 0.30f, -0.50f,  0.30f},
        { 0.03f, -0.60f, -0.20f},
        {-0.15f,  0.00f,  0.00f},
        { 0.65f, -0.78f,  0.00f},
        { 0.50f, -0.80f,  0.50f},
        { 0.50f, -0.80f, -0.50f},
    };
    float expected[] = {
        -0.072037f, -0.0437704f, 0.15f, 0.15f,
        -0.135f, 0.15f, 0.15f, 0.15f
    };
    int i = 0;
    for (auto& p : points) {
        float v = pin_offset(p.c, p.e, p.w);
        INFO("pin terrain_offset(" << p.c << ", " << p.e << ", " << p.w
             << ") = " << v << " (expected " << expected[i] << ")");
        CHECK(v == doctest::Approx(expected[i]).epsilon(0.0005f));
        ++i;
    }
}

TEST_CASE("Reference: factor pinning") {
    SplineTriple points[] = {
        { 0.50f,  0.00f,  0.00f},
        { 0.80f,  0.50f,  0.00f},
        { 0.30f, -0.50f,  0.30f},
        { 0.03f, -0.60f, -0.20f},
        {-0.15f,  0.00f,  0.00f},
        { 0.65f, -0.78f,  0.00f},
        { 0.50f, -0.80f,  0.50f},
        { 0.50f, -0.80f, -0.50f},
    };
    float expected[] = {
        5.35846f, 1.37f, 2.67f, 6.3f,
        6.03302f, 5.495f, 4.69f, 6.3f
    };
    int i = 0;
    for (auto& p : points) {
        float v = pin_factor(p.c, p.e, p.w);
        INFO("pin factor(" << p.c << ", " << p.e << ", " << p.w
             << ") = " << v << " (expected " << expected[i] << ")");
        CHECK(v == doctest::Approx(expected[i]).epsilon(0.001f));
        ++i;
    }
}

TEST_CASE("Reference: jaggedness pinning") {
    SplineTriple points[] = {
        { 0.50f,  0.00f,  0.00f},
        { 0.80f,  0.50f,  0.00f},
        { 0.30f, -0.50f,  0.30f},
        { 0.03f, -0.60f, -0.20f},
        {-0.15f,  0.00f,  0.00f},
        { 0.65f, -0.78f,  0.00f},
        { 0.50f, -0.80f,  0.50f},
        { 0.50f, -0.80f, -0.50f},
    };
    float expected[] = {
        0.0f, 0.0f, 0.0f, 0.0575094f,
        0.0f, 0.465f, 0.0f, 0.0f
    };
    int i = 0;
    for (auto& p : points) {
        float v = pin_jaggedness(p.c, p.e, p.w);
        INFO("pin jaggedness(" << p.c << ", " << p.e << ", " << p.w
             << ") = " << v << " (expected " << expected[i] << ")");
        CHECK(v == doctest::Approx(expected[i]).epsilon(0.001f));
        ++i;
    }
}

// ===========================================================================
// Existing range tests
// ===========================================================================

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
