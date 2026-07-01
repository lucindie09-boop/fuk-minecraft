#ifndef FUK_MINECRAFT_MC_SPLINE_DATA_HPP
#define FUK_MINECRAFT_MC_SPLINE_DATA_HPP
#include "core/spline.hpp"

namespace VoxelEngine {
namespace mc_spline {

inline SplinePtr offset_s5_s0 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.38940096f, /*value*/-0.08880186f},
                {/*loc*/1.0f, /*deriv*/0.38940096f, /*value*/0.69000006f}
            }
            });

inline SplinePtr offset_s5_s1 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.37788022f, /*value*/-0.115760356f},
                {/*loc*/1.0f, /*deriv*/0.37788022f, /*value*/0.6400001f}
            }
            });

inline SplinePtr offset_s5_s2 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.0f, /*value*/-0.2222f},
                {/*loc*/-0.75f, /*deriv*/0.0f, /*value*/-0.2222f},
                {/*loc*/-0.65f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/0.5954547f, /*deriv*/0.0f, /*value*/2.9802322e-08f},
                {/*loc*/0.6054547f, /*deriv*/0.2534563f, /*value*/2.9802322e-08f},
                {/*loc*/1.0f, /*deriv*/0.2534563f, /*value*/0.100000024f}
            }
            });

inline SplinePtr offset_s5_s3 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.3f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.05f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.05f},
                {/*loc*/0.4f, /*deriv*/0.0f, /*value*/0.05f},
                {/*loc*/1.0f, /*deriv*/0.007000001f, /*value*/0.060000002f}
            }
            });

inline SplinePtr offset_s5_s4 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.15f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/0.4f, /*deriv*/0.1f, /*value*/0.05f},
                {/*loc*/1.0f, /*deriv*/0.007000001f, /*value*/0.060000002f}
            }
            });

inline SplinePtr offset_s5_s5 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.15f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/0.4f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/1.0f, /*deriv*/0.0f, /*value*/0.0f}
            }
            });

inline SplinePtr offset_s5_s6 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.0f, /*value*/-0.02f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/-0.03f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/-0.03f},
                {/*loc*/0.4f, /*deriv*/0.06f, /*value*/0.0f},
                {/*loc*/1.0f, /*deriv*/0.0f, /*value*/0.0f}
            }
            });

inline SplinePtr offset_s5 = std::make_shared<Spline>(Spline{
        Coord::Erosion,
        {
            {/*loc*/-0.85f, /*deriv*/0.0f, /*value*/offset_s5_s0},
            {/*loc*/-0.7f, /*deriv*/0.0f, /*value*/offset_s5_s1},
            {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/offset_s5_s2},
            {/*loc*/-0.35f, /*deriv*/0.0f, /*value*/offset_s5_s3},
            {/*loc*/-0.1f, /*deriv*/0.0f, /*value*/offset_s5_s4},
            {/*loc*/0.2f, /*deriv*/0.0f, /*value*/offset_s5_s5},
            {/*loc*/0.7f, /*deriv*/0.0f, /*value*/offset_s5_s6}
        }
        });

inline SplinePtr offset_s6_s0 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.38940096f, /*value*/-0.08880186f},
                {/*loc*/1.0f, /*deriv*/0.38940096f, /*value*/0.69000006f}
            }
            });

inline SplinePtr offset_s6_s1 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.37788022f, /*value*/-0.115760356f},
                {/*loc*/1.0f, /*deriv*/0.37788022f, /*value*/0.6400001f}
            }
            });

inline SplinePtr offset_s6_s2 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.0f, /*value*/-0.2222f},
                {/*loc*/-0.75f, /*deriv*/0.0f, /*value*/-0.2222f},
                {/*loc*/-0.65f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/0.5954547f, /*deriv*/0.0f, /*value*/2.9802322e-08f},
                {/*loc*/0.6054547f, /*deriv*/0.2534563f, /*value*/2.9802322e-08f},
                {/*loc*/1.0f, /*deriv*/0.2534563f, /*value*/0.100000024f}
            }
            });

inline SplinePtr offset_s6_s3 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.3f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.05f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.05f},
                {/*loc*/0.4f, /*deriv*/0.0f, /*value*/0.05f},
                {/*loc*/1.0f, /*deriv*/0.007000001f, /*value*/0.060000002f}
            }
            });

inline SplinePtr offset_s6_s4 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.15f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/0.4f, /*deriv*/0.1f, /*value*/0.05f},
                {/*loc*/1.0f, /*deriv*/0.007000001f, /*value*/0.060000002f}
            }
            });

inline SplinePtr offset_s6_s5 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.15f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/0.4f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/1.0f, /*deriv*/0.0f, /*value*/0.0f}
            }
            });

inline SplinePtr offset_s6_s6 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.0f, /*value*/-0.02f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/-0.03f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/-0.03f},
                {/*loc*/0.4f, /*deriv*/0.06f, /*value*/0.0f},
                {/*loc*/1.0f, /*deriv*/0.0f, /*value*/0.0f}
            }
            });

