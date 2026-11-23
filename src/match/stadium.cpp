#include "stadium.h"
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/shader.hpp>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/procedural_sky_material.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/classes/world_environment.hpp>

using namespace godot;

namespace pm {
namespace stadium {

Ref<StandardMaterial3D> mat(Color c, bool unshaded, float emission) {
	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_albedo(c);
	m->set_roughness(0.9f);
	if (unshaded) m->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	if (emission > 0.f) {
		m->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
		m->set_emission(c);
		m->set_emission_energy_multiplier(emission);
	}
	return m;
}

MeshInstance3D *mesh(Node3D *root, const Ref<Mesh> &m, const Ref<Material> &mt, Vector3 pos, Vector3 rot) {
	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(m);
	mi->set_material_override(mt);
	mi->set_position(pos);
	mi->set_rotation(rot);
	mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	root->add_child(mi);
	return mi;
}

MeshInstance3D *box(Node3D *root, Vector3 size, Vector3 pos, const Ref<Material> &m, Vector3 rot) {
	Ref<BoxMesh> b;
	b.instantiate();
	b->set_size(size);
	return mesh(root, b, m, pos, rot);
}

static Ref<CylinderMesh> cyl(float r, float h, int seg = 12) {
	Ref<CylinderMesh> c;
	c.instantiate();
	c->set_top_radius(r);
	c->set_bottom_radius(r);
	c->set_height(h);
	c->set_radial_segments(seg);
	c->set_rings(1);
	return c;
}

static void environment(Node3D *root) {
	Ref<Environment> env;
	env.instantiate();
	Ref<Sky> sky;
	sky.instantiate();
	Ref<ProceduralSkyMaterial> sm;
	sm.instantiate();
	sm->set_sky_top_color(Color::html("0b1e4a"));
	sm->set_sky_horizon_color(Color::html("5d7fb8"));
	sm->set_ground_horizon_color(Color::html("3b4a63"));
	sm->set_ground_bottom_color(Color::html("1b2333"));
	sky->set_material(sm);
	env->set_background(Environment::BG_SKY);
	env->set_sky(sky);
	env->set_ambient_source(Environment::AMBIENT_SOURCE_COLOR);
	env->set_ambient_light_color(Color::html("b8c4e0"));
	env->set_ambient_light_energy(0.45f);
	env->set_tonemapper(Environment::TONE_MAPPER_FILMIC);
	env->set_tonemap_exposure(1.0f);
	// no glow / colour adjustments: on phones the GL Compatibility renderer may skip the post-process
	// pass, so the look must come from lighting + tonemapping only to match on every device
	env->set_adjustment_enabled(false);
	env->set_glow_enabled(false);
	WorldEnvironment *we = memnew(WorldEnvironment);
	we->set_environment(env);
	root->add_child(we);
	DirectionalLight3D *sun = memnew(DirectionalLight3D);
	sun->set_rotation_degrees(Vector3(-58, 28, 0));
	sun->set_param(Light3D::PARAM_ENERGY, 0.92f);
	sun->set_color(Color(1.f, 0.97f, 0.92f));
	sun->set_shadow(true);
	sun->set_shadow_mode(DirectionalLight3D::SHADOW_PARALLEL_2_SPLITS);
	sun->set_param(Light3D::PARAM_SHADOW_MAX_DISTANCE, 95.f);
	sun->set_param(Light3D::PARAM_SHADOW_OPACITY, 0.72f);
	sun->set_param(Light3D::PARAM_SHADOW_BLUR, 1.4f);
	sun->set_param(Light3D::PARAM_SHADOW_NORMAL_BIAS, 1.2f);
	root->add_child(sun);
}

static const char *GRASS_SHADER = R"(
shader_type spatial;
render_mode diffuse_lambert, specular_schlick_ggx;
uniform vec3 c1 : source_color = vec3(0.165, 0.30, 0.12);
uniform vec3 c2 : source_color = vec3(0.205, 0.36, 0.15);
uniform float hl = 52.5;
uniform float hw = 34.0;
varying vec3 wp;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vnoise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);
}
void vertex() { wp = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz; }
void fragment() {
	float sw = (hl * 2.0) / 18.0;
	float s = mod(floor((wp.x + hl) / sw), 2.0);
	float ring = mod(floor(length(wp.xz) / 9.15), 2.0);
	vec3 col = mix(c1, c2, s);
	float n = vnoise(wp.xz * 0.25) * 0.55 + vnoise(wp.xz * 1.7) * 0.3 + vnoise(wp.xz * 11.0) * 0.15;
	col *= 0.86 + 0.26 * n;
	// worn grass in the goal mouths and the centre
	float wear = smoothstep(7.0, 0.0, length(vec2(abs(wp.x) - (hl - 4.0), wp.z * 0.8)));
	wear += smoothstep(4.0, 0.0, length(wp.xz)) * 0.5;
	col = mix(col, vec3(0.36, 0.40, 0.18), wear * 0.35 * n);
	float outside = max(step(hl + 0.2, abs(wp.x)), step(hw + 0.2, abs(wp.z)));
	col *= mix(1.0, 0.8, outside);
	float edge = smoothstep(hl + 9.0, hl + 3.0, abs(wp.x)) * smoothstep(hw + 9.0, hw + 3.0, abs(wp.z));
	col *= mix(0.72, 1.0, edge);
	ALBEDO = col;
	ROUGHNESS = 0.82 - 0.1 * s;
	SPECULAR = 0.3;
}
)";

