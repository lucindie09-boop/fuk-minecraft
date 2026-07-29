#include "worldgen/terrain_spline.hpp"
#include <cmath>
#include <algorithm>

namespace VoxelEngine {

// ---------------------------------------------------------------------------
// Cubiomes lerp convention: lerp(t, a, b) = a + t * (b - a)
// ---------------------------------------------------------------------------
static float spline_lerp(float t, float a, float b) {
    return a + t * (b - a);
}

// ---------------------------------------------------------------------------
// addSplineVal — append a control point to a spline node
// ---------------------------------------------------------------------------
static void add_spline_val(TerrainSpline* sp, float loc,
                           TerrainSpline* val, float der) {
    sp->loc[sp->len]  = loc;
    sp->val[sp->len]  = val;
    sp->der[sp->len]  = der;
    sp->len++;
}

// ---------------------------------------------------------------------------
// getOffsetValue — compute the weirdness-to-ridge offset for a given
// continentalness value.  Matches cubiomes' getOffsetValue().
// ---------------------------------------------------------------------------
static float get_offset_value(float weirdness, float continentalness) {
    float f0 = 1.0f - (1.0f - continentalness) * 0.5f;
    float f1 = 0.5f * (1.0f - continentalness);
    float f2 = (weirdness + 1.17f) * 0.46082947f;
    float off = f2 * f0 - f1;
    if (weirdness < -0.7f)
        return off > -0.2222f ? off : -0.2222f;
    else
        return off > 0 ? off : 0;
}

// ---------------------------------------------------------------------------
// createSpline_38219 — weirdness (ridges) sub-spline for a given
// continentalness value.  Name matches the obfuscated Yarn/MCP mapping.
// ---------------------------------------------------------------------------
static TerrainSpline* create_spline_38219(SplineStack& ss, float f, int bl) {
    TerrainSpline* sp = ss.alloc();
    sp->typ = SP_RIDGES;

    float i = get_offset_value(-1.0f, f);
    float k = get_offset_value( 1.0f, f);
    float l = 1.0f - (1.0f - f) * 0.5f;
    float u = 0.5f * (1.0f - f);
    l = u / (0.46082947f * l) - 1.17f;

    if (-0.65f < l && l < 1.0f) {
        float p, q, r, s;
        u = get_offset_value(-0.65f, f);
        p = get_offset_value(-0.75f, f);
        q = (p - i) * 4.0f;
        r = get_offset_value(l, f);
        s = (k - r) / (1.0f - l);

        add_spline_val(sp, -1.0f,    ss.alloc_fix(i), q);
        add_spline_val(sp, -0.75f,   ss.alloc_fix(p), 0);
        add_spline_val(sp, -0.65f,   ss.alloc_fix(u), 0);
        add_spline_val(sp, l - 0.01f, ss.alloc_fix(r), 0);
        add_spline_val(sp, l,         ss.alloc_fix(r), s);
        add_spline_val(sp, 1.0f,      ss.alloc_fix(k), s);
    } else {
        u = (k - i) * 0.5f;
        if (bl) {
            add_spline_val(sp, -1.0f, ss.alloc_fix(i > 0.2f ? i : 0.2f), 0);
            add_spline_val(sp,  0.0f, ss.alloc_fix(spline_lerp(0.5f, i, k)), u);
        } else {
            add_spline_val(sp, -1.0f, ss.alloc_fix(i), u);
        }
        add_spline_val(sp, 1.0f, ss.alloc_fix(k), u);
    }
    return sp;
}

// ---------------------------------------------------------------------------
// createFlatOffsetSpline — erosion-indexed sub-spline with fixed control
// points along the ridges axis.
// ---------------------------------------------------------------------------
static TerrainSpline* create_flat_offset_spline(
    SplineStack& ss, float f, float g, float h, float i, float j, float k)
{
    TerrainSpline* sp = ss.alloc();
    sp->typ = SP_RIDGES;

    float l = 0.5f * (g - f);
    if (l < k) l = k;
    float m = 5.0f * (h - g);

    add_spline_val(sp, -1.0f, ss.alloc_fix(f), l);
    add_spline_val(sp, -0.4f, ss.alloc_fix(g), l < m ? l : m);
    add_spline_val(sp,  0.0f, ss.alloc_fix(h), m);
    add_spline_val(sp,  0.4f, ss.alloc_fix(i), 2.0f * (i - h));
    add_spline_val(sp,  1.0f, ss.alloc_fix(j), 0.7f * (j - i));

    return sp;
}

// ---------------------------------------------------------------------------
// createLandSpline — erosion-axis sub-spline for one continentalness band.
// `bl` controls whether the "jaggedness" spike is included (1 for higher
// continentalness bands, 0 for ocean-adjacent bands).
// ---------------------------------------------------------------------------
static TerrainSpline* create_land_spline(
    SplineStack& ss, float f, float g, float h, float i, float j, float k, int bl)
{
    TerrainSpline* sp1 = create_spline_38219(ss, spline_lerp(i, 0.6f, 1.5f), bl);
    TerrainSpline* sp2 = create_spline_38219(ss, spline_lerp(i, 0.6f, 1.0f), bl);
    TerrainSpline* sp3 = create_spline_38219(ss, i, bl);
    const float ih = 0.5f * i;
    TerrainSpline* sp4 = create_flat_offset_spline(ss, f - 0.15f, ih, ih, ih, i * 0.6f, 0.5f);
    TerrainSpline* sp5 = create_flat_offset_spline(ss, f, j * i, g * i, ih, i * 0.6f, 0.5f);
    TerrainSpline* sp6 = create_flat_offset_spline(ss, f, j, j, g, h, 0.5f);
    TerrainSpline* sp7 = create_flat_offset_spline(ss, f, j, j, g, h, 0.5f);

    TerrainSpline* sp8 = ss.alloc();
    sp8->typ = SP_RIDGES;
    add_spline_val(sp8, -1.0f, ss.alloc_fix(f), 0.0f);
    add_spline_val(sp8, -0.4f, sp6, 0.0f);
    add_spline_val(sp8,  0.0f, ss.alloc_fix(h + 0.07f), 0.0f);

    TerrainSpline* sp9 = create_flat_offset_spline(ss, -0.02f, k, k, g, h, 0.0f);

    TerrainSpline* sp = ss.alloc();
    sp->typ = SP_EROSION;
    add_spline_val(sp, -0.85f, sp1, 0.0f);
    add_spline_val(sp, -0.7f,  sp2, 0.0f);
    add_spline_val(sp, -0.4f,  sp3, 0.0f);
    add_spline_val(sp, -0.35f, sp4, 0.0f);
    add_spline_val(sp, -0.1f,  sp5, 0.0f);
    add_spline_val(sp,  0.2f,  sp6, 0.0f);
    if (bl) {
        add_spline_val(sp, 0.4f,  sp7, 0.0f);
        add_spline_val(sp, 0.45f, sp8, 0.0f);
        add_spline_val(sp, 0.55f, sp8, 0.0f);
        add_spline_val(sp, 0.58f, sp7, 0.0f);
    }
    add_spline_val(sp, 0.7f, sp9, 0.0f);
    return sp;
}

// ---------------------------------------------------------------------------
// init_terrain_spline — build the full spline DAG.  The root node is indexed
// by continentalness.
//
// This is the exact construction from cubiomes' initBiomeNoise().
// ---------------------------------------------------------------------------
TerrainSpline* init_terrain_spline(SplineStack& ss) {
    ss.reset();

    TerrainSpline* sp = ss.alloc();
    sp->typ = SP_CONTINENTALNESS;

    TerrainSpline* sp1 = create_land_spline(ss, -0.15f, 0.00f, 0.0f, 0.1f, 0.00f, -0.03f, 0);
    TerrainSpline* sp2 = create_land_spline(ss, -0.10f, 0.03f, 0.1f, 0.1f, 0.01f, -0.03f, 0);
    TerrainSpline* sp3 = create_land_spline(ss, -0.10f, 0.03f, 0.1f, 0.7f, 0.01f, -0.03f, 1);
    TerrainSpline* sp4 = create_land_spline(ss, -0.05f, 0.03f, 0.1f, 1.0f, 0.01f,  0.01f, 1);

    add_spline_val(sp, -1.10f, ss.alloc_fix( 0.044f), 0.0f);
    add_spline_val(sp, -1.02f, ss.alloc_fix(-0.2222f), 0.0f);
    add_spline_val(sp, -0.51f, ss.alloc_fix(-0.2222f), 0.0f);
    add_spline_val(sp, -0.44f, ss.alloc_fix(-0.12f),   0.0f);
    add_spline_val(sp, -0.18f, ss.alloc_fix(-0.12f),   0.0f);
    add_spline_val(sp, -0.16f, sp1, 0.0f);
    add_spline_val(sp, -0.15f, sp1, 0.0f);
    add_spline_val(sp, -0.10f, sp2, 0.0f);
    add_spline_val(sp,  0.25f, sp3, 0.0f);
    add_spline_val(sp,  1.00f, sp4, 0.0f);

    return sp;
}

// ---------------------------------------------------------------------------
// get_spline — recursive Hermite interpolation evaluator.
// Matches cubiomes' getSpline().
// ---------------------------------------------------------------------------
float get_spline(const TerrainSpline* sp, const float vals[4]) {
    if (!sp || sp->len <= 0 || sp->len >= 12)
        return 0.0f;

    // FixSpline: single constant value
    if (sp->len == 1)
        return sp->fixed_val;

    float f = vals[sp->typ];
    int i;

    for (i = 0; i < sp->len; i++)
        if (sp->loc[i] >= f)
            break;

    if (i == 0 || i == sp->len) {
        if (i) i--;
        float v = get_spline(sp->val[i], vals);
        return v + sp->der[i] * (f - sp->loc[i]);
    }

    const TerrainSpline* sp1 = sp->val[i - 1];
    const TerrainSpline* sp2 = sp->val[i];
    float g = sp->loc[i - 1];
    float h = sp->loc[i];
    float t = (f - g) / (h - g);
    float dl = sp->der[i - 1];
    float dr = sp->der[i];
    float n = get_spline(sp1, vals);
    float o = get_spline(sp2, vals);
    float p = dl * (h - g) - (o - n);
    float q = -dr * (h - g) + (o - n);
    return spline_lerp(t, n, o) + t * (1.0f - t) * spline_lerp(t, p, q);
}

// ---------------------------------------------------------------------------
// compute_terrain_offset — public convenience wrapper.
// Samples the spline from raw climate values in their native range.
// ---------------------------------------------------------------------------
float compute_terrain_offset(float c, float e, float w) {
    static SplineStack stack;
    static TerrainSpline* root = nullptr;
    if (!root)
        root = init_terrain_spline(stack);
    return compute_terrain_offset(c, e, w, root);
}

float compute_terrain_offset(float c, float e, float w, const TerrainSpline* root) {
    float wr = -3.0f * (std::fabs(std::fabs(w) - 0.6666667f) - 0.33333334f);
    float vals[4] = { c, e, wr, w };
    float result = get_spline(root, vals) + 0.015f;
    return std::clamp(result, -0.22f, 0.15f);
}

} // namespace VoxelEngine