inline SplinePtr offset_s6 = std::make_shared<Spline>(Spline{
        Coord::Erosion,
        {
            {/*loc*/-0.85f, /*deriv*/0.0f, /*value*/offset_s6_s0},
            {/*loc*/-0.7f, /*deriv*/0.0f, /*value*/offset_s6_s1},
            {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/offset_s6_s2},
            {/*loc*/-0.35f, /*deriv*/0.0f, /*value*/offset_s6_s3},
            {/*loc*/-0.1f, /*deriv*/0.0f, /*value*/offset_s6_s4},
            {/*loc*/0.2f, /*deriv*/0.0f, /*value*/offset_s6_s5},
            {/*loc*/0.7f, /*deriv*/0.0f, /*value*/offset_s6_s6}
        }
        });

inline SplinePtr offset_s7_s0 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.38940096f, /*value*/-0.08880186f},
                {/*loc*/1.0f, /*deriv*/0.38940096f, /*value*/0.69000006f}
            }
            });

inline SplinePtr offset_s7_s1 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.37788022f, /*value*/-0.115760356f},
                {/*loc*/1.0f, /*deriv*/0.37788022f, /*value*/0.6400001f}
            }
            });

inline SplinePtr offset_s7_s2 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.0f, /*value*/-0.2222f},
                {/*loc*/-0.75f, /*deriv*/0.0f, /*value*/-0.2222f},
                {/*loc*/-0.65f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/0.5954547f, /*deriv*/0.0f, /*value*/2.9802322e-08f},
                {/*loc*/0.6054547f, /*deriv*/0.2534563f, /*value*/2.9802322e-08f},
                {/*loc*/1.0f, /*deriv*/0.2534563f, /*value*/0.100000024f}
            }
            });

inline SplinePtr offset_s7_s3 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.25f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.05f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.05f},
                {/*loc*/0.4f, /*deriv*/0.0f, /*value*/0.05f},
                {/*loc*/1.0f, /*deriv*/0.007000001f, /*value*/0.060000002f}
            }
            });

inline SplinePtr offset_s7_s4 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.1f},
                {/*loc*/-0.4f, /*deriv*/0.01f, /*value*/0.001f},
                {/*loc*/0.0f, /*deriv*/0.01f, /*value*/0.003f},
                {/*loc*/0.4f, /*deriv*/0.094000004f, /*value*/0.05f},
                {/*loc*/1.0f, /*deriv*/0.007000001f, /*value*/0.060000002f}
            }
            });

inline SplinePtr offset_s7_s5 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.1f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.01f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.01f},
                {/*loc*/0.4f, /*deriv*/0.04f, /*value*/0.03f},
                {/*loc*/1.0f, /*deriv*/0.049f, /*value*/0.1f}
            }
            });

inline SplinePtr offset_s7_s6 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.0f, /*value*/-0.02f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/-0.03f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/-0.03f},
                {/*loc*/0.4f, /*deriv*/0.12f, /*value*/0.03f},
                {/*loc*/1.0f, /*deriv*/0.049f, /*value*/0.1f}
            }
            });

inline SplinePtr offset_s7 = std::make_shared<Spline>(Spline{
        Coord::Erosion,
        {
            {/*loc*/-0.85f, /*deriv*/0.0f, /*value*/offset_s7_s0},
            {/*loc*/-0.7f, /*deriv*/0.0f, /*value*/offset_s7_s1},
            {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/offset_s7_s2},
            {/*loc*/-0.35f, /*deriv*/0.0f, /*value*/offset_s7_s3},
            {/*loc*/-0.1f, /*deriv*/0.0f, /*value*/offset_s7_s4},
            {/*loc*/0.2f, /*deriv*/0.0f, /*value*/offset_s7_s5},
            {/*loc*/0.7f, /*deriv*/0.0f, /*value*/offset_s7_s6}
        }
        });

inline SplinePtr offset_s8_s0 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.0f, /*value*/0.20235021f},
                {/*loc*/0.0f, /*deriv*/0.5138249f, /*value*/0.7161751f},
                {/*loc*/1.0f, /*deriv*/0.5138249f, /*value*/1.23f}
            }
            });

inline SplinePtr offset_s8_s1 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.0f, /*value*/0.2f},
                {/*loc*/0.0f, /*deriv*/0.43317974f, /*value*/0.44682026f},
                {/*loc*/1.0f, /*deriv*/0.43317974f, /*value*/0.88f}
            }
            });

inline SplinePtr offset_s8_s2 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.0f, /*value*/0.2f},
                {/*loc*/0.0f, /*deriv*/0.3917051f, /*value*/0.30829495f},
                {/*loc*/1.0f, /*deriv*/0.3917051f, /*value*/0.70000005f}
            }
            });

inline SplinePtr offset_s8_s3 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.25f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.35f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.35f},
                {/*loc*/0.4f, /*deriv*/0.0f, /*value*/0.35f},
                {/*loc*/1.0f, /*deriv*/0.049000014f, /*value*/0.42000002f}
            }
            });

