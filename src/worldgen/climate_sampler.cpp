#include "worldgen/climate_sampler.hpp"

namespace VoxelEngine {

// ---------------------------------------------------------------------------
// Vanilla amplitude arrays and octave ranges (from biomenoise.c)
//
// Parameter     | Amplitudes                      | first_octave | len
// ------------- | --------------------------------| -------------| ----
// Shift         | {1, 1, 1, 0}                    | -3           | 4
// Temperature   | {1.5, 0, 1, 0, 0, 0}            | -10          | 6
// Humidity      | {1, 1, 0, 0, 0, 0}              | -8           | 6
// Continentalness | {1, 1, 2, 2, 2, 1, 1, 1, 1}  | -9           | 9
// Erosion       | {1, 1, 0, 1, 1}                 | -9           | 5
// Weirdness     | {1, 2, 1, 0, 0, 0}              | -7           | 6
// ---------------------------------------------------------------------------

static constexpr double SHIFT_AMPS[]           = {1.0, 1.0, 1.0, 0.0};
static constexpr int    SHIFT_FIRST_OCTAVE     = -3;
static constexpr int    SHIFT_LEN              = 4;

static constexpr double TEMPERATURE_AMPS[]     = {1.5, 0.0, 1.0, 0.0, 0.0, 0.0};
static constexpr int    TEMPERATURE_FIRST_OCTAVE = -10;
static constexpr int    TEMPERATURE_LEN        = 6;

static constexpr double HUMIDITY_AMPS[]        = {1.0, 1.0, 0.0, 0.0, 0.0, 0.0};
static constexpr int    HUMIDITY_FIRST_OCTAVE  = -8;
static constexpr int    HUMIDITY_LEN           = 6;

static constexpr double CONTINENTALNESS_AMPS[] = {1.0, 1.0, 2.0, 2.0, 2.0, 1.0, 1.0, 1.0, 1.0};
static constexpr int    CONTINENTALNESS_FIRST_OCTAVE = -9;
static constexpr int    CONTINENTALNESS_LEN    = 9;

static constexpr double EROSION_AMPS[]         = {1.0, 1.0, 0.0, 1.0, 1.0};
static constexpr int    EROSION_FIRST_OCTAVE   = -9;
static constexpr int    EROSION_LEN            = 5;

static constexpr double WEIRDNESS_AMPS[]       = {1.0, 2.0, 1.0, 0.0, 0.0, 0.0};
static constexpr int    WEIRDNESS_FIRST_OCTAVE = -7;
static constexpr int    WEIRDNESS_LEN          = 6;

// Domain warp strength: vanilla's exact multiplier (cubiomes: sampleDoublePerlin * 4.0).
static constexpr double DEFAULT_WARP_STRENGTH  = 4.0;

// Per-parameter seed offsets. Each parameter gets a distinct RNG stream
// derived from the world seed. These are arbitrary distinct values.
static constexpr uint64_t SHIFT_SEED_OFFSET          = 0;
static constexpr uint64_t CONTINENTALNESS_SEED_OFFSET = 1000;
static constexpr uint64_t EROSION_SEED_OFFSET        = 2000;
static constexpr uint64_t WEIRDNESS_SEED_OFFSET      = 3000;
static constexpr uint64_t TEMPERATURE_SEED_OFFSET    = 4000;
static constexpr uint64_t HUMIDITY_SEED_OFFSET       = 5000;

ClimateSampler::ClimateSampler(uint64_t world_seed)
    : shift_           (world_seed + SHIFT_SEED_OFFSET,
                        SHIFT_FIRST_OCTAVE, SHIFT_AMPS, SHIFT_LEN)
    , continentalness_ (world_seed + CONTINENTALNESS_SEED_OFFSET,
                        CONTINENTALNESS_FIRST_OCTAVE, CONTINENTALNESS_AMPS, CONTINENTALNESS_LEN)
    , erosion_         (world_seed + EROSION_SEED_OFFSET,
                        EROSION_FIRST_OCTAVE, EROSION_AMPS, EROSION_LEN)
    , weirdness_       (world_seed + WEIRDNESS_SEED_OFFSET,
                        WEIRDNESS_FIRST_OCTAVE, WEIRDNESS_AMPS, WEIRDNESS_LEN)
    , temperature_     (world_seed + TEMPERATURE_SEED_OFFSET,
                        TEMPERATURE_FIRST_OCTAVE, TEMPERATURE_AMPS, TEMPERATURE_LEN)
    , humidity_        (world_seed + HUMIDITY_SEED_OFFSET,
                        HUMIDITY_FIRST_OCTAVE, HUMIDITY_AMPS, HUMIDITY_LEN)
    , warp_strength_(DEFAULT_WARP_STRENGTH)
{
}

double ClimateSampler::sample_raw(const DoublePerlinNoise& field, double x, double z) const {
    return field.sample(x, 0.0, z);
}

double ClimateSampler::sample_shift_x(double x, double z) const {
    // Vanilla: shift.sample(x, 0, z) * 4.0
    return sample_raw(shift_, x, z) * warp_strength_;
}

double ClimateSampler::sample_shift_z(double x, double z) const {
    // Vanilla decorrelates Z from X by resampling with swapped inputs:
    // shift.sample(z, x, 0) instead of shift.sample(x, 0, z)
    return sample_raw(shift_, z, x) * warp_strength_;
}

double ClimateSampler::sample_continentalness(double x, double z) const {
    double wx = x + sample_shift_x(x, z);
    double wz = z + sample_shift_z(x, z);
    return sample_raw(continentalness_, wx, wz);
}

double ClimateSampler::sample_erosion(double x, double z) const {
    double wx = x + sample_shift_x(x, z);
    double wz = z + sample_shift_z(x, z);
    return sample_raw(erosion_, wx, wz);
}

double ClimateSampler::sample_weirdness(double x, double z) const {
    double wx = x + sample_shift_x(x, z);
    double wz = z + sample_shift_z(x, z);
    return sample_raw(weirdness_, wx, wz);
}

double ClimateSampler::sample_temperature(double x, double z) const {
    double wx = x + sample_shift_x(x, z);
    double wz = z + sample_shift_z(x, z);
    return sample_raw(temperature_, wx, wz);
}

double ClimateSampler::sample_humidity(double x, double z) const {
    double wx = x + sample_shift_x(x, z);
    double wz = z + sample_shift_z(x, z);
    return sample_raw(humidity_, wx, wz);
}

} // namespace VoxelEngine
