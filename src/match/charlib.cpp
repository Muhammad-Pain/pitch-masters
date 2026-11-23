#include "charlib.h"

#include <godot_cpp/classes/importer_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/skeleton3d.hpp>
#include <godot_cpp/classes/skin.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/typed_array.hpp>

using namespace godot;

namespace pm {

static const char *MODEL_PATH = "res://assets/characters/AnimationLibrary_Godot_Standard.gltf";

enum Region { RG_SHIRT = 0, RG_SHORTS = 1, RG_SOCKS = 2, RG_BOOTS = 3, RG_SKIN = 4, RG_HAIR = 5, RG_TRIM = 6, RG_HANDS = 7 };

CharLib &CharLib::get() {
	static CharLib c;
	return c;
}

void CharLib::shutdown() {
	scene.unref();
	mesh.unref();
	shader.unref();
	ok = false;
}

static int region_for(const String &bone, const Vector3 &v) {
	float ax = std::fabs(v.x);
	if (bone.contains("head")) return (v.y > 1.71f || (v.y > 1.62f && v.z < -0.05f)) ? RG_HAIR : RG_SKIN;
	if (bone.contains("neck")) return v.y < 1.47f ? RG_TRIM : RG_SKIN;
	if (bone.contains("upper_arm")) {
		if (ax > 0.37f) return RG_SKIN;
		if (ax > 0.33f) return RG_TRIM;
		return RG_SHIRT;
	}
	if (bone.contains("forearm")) return RG_SKIN;
	if (bone.contains("hand") || bone.contains("f_") || bone.contains("thumb")) return RG_HANDS;
	if (bone.contains("spine") || bone.contains("shoulder")) return RG_SHIRT;
	if (bone.contains("hips")) return v.y > 0.97f ? RG_SHIRT : RG_SHORTS;
	if (bone.contains("thigh")) return v.y > 0.63f ? RG_SHORTS : RG_SKIN;
	if (bone.contains("shin")) return v.y < 0.47f ? RG_SOCKS : RG_SKIN;
	if (bone.contains("foot") || bone.contains("toe")) return RG_BOOTS;
	return RG_SHORTS;
}

bool CharLib::init() {
	if (ok) return true;
	if (!ResourceLoader::get_singleton()->exists(MODEL_PATH)) return false;
	scene = ResourceLoader::get_singleton()->load(MODEL_PATH);
	if (scene.is_null()) return false;
	Node *inst = scene->instantiate();
	if (!inst) return false;
	MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(inst->find_child("Mannequin", true, false));
	Skeleton3D *sk = Object::cast_to<Skeleton3D>(inst->find_child("Skeleton3D", true, false));
	if (!mi || !sk || mi->get_mesh().is_null()) {
		inst->queue_free();
		return false;
	}
	Ref<Mesh> src = mi->get_mesh();
	Ref<Skin> skin = mi->get_skin();
	bone_thigh_r = sk->find_bone("DEF-thigh.R");
	bone_shin_r = sk->find_bone("DEF-shin.R");
	bone_thigh_l = sk->find_bone("DEF-thigh.L");
	bone_shin_l = sk->find_bone("DEF-shin.L");
	bone_spine = sk->find_bone("DEF-spine.003");
	bone_upper_arm_l = sk->find_bone("DEF-upper_arm.L");
	bone_upper_arm_r = sk->find_bone("DEF-upper_arm.R");
	bone_head = sk->find_bone("DEF-head");
	if (bone_spine >= 0) spine_rest = sk->get_bone_global_rest(bone_spine);
	// bind index -> bone name
	std::vector<String> bind_names;
	int nb = skin.is_valid() ? skin->get_bind_count() : sk->get_bone_count();
	for (int b = 0; b < nb; b++) {
		String n;
		if (skin.is_valid()) {
			n = skin->get_bind_name(b);
			if (n.is_empty()) {
				int bb = skin->get_bind_bone(b);
				if (bb >= 0 && bb < sk->get_bone_count()) n = sk->get_bone_name(bb);
			}
		} else {
			n = sk->get_bone_name(b);
		}
		bind_names.push_back(n);
	}
	Ref<ImporterMesh> im;
	im.instantiate();
	for (int s = 0; s < src->get_surface_count(); s++) {
		Array arr = src->surface_get_arrays(s);
		PackedVector3Array verts = arr[Mesh::ARRAY_VERTEX];
		PackedInt32Array bones = arr[Mesh::ARRAY_BONES];
		PackedFloat32Array weights = arr[Mesh::ARRAY_WEIGHTS];
		int n = (int)verts.size();
		int per = n > 0 ? (int)bones.size() / n : 4;
		PackedColorArray colors;
		colors.resize(n);
		PackedVector2Array uv2;
		uv2.resize(n);
		for (int i = 0; i < n; i++) {
			int best = 0;
			float bw = -1.f;
			for (int k = 0; k < per; k++) {
				float w = weights.size() > i * per + k ? weights[i * per + k] : 0.f;
				if (w > bw) {
					bw = w;
					best = bones[i * per + k];
				}
			}
			String bn = best >= 0 && best < (int)bind_names.size() ? bind_names[best] : String();
			Vector3 v = verts[i];
			int rg = region_for(bn, v);
			colors.set(i, Color((rg + 0.5f) / 8.f, clampf(v.z + 0.5f, 0.f, 1.f), 0.f, 1.f));
			uv2.set(i, Vector2(v.x, v.y));
		}
		arr[Mesh::ARRAY_COLOR] = colors;
		arr[Mesh::ARRAY_TEX_UV2] = uv2;
		uint64_t flags = per == 8 ? (uint64_t)Mesh::ARRAY_FLAG_USE_8_BONE_WEIGHTS : 0;
		im->add_surface(Mesh::PRIMITIVE_TRIANGLES, arr, TypedArray<Array>(), Dictionary(), Ref<Material>(), String("s") + I(s), flags);
	}
	im->generate_lods(60.f, 60.f, Array());
	mesh = im->get_mesh();
	inst->queue_free();
	if (mesh.is_null()) return false;
	shader.instantiate();
	shader->set_code(R"(shader_type spatial;
render_mode cull_back;
uniform vec3 shirt : source_color = vec3(0.1, 0.3, 0.9);
uniform vec3 shirt2 : source_color = vec3(1.0);
uniform vec3 shorts : source_color = vec3(1.0);
uniform vec3 socks : source_color = vec3(0.1, 0.3, 0.9);
uniform vec3 boots : source_color = vec3(0.08);
uniform vec3 skin : source_color = vec3(0.9, 0.75, 0.6);
uniform vec3 hair : source_color = vec3(0.1, 0.08, 0.06);
uniform vec3 gloves : source_color = vec3(0.9);
uniform int pattern = 0;
uniform bool keeper = false;
varying flat int region;
varying vec3 bp;
void vertex() {
	region = int(COLOR.r * 8.0);
	bp = vec3(UV2.x, UV2.y, COLOR.g - 0.5);
}
void fragment() {
	vec3 c = skin;
	float rough = 0.72;
	float spec = 0.35;
	if (region == 0) {
		c = shirt;
		float s = 0.0;
		if (pattern == 1) s = step(0.5, fract(bp.x * 7.0 + 0.25));
		else if (pattern == 2) s = step(0.5, fract(bp.y * 6.0));
		else if (pattern == 3) s = step(abs(bp.x + (bp.y - 1.2) * 0.9), 0.07);
		else if (pattern == 4) s = step(0.0, bp.x);
		c = mix(c, shirt2, s);
		rough = 0.6;
	} else if (region == 1) { c = shorts; rough = 0.62; }
	else if (region == 2) { c = socks; rough = 0.8; }
	else if (region == 3) { c = boots; rough = 0.25; spec = 0.6; }
	else if (region == 5) { c = hair; rough = 0.9; spec = 0.1; }
	else if (region == 6) { c = shirt2; rough = 0.6; }
	else if (region == 7) { c = keeper ? gloves : skin; }
	ALBEDO = c;
	ROUGHNESS = rough;
	SPECULAR = spec;
	RIM = 0.25;
	RIM_TINT = 0.4;
})");
	ok = true;
	return true;
}

Ref<ShaderMaterial> CharLib::material(const KitColors &k) const {
	Ref<ShaderMaterial> m;
	m.instantiate();
	m->set_shader(shader);
	m->set_shader_parameter("shirt", k.shirt);
	m->set_shader_parameter("shirt2", k.shirt2);
	m->set_shader_parameter("shorts", k.shorts);
	m->set_shader_parameter("socks", k.socks);
	m->set_shader_parameter("boots", k.boots);
	m->set_shader_parameter("skin", k.skin);
	m->set_shader_parameter("hair", k.hair);
	m->set_shader_parameter("gloves", k.gloves);
	m->set_shader_parameter("pattern", k.pattern);
	m->set_shader_parameter("keeper", k.keeper);
	return m;
}

KitColors CharLib::team_kit(Color shirt, Color shorts, int seed) {
	KitColors k;
	k.shirt = shirt;
	k.shorts = shorts;
	float lum = shirt.get_luminance();
	k.shirt2 = shorts.is_equal_approx(shirt) ? (lum > 0.5f ? Color(0.08f, 0.1f, 0.18f) : Color(1, 1, 1)) : shorts;
	if (std::fabs(k.shirt2.get_luminance() - lum) < 0.12f) k.shirt2 = lum > 0.5f ? Color(0.08f, 0.1f, 0.18f) : Color(1, 1, 1);
	k.socks = shirt;
	Rng r((uint64_t)(seed * 977 + 13));
	k.pattern = r.irange(0, 9) < 5 ? 0 : r.irange(1, 4);
	static const Color boots[4] = { Color(0.07f, 0.07f, 0.09f), Color(0.95f, 0.95f, 0.95f), Color(0.9f, 0.2f, 0.1f), Color(0.1f, 0.55f, 0.95f) };
	k.boots = boots[r.irange(0, 3)];
	return k;
}

Color CharLib::skin_tone(int i) {
	static const Color t[6] = { Color(0.95f, 0.8f, 0.68f), Color(0.88f, 0.69f, 0.53f), Color(0.78f, 0.57f, 0.41f), Color(0.62f, 0.43f, 0.3f),
		Color(0.45f, 0.3f, 0.21f), Color(0.33f, 0.22f, 0.16f) };
	return t[((i % 6) + 6) % 6];
}

Color CharLib::hair_tone(int i) {
	static const Color t[5] = { Color(0.06f, 0.05f, 0.04f), Color(0.2f, 0.12f, 0.07f), Color(0.42f, 0.28f, 0.14f), Color(0.75f, 0.6f, 0.35f), Color(0.12f, 0.1f, 0.1f) };
	return t[((i % 5) + 5) % 5];
}

} // namespace pm