inline SplinePtr offset_s8_s4 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.1f},
                {/*loc*/-0.4f, /*deriv*/0.07f, /*value*/0.0069999998f},
                {/*loc*/0.0f, /*deriv*/0.07f, /*value*/0.021f},
                {/*loc*/0.4f, /*deriv*/0.658f, /*value*/0.35f},
                {/*loc*/1.0f, /*deriv*/0.049000014f, /*value*/0.42000002f}
            }
            });

inline SplinePtr offset_s8_s5 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.1f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.01f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.01f},
                {/*loc*/0.4f, /*deriv*/0.04f, /*value*/0.03f},
                {/*loc*/1.0f, /*deriv*/0.049f, /*value*/0.1f}
            }
            });

inline SplinePtr offset_s8_s6 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.1f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.01f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.01f},
                {/*loc*/0.4f, /*deriv*/0.04f, /*value*/0.03f},
                {/*loc*/1.0f, /*deriv*/0.049f, /*value*/0.1f}
            }
            });

inline SplinePtr offset_s8_s7_s1 = std::make_shared<Spline>(Spline{
                Coord::RidgesFolded,
                {
                    {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.1f},
                    {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.01f},
                    {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.01f},
                    {/*loc*/0.4f, /*deriv*/0.04f, /*value*/0.03f},
                    {/*loc*/1.0f, /*deriv*/0.049f, /*value*/0.1f}
                }
                });

inline SplinePtr offset_s8_s7 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.0f, /*value*/-0.1f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/offset_s8_s7_s1},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.17f}
            }
            });

inline SplinePtr offset_s8_s8_s1 = std::make_shared<Spline>(Spline{
                Coord::RidgesFolded,
                {
                    {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.1f},
                    {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.01f},
                    {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.01f},
                    {/*loc*/0.4f, /*deriv*/0.04f, /*value*/0.03f},
                    {/*loc*/1.0f, /*deriv*/0.049f, /*value*/0.1f}
                }
                });

inline SplinePtr offset_s8_s8 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.0f, /*value*/-0.1f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/offset_s8_s8_s1},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.17f}
            }
            });

inline SplinePtr offset_s8_s9 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.1f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.01f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.01f},
                {/*loc*/0.4f, /*deriv*/0.04f, /*value*/0.03f},
                {/*loc*/1.0f, /*deriv*/0.049f, /*value*/0.1f}
            }
            });

inline SplinePtr offset_s8_s10 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.0f, /*value*/-0.02f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/-0.03f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/-0.03f},
                {/*loc*/0.4f, /*deriv*/0.12f, /*value*/0.03f},
                {/*loc*/1.0f, /*deriv*/0.049f, /*value*/0.1f}
            }
            });

inline SplinePtr offset_s8 = std::make_shared<Spline>(Spline{
        Coord::Erosion,
        {
            {/*loc*/-0.85f, /*deriv*/0.0f, /*value*/offset_s8_s0},
            {/*loc*/-0.7f, /*deriv*/0.0f, /*value*/offset_s8_s1},
            {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/offset_s8_s2},
            {/*loc*/-0.35f, /*deriv*/0.0f, /*value*/offset_s8_s3},
            {/*loc*/-0.1f, /*deriv*/0.0f, /*value*/offset_s8_s4},
            {/*loc*/0.2f, /*deriv*/0.0f, /*value*/offset_s8_s5},
            {/*loc*/0.4f, /*deriv*/0.0f, /*value*/offset_s8_s6},
            {/*loc*/0.45f, /*deriv*/0.0f, /*value*/offset_s8_s7},
            {/*loc*/0.55f, /*deriv*/0.0f, /*value*/offset_s8_s8},
            {/*loc*/0.58f, /*deriv*/0.0f, /*value*/offset_s8_s9},
            {/*loc*/0.7f, /*deriv*/0.0f, /*value*/offset_s8_s10}
        }
        });

inline SplinePtr offset_s9_s0 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.0f, /*value*/0.34792626f},
                {/*loc*/0.0f, /*deriv*/0.5760369f, /*value*/0.9239631f},
                {/*loc*/1.0f, /*deriv*/0.5760369f, /*value*/1.5f}
            }
            });

inline SplinePtr offset_s9_s1 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.0f, /*value*/0.2f},
                {/*loc*/0.0f, /*deriv*/0.4608295f, /*value*/0.5391705f},
                {/*loc*/1.0f, /*deriv*/0.4608295f, /*value*/1.0f}
            }
            });

inline SplinePtr offset_s9_s2 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.0f, /*value*/0.2f},
                {/*loc*/0.0f, /*deriv*/0.4608295f, /*value*/0.5391705f},
                {/*loc*/1.0f, /*deriv*/0.4608295f, /*value*/1.0f}
            }
            });

inline SplinePtr offset_s9_s3 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.2f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.5f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.5f},
                {/*loc*/0.4f, /*deriv*/0.0f, /*value*/0.5f},
                {/*loc*/1.0f, /*deriv*/0.070000015f, /*value*/0.6f}
            }
            });

