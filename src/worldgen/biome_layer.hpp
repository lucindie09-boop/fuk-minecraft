#ifndef FUK_MINECRAFT_BIOME_LAYER_HPP
#define FUK_MINECRAFT_BIOME_LAYER_HPP
#include <cstdint>
#include <memory>

// cubiomes forward declarations
struct LayerStack;

namespace VoxelEngine {

// BiomeLayer — wraps cubiomes LayerStack + genArea for MC 1.7 biome generation.
class BiomeLayer {
public:
    explicit BiomeLayer(uint64_t world_seed);
    ~BiomeLayer();

    BiomeLayer(const BiomeLayer&) = delete;
    BiomeLayer& operator=(const BiomeLayer&) = delete;

    // 10×10 quarter-res map at (chunk_x*4-2, chunk_z*4-2) for density blending.
    void get_terrain_biomes(int chunk_x, int chunk_z, int out[10][10]) const;

    // 16×16 full-res map at (chunk_x*16, chunk_z*16) for surface blocks.
    void get_surface_biomes(int chunk_x, int chunk_z, int out[16][16]) const;

private:
    std::unique_ptr<LayerStack> stack_;
    bool initialized_ = false;
};

} // namespace VoxelEngine
#endif // FUK_MINECRAFT_BIOME_LAYER_HPP
