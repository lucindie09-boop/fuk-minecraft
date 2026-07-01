#ifndef FUK_MINECRAFT_SPLINE_HPP
#define FUK_MINECRAFT_SPLINE_HPP

#include <variant>
#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>

namespace VoxelEngine {

struct Spline;
using SplinePtr = std::shared_ptr<Spline>;
using SplineValue = std::variant<float, SplinePtr>;

enum class Coord { Continents, Erosion, Ridges, RidgesFolded };

struct SplinePoint {
    float location;
    float derivative;
    SplineValue value;
};

struct Spline {
    Coord coordinate;
    std::vector<SplinePoint> points;

    static float lerp(float t, float a, float b) {
        return a + t * (b - a);
    }

    float valueAt(const SplinePoint& p, float c, float e, float r, float fr) const {
        if (std::holds_alternative<float>(p.value)) {
            return std::get<float>(p.value);
        }
        return std::get<SplinePtr>(p.value)->compute(c, e, r, fr);
    }

    float compute(float continents, float erosion, float ridges, float foldedRidges) const {
        float x = coordinate == Coord::Continents ? continents
                : coordinate == Coord::Erosion    ? erosion
                : coordinate == Coord::Ridges      ? ridges : foldedRidges;
        int n = (int)points.size();
        if (x <= points[0].location) {
            float v0 = valueAt(points[0], continents, erosion, ridges, foldedRidges);
            return v0 + points[0].derivative * (x - points[0].location);
        }
        if (x >= points[n-1].location) {
            float vN = valueAt(points[n-1], continents, erosion, ridges, foldedRidges);
            return vN + points[n-1].derivative * (x - points[n-1].location);
        }
        int i = 0;
        while (i + 1 < n && points[i+1].location <= x) i++;
        auto& p0 = points[i];
        auto& p1 = points[i+1];
        float f = (x - p0.location) / (p1.location - p0.location);
        float v0 = valueAt(p0, continents, erosion, ridges, foldedRidges);
        float v1 = valueAt(p1, continents, erosion, ridges, foldedRidges);
        float g = p0.derivative * (p1.location - p0.location) - (v1 - v0);
        float h = -p1.derivative * (p1.location - p0.location) + (v1 - v0);
        return lerp(f, v0, v1) + f * (1.0f - f) * lerp(f, g, h);
    }
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_SPLINE_HPP