inline SplinePtr offset_s9_s4 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.05f},
                {/*loc*/-0.4f, /*deriv*/0.099999994f, /*value*/0.01f},
                {/*loc*/0.0f, /*deriv*/0.099999994f, /*value*/0.03f},
                {/*loc*/0.4f, /*deriv*/0.94f, /*value*/0.5f},
                {/*loc*/1.0f, /*deriv*/0.070000015f, /*value*/0.6f}
            }
            });

inline SplinePtr offset_s9_s5 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.05f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.01f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.01f},
                {/*loc*/0.4f, /*deriv*/0.04f, /*value*/0.03f},
                {/*loc*/1.0f, /*deriv*/0.049f, /*value*/0.1f}
            }
            });

inline SplinePtr offset_s9_s6 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.05f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.01f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.01f},
                {/*loc*/0.4f, /*deriv*/0.04f, /*value*/0.03f},
                {/*loc*/1.0f, /*deriv*/0.049f, /*value*/0.1f}
            }
            });

inline SplinePtr offset_s9_s7_s1 = std::make_shared<Spline>(Spline{
                Coord::RidgesFolded,
                {
                    {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.05f},
                    {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.01f},
                    {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.01f},
                    {/*loc*/0.4f, /*deriv*/0.04f, /*value*/0.03f},
                    {/*loc*/1.0f, /*deriv*/0.049f, /*value*/0.1f}
                }
                });

inline SplinePtr offset_s9_s7 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.0f, /*value*/-0.05f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/offset_s9_s7_s1},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.17f}
            }
            });

inline SplinePtr offset_s9_s8_s1 = std::make_shared<Spline>(Spline{
                Coord::RidgesFolded,
                {
                    {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.05f},
                    {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.01f},
                    {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.01f},
                    {/*loc*/0.4f, /*deriv*/0.04f, /*value*/0.03f},
                    {/*loc*/1.0f, /*deriv*/0.049f, /*value*/0.1f}
                }
                });

inline SplinePtr offset_s9_s8 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.0f, /*value*/-0.05f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/offset_s9_s8_s1},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.17f}
            }
            });

inline SplinePtr offset_s9_s9 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.5f, /*value*/-0.05f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.01f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.01f},
                {/*loc*/0.4f, /*deriv*/0.04f, /*value*/0.03f},
                {/*loc*/1.0f, /*deriv*/0.049f, /*value*/0.1f}
            }
            });

inline SplinePtr offset_s9_s10 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-1.0f, /*deriv*/0.015f, /*value*/-0.02f},
                {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/0.01f},
                {/*loc*/0.0f, /*deriv*/0.0f, /*value*/0.01f},
                {/*loc*/0.4f, /*deriv*/0.04f, /*value*/0.03f},
                {/*loc*/1.0f, /*deriv*/0.049f, /*value*/0.1f}
            }
            });

inline SplinePtr offset_s9 = std::make_shared<Spline>(Spline{
        Coord::Erosion,
        {
            {/*loc*/-0.85f, /*deriv*/0.0f, /*value*/offset_s9_s0},
            {/*loc*/-0.7f, /*deriv*/0.0f, /*value*/offset_s9_s1},
            {/*loc*/-0.4f, /*deriv*/0.0f, /*value*/offset_s9_s2},
            {/*loc*/-0.35f, /*deriv*/0.0f, /*value*/offset_s9_s3},
            {/*loc*/-0.1f, /*deriv*/0.0f, /*value*/offset_s9_s4},
            {/*loc*/0.2f, /*deriv*/0.0f, /*value*/offset_s9_s5},
            {/*loc*/0.4f, /*deriv*/0.0f, /*value*/offset_s9_s6},
            {/*loc*/0.45f, /*deriv*/0.0f, /*value*/offset_s9_s7},
            {/*loc*/0.55f, /*deriv*/0.0f, /*value*/offset_s9_s8},
            {/*loc*/0.58f, /*deriv*/0.0f, /*value*/offset_s9_s9},
            {/*loc*/0.7f, /*deriv*/0.0f, /*value*/offset_s9_s10}
        }
        });

inline SplinePtr offset = std::make_shared<Spline>(Spline{
    Coord::Continents,
    {
        {/*loc*/-1.1f, /*deriv*/0.0f, /*value*/0.044f},
        {/*loc*/-1.02f, /*deriv*/0.0f, /*value*/-0.2222f},
        {/*loc*/-0.51f, /*deriv*/0.0f, /*value*/-0.2222f},
        {/*loc*/-0.44f, /*deriv*/0.0f, /*value*/-0.12f},
        {/*loc*/-0.18f, /*deriv*/0.0f, /*value*/-0.12f},
        {/*loc*/-0.16f, /*deriv*/0.0f, /*value*/offset_s5},
        {/*loc*/-0.15f, /*deriv*/0.0f, /*value*/offset_s6},
        {/*loc*/-0.1f, /*deriv*/0.0f, /*value*/offset_s7},
        {/*loc*/0.25f, /*deriv*/0.0f, /*value*/offset_s8},
        {/*loc*/1.0f, /*deriv*/0.0f, /*value*/offset_s9}
    }
    });

