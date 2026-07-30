#include "worldgen/terrain_spline.hpp"
#include <cmath>
#include <algorithm>
#include <mutex>

namespace VoxelEngine {

static float spline_lerp(float t, float a, float b) {
    return a + t * (b - a);
}

static void add_spline_val(TerrainSpline* sp, float loc,
                            TerrainSpline* val, float der) {
    sp->loc[sp->len]  = loc;
    sp->val[sp->len]  = val;
    sp->der[sp->len]  = der;
    sp->len++;
}

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

// ===========================================================================
// Factor and Jaggedness spline builders
// (ported from vanilla 1.20-pre2 factor.json and 26.1.1 jaggedness.json)
//
// These splines use SP_WEIRDNESS (vals[3] = raw weirdness) for the "ridges"
// coordinate and SP_RIDGES_FOLDED (vals[4] = 1 - 2*|w|) for folded ridges.
// ===========================================================================

// Helper: ridges2 — 2-point spline on SP_WEIRDNESS (raw weirdness = 1.20 "ridges")
static TerrainSpline* r2(SplineStack& ss, float l1, float v1, float l2, float v2) {
    TerrainSpline* sp = ss.alloc();
    sp->typ = SP_WEIRDNESS;
    add_spline_val(sp, l1, ss.alloc_fix(v1), 0);
    add_spline_val(sp, l2, ss.alloc_fix(v2), 0);
    return sp;
}

// ---- Factor spline ----

// Build RF sub-spline used at factor erosion 0.45/0.55 for cont=-0.15,-0.10,0.03:
//   RF(-0.9: band_max, -0.69: ridges2(0.0->band_max, 0.1->0.625))
static TerrainSpline* factor_rf_v1(SplineStack& ss, float band_max) {
    TerrainSpline* inner = r2(ss, 0.0f, band_max, 0.1f, 0.625f);
    TerrainSpline* sp = ss.alloc();
    sp->typ = SP_RIDGES_FOLDED;
    add_spline_val(sp, -0.9f, ss.alloc_fix(band_max), 0);
    add_spline_val(sp, -0.69f, inner, 0);
    return sp;
}

// Erosion sub-spline for cont=-0.15, -0.10, 0.03 (10 erosion CPs)
static TerrainSpline* factor_erosion_10(SplineStack& ss, float band_max) {
    TerrainSpline* r_mtn = r2(ss, -0.2f, 6.3f, 0.2f, band_max);
    TerrainSpline* r_clf = r2(ss, -0.05f, 6.3f, 0.05f, 2.67f);
    TerrainSpline* r_rev = r2(ss, -0.05f, 2.67f, 0.05f, 6.3f);
    TerrainSpline* rf_outer = factor_rf_v1(ss, band_max);

    TerrainSpline* sp = ss.alloc();
    sp->typ = SP_EROSION;
    add_spline_val(sp, -0.6f,  r_mtn, 0);
    add_spline_val(sp, -0.5f,  r_clf, 0);
    add_spline_val(sp, -0.35f, r_mtn, 0);
    add_spline_val(sp, -0.25f, r_mtn, 0);
    add_spline_val(sp, -0.1f,  r_rev, 0);
    add_spline_val(sp, 0.03f,  r_mtn, 0);
    add_spline_val(sp, 0.35f,  ss.alloc_fix(band_max), 0);
    add_spline_val(sp, 0.45f,  rf_outer, 0);
    add_spline_val(sp, 0.55f,  rf_outer, 0);
    add_spline_val(sp, 0.62f,  ss.alloc_fix(band_max), 0);
    return sp;
}

// Erosion sub-spline for cont=0.06 (11 erosion CPs, different RF patterns)
static TerrainSpline* factor_erosion_11(SplineStack& ss, float band_max) {
    TerrainSpline* r_mtn = r2(ss, -0.2f, 6.3f, 0.2f, band_max);
    TerrainSpline* r_clf = r2(ss, -0.05f, 6.3f, 0.05f, 2.67f);
    TerrainSpline* r_rev = r2(ss, -0.05f, 2.67f, 0.05f, 6.3f);

    // RF at erosion 0.05/0.4: RF(0.45->r_mtn, 0.7->1.56)
    TerrainSpline* rf0 = ss.alloc();
    rf0->typ = SP_RIDGES_FOLDED;
    add_spline_val(rf0, 0.45f, r_mtn, 0);
    add_spline_val(rf0, 0.7f,  ss.alloc_fix(1.56f), 0);

    // RF at erosion 0.45/0.55: RF(-0.7->r_mtn, -0.15->1.37)
    TerrainSpline* rf1 = ss.alloc();
    rf1->typ = SP_RIDGES_FOLDED;
    add_spline_val(rf1, -0.7f,  r_mtn, 0);
    add_spline_val(rf1, -0.15f, ss.alloc_fix(1.37f), 0);

    TerrainSpline* sp = ss.alloc();
    sp->typ = SP_EROSION;
    add_spline_val(sp, -0.6f,  r_mtn, 0);
    add_spline_val(sp, -0.5f,  r_clf, 0);
    add_spline_val(sp, -0.35f, r_mtn, 0);
    add_spline_val(sp, -0.25f, r_mtn, 0);
    add_spline_val(sp, -0.1f,  r_rev, 0);
    add_spline_val(sp, 0.03f,  r_mtn, 0);
    add_spline_val(sp, 0.05f,  rf0, 0);
    add_spline_val(sp, 0.4f,   rf0, 0);
    add_spline_val(sp, 0.45f,  rf1, 0);
    add_spline_val(sp, 0.55f,  rf1, 0);
    add_spline_val(sp, 0.58f,  ss.alloc_fix(band_max), 0);
    return sp;
}

TerrainSpline* init_factor_spline(SplineStack& ss) {
    ss.reset();
    TerrainSpline* sp = ss.alloc();
    sp->typ = SP_CONTINENTALNESS;

    add_spline_val(sp, -0.19f, ss.alloc_fix(3.95f), 0);
    add_spline_val(sp, -0.15f, factor_erosion_10(ss, 6.25f), 0);
    add_spline_val(sp, -0.10f, factor_erosion_10(ss, 5.47f), 0);
    add_spline_val(sp, 0.03f,  factor_erosion_10(ss, 5.08f), 0);
    add_spline_val(sp, 0.06f,  factor_erosion_11(ss, 4.69f), 0);
    return sp;
}

// ---- Jaggedness spline ----

// 3-point ridges_folded sub-spline for jaggedness.
// Structure: RF(0.2: 0, 0.45: [fix_or_ridges], 1.0: ridges(-0.01->v1, 0.01->v2))
static TerrainSpline* j_rf(SplineStack& ss, float v1, float v2, bool sub_at_45) {
    TerrainSpline* inner = r2(ss, -0.01f, v1, 0.01f, v2);
    TerrainSpline* sp = ss.alloc();
    sp->typ = SP_RIDGES_FOLDED;
    add_spline_val(sp, 0.19999999f, ss.alloc_fix(0.0f), 0);
    add_spline_val(sp, 0.44999996f, sub_at_45 ? (TerrainSpline*)inner : ss.alloc_fix(0.0f), 0);
    add_spline_val(sp, 1.0f, inner, 0);
    return sp;
}

// Erosion sub-spline for jaggedness (4 erosion CPs)
static TerrainSpline* jaggedness_erosion_03(SplineStack& ss) {
    TerrainSpline* rf_a = j_rf(ss, 0.63f, 0.3f, false);   // erosion -1.0
    TerrainSpline* rf_b = j_rf(ss, 0.315f, 0.15f, false);  // erosion -0.78, -0.5775

    TerrainSpline* sp = ss.alloc();
    sp->typ = SP_EROSION;
    add_spline_val(sp, -1.0f,    rf_a, 0);
    add_spline_val(sp, -0.78f,   rf_b, 0);
    add_spline_val(sp, -0.5775f, rf_b, 0);
    add_spline_val(sp, -0.375f,  ss.alloc_fix(0.0f), 0);
    return sp;
}

static TerrainSpline* jaggedness_erosion_65(SplineStack& ss) {
    TerrainSpline* rf_a = j_rf(ss, 0.63f, 0.3f, true);   // erosion -1.0
    TerrainSpline* rf_b = j_rf(ss, 0.63f, 0.3f, false);  // erosion -0.78, -0.5775

    TerrainSpline* sp = ss.alloc();
    sp->typ = SP_EROSION;
    add_spline_val(sp, -1.0f,    rf_a, 0);
    add_spline_val(sp, -0.78f,   rf_b, 0);
    add_spline_val(sp, -0.5775f, rf_b, 0);
    add_spline_val(sp, -0.375f,  ss.alloc_fix(0.0f), 0);
    return sp;
}

TerrainSpline* init_jaggedness_spline(SplineStack& ss) {
    ss.reset();
    TerrainSpline* sp = ss.alloc();
    sp->typ = SP_CONTINENTALNESS;

    add_spline_val(sp, -0.11f, ss.alloc_fix(0.0f), 0);
    add_spline_val(sp, 0.03f,  jaggedness_erosion_03(ss), 0);
    add_spline_val(sp, 0.65f,  jaggedness_erosion_65(ss), 0);
    return sp;
}

// ===========================================================================
// get_spline — recursive Hermite interpolation evaluator.
// Matches cubiomes' getSpline().
// ===========================================================================
float get_spline(const TerrainSpline* sp, const float vals[5]) {
    if (!sp || sp->len <= 0 || sp->len >= 12)
        return 0.0f;

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

float compute_terrain_offset(float c, float e, float w) {
    static SplineStack stack;
    static TerrainSpline* root = nullptr;
    static std::once_flag init_flag;
    std::call_once(init_flag, [&]() { root = init_terrain_spline(stack); });
    return compute_terrain_offset(c, e, w, root);
}

float compute_terrain_offset(float c, float e, float w, const TerrainSpline* root) {
    float wr = -3.0f * (std::fabs(std::fabs(w) - 0.6666667f) - 0.33333334f);
    float rf = 1.0f - 2.0f * std::fabs(w);
    float vals[5] = { c, e, wr, w, rf };
    float result = get_spline(root, vals) + 0.015f;
    return std::clamp(result, -0.22f, 0.15f);
}

float compute_factor(float c, float e, float w) {
    static SplineStack stack;
    static TerrainSpline* root = nullptr;
    static std::once_flag init_flag;
    std::call_once(init_flag, [&]() { root = init_factor_spline(stack); });
    return compute_factor(c, e, w, root);
}

float compute_factor(float c, float e, float w, const TerrainSpline* root) {
    float wr = -3.0f * (std::fabs(std::fabs(w) - 0.6666667f) - 0.33333334f);
    float rf = 1.0f - 2.0f * std::fabs(w);
    float vals[5] = { c, e, wr, w, rf };
    return get_spline(root, vals);
}

float compute_jaggedness(float c, float e, float w) {
    static SplineStack stack;
    static TerrainSpline* root = nullptr;
    static std::once_flag init_flag;
    std::call_once(init_flag, [&]() { root = init_jaggedness_spline(stack); });
    return compute_jaggedness(c, e, w, root);
}

float compute_jaggedness(float c, float e, float w, const TerrainSpline* root) {
    float wr = -3.0f * (std::fabs(std::fabs(w) - 0.6666667f) - 0.33333334f);
    float rf = 1.0f - 2.0f * std::fabs(w);
    float vals[5] = { c, e, wr, w, rf };
    return get_spline(root, vals);
}

} // namespace VoxelEngine