static void grass(Node3D *root) {
	Ref<Shader> sh;
	sh.instantiate();
	sh->set_code(GRASS_SHADER);
	Ref<ShaderMaterial> m;
	m.instantiate();
	m->set_shader(sh);
	m->set_shader_parameter("hl", HL);
	m->set_shader_parameter("hw", HW);
	Ref<PlaneMesh> pm;
	pm.instantiate();
	pm->set_size(Vector2(160, 120));
	pm->set_subdivide_width(8);
	pm->set_subdivide_depth(6);
	mesh(root, pm, m, Vector3(0, -0.01f, 0));
}

static void lines(Node3D *root) {
	Ref<StandardMaterial3D> m = mat(Color(0.86f, 0.9f, 0.86f), true);
	float t = 0.12f, y = 0.012f;
	for (float s : { -1.f, 1.f }) {
		box(root, Vector3(HL * 2 + t, 0.01f, t), Vector3(0, y, HW * s), m);
		box(root, Vector3(t, 0.01f, HW * 2 + t), Vector3(HL * s, y, 0), m);
		box(root, Vector3(t, 0.01f, 40.32f), Vector3(HL * s - 16.5f * s, y, 0), m);
		for (float zs : { -1.f, 1.f }) {
			box(root, Vector3(16.5f, 0.01f, t), Vector3(HL * s - 8.25f * s, y, 20.16f * zs), m);
			box(root, Vector3(5.5f, 0.01f, t), Vector3(HL * s - 2.75f * s, y, 9.16f * zs), m);
		}
		box(root, Vector3(t, 0.01f, 18.32f), Vector3(HL * s - 5.5f * s, y, 0), m);
		mesh(root, cyl(0.16f, 0.01f), m, Vector3(HL * s - 11.f * s, y, 0));
		for (int k = 0; k < 9; k++) {
			float ang = Math::deg_to_rad(-36.f + k * 9.f);
			float cx = HL * s - 11.f * s - std::cos(ang) * 9.15f * s;
			float cz = std::sin(ang) * 9.15f;
			if (std::fabs(cx) < HL - 16.5f) box(root, Vector3(t, 0.01f, 1.45f), Vector3(cx, y, cz), m, Vector3(0, ang * s, 0));
		}
	}
	box(root, Vector3(t, 0.01f, HW * 2), Vector3(0, y, 0), m);
	Ref<TorusMesh> tor;
	tor.instantiate();
	tor->set_inner_radius(9.15f - t * 0.5f);
	tor->set_outer_radius(9.15f + t * 0.5f);
	tor->set_rings(64);
	tor->set_ring_segments(3);
	MeshInstance3D *ci = mesh(root, tor, m, Vector3(0, y, 0));
	ci->set_scale(Vector3(1, 0.02f, 1));
	mesh(root, cyl(0.2f, 0.01f), m, Vector3(0, y, 0));
}

static Ref<StandardMaterial3D> net_material() {
	Ref<Image> img = Image::create(32, 32, false, Image::FORMAT_RGBA8);
	img->fill(Color(1, 1, 1, 0));
	for (int i = 0; i < 32; i++)
		for (int j = 0; j < 2; j++) {
			img->set_pixel(i, j, Color(1, 1, 1, 0.9f));
			img->set_pixel(j, i, Color(1, 1, 1, 0.9f));
		}
	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, ImageTexture::create_from_image(img));
	m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
	m->set_alpha_scissor_threshold(0.3f);
	m->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	m->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	m->set_uv1_scale(Vector3(14, 6, 1));
	return m;
}