inline SplinePtr factor_s1_s0 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.2f, /*deriv*/0.0f, /*value*/6.25f}
            }
            });

inline SplinePtr factor_s1_s1 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.05f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.05f, /*deriv*/0.0f, /*value*/2.67f}
            }
            });

inline SplinePtr factor_s1_s2 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.2f, /*deriv*/0.0f, /*value*/6.25f}
            }
            });

inline SplinePtr factor_s1_s3 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.2f, /*deriv*/0.0f, /*value*/6.25f}
            }
            });

inline SplinePtr factor_s1_s4 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.05f, /*deriv*/0.0f, /*value*/2.67f},
                {/*loc*/0.05f, /*deriv*/0.0f, /*value*/6.3f}
            }
            });

inline SplinePtr factor_s1_s5 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.2f, /*deriv*/0.0f, /*value*/6.25f}
            }
            });

inline SplinePtr factor_s1_s7_s1 = std::make_shared<Spline>(Spline{
                Coord::Ridges,
                {
                    {/*loc*/0.0f, /*deriv*/0.0f, /*value*/6.25f},
                    {/*loc*/0.1f, /*deriv*/0.0f, /*value*/0.625f}
                }
                });

inline SplinePtr factor_s1_s7 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-0.9f, /*deriv*/0.0f, /*value*/6.25f},
                {/*loc*/-0.69f, /*deriv*/0.0f, /*value*/factor_s1_s7_s1}
            }
            });

inline SplinePtr factor_s1_s8_s1 = std::make_shared<Spline>(Spline{
                Coord::Ridges,
                {
                    {/*loc*/0.0f, /*deriv*/0.0f, /*value*/6.25f},
                    {/*loc*/0.1f, /*deriv*/0.0f, /*value*/0.625f}
                }
                });

inline SplinePtr factor_s1_s8 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-0.9f, /*deriv*/0.0f, /*value*/6.25f},
                {/*loc*/-0.69f, /*deriv*/0.0f, /*value*/factor_s1_s8_s1}
            }
            });

inline SplinePtr factor_s1 = std::make_shared<Spline>(Spline{
        Coord::Erosion,
        {
            {/*loc*/-0.6f, /*deriv*/0.0f, /*value*/factor_s1_s0},
            {/*loc*/-0.5f, /*deriv*/0.0f, /*value*/factor_s1_s1},
            {/*loc*/-0.35f, /*deriv*/0.0f, /*value*/factor_s1_s2},
            {/*loc*/-0.25f, /*deriv*/0.0f, /*value*/factor_s1_s3},
            {/*loc*/-0.1f, /*deriv*/0.0f, /*value*/factor_s1_s4},
            {/*loc*/0.03f, /*deriv*/0.0f, /*value*/factor_s1_s5},
            {/*loc*/0.35f, /*deriv*/0.0f, /*value*/6.25f},
            {/*loc*/0.45f, /*deriv*/0.0f, /*value*/factor_s1_s7},
            {/*loc*/0.55f, /*deriv*/0.0f, /*value*/factor_s1_s8},
            {/*loc*/0.62f, /*deriv*/0.0f, /*value*/6.25f}
        }
        });

inline SplinePtr factor_s2_s0 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.2f, /*deriv*/0.0f, /*value*/5.47f}
            }
            });

inline SplinePtr factor_s2_s1 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.05f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.05f, /*deriv*/0.0f, /*value*/2.67f}
            }
            });

inline SplinePtr factor_s2_s2 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.2f, /*deriv*/0.0f, /*value*/5.47f}
            }
            });

inline SplinePtr factor_s2_s3 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.2f, /*deriv*/0.0f, /*value*/5.47f}
            }
            });

inline SplinePtr factor_s2_s4 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.05f, /*deriv*/0.0f, /*value*/2.67f},
                {/*loc*/0.05f, /*deriv*/0.0f, /*value*/6.3f}
            }
            });

inline SplinePtr factor_s2_s5 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.2f, /*deriv*/0.0f, /*value*/5.47f}
            }
            });

inline SplinePtr factor_s2_s7_s1 = std::make_shared<Spline>(Spline{
                Coord::Ridges,
                {
                    {/*loc*/0.0f, /*deriv*/0.0f, /*value*/5.47f},
                    {/*loc*/0.1f, /*deriv*/0.0f, /*value*/0.625f}
                }
                });

inline SplinePtr factor_s2_s7 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-0.9f, /*deriv*/0.0f, /*value*/5.47f},
                {/*loc*/-0.69f, /*deriv*/0.0f, /*value*/factor_s2_s7_s1}
            }
            });

inline SplinePtr factor_s2_s8_s1 = std::make_shared<Spline>(Spline{
                Coord::Ridges,
                {
                    {/*loc*/0.0f, /*deriv*/0.0f, /*value*/5.47f},
                    {/*loc*/0.1f, /*deriv*/0.0f, /*value*/0.625f}
                }
                });

