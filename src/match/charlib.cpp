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
uniform int hair_style = 0;
uniform bool beard = false;
float hash3(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }
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
	// ---- head: hair line, face details (painted from the bind pose)
	bool head = (region == 4 || region == 5) && bp.y > 1.555;
	if (head) {
		float ax = abs(bp.x);
		float front = smoothstep(0.02, 0.07, bp.z);
		float top = 1.752 - 0.03 * smoothstep(0.03, 0.09, ax);           // forehead hairline, lower at the temples
		float back = hair_style == 2 ? 1.585 : 1.63;                       // nape
		float h = step(top, bp.y) * front + (1.0 - front) * step(mix(back, top, smoothstep(-0.06, 0.04, bp.z)), bp.y);
		if (hair_style == 4) h = max(h, step(1.735, bp.y));
		float n = hash3(floor(bp * 260.0));
		vec3 hc = hair * (0.85 + 0.3 * n);
		if (hair_style == 1) hc = mix(skin * 0.8, hair, 0.55);
		c = skin;
		if (hair_style != 3) c = mix(c, hc, h);
		else c = mix(c, skin * 0.92, h);
		if (front > 0.5 && bp.y < 1.75) {
			vec2 f = vec2(ax, bp.y);
			float eye = 1.0 - smoothstep(0.008, 0.012, length((f - vec2(0.036, 1.697)) * vec2(1.0, 1.7)));
			float pupil = 1.0 - smoothstep(0.0035, 0.0055, length(f - vec2(0.036, 1.696)));
			float brow = (1.0 - smoothstep(0.003, 0.006, abs(f.y - (1.719 + (f.x - 0.036) * 0.12)))) * step(0.016, f.x) * step(f.x, 0.058);
			float nose = (1.0 - smoothstep(0.0, 0.012, abs(f.x - 0.008))) * smoothstep(1.645, 1.66, f.y) * (1.0 - smoothstep(1.67, 1.69, f.y));
			float mouth = (1.0 - smoothstep(0.0025, 0.005, abs(f.y - 1.628 + f.x * f.x * 6.0))) * step(f.x, 0.024);
			float socket = 1.0 - smoothstep(0.012, 0.03, length((f - vec2(0.036, 1.70)) * vec2(1.0, 1.4)));
			c *= 1.0 - 0.12 * socket;
			c *= 1.0 - 0.15 * nose;
			c = mix(c, vec3(0.92, 0.9, 0.88), eye * 0.85);
			c = mix(c, vec3(0.05, 0.04, 0.035), pupil);
			c = mix(c, hair * 0.8, brow * 0.9);
			c = mix(c, skin * vec3(0.62, 0.42, 0.4), mouth * 0.85);
			if (beard) c = mix(c, hair * 0.9, (1.0 - smoothstep(1.62, 1.655, f.y)) * 0.75 * smoothstep(0.012, 0.03, length(f - vec2(0.0, 1.628)) + 0.02));
		}
		rough = 0.65;
	}
	if (head) {
	} else if (region == 0) {
		c = shirt;
		float s = 0.0;
		if (pattern == 1) s = step(0.5, fract(bp.x * 7.0 + 0.25));
		else if (pattern == 2) s = step(0.5, fract(bp.y * 6.0));
		else if (pattern == 3) s = step(abs(bp.x + (bp.y - 1.2) * 0.9), 0.07);
		else if (pattern == 4) s = step(0.0, bp.x);
		c = mix(c, shirt2, s);
		// crest on the left chest + V collar
		float crest = 1.0 - smoothstep(0.018, 0.024, length(vec2(bp.x - 0.08, bp.y - 1.33)));
		c = mix(c, shirt2, crest * step(0.06, bp.z));
		float vneck = step(0.02, bp.z) * (1.0 - smoothstep(0.004, 0.009, abs(abs(bp.x) - (1.47 - bp.y) * 0.5))) * step(1.415, bp.y) * step(abs(bp.x), 0.04);
		c = mix(c, shirt2, vneck);
		// sleeve cuffs
		c = mix(c, shirt2, step(0.315, abs(bp.x)) * step(abs(bp.x), 0.335));
		rough = 0.6;
	} else if (region == 1) {
		c = shorts;
		float side = 1.0 - smoothstep(0.008, 0.014, abs(abs(bp.x) - 0.15));
		c = mix(c, shirt, side * step(abs(bp.z), 0.05) * 0.9);
		rough = 0.62;
	} else if (region == 2) {
		c = socks;
		c = mix(c, shirt2, step(0.40, bp.y) * step(bp.y, 0.425));
		rough = 0.8;
	} else if (region == 3) {
		c = boots;
		float stripe = step(0.5, fract((bp.y + bp.z * 0.6) * 38.0)) * step(0.045, abs(bp.x - sign(bp.x) * 0.1)) * step(bp.y, 0.09) * step(0.03, bp.y);
		c = mix(c, vec3(0.95), stripe * 0.8);
		rough = 0.25;
		spec = 0.6;
	}
	else if (region == 5) { c = hair; rough = 0.9; spec = 0.1; }
	else if (region == 6) { c = shirt2; rough = 0.6; }
	else if (region == 7) { c = keeper ? gloves : skin; }
	// subtle fabric / skin grain so surfaces are not flat plastic
	c *= 0.95 + 0.07 * hash3(floor(bp * 420.0));
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
	m->set_shader_parameter("hair_style", k.hair_style);
	m->set_shader_parameter("beard", k.beard);
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
	static const Color t[6] = { Color(0.86f, 0.68f, 0.56f), Color(0.8f, 0.6f, 0.45f), Color(0.72f, 0.52f, 0.37f), Color(0.6f, 0.41f, 0.28f),
		Color(0.45f, 0.3f, 0.21f), Color(0.33f, 0.22f, 0.16f) };
	return t[((i % 6) + 6) % 6];
}

Color CharLib::hair_tone(int i) {
	static const Color t[5] = { Color(0.06f, 0.05f, 0.04f), Color(0.2f, 0.12f, 0.07f), Color(0.42f, 0.28f, 0.14f), Color(0.75f, 0.6f, 0.35f), Color(0.12f, 0.1f, 0.1f) };
	return t[((i % 5) + 5) % 5];
}

} // namespace pm
