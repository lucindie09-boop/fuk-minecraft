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
    static constexpr int MAX_FIX      = 256;

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

// Evaluate a spline node.  vals[] contains the four climate parameters:
//   vals[0] = continentalness, vals[1] = erosion,
//   vals[2] = weirdness-transformed (ridges), vals[3] = weirdness.
float get_spline(const TerrainSpline* sp, const float vals[4]);

// Build the complete terrain spline DAG into `ss`.  The returned pointer is
// the root node (a continentalness-axis spline).
TerrainSpline* init_terrain_spline(SplineStack& ss);

// Convenience: compute the terrain depth offset from raw climate parameters.
// c, e, w are in their native DoublePerlinNoise range (roughly [-1, 1]).
// Returns the spline offset value.
// The single-argument overload uses an internal static stack (for standalone use).
// Pass an explicit root to share a pre-built spline DAG (avoids duplication).
float compute_terrain_offset(float c, float e, float w);
float compute_terrain_offset(float c, float e, float w, const TerrainSpline* root);

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_TERRAIN_SPLINE_HPP
