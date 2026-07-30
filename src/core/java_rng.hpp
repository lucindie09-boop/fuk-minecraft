#ifndef FUK_MINECRAFT_JAVA_RNG_HPP
#define FUK_MINECRAFT_JAVA_RNG_HPP
#include <cstdint>

namespace VoxelEngine {

// Java LCG — matches java.util.Random exactly.
// state is a 48-bit seed stored in the low 48 bits of a uint64_t.

inline uint64_t java_set_seed(uint64_t& state, uint64_t seed) {
    state = (seed ^ 0x5DEECE66DULL) & ((1ULL << 48) - 1);
    return state;
}

inline int32_t java_next(uint64_t& state, int bits) {
    state = (state * 0x5DEECE66DULL + 0xBULL) & ((1ULL << 48) - 1);
    return static_cast<int32_t>(state >> (48 - bits));
}

inline int64_t java_next_long(uint64_t& state) {
    return (static_cast<int64_t>(java_next(state, 32)) << 32) + static_cast<int64_t>(java_next(state, 32));
}

inline double java_next_double(uint64_t& state) {
    return (static_cast<double>((static_cast<int64_t>(java_next(state, 26)) << 27) + java_next(state, 27)))
         / static_cast<double>(1LL << 53);
}

inline int32_t java_next_int(uint64_t& state, int32_t bound) {
    if ((bound & -bound) == bound) {
        return static_cast<int32_t>((static_cast<int64_t>(bound) * static_cast<int64_t>(java_next(state, 31))) >> 31);
    }
    int32_t bits, val;
    do {
        bits = java_next(state, 31);
        val = bits % bound;
    } while (bits - val + (bound - 1) < 0);
    return val;
}

} // namespace VoxelEngine
#endif // FUK_MINECRAFT_JAVA_RNG_HPP
