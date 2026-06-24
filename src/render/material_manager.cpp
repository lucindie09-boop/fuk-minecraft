#include "render/material_manager.hpp"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d_array.hpp>
#include "worldgen/texture_array_generator.hpp"

using namespace godot;
using namespace VoxelEngine;

void MaterialManager::update_shader_parameters(float sky_intensity, const Color& sky_color) {
    Ref<ShaderMaterial> material = get_material();
    if (material.is_valid()) {
        material->set_shader_parameter("sky_light_intensity", sky_intensity);
        material->set_shader_parameter("sky_light_color", sky_color);
    }
}

void MaterialManager::update_fog_parameters(float fog_begin, float fog_end, const Color& fog_color) {
    Ref<ShaderMaterial> material = get_material();
    if (material.is_valid()) {
        material->set_shader_parameter("fog_begin", fog_begin);
        material->set_shader_parameter("fog_end", fog_end);
        material->set_shader_parameter("fog_color", fog_color);
    }
}

void MaterialManager::update_player_light(const Vector3& position, float radius, float intensity, const Color& color) {
Ref<ShaderMaterial> material = get_material();
if (material.is_valid()) {
material->set_shader_parameter("player_light_position", position);
material->set_shader_parameter("player_light_radius", radius);
material->set_shader_parameter("player_light_intensity", intensity);
material->set_shader_parameter("player_light_color", color);
}
}

Ref<ShaderMaterial> MaterialManager::get_material() {
    if (!cached_material.is_valid()) {
        ResourceLoader* loader = ResourceLoader::get_singleton();
        cached_material = loader->load("res://materials/voxel_material.tres");
        if (!cached_material.is_valid()) {
            ERR_PRINT("Failed to load voxel_material.tres");
            return cached_material;
        }
        Ref<Texture2DArray> texture_array = TextureArrayGenerator::get_instance().get_texture_array();
        if (texture_array.is_valid()) {
            cached_material->set_shader_parameter("texture_array", texture_array);
        }
    }
    return cached_material;
}
