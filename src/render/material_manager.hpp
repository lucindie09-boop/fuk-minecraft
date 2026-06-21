#ifndef FUK_MINECRAFT_MATERIAL_MANAGER_HPP
#define FUK_MINECRAFT_MATERIAL_MANAGER_HPP
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/color.hpp>

namespace godot {
class ShaderMaterial;
}

namespace VoxelEngine {

class MaterialManager {
public:
    void update_shader_parameters(float sky_intensity, const godot::Color& sky_color);
    godot::Ref<godot::ShaderMaterial> get_material();

private:
    godot::Ref<godot::ShaderMaterial> cached_material;
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_MATERIAL_MANAGER_HPP