static MeshInstance3D *quad(Node3D *root, Vector2 size, Vector3 pos, Vector3 rot_deg, const Ref<Material> &m) {
	Ref<QuadMesh> q;
	q.instantiate();
	q->set_size(size);
	MeshInstance3D *mi = mesh(root, q, m, pos);
	mi->set_rotation_degrees(rot_deg);
	return mi;
}

static void goal(Node3D *root, float s, const Ref<StandardMaterial3D> &net) {
	Ref<StandardMaterial3D> white = mat(Color(1, 1, 1), false, 0.15f);
	float r = 0.07f;
	for (float zs : { -1.f, 1.f }) {
		mesh(root, cyl(r, GH), white, Vector3(HL * s, GH * 0.5f, GW * zs))->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_ON);
		box(root, Vector3(0.05f, GH, 0.05f), Vector3((HL + 2.f) * s, GH * 0.5f, GW * zs), white);
	}
	mesh(root, cyl(r, GW * 2 + r * 2), white, Vector3(HL * s, GH, 0), Vector3(Math_PI * 0.5f, 0, 0))->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_ON);
	quad(root, Vector2(GW * 2, GH), Vector3((HL + 2.f) * s, GH * 0.5f, 0), Vector3(0, 90, 0), net);
	quad(root, Vector2(2.f, GW * 2), Vector3((HL + 1.f) * s, GH, 0), Vector3(-90, 0, 0), net);
	for (float zs : { -1.f, 1.f }) quad(root, Vector2(2.f, GH), Vector3((HL + 1.f) * s, GH * 0.5f, GW * zs), Vector3(), net);
}

static void boards(Node3D *root, const std::vector<String> &words) {
	Color colors[6] = { Color::html("1d4ed8"), Color::html("0f172a"), Color::html("dc2626"), Color::html("0f172a"), Color::html("16a34a"), Color::html("0f172a") };
	float seg = 10.f;
	int i = 0;
	for (float zs : { -1.f, 1.f }) {
		for (float x = -60.f; x < 60.f; x += seg) {
			float bz = (HW + 4.f) * zs;
			box(root, Vector3(seg - 0.2f, 0.95f, 0.15f), Vector3(x + seg * 0.5f, 0.48f, bz), mat(colors[i % 6], false, 0.6f));
			Label3D *l = memnew(Label3D);
			l->set_text(words.empty() ? String("PITCH MASTERS") : words[i % words.size()]);
			l->set_font_size(96);
			l->set_pixel_size(0.0085f);
			l->set_outline_size(0);
			l->set_position(Vector3(x + seg * 0.5f, 0.48f, bz - 0.09f * zs));
			l->set_rotation_degrees(Vector3(0, zs < 0 ? 0 : 180, 0));
			l->set_draw_flag(Label3D::FLAG_DOUBLE_SIDED, false);
			root->add_child(l);
			i++;
		}
	}
	for (float xs : { -1.f, 1.f })
		for (float zs : { -1.f, 1.f }) {
			box(root, Vector3(0.15f, 0.95f, 26.f), Vector3((HL + 5.f) * xs, 0.48f, 18.f * zs), mat(colors[(i + 1) % 6], false, 0.5f));
			i++;
		}
}

static Ref<StandardMaterial3D> crowd_material(int seed, Color home, Color away) {
	// 8x10 px per seat: head, shoulders, torso; rows separated by a dark step
	const int sw = 6, sh = 10, cols = 48, rows = 8;
	const int w = sw * cols, h = sh * rows;
	Ref<Image> img = Image::create(w, h, false, Image::FORMAT_RGB8);
	Color seat_a = Color::html("1b2233"), seat_b = Color::html("222b40"), step = Color::html("0c1018");
	Color neutral[6] = { Color::html("e5e7eb"), Color::html("1f2937"), Color::html("374151"), Color::html("0f172a"), Color::html("6b7280"), Color::html("a3a3a3") };
	Color skins[4] = { Color::html("e8b996"), Color::html("c68b62"), Color::html("8d5a3b"), Color::html("f1d0b5") };
	for (int ry = 0; ry < rows; ry++)
		for (int cx = 0; cx < cols; cx++) {
			Rng r((uint64_t)(cx * 131 + ry * 977 + seed * 7919 + 1));
			uint64_t hv = r.next();
			bool occupied = hv % 100 < 88;
			int k = (int)((hv >> 8) % 100);
			Color shirt = k < 46 ? home : (k < 66 ? away : neutral[(hv >> 16) % 6]);
			shirt = shirt.darkened((float)((hv >> 20) % 28) / 100.f);
			Color skin = skins[(hv >> 24) % 4].darkened(0.1f);
			int bob = (int)((hv >> 28) % 2);
			for (int y = 0; y < sh; y++)
				for (int x = 0; x < sw; x++) {
					Color c = ((cx + ry) % 2 == 0) ? seat_a : seat_b;
					if (y >= sh - 2) c = step;
					else if (occupied) {
						int yy = y - bob;
						if (yy >= 1 && yy <= 2 && x >= 2 && x <= 3) c = skin;
						else if (yy >= 3 && yy <= 7 && x >= 1 && x <= 4) c = shirt;
						else if (yy >= 4 && yy <= 6 && (x == 0 || x == 5) && ((hv >> 30) % 5 == 0)) c = skin; // raised arms
					}
					img->set_pixel(cx * sw + x, ry * sh + y, c);
				}
		}
	img->generate_mipmaps();
	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, ImageTexture::create_from_image(img));
	m->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC);
	m->set_roughness(1.f);
	m->set_albedo(Color(0.9f, 0.9f, 0.95f));
	return m;
}

