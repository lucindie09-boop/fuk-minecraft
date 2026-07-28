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

    // Domain warp: shifts (x, z) using the shift noise field, then samples
    // the requested climate parameter at the warped coordinates.
    double sample_continentalness(double x, double z) const;
    double sample_erosion(double x, double z) const;
    double sample_weirdness(double x, double z) const;
    double sample_temperature(double x, double z) const;
    double sample_humidity(double x, double z) const;

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
