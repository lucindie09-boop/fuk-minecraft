#ifndef FUK_MINECRAFT_PERLIN_NOISE_HPP
#define FUK_MINECRAFT_PERLIN_NOISE_HPP
#include <cstdint>
#include <cmath>
#include <array>
#include <vector>

namespace VoxelEngine {

// -----------------------------------------------------------------------------
// RandomSource — Fork A: well-distributed seeded PRNG (not Java-compatible)
// Used internally by ImprovedNoise for permutation shuffles and offsets.
// Each noise primitive takes a uint64_t seed and creates its own RandomSource,
// avoiding reference-passing lifetime issues.
// -----------------------------------------------------------------------------
class RandomSource {
    uint64_t state_;

    static uint64_t splitmix64(uint64_t& s) {
        uint64_t z = (s += 0x9e3779b97f4a7c15);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
        z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
        return z ^ (z >> 31);
    }

public:
    explicit RandomSource(uint64_t seed) : state_(splitmix64(seed)) {}

    uint64_t next_long() {
        return splitmix64(state_);
    }

    int next_int(int bound) {
        return static_cast<int>(next_long() % static_cast<uint64_t>(bound));
    }

    double next_double() {
        return static_cast<double>(next_long() & 0x1FFFFFFFFFFFFF)
             / static_cast<double>(1ULL << 53);
    }
};

// -----------------------------------------------------------------------------
// ImprovedNoise — Ken Perlin's 2002 Improved Noise (3D)
//
// Uses vanilla's exact algorithm: quintic fade curve, forward Fisher-Yates
// shuffle, and the 16-entry gradient selection keyed on low 4 bits of the
// permutation value. The per-instance random offsets (a, b, c) in [0, 256)
// decorrelate octaves even when frequencies coincide.
// -----------------------------------------------------------------------------
class ImprovedNoise {
    double a_, b_, c_;
    std::array<uint8_t, 257> perm_; // perm[256] = perm[0] for wraparound

    static double indexed_lerp(int index, double x, double y, double z) {
        index = index & 15;
        switch (index) {
            case 0:  return  x + y;
            case 1:  return -x + y;
            case 2:  return  x - y;
            case 3:  return -x - y;
            case 4:  return  x + z;
            case 5:  return -x + z;
            case 6:  return  x - z;
            case 7:  return -x - z;
            case 8:  return  y + z;
            case 9:  return -y + z;
            case 10: return  y - z;
            case 11: return -y - z;
            case 12: return  y + x;
            case 13: return -y + z;
            case 14: return  y - x;
            case 15: return -y - z;
        }
        return 0;
    }

    static inline double fade(double t) noexcept {
        return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
    }

    static inline double lerp(double a, double b, double t) noexcept {
        return a + t * (b - a);
    }

public:
    ImprovedNoise(); // default: identity/zero noise (for array storage)
    explicit ImprovedNoise(uint64_t seed);

    double sample(double x, double y, double z) const;

    double get_offset_a() const { return a_; }
    double get_offset_b() const { return b_; }
    double get_offset_c() const { return c_; }
    uint8_t perm_hash(int i) const { return perm_[i & 255]; }
};

// -----------------------------------------------------------------------------
// OctaveNoise — a stack of ImprovedNoise at related frequencies, summed.
//
// Constructed from an explicit amplitude array (like vanilla's xOctaveInit).
// lacunarity_i = 2^(first_octave + i), persistence halves each step,
// amplitude_i = amplitudes[i] * persistence_i.
//
// Uses a fixed-size array (max 16 octaves) to avoid std::vector copy/move
// issues with ImprovedNoise objects during reallocation.
// -----------------------------------------------------------------------------
class OctaveNoise {
    struct Octave {
        ImprovedNoise noise;
        double amplitude;
        double lacunarity;
    };
    static constexpr int MAX_OCTAVES = 16;
    std::array<Octave, MAX_OCTAVES> octaves_;
    int count_ = 0;

public:
    OctaveNoise(uint64_t base_seed, int first_octave, const double* amplitudes, int len);

    double sample(double x, double y, double z) const;
};

// -----------------------------------------------------------------------------
// DoublePerlinNoise — two independently-seeded OctaveNoise stacks, vanilla's combine rule.
//
// Both stacks use the same amplitude array and octave range, but each gets its
// own independently-seeded set of ImprovedNoise instances. The second stack
// samples at a frequency scaled by 337/331 (an exact vanilla constant).
// -----------------------------------------------------------------------------
class DoublePerlinNoise {
    OctaveNoise octave_a_, octave_b_;
    double amplitude_;

public:
    DoublePerlinNoise(uint64_t base_seed, int first_octave, const double* amplitudes, int len);

    double sample(double x, double y, double z) const;

    static double compute_amplitude(const double* amplitudes, int len);
};

// -----------------------------------------------------------------------------
// JavaOctaveNoise — octave noise seeded via Java LCG chain.
//
// Used in the 1.7-style terrain generator. The constructor advances a
// uint64_t Java LCG state to seed each octave, matching the vanilla
// NoiseGeneratorOctaves::__init__ seed chain convention.
// -----------------------------------------------------------------------------
class JavaOctaveNoise {
    std::vector<ImprovedNoise> octaves_;

public:
    JavaOctaveNoise() = default;

    // Advances seed_state once per octave via java_next_long.
    // first_octave controls lacunarity = 2^{first_octave + i}.
    // num_octaves = number of ImprovedNoise instances to create.
    JavaOctaveNoise(uint64_t& seed_state, int first_octave, int num_octaves);

    // Fills a flat double array matching vanilla generateNoiseOctaves.
    // Layout: out[dz * xSz * ySz + dy * xSz + dx]
    void generate_noise_3d(double* out, int xOff, int yOff, int zOff,
                           int xSz, int ySz, int zSz,
                           double xScale, double yScale, double zScale) const;

    int count() const { return static_cast<int>(octaves_.size()); }
    bool empty() const { return octaves_.empty(); }
};

// -----------------------------------------------------------------------------
// JavaPerlinNoise — single-noise chain for surface/world-gen detail.
//
// Used for surfaceNoise in the 1.7-style generator: stores a single
// ImprovedNoise and samples it at multiple octaves scaled by persistence.
// -----------------------------------------------------------------------------
class JavaPerlinNoise {
    ImprovedNoise noise_;
    int octaves_ = 0;
    double persistence_ = 0.5;

public:
    JavaPerlinNoise() = default;

    // Advances seed_state once via java_next_long, then uses that seed for
    // the underlying ImprovedNoise. octaves controls the number of octaves
    // in sample().
    JavaPerlinNoise(uint64_t& seed_state, int octaves);

    double sample(double x, double y, double z) const;
    double sample(double x, double z) const;
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_PERLIN_NOISE_HPP
