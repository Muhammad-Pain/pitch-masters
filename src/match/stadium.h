#pragma once
#include "../util.h"

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>

#include <vector>

namespace pm {
namespace stadium {

constexpr float HL = 52.5f;
constexpr float HW = 34.0f;
constexpr float GW = 3.66f;
constexpr float GH = 2.44f;

godot::Ref<godot::StandardMaterial3D> mat(Color c, bool unshaded = false, float emission = 0.0f);
godot::MeshInstance3D *mesh(godot::Node3D *root, const godot::Ref<godot::Mesh> &m, const godot::Ref<godot::Material> &mt, Vector3 pos, Vector3 rot = Vector3());
godot::MeshInstance3D *box(godot::Node3D *root, Vector3 size, Vector3 pos, const godot::Ref<godot::Material> &m, Vector3 rot = Vector3());
// Builds pitch, goals, ad boards (texts), stands, floodlights, sky and light.
void set_excitement(float e);
void release();
void build(godot::Node3D *root, const std::vector<String> &boards, Color home = Color(0.12f, 0.23f, 0.54f), Color away = Color(0.62f, 0.07f, 0.22f));

} // namespace stadium
} // namespace pm
