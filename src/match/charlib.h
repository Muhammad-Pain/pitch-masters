#pragma once
// Shared 3D footballer assets: skinned model (Quaternius UAL, CC0), recoloured kit mesh and kit shader.

#include "../util.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>

namespace pm {

struct KitColors {
	Color shirt = Color(0.1f, 0.3f, 0.9f), shirt2 = Color(1, 1, 1), shorts = Color(1, 1, 1), socks = Color(0.1f, 0.3f, 0.9f);
	Color boots = Color(0.08f, 0.08f, 0.1f), skin = Color(0.9f, 0.75f, 0.6f), hair = Color(0.1f, 0.08f, 0.06f), gloves = Color(0.9f, 0.9f, 0.9f);
	int pattern = 0; // 0 plain, 1 stripes, 2 hoops, 3 sash, 4 halves
	bool keeper = false;
	int hair_style = 0; // 0 short, 1 buzz, 2 longer, 3 shaved, 4 curly top
	bool beard = false;
};

class CharLib {
public:
	static CharLib &get();
	bool ok = false;
	godot::Ref<godot::PackedScene> scene;
	godot::Ref<godot::ArrayMesh> mesh;
	godot::Ref<godot::Shader> shader;
	int bone_thigh_r = -1, bone_shin_r = -1, bone_thigh_l = -1, bone_shin_l = -1, bone_spine = -1;
	int bone_upper_arm_l = -1, bone_upper_arm_r = -1, bone_head = -1;
	godot::Transform3D spine_rest; // global rest of the upper spine (for the shirt number)

	bool init();
	void shutdown();
	godot::Ref<godot::ShaderMaterial> material(const KitColors &k) const;
	static KitColors team_kit(Color shirt, Color shorts, int pattern_seed);
	static Color skin_tone(int i);
	static Color hair_tone(int i);
};

} // namespace pm
