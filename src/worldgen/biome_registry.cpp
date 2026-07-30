#include "worldgen/biome_registry.hpp"

namespace VoxelEngine {

// Vanilla 1.7 biome terrain parameters, indexed by biome ID.
// Only biomes with distinct terrain properties are listed; others default to {0, 0}.
// Source: MC 1.7 source (GenLayer / BiomeGenBase) and TerrainFinderCpp.
const BiomeTerrainParams BIOME_TERRAIN_TABLE[256] = {
    /*  0 */ {-1.0f, 0.1f},     // Ocean
    /*  1 */ {0.125f, 0.05f},   // Plains
    /*  2 */ {0.125f, 0.05f},   // Desert
    /*  3 */ {1.0f, 0.5f},      // Extreme Hills
    /*  4 */ {0.1f, 0.2f},      // Forest
    /*  5 */ {0.2f, 0.2f},      // Taiga
    /*  6 */ {-0.2f, 0.1f},     // Swampland
    /*  7 */ {-0.5f, 0.0f},     // River
    /*  8 */ {0.0f, 0.1f},      // Hell (Nether)
    /*  9 */ {0.0f, 0.1f},      // The End
    /* 10 */ {-1.0f, 0.1f},     // FrozenOcean
    /* 11 */ {-0.5f, 0.0f},     // FrozenRiver
    /* 12 */ {0.125f, 0.05f},   // Ice Plains
    /* 13 */ {1.0f, 0.5f},      // Ice Mountains
    /* 14 */ {0.2f, 0.3f},      // Mushroom Island
    /* 15 */ {0.0f, 0.025f},    // Mushroom Island Shore
    /* 16 */ {0.0f, 0.025f},    // Beach
    /* 17 */ {0.125f, 0.05f},   // Desert Hills
    /* 18 */ {0.1f, 0.2f},      // Forest Hills
    /* 19 */ {0.2f, 0.2f},      // Taiga Hills
    /* 20 */ {1.0f, 0.5f},      // Extreme Hills Edge
    /* 21 */ {0.1f, 0.2f},      // Jungle
    /* 22 */ {0.1f, 0.2f},      // Jungle Hills
    /* 23 */ {0.1f, 0.2f},      // Jungle Edge
    /* 24 */ {-1.8f, 0.1f},     // Deep Ocean
    /* 25 */ {0.1f, 0.8f},      // Stone Beach
    /* 26 */ {0.0f, 0.025f},    // Cold Beach
    /* 27 */ {0.1f, 0.2f},      // Birch Forest
    /* 28 */ {0.1f, 0.2f},      // Birch Forest Hills
    /* 29 */ {0.1f, 0.2f},      // Roofed Forest
    /* 30 */ {0.2f, 0.2f},      // Cold Taiga
    /* 31 */ {0.2f, 0.2f},      // Cold Taiga Hills
    /* 32 */ {0.3f, 0.4f},      // Mega Taiga
    /* 33 */ {0.3f, 0.4f},      // Mega Taiga Hills
    /* 34 */ {1.0f, 0.5f},      // Extreme Hills+
    /* 35 */ {0.125f, 0.05f},   // Savanna
    /* 36 */ {0.0f, 0.025f},    // Savanna Plateau
    /* 37 */ {1.5f, 0.025f},    // Mesa
    /* 38 */ {1.5f, 0.025f},    // Mesa Plateau F
    /* 39 */ {1.5f, 0.025f},    // Mesa Plateau
    /* 40+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /* 50+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /* 60+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /* 70+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /* 80+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /* 90+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /*100+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /*110+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /*120+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /*130+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /*140+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /*150+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /*160+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /*170+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /*180+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /*190+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /*200+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /*210+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /*220+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /*230+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /*240+ */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /*250+ */ {}, {}, {}, {}, {}, {},
};

} // namespace VoxelEngine
