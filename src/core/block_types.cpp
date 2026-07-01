#include "core/block_types.hpp"

namespace VoxelEngine {

void BlockRegistry::initialize_default_blocks() noexcept {
    // Helper for solid, opaque, AO-generating blocks with all 6 faces visible.
    const auto solid = [&](const char* name) {
        register_block({
            0, name,
            BlockProperty::Solid | BlockProperty::Opaque,
             {true, true, true, true, true, true},
            {0, 0, 0, 0, 0, 0},
            0,
            0, 0, 0,
            LightEmissionPattern::Diamond
        });
    };

    // 0: Air
    register_block({
        0, "air",
        BlockProperty::Transparent | BlockProperty::NoOcclusion,
        {false, false, false, false, false, false},
        {0, 0, 0, 0, 0, 0},
        0,
        0, 0, 0
    });

    // 1-4: Basic solids
    solid("stone");
    solid("dirt");

    // 3: Grass (bottom face hidden by dirt underneath)
    register_block({
        0, "grass",
        BlockProperty::Solid | BlockProperty::Opaque,
        {true, true, true, false, true, true},
        {0, 0, 0, 0, 0, 0},
        0,
        0, 0, 0
    });

    solid("sand");

    // 5: Surface water (lowered top face)
    register_block({
        0, "surface_water",
        BlockProperty::Liquid | BlockProperty::Transparent,
        {true, true, true, false, true, true},
        {0, 0, 0, 0, 0, 0},
        0,
        0, 0, 0,
        LightEmissionPattern::Diamond,
        0.12f
    });

    // 6: Water (lowered top face)
    register_block({
        0, "water",
        BlockProperty::Liquid | BlockProperty::Transparent,
        {true, true, true, true, true, true},
        {0, 0, 0, 0, 0, 0},
        0,
        0, 0, 0,
        LightEmissionPattern::Diamond,
        0.12f
    });

    // 7-8: Wood & Leaves
    solid("wood");

    register_block({
        0, "leaves",
        BlockProperty::Solid | BlockProperty::Transparent,
        {true, true, true, true, true, true},
        {0, 0, 0, 0, 0, 0},
        0,
        0, 0, 0
    });

    // 9: Bedrock
    solid("bedrock");

    // 10: Mud (lowered top face)
    register_block({
        0, "mud",
        BlockProperty::Solid | BlockProperty::Opaque,
        {true, true, true, true, true, true},
        {0, 0, 0, 0, 0, 0},
        0,
        0, 0, 0,
        LightEmissionPattern::Diamond,
        0.0625f
    });

    // 11: Wet sand (lowered top face)
    register_block({
        0, "wet_sand",
        BlockProperty::Solid | BlockProperty::Opaque,
        {true, true, true, true, true, true},
        {0, 0, 0, 0, 0, 0},
        0,
        0, 0, 0,
        LightEmissionPattern::Diamond,
        0.0625f
    });

    // 12-13: Full variants (no offset)
    solid("mud_full");
    solid("wet_sand_full");

    // 14-17: Light blocks (emissive)
    register_block({
        0, "light_block",
        BlockProperty::Solid | BlockProperty::Opaque | BlockProperty::Emissive,
        {true, true, true, true, true, true},
        {0, 0, 0, 0, 0, 0},
        15,
        15, 15, 15,
        LightEmissionPattern::Diamond
    });

    register_block({
        0, "light_red",
        BlockProperty::Solid | BlockProperty::Opaque | BlockProperty::Emissive,
        {true, true, true, true, true, true},
        {0, 0, 0, 0, 0, 0},
        15,
        15, 0, 0,
        LightEmissionPattern::Diamond
    });

    register_block({
        0, "light_green",
        BlockProperty::Solid | BlockProperty::Opaque | BlockProperty::Emissive,
        {true, true, true, true, true, true},
        {0, 0, 0, 0, 0, 0},
        15,
        0, 15, 0,
        LightEmissionPattern::Diamond
    });

    register_block({
        0, "light_blue",
        BlockProperty::Solid | BlockProperty::Opaque | BlockProperty::Emissive,
        {true, true, true, true, true, true},
        {0, 0, 0, 0, 0, 0},
        15,
        0, 0, 15,
        LightEmissionPattern::Diamond
    });
}

} // namespace VoxelEngine