inline SplinePtr factor_s2_s8 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-0.9f, /*deriv*/0.0f, /*value*/5.47f},
                {/*loc*/-0.69f, /*deriv*/0.0f, /*value*/factor_s2_s8_s1}
            }
            });

inline SplinePtr factor_s2 = std::make_shared<Spline>(Spline{
        Coord::Erosion,
        {
            {/*loc*/-0.6f, /*deriv*/0.0f, /*value*/factor_s2_s0},
            {/*loc*/-0.5f, /*deriv*/0.0f, /*value*/factor_s2_s1},
            {/*loc*/-0.35f, /*deriv*/0.0f, /*value*/factor_s2_s2},
            {/*loc*/-0.25f, /*deriv*/0.0f, /*value*/factor_s2_s3},
            {/*loc*/-0.1f, /*deriv*/0.0f, /*value*/factor_s2_s4},
            {/*loc*/0.03f, /*deriv*/0.0f, /*value*/factor_s2_s5},
            {/*loc*/0.35f, /*deriv*/0.0f, /*value*/5.47f},
            {/*loc*/0.45f, /*deriv*/0.0f, /*value*/factor_s2_s7},
            {/*loc*/0.55f, /*deriv*/0.0f, /*value*/factor_s2_s8},
            {/*loc*/0.62f, /*deriv*/0.0f, /*value*/5.47f}
        }
        });

inline SplinePtr factor_s3_s0 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.2f, /*deriv*/0.0f, /*value*/5.08f}
            }
            });

inline SplinePtr factor_s3_s1 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.05f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.05f, /*deriv*/0.0f, /*value*/2.67f}
            }
            });

inline SplinePtr factor_s3_s2 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.2f, /*deriv*/0.0f, /*value*/5.08f}
            }
            });

inline SplinePtr factor_s3_s3 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.2f, /*deriv*/0.0f, /*value*/5.08f}
            }
            });

inline SplinePtr factor_s3_s4 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.05f, /*deriv*/0.0f, /*value*/2.67f},
                {/*loc*/0.05f, /*deriv*/0.0f, /*value*/6.3f}
            }
            });

inline SplinePtr factor_s3_s5 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.2f, /*deriv*/0.0f, /*value*/5.08f}
            }
            });

inline SplinePtr factor_s3_s7_s1 = std::make_shared<Spline>(Spline{
                Coord::Ridges,
                {
                    {/*loc*/0.0f, /*deriv*/0.0f, /*value*/5.08f},
                    {/*loc*/0.1f, /*deriv*/0.0f, /*value*/0.625f}
                }
                });

inline SplinePtr factor_s3_s7 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-0.9f, /*deriv*/0.0f, /*value*/5.08f},
                {/*loc*/-0.69f, /*deriv*/0.0f, /*value*/factor_s3_s7_s1}
            }
            });

inline SplinePtr factor_s3_s8_s1 = std::make_shared<Spline>(Spline{
                Coord::Ridges,
                {
                    {/*loc*/0.0f, /*deriv*/0.0f, /*value*/5.08f},
                    {/*loc*/0.1f, /*deriv*/0.0f, /*value*/0.625f}
                }
                });

inline SplinePtr factor_s3_s8 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-0.9f, /*deriv*/0.0f, /*value*/5.08f},
                {/*loc*/-0.69f, /*deriv*/0.0f, /*value*/factor_s3_s8_s1}
            }
            });

inline SplinePtr factor_s3 = std::make_shared<Spline>(Spline{
        Coord::Erosion,
        {
            {/*loc*/-0.6f, /*deriv*/0.0f, /*value*/factor_s3_s0},
            {/*loc*/-0.5f, /*deriv*/0.0f, /*value*/factor_s3_s1},
            {/*loc*/-0.35f, /*deriv*/0.0f, /*value*/factor_s3_s2},
            {/*loc*/-0.25f, /*deriv*/0.0f, /*value*/factor_s3_s3},
            {/*loc*/-0.1f, /*deriv*/0.0f, /*value*/factor_s3_s4},
            {/*loc*/0.03f, /*deriv*/0.0f, /*value*/factor_s3_s5},
            {/*loc*/0.35f, /*deriv*/0.0f, /*value*/5.08f},
            {/*loc*/0.45f, /*deriv*/0.0f, /*value*/factor_s3_s7},
            {/*loc*/0.55f, /*deriv*/0.0f, /*value*/factor_s3_s8},
            {/*loc*/0.62f, /*deriv*/0.0f, /*value*/5.08f}
        }
        });

inline SplinePtr factor_s4_s0 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.2f, /*deriv*/0.0f, /*value*/4.69f}
            }
            });

inline SplinePtr factor_s4_s1 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.05f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.05f, /*deriv*/0.0f, /*value*/2.67f}
            }
            });

inline SplinePtr factor_s4_s2 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.2f, /*deriv*/0.0f, /*value*/4.69f}
            }
            });

