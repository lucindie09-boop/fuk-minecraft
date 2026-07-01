#ifndef FUK_MINECRAFT_TEXTURE_ARRAY_GENERATOR_HPP
#define FUK_MINECRAFT_TEXTURE_ARRAY_GENERATOR_HPP
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/texture2d_array.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <map>
#include <set>
#include <array>
#include <cstddef>
#include "core/block_types.hpp"

namespace VoxelEngine {

class TextureArrayGenerator {
private:
    // Face slot order used by populate_block_registry():
    // 0=right(+X), 1=left(-X), 2=top(+Y), 3=bottom(-Y), 4=front(+Z), 5=back(-Z)
    struct BlockTextureMapping {
        std::array<godot::String, 6> faces;
    };

    std::map<godot::String, BlockTextureMapping> block_texture_mappings;
    std::map<godot::String, godot::String> texture_path_cache;
    size_t last_registry_count = 0;

    // Singleton-global texture state (replaces file-level globals)
    static inline godot::Ref<godot::Texture2DArray> s_global_texture_array;
    static inline std::map<godot::String, int> s_global_texture_name_to_index;
    static inline bool s_global_texture_initialized = false;

    // -------------------------------------------------------------------------
    // Internal Helpers
    // -------------------------------------------------------------------------
    void initialize_texture_mappings();

    // Cached variant of the old get_safe_texture_path — filesystem check only once per name.
    [[nodiscard]] godot::String get_safe_texture_path(const godot::String& texture_name);

public:
    TextureArrayGenerator();
    ~TextureArrayGenerator() = default;

    // Non-copyable, non-movable singleton
    TextureArrayGenerator(const TextureArrayGenerator&) = delete;
    TextureArrayGenerator& operator=(const TextureArrayGenerator&) = delete;

    [[nodiscard]] static TextureArrayGenerator& get_instance();

    // Build the GPU texture array from all registered block face textures.
    // NOTE: not nodiscard — called internally for side-effects.
    godot::Ref<godot::Texture2DArray> generate_texture_array();

    // Write texture layer indices into every block in the registry.
    // Cheap no-op if no new blocks have been added since the last call.
    void populate_block_registry();

    // Force full rebuild of texture array and block indices.
    void force_regenerate();

    // Get (and lazily initialize) the global texture array.
    [[nodiscard]] godot::Ref<godot::Texture2DArray> get_texture_array();

    // Look up a texture's layer index inside the array.
    [[nodiscard]] int get_texture_index(const godot::String& texture_name);

    // Look up a specific face's texture index for a block.
    [[nodiscard]] int get_block_texture_index(const godot::String& block_name, const godot::String& face);

