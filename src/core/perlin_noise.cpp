#include "core/perlin_noise.hpp"
#include <algorithm>
#include <cmath>

namespace VoxelEngine {

// -----------------------------------------------------------------------------
// ImprovedNoise
// -----------------------------------------------------------------------------

ImprovedNoise::ImprovedNoise() : a_(0), b_(0), c_(0) {
    for (int i = 0; i < 257; ++i) perm_[i] = 0;
}

ImprovedNoise::ImprovedNoise(uint64_t seed) {
    RandomSource rng(seed);
    a_ = rng.next_double() * 256.0;
    b_ = rng.next_double() * 256.0;
    c_ = rng.next_double() * 256.0;

    // Forward Fisher-Yates shuffle (matches vanilla convention: i from 0 to 255)
    for (int i = 0; i < 256; ++i) perm_[i] = static_cast<uint8_t>(i);
    for (int i = 0; i < 256; ++i) {
        int j = rng.next_int(256 - i) + i;
        std::swap(perm_[i], perm_[j]);
    }
    perm_[256] = perm_[0]; // wraparound
}

double ImprovedNoise::sample(double x, double y, double z) const {
    x += a_;
    y += b_;
    z += c_;

    double xi = std::floor(x);
    double yi = std::floor(y);
    double zi = std::floor(z);

    double xf = x - xi;
    double yf = y - yi;
    double zf = z - zi;

    double u = fade(xf);
    double v = fade(yf);
    double w = fade(zf);

    int X = static_cast<int>(xi) & 255;
    int Y = static_cast<int>(yi) & 255;
    int Z = static_cast<int>(zi) & 255;

    int A  = perm_[X] + Y;
    int AA = perm_[A] + Z;
    int AB = perm_[A + 1] + Z;
    int B  = perm_[X + 1] + Y;
    int BA = perm_[B] + Z;
    int BB = perm_[B + 1] + Z;

    double x1 = lerp(
        indexed_lerp(perm_[AA],     xf,       yf,       zf),
        indexed_lerp(perm_[BA],     xf - 1.0, yf,       zf),
        u);
    double x2 = lerp(
        indexed_lerp(perm_[AB],     xf,       yf - 1.0, zf),
        indexed_lerp(perm_[BB],     xf - 1.0, yf - 1.0, zf),
        u);
    double y1 = lerp(x1, x2, v);

    x1 = lerp(
        indexed_lerp(perm_[AA + 1], xf,       yf,       zf - 1.0),
        indexed_lerp(perm_[BA + 1], xf - 1.0, yf,       zf - 1.0),
        u);
    x2 = lerp(
        indexed_lerp(perm_[AB + 1], xf,       yf - 1.0, zf - 1.0),
        indexed_lerp(perm_[BB + 1], xf - 1.0, yf - 1.0, zf - 1.0),
        u);
    double y2 = lerp(x1, x2, v);

    return lerp(y1, y2, w);
}

// -----------------------------------------------------------------------------
// OctaveNoise
// -----------------------------------------------------------------------------

static uint64_t mix_seed(uint64_t base, uint64_t index) {
    uint64_t z = base ^ (index * 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
}

OctaveNoise::OctaveNoise(uint64_t base_seed, int first_octave, const double* amplitudes, int len) {
    count_ = 0;
    double two_pow_len = std::ldexp(1.0, len);
    double persistence = (two_pow_len - 2.0) / (two_pow_len - 1.0);

    uint64_t octave_idx = 0;
    for (int i = 0; i < len && count_ < MAX_OCTAVES; ++i) {
        if (amplitudes[i] == 0.0) {
            persistence *= 0.5;
            continue;
        }

        double lacunarity = std::ldexp(1.0, first_octave + i);
        double amp = amplitudes[i] * persistence;

        uint64_t octave_seed = mix_seed(base_seed, octave_idx);
        octaves_[count_] = {ImprovedNoise(octave_seed), amp, lacunarity};
        ++count_;

        persistence *= 0.5;
        ++octave_idx;
    }
}

double OctaveNoise::sample(double x, double y, double z) const {
    double v = 0.0;
    for (int i = 0; i < count_; ++i) {
        const auto& o = octaves_[i];
        v += o.amplitude * o.noise.sample(x * o.lacunarity, y * o.lacunarity, z * o.lacunarity);
    }
    return v;
}

// -----------------------------------------------------------------------------
// DoublePerlinNoise
// -----------------------------------------------------------------------------

double DoublePerlinNoise::compute_amplitude(const double* amplitudes, int len) {
    int lo = 0, hi = len - 1;
    while (lo < len && amplitudes[lo] == 0.0) ++lo;
    while (hi >= 0 && amplitudes[hi] == 0.0) --hi;
    int trimmed_len = (lo > hi) ? 0 : (hi - lo + 1);

    if (trimmed_len == 0) return 0.0;

    return (5.0 / 3.0) * static_cast<double>(trimmed_len) / static_cast<double>(trimmed_len + 1);
}

DoublePerlinNoise::DoublePerlinNoise(uint64_t base_seed, int first_octave, const double* amplitudes, int len)
    : octave_a_(base_seed, first_octave, amplitudes, len)
    , octave_b_(mix_seed(base_seed, 0xDEADBEEF), first_octave, amplitudes, len)
    , amplitude_(compute_amplitude(amplitudes, len))
{
}

double DoublePerlinNoise::sample(double x, double y, double z) const {
    static constexpr double FREQ_RATIO = 337.0 / 331.0;
    double v = octave_a_.sample(x, y, z)
             + octave_b_.sample(x * FREQ_RATIO, y * FREQ_RATIO, z * FREQ_RATIO);
    return v * amplitude_;
}

} // namespace VoxelEngine