static void slope(Node3D *root, const Ref<StandardMaterial3D> &m, float near, float far, float y0, float y1, float length, float side, bool along_x) {
	float depth = far - near, rise = y1 - y0;
	float slen = std::sqrt(depth * depth + rise * rise);
	float ang = std::atan2(rise, depth);
	Ref<QuadMesh> q;
	q.instantiate();
	q->set_size(Vector2(length, slen));
	Ref<StandardMaterial3D> mm = m->duplicate();
	mm->set_uv1_scale(Vector3(length / 26.f, slen / 6.5f, 1));
	float mid = (near + far) * 0.5f, midy = (y0 + y1) * 0.5f;
	MeshInstance3D *mi = mesh(root, q, mm, along_x ? Vector3(mid * side, midy, 0) : Vector3(0, midy, mid * side));
	if (along_x) mi->set_rotation(Vector3(0, -Math_PI * 0.5f * side, 0));
	else mi->set_rotation(Vector3(0, side < 0 ? 0 : Math_PI, 0));
	mi->rotate_object_local(Vector3(1, 0, 0), -(Math_PI * 0.5f - ang));
}

static void stands(Node3D *root, Color home, Color away) {
	Ref<StandardMaterial3D> concrete = mat(Color::html("2a3142"));
	Ref<StandardMaterial3D> roof = mat(Color::html("161b26"));
	for (float zs : { -1.f, 1.f }) {
		float near = HW + 8.f, far = HW + 34.f, y1 = 20.f, length = 136.f;
		slope(root, crowd_material(zs < 0 ? 7 : 11, home, away), near, far, 1.2f, y1, length, zs, false);
		box(root, Vector3(length, 1.2f, 2.f), Vector3(0, 0.6f, (near - 1.f) * zs), concrete);
		box(root, Vector3(length, 2.f, 18.f), Vector3(0, y1 + 9.f, (far - 7.f) * zs), roof, Vector3(-0.12f * zs, 0, 0));
		box(root, Vector3(length, y1 + 9.f, 1.f), Vector3(0, (y1 + 9.f) * 0.5f, (far + 0.5f) * zs), concrete);
	}
	for (float xs : { -1.f, 1.f }) {
		slope(root, crowd_material(xs < 0 ? 13 : 17, xs < 0 ? home : away, xs < 0 ? away : home), HL + 9.f, HL + 30.f, 1.2f, 16.f, 84.f, xs, true);
		box(root, Vector3(2.f, 1.2f, 84.f), Vector3((HL + 8.f) * xs, 0.6f, 0), concrete);
	}
}

static void lights(Node3D *root) {
	Ref<StandardMaterial3D> pole = mat(Color::html("3b4252"));
	Ref<StandardMaterial3D> lamp = mat(Color::html("fffbe6"), true);
	for (float xs : { -1.f, 1.f })
		for (float zs : { -1.f, 1.f }) {
			Vector3 p((HL + 22.f) * xs, 0, (HW + 26.f) * zs);
			box(root, Vector3(0.8f, 42.f, 0.8f), p + Vector3(0, 21.f, 0), pole);
			box(root, Vector3(7.f, 3.5f, 0.6f), p + Vector3(0, 43.f, 0), lamp, Vector3(0.5f * zs, std::atan2(p.x, p.z), 0));
		}
}

void build(Node3D *root, const std::vector<String> &board_words, Color home, Color away) {
	environment(root);
	grass(root);
	lines(root);
	Ref<StandardMaterial3D> net = net_material();
	goal(root, -1.f, net);
	goal(root, 1.f, net);
	boards(root, board_words);
	stands(root, home, away);
	lights(root);
}

} // namespace stadium
} // namespace pm