    // Release global Godot references before DLL unload.
    // Also resets internal caches so the next load starts fresh.
    static void cleanup() {
        s_global_texture_array.unref();
        s_global_texture_name_to_index.clear();
        s_global_texture_initialized = false;
        get_instance().last_registry_count = 0;
        get_instance().texture_path_cache.clear();
    }
};

// -----------------------------------------------------------------------------
// Inline Implementation
// -----------------------------------------------------------------------------
inline TextureArrayGenerator::TextureArrayGenerator() {
    initialize_texture_mappings();
}

inline TextureArrayGenerator& TextureArrayGenerator::get_instance() {
    static TextureArrayGenerator instance;
    return instance;
}

inline void TextureArrayGenerator::initialize_texture_mappings() {
    auto add = [this](const godot::String& name,
                      const godot::String& right, const godot::String& left,
                      const godot::String& top, const godot::String& bottom,
                      const godot::String& front, const godot::String& back) {
        block_texture_mappings[name] = BlockTextureMapping{{right, left, top, bottom, front, back}};
    };

    add("stone",   "stone", "stone", "stone", "stone", "stone", "stone");
    add("dirt",    "dirt", "dirt", "dirt", "dirt", "dirt", "dirt");
    add("sand",    "sand", "sand", "sand", "sand", "sand", "sand");
    add("bedrock", "bedrock", "bedrock", "bedrock", "bedrock", "bedrock", "bedrock");
    add("grass",   "grass_side", "grass_side", "grass_top", "dirt", "grass_side", "grass_side");

    // Fallback textures handled by get_safe_texture_path until .png files are added
    add("wood",          "wood_side", "wood_side", "wood_top", "wood_top", "wood_side", "wood_side");
    add("leaves",        "leaves", "leaves", "leaves", "leaves", "leaves", "leaves");
    add("surface_water", "water", "water", "water", "water", "water", "water");
    add("water",         "water", "water", "water", "water", "water", "water");
    add("mud",           "mud", "mud", "mud", "mud", "mud", "mud");
    add("wet_sand",      "wet_sand", "wet_sand", "wet_sand", "wet_sand", "wet_sand", "wet_sand");
    add("mud_full",      "mud", "mud", "mud", "mud", "mud", "mud");
    add("wet_sand_full", "wet_sand", "wet_sand", "wet_sand", "wet_sand", "wet_sand", "wet_sand");
    add("snow",   "snow", "snow", "snow", "snow", "snow", "snow");
    add("gravel", "gravel", "gravel", "gravel", "gravel", "gravel", "gravel");
    add("cactus", "cactus", "cactus", "cactus", "cactus", "cactus", "cactus");
}

inline godot::String TextureArrayGenerator::get_safe_texture_path(const godot::String& texture_name) {
    auto it = texture_path_cache.find(texture_name);
    if (it != texture_path_cache.end()) {
        return it->second;
    }

    godot::String primary_path = "res://textures/blocks/" + texture_name + ".png";
    godot::String result;
    if (godot::FileAccess::file_exists(primary_path)) {
        result = primary_path;
    } else {
        result = "res://textures/blocks/stone.png";
        WARN_PRINT("Texture not found: " + texture_name + ", falling back to stone.png");
    }
    texture_path_cache.emplace(texture_name, result);
    return result;
}

inline godot::Ref<godot::Texture2DArray> TextureArrayGenerator::generate_texture_array() {
    std::set<godot::String> unique_textures;
    for (const auto& [block_name, mapping] : block_texture_mappings) {
        for (const auto& face : mapping.faces) {
            unique_textures.insert(face);
        }
    }

    godot::PackedStringArray texture_paths;
    for (const godot::String& texture_name : unique_textures) {
        texture_paths.append(get_safe_texture_path(texture_name));
    }

    s_global_texture_array.instantiate();
    godot::ResourceLoader* loader = godot::ResourceLoader::get_singleton();

    if (texture_paths.size() == 0) {
        ERR_PRINT("No block textures available to load.");
        return s_global_texture_array;
    }

    godot::Ref<godot::Texture2D> base_texture = loader->load(texture_paths[0]);
    if (!base_texture.is_valid()) {
        ERR_PRINT("Failed to load base layout reference texture: " + texture_paths[0]);
        return s_global_texture_array;
    }

    godot::Ref<godot::Image> base_image = base_texture->get_image();
    const int width  = base_image->get_width();
    const int height = base_image->get_height();

    godot::Array textures;
    s_global_texture_name_to_index.clear();

    for (int i = 0; i < texture_paths.size(); ++i) {
        godot::Ref<godot::Texture2D> texture = loader->load(texture_paths[i]);
        if (!texture.is_valid()) {
            continue;
        }

        godot::Ref<godot::Image> image = texture->get_image();
        if (!image.is_valid()) {
            continue;
        }

        if (image->get_width() != width || image->get_height() != height) {
            image->resize(width, height, godot::Image::INTERPOLATE_NEAREST);
        }

        image->generate_mipmaps();

        const int layer_index = textures.size();
        textures.append(image);

        godot::String file_name = texture_paths[i].get_file().get_basename();
        s_global_texture_name_to_index[file_name] = layer_index;
    }

    if (textures.size() > 0) {
        s_global_texture_array->create_from_images(textures);
        godot::print_line("Generated texture array with " + godot::String::num_int64(s_global_texture_array->get_layers()) + " layers (with mipmaps)");
    }

    return s_global_texture_array;
}

inline void TextureArrayGenerator::populate_block_registry() {
    BlockRegistry& registry = BlockRegistry::get_instance();
    const size_t block_count = registry.get_count();

    // Early-out: nothing changed since last populate
    if (block_count == last_registry_count) {
        return;
    }
    last_registry_count = block_count;

    for (size_t i = 0; i < block_count; ++i) {
        BlockType* block = registry.get_block_mutable(static_cast<BlockID>(i));
        if (!block || !block->name) {
            continue;
        }

        auto it = block_texture_mappings.find(block->name);
        if (it == block_texture_mappings.end()) {
            continue;
        }

        // FIX: use get_texture_index() so missing textures fall back to stone correctly.
        const auto& faces = it->second.faces;
        for (int f = 0; f < 6; ++f) {
            block->texture_indices[f] = get_texture_index(faces[f]);
        }
    }
}

inline void TextureArrayGenerator::force_regenerate() {
    s_global_texture_array.unref();
    s_global_texture_initialized = false;
    last_registry_count = 0; // Force repopulate
    generate_texture_array();
    populate_block_registry();
    s_global_texture_initialized = true;
}

// FIX: restore lazy initialization so callers don't need to manually generate.
inline godot::Ref<godot::Texture2DArray> TextureArrayGenerator::get_texture_array() {
    if (!s_global_texture_array.is_valid() || !s_global_texture_initialized) {
        generate_texture_array();
        populate_block_registry();
        s_global_texture_initialized = true;
    } else {
        populate_block_registry();
    }
    return s_global_texture_array;
}

inline int TextureArrayGenerator::get_texture_index(const godot::String& texture_name) {
    auto it = s_global_texture_name_to_index.find(texture_name);
    if (it != s_global_texture_name_to_index.end()) {
        return it->second;
    }

    // Fallback: if the raw name wasn't loaded, resolve through the safe path
    godot::String safe_fallback = get_safe_texture_path(texture_name).get_file().get_basename();
    it = s_global_texture_name_to_index.find(safe_fallback);
    if (it != s_global_texture_name_to_index.end()) {
        return it->second;
    }

    return 0;
}

inline int TextureArrayGenerator::get_block_texture_index(const godot::String& block_name, const godot::String& face) {
    auto it = block_texture_mappings.find(block_name);
    if (it == block_texture_mappings.end()) {
        return 0;
    }

    const auto& faces = it->second.faces;
    if (face == "right")  return get_texture_index(faces[0]);
    if (face == "left")   return get_texture_index(faces[1]);
    if (face == "top")    return get_texture_index(faces[2]);
    if (face == "bottom") return get_texture_index(faces[3]);
    if (face == "front")  return get_texture_index(faces[4]);
    if (face == "back")   return get_texture_index(faces[5]);
    return get_texture_index(faces[2]); // default to top
}

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_TEXTURE_ARRAY_GENERATOR_HPP