inline SplinePtr factor_s4_s3 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.2f, /*deriv*/0.0f, /*value*/4.69f}
            }
            });

inline SplinePtr factor_s4_s4 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.05f, /*deriv*/0.0f, /*value*/2.67f},
                {/*loc*/0.05f, /*deriv*/0.0f, /*value*/6.3f}
            }
            });

inline SplinePtr factor_s4_s5 = std::make_shared<Spline>(Spline{
            Coord::Ridges,
            {
                {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                {/*loc*/0.2f, /*deriv*/0.0f, /*value*/4.69f}
            }
            });

inline SplinePtr factor_s4_s6_s0 = std::make_shared<Spline>(Spline{
                Coord::Ridges,
                {
                    {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                    {/*loc*/0.2f, /*deriv*/0.0f, /*value*/4.69f}
                }
                });

inline SplinePtr factor_s4_s6 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/0.45f, /*deriv*/0.0f, /*value*/factor_s4_s6_s0},
                {/*loc*/0.7f, /*deriv*/0.0f, /*value*/1.56f}
            }
            });

inline SplinePtr factor_s4_s7_s0 = std::make_shared<Spline>(Spline{
                Coord::Ridges,
                {
                    {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                    {/*loc*/0.2f, /*deriv*/0.0f, /*value*/4.69f}
                }
                });

inline SplinePtr factor_s4_s7 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/0.45f, /*deriv*/0.0f, /*value*/factor_s4_s7_s0},
                {/*loc*/0.7f, /*deriv*/0.0f, /*value*/1.56f}
            }
            });

inline SplinePtr factor_s4_s8_s0 = std::make_shared<Spline>(Spline{
                Coord::Ridges,
                {
                    {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                    {/*loc*/0.2f, /*deriv*/0.0f, /*value*/4.69f}
                }
                });

inline SplinePtr factor_s4_s8 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-0.7f, /*deriv*/0.0f, /*value*/factor_s4_s8_s0},
                {/*loc*/-0.15f, /*deriv*/0.0f, /*value*/1.37f}
            }
            });

inline SplinePtr factor_s4_s9_s0 = std::make_shared<Spline>(Spline{
                Coord::Ridges,
                {
                    {/*loc*/-0.2f, /*deriv*/0.0f, /*value*/6.3f},
                    {/*loc*/0.2f, /*deriv*/0.0f, /*value*/4.69f}
                }
                });

inline SplinePtr factor_s4_s9 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/-0.7f, /*deriv*/0.0f, /*value*/factor_s4_s9_s0},
                {/*loc*/-0.15f, /*deriv*/0.0f, /*value*/1.37f}
            }
            });

inline SplinePtr factor_s4 = std::make_shared<Spline>(Spline{
        Coord::Erosion,
        {
            {/*loc*/-0.6f, /*deriv*/0.0f, /*value*/factor_s4_s0},
            {/*loc*/-0.5f, /*deriv*/0.0f, /*value*/factor_s4_s1},
            {/*loc*/-0.35f, /*deriv*/0.0f, /*value*/factor_s4_s2},
            {/*loc*/-0.25f, /*deriv*/0.0f, /*value*/factor_s4_s3},
            {/*loc*/-0.1f, /*deriv*/0.0f, /*value*/factor_s4_s4},
            {/*loc*/0.03f, /*deriv*/0.0f, /*value*/factor_s4_s5},
            {/*loc*/0.05f, /*deriv*/0.0f, /*value*/factor_s4_s6},
            {/*loc*/0.4f, /*deriv*/0.0f, /*value*/factor_s4_s7},
            {/*loc*/0.45f, /*deriv*/0.0f, /*value*/factor_s4_s8},
            {/*loc*/0.55f, /*deriv*/0.0f, /*value*/factor_s4_s9},
            {/*loc*/0.58f, /*deriv*/0.0f, /*value*/4.69f}
        }
        });

inline SplinePtr factor = std::make_shared<Spline>(Spline{
    Coord::Continents,
    {
        {/*loc*/-0.19f, /*deriv*/0.0f, /*value*/3.95f},
        {/*loc*/-0.15f, /*deriv*/0.0f, /*value*/factor_s1},
        {/*loc*/-0.1f, /*deriv*/0.0f, /*value*/factor_s2},
        {/*loc*/0.03f, /*deriv*/0.0f, /*value*/factor_s3},
        {/*loc*/0.06f, /*deriv*/0.0f, /*value*/factor_s4}
    }
    });

inline SplinePtr jaggedness_s1_s0_s2 = std::make_shared<Spline>(Spline{
                Coord::Ridges,
                {
                    {/*loc*/-0.01f, /*deriv*/0.0f, /*value*/0.63f},
                    {/*loc*/0.01f, /*deriv*/0.0f, /*value*/0.3f}
                }
                });

inline SplinePtr jaggedness_s1_s0 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/0.19999999f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/0.44999996f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/1.0f, /*deriv*/0.0f, /*value*/jaggedness_s1_s0_s2}
            }
            });

