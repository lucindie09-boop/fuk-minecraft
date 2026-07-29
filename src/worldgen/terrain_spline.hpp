#ifndef FUK_MINECRAFT_TERRAIN_SPLINE_HPP
#define FUK_MINECRAFT_TERRAIN_SPLINE_HPP
#include <cstring>

namespace VoxelEngine {

// ---------------------------------------------------------------------------
// Terrain spline system — ported from cubiomes' BiomeNoise spline construction.
//
// Vanilla computes a terrain-height "depth offset" from (continentalness,
// erosion, weirdness) via a recursive DAG of piecewise-Hermite spline nodes.
// The top-level axis is continentalness; each control point is either a fixed
// value or a sub-spline indexed by erosion, which in turn can contain
// sub-sub-splines indexed by a weirdness transform.
//
// Reference: Cubitect/cubiomes  initBiomeNoise(), getSpline()
// ---------------------------------------------------------------------------

enum SplineAxis {
    SP_CONTINENTALNESS = 0,
    SP_EROSION         = 1,
    SP_RIDGES          = 2,   // weirdness-transformed value
    SP_WEIRDNESS       = 3,
    SP_RIDGES_FOLDED   = 4,   // 1 - 2*|weirdness|  (vanilla's ridges_folded)
    SP_SPLINE_AXIS_COUNT = 5,
};

// A single node in the spline DAG.  When len == 1 this acts as a FixSpline
// (constant value stored in fixed_val).  When len > 1 it is a regular spline
// with control points along axis `typ`.
struct TerrainSpline {
    int typ = 0;
    int len = 0;
    float loc[12]  = {};
    float der[12]  = {};
    TerrainSpline* val[12] = {};
    float fixed_val = 0.0f;
};

// Bump-allocator for spline nodes.  All nodes live in fixed-size arrays;
// no heap allocation during construction or evaluation.
struct SplineStack {
    static constexpr int MAX_SPLINES  = 512;
    static constexpr int MAX_FIX      = 512;

    TerrainSpline pool[MAX_SPLINES];
    int pool_len = 0;

    TerrainSpline fix_pool[MAX_FIX];
    int fix_len = 0;

    void reset() { pool_len = 0; fix_len = 0; }

    TerrainSpline* alloc() { return &pool[pool_len++]; }

    TerrainSpline* alloc_fix(float v) {
        TerrainSpline* s = &fix_pool[fix_len++];
        s->len = 1;
        s->fixed_val = v;
        return s;
    }
};

// Evaluate a spline node.  vals[] contains the five climate parameters:
//   vals[0] = continentalness, vals[1] = erosion,
//   vals[2] = cubiomes ridges transform (-3*(||w|-0.6667|-0.3333)),
//   vals[3] = raw weirdness,
//   vals[4] = ridges_folded (1 - 2*|weirdness|).
float get_spline(const TerrainSpline* sp, const float vals[5]);

// Build the complete terrain spline DAG into `ss`.  The returned pointer is
// the root node (a continentalness-axis spline).
TerrainSpline* init_terrain_spline(SplineStack& ss);

// Build the factor spline DAG (from vanilla 1.20-pre2 factor.json, inner spline
// without the blend_alpha wrapper).  Used to amplify terrain amplitude based
// on continentalness/erosion/ridges/ridges_folded.
TerrainSpline* init_factor_spline(SplineStack& ss);

// Build the jaggedness spline DAG (from vanilla 26.1.1 jaggedness.json, inner
// spline without the blend_alpha wrapper).  Controls weirdness-based terrain
// jaggedness for mountain vs. flat biomes.
TerrainSpline* init_jaggedness_spline(SplineStack& ss);

// Convenience: compute the terrain depth offset from raw climate parameters.
// c, e, w are in their native DoublePerlinNoise range (roughly [-1, 1]).
// Returns the spline offset value.
// The single-argument overload uses an internal static stack (for standalone use).
// Pass an explicit root to share a pre-built spline DAG (avoids duplication).
float compute_terrain_offset(float c, float e, float w);
float compute_terrain_offset(float c, float e, float w, const TerrainSpline* root);

// Compute the terrain factor (amplitude multiplier) from raw climate params.
// Returns a dimensionless value (vanilla range ~0.625–6.3).
float compute_factor(float c, float e, float w);
float compute_factor(float c, float e, float w, const TerrainSpline* root);

// Compute the terrain jaggedness from raw climate params.
// Returns a dimensionless value (vanilla range 0–~0.63).
float compute_jaggedness(float c, float e, float w);
float compute_jaggedness(float c, float e, float w, const TerrainSpline* root);

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_TERRAIN_SPLINE_HPP
