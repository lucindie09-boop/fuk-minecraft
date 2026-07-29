#ifndef FUK_MINECRAFT_CLIMATE_SAMPLER_HPP
#define FUK_MINECRAFT_CLIMATE_SAMPLER_HPP
#include <cstdint>
#include "core/perlin_noise.hpp"

namespace VoxelEngine {

// -----------------------------------------------------------------------------
// ClimateSampler — owns the 6 DoublePerlinNoise fields that drive vanilla's
// overworld climate: shift (coordinate warp), continentalness, erosion,
// weirdness, temperature, and humidity.
//
// Each field has its own amplitude array and octave range, transcribed from
// biomenoise.c's init_climate_seed(). The shift field is applied as a domain
// warp on (x, z) before sampling the other five.
// -----------------------------------------------------------------------------
class ClimateSampler {
private:
    DoublePerlinNoise shift_;
    DoublePerlinNoise continentalness_;
    DoublePerlinNoise erosion_;
    DoublePerlinNoise weirdness_;
    DoublePerlinNoise temperature_;
    DoublePerlinNoise humidity_;

    double warp_strength_;

public:
    explicit ClimateSampler(uint64_t world_seed);

    // Warped coordinate — compute once and reuse
    struct WarpedCoord { double wx, wz; };
    WarpedCoord warp(double x, double z) const;

    // Domain warp: shifts (x, z) using the shift noise field, then samples
    // the requested climate parameter at the warped coordinates.
    double sample_continentalness(double x, double z) const;
    double sample_erosion(double x, double z) const;
    double sample_weirdness(double x, double z) const;
    double sample_temperature(double x, double z) const;
    double sample_humidity(double x, double z) const;

    // Already-warped overloads — skip the domain warp, sample directly
    double sample_continentalness(WarpedCoord wc) const { return sample_raw(continentalness_, wc.wx, wc.wz); }
    double sample_erosion(WarpedCoord wc) const          { return sample_raw(erosion_, wc.wx, wc.wz); }
    double sample_weirdness(WarpedCoord wc) const        { return sample_raw(weirdness_, wc.wx, wc.wz); }
    double sample_temperature(WarpedCoord wc) const      { return sample_raw(temperature_, wc.wx, wc.wz); }
    double sample_humidity(WarpedCoord wc) const         { return sample_raw(humidity_, wc.wx, wc.wz); }

    // Raw shift values (for debugging / domain-warp inspection)
    double sample_shift_x(double x, double z) const;
    double sample_shift_z(double x, double z) const;

    // Access to warp strength
    double get_warp_strength() const { return warp_strength_; }

private:
    // Internal: sample a climate field at already-warped coordinates
    double sample_raw(const DoublePerlinNoise& field, double x, double z) const;
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_CLIMATE_SAMPLER_HPP