inline SplinePtr jaggedness_s1_s1_s2 = std::make_shared<Spline>(Spline{
                Coord::Ridges,
                {
                    {/*loc*/-0.01f, /*deriv*/0.0f, /*value*/0.315f},
                    {/*loc*/0.01f, /*deriv*/0.0f, /*value*/0.15f}
                }
                });

inline SplinePtr jaggedness_s1_s1 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/0.19999999f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/0.44999996f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/1.0f, /*deriv*/0.0f, /*value*/jaggedness_s1_s1_s2}
            }
            });

inline SplinePtr jaggedness_s1_s2_s2 = std::make_shared<Spline>(Spline{
                Coord::Ridges,
                {
                    {/*loc*/-0.01f, /*deriv*/0.0f, /*value*/0.315f},
                    {/*loc*/0.01f, /*deriv*/0.0f, /*value*/0.15f}
                }
                });

inline SplinePtr jaggedness_s1_s2 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/0.19999999f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/0.44999996f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/1.0f, /*deriv*/0.0f, /*value*/jaggedness_s1_s2_s2}
            }
            });

inline SplinePtr jaggedness_s1 = std::make_shared<Spline>(Spline{
        Coord::Erosion,
        {
            {/*loc*/-1.0f, /*deriv*/0.0f, /*value*/jaggedness_s1_s0},
            {/*loc*/-0.78f, /*deriv*/0.0f, /*value*/jaggedness_s1_s1},
            {/*loc*/-0.5775f, /*deriv*/0.0f, /*value*/jaggedness_s1_s2},
            {/*loc*/-0.375f, /*deriv*/0.0f, /*value*/0.0f}
        }
        });

inline SplinePtr jaggedness_s2_s0_s1 = std::make_shared<Spline>(Spline{
                Coord::Ridges,
                {
                    {/*loc*/-0.01f, /*deriv*/0.0f, /*value*/0.63f},
                    {/*loc*/0.01f, /*deriv*/0.0f, /*value*/0.3f}
                }
                });

inline SplinePtr jaggedness_s2_s0_s2 = std::make_shared<Spline>(Spline{
                Coord::Ridges,
                {
                    {/*loc*/-0.01f, /*deriv*/0.0f, /*value*/0.63f},
                    {/*loc*/0.01f, /*deriv*/0.0f, /*value*/0.3f}
                }
                });

inline SplinePtr jaggedness_s2_s0 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/0.19999999f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/0.44999996f, /*deriv*/0.0f, /*value*/jaggedness_s2_s0_s1},
                {/*loc*/1.0f, /*deriv*/0.0f, /*value*/jaggedness_s2_s0_s2}
            }
            });

inline SplinePtr jaggedness_s2_s1_s2 = std::make_shared<Spline>(Spline{
                Coord::Ridges,
                {
                    {/*loc*/-0.01f, /*deriv*/0.0f, /*value*/0.63f},
                    {/*loc*/0.01f, /*deriv*/0.0f, /*value*/0.3f}
                }
                });

inline SplinePtr jaggedness_s2_s1 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/0.19999999f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/0.44999996f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/1.0f, /*deriv*/0.0f, /*value*/jaggedness_s2_s1_s2}
            }
            });

inline SplinePtr jaggedness_s2_s2_s2 = std::make_shared<Spline>(Spline{
                Coord::Ridges,
                {
                    {/*loc*/-0.01f, /*deriv*/0.0f, /*value*/0.63f},
                    {/*loc*/0.01f, /*deriv*/0.0f, /*value*/0.3f}
                }
                });

inline SplinePtr jaggedness_s2_s2 = std::make_shared<Spline>(Spline{
            Coord::RidgesFolded,
            {
                {/*loc*/0.19999999f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/0.44999996f, /*deriv*/0.0f, /*value*/0.0f},
                {/*loc*/1.0f, /*deriv*/0.0f, /*value*/jaggedness_s2_s2_s2}
            }
            });

inline SplinePtr jaggedness_s2 = std::make_shared<Spline>(Spline{
        Coord::Erosion,
        {
            {/*loc*/-1.0f, /*deriv*/0.0f, /*value*/jaggedness_s2_s0},
            {/*loc*/-0.78f, /*deriv*/0.0f, /*value*/jaggedness_s2_s1},
            {/*loc*/-0.5775f, /*deriv*/0.0f, /*value*/jaggedness_s2_s2},
            {/*loc*/-0.375f, /*deriv*/0.0f, /*value*/0.0f}
        }
        });

inline SplinePtr jaggedness = std::make_shared<Spline>(Spline{
    Coord::Continents,
    {
        {/*loc*/-0.11f, /*deriv*/0.0f, /*value*/0.0f},
        {/*loc*/0.03f, /*deriv*/0.0f, /*value*/jaggedness_s1},
        {/*loc*/0.65f, /*deriv*/0.0f, /*value*/jaggedness_s2}
    }
    });

} // namespace mc_spline
} // namespace VoxelEngine

#endif // FUK_MINECRAFT_MC_SPLINE_DATA_HPP

