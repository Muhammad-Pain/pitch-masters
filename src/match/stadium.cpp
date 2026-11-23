#include "stadium.h"
#include <godot_cpp/classes/gradient_texture2d.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include "../game_data.h"
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
	int q = GameData::get().eff_quality();
	sun->set_shadow(q >= 1);
	sun->set_shadow_mode(q >= 2 ? DirectionalLight3D::SHADOW_PARALLEL_2_SPLITS : DirectionalLight3D::SHADOW_ORTHOGONAL);
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

static Ref<ShaderMaterial> led(Color c);

static void boards(Node3D *root, const std::vector<String> &words) {
	Color colors[6] = { Color::html("1d4ed8"), Color::html("0f172a"), Color::html("dc2626"), Color::html("0f172a"), Color::html("16a34a"), Color::html("0f172a") };
	float seg = 10.f;
	int i = 0;
	for (float zs : { -1.f, 1.f }) {
		for (float x = -60.f; x < 60.f; x += seg) {
			float bz = (HW + 4.f) * zs;
			box(root, Vector3(seg - 0.2f, 0.95f, 0.15f), Vector3(x + seg * 0.5f, 0.48f, bz), led(colors[i % 6]));
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
			box(root, Vector3(0.15f, 0.95f, 26.f), Vector3((HL + 5.f) * xs, 0.48f, 18.f * zs), led(colors[(i + 1) % 6]));
			i++;
		}
}

static Ref<ImageTexture> crowd_texture(int seed, Color home, Color away) {
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
	return ImageTexture::create_from_image(img);
}

static std::vector<Ref<ShaderMaterial>> g_crowd;
static Ref<Shader> g_crowd_shader, g_led_shader;
static float g_excite = 0.f;

static const char *CROWD_SHADER = R"(
shader_type spatial;
render_mode diffuse_lambert, specular_disabled;
uniform sampler2D tex : source_color, filter_linear_mipmap_anisotropic;
uniform vec2 tiles = vec2(1.0);
uniform float excite = 0.0;
void fragment() {
	vec2 uv = UV * tiles;
	// every seat column bobs on its own phase; on a goal the whole stand jumps
	float col = floor(uv.x * 48.0) + floor(uv.y * 8.0) * 13.0;
	float ph = fract(sin(col * 12.9898) * 43758.5453) * 6.283;
	float bob = sin(TIME * (2.0 + excite * 6.0) + ph) * (0.012 + excite * 0.05);
	uv.y += max(bob, -0.02) / 8.0;
	vec3 c = texture(tex, uv).rgb;
	ALBEDO = c * 0.9;
	ROUGHNESS = 1.0;
}
)";

static Ref<ShaderMaterial> crowd_material(int seed, Color home, Color away) {
	if (g_crowd_shader.is_null()) {
		g_crowd_shader.instantiate();
		g_crowd_shader->set_code(CROWD_SHADER);
	}
	Ref<ShaderMaterial> m;
	m.instantiate();
	m->set_shader(g_crowd_shader);
	m->set_shader_parameter("tex", crowd_texture(seed, home, away));
	m->set_shader_parameter("excite", g_excite);
	g_crowd.push_back(m);
	return m;
}

static void slope(Node3D *root, const Ref<ShaderMaterial> &m, float near, float far, float y0, float y1, float length, float side, bool along_x) {
	float depth = far - near, rise = y1 - y0;
	float slen = std::sqrt(depth * depth + rise * rise);
	float ang = std::atan2(rise, depth);
	Ref<QuadMesh> q;
	q.instantiate();
	q->set_size(Vector2(length, slen));
	Ref<ShaderMaterial> mm = m;
	mm->set_shader_parameter("tiles", Vector2(length / 26.f, slen / 6.5f));
	float mid = (near + far) * 0.5f, midy = (y0 + y1) * 0.5f;
	MeshInstance3D *mi = mesh(root, q, mm, along_x ? Vector3(mid * side, midy, 0) : Vector3(0, midy, mid * side));
	if (along_x) mi->set_rotation(Vector3(0, -Math_PI * 0.5f * side, 0));
	else mi->set_rotation(Vector3(0, side < 0 ? 0 : Math_PI, 0));
	mi->rotate_object_local(Vector3(1, 0, 0), -(Math_PI * 0.5f - ang));
}

static void stands(Node3D *root, Color home, Color away) {
	Ref<StandardMaterial3D> concrete = mat(Color::html("2a3142"));
	Ref<StandardMaterial3D> roof = mat(Color::html("1c2230"), false, 0.5f);
	Ref<StandardMaterial3D> steel = mat(Color::html("8a94a6"), false, 0.35f);
	Ref<StandardMaterial3D> band = mat(Color::html("0b1220"));
	Ref<StandardMaterial3D> strip = mat(Color::html("e6f2ff"), true);
	for (float zs : { -1.f, 1.f }) {
		float length = 136.f;
		// lower tier
		slope(root, crowd_material(zs < 0 ? 7 : 11, home, away), HW + 8.f, HW + 24.f, 1.2f, 11.f, length, zs, false);
		box(root, Vector3(length, 1.2f, 2.f), Vector3(0, 0.6f, (HW + 7.f) * zs), concrete);
		// concourse band with a light strip
		box(root, Vector3(length, 2.4f, 2.f), Vector3(0, 12.2f, (HW + 24.8f) * zs), band);
		box(root, Vector3(length, 0.25f, 0.1f), Vector3(0, 11.6f, (HW + 23.75f) * zs), strip);
		// upper tier
		slope(root, crowd_material(zs < 0 ? 19 : 23, home, away), HW + 25.5f, HW + 42.f, 13.4f, 27.f, length, zs, false);
		box(root, Vector3(length, 31.f, 1.f), Vector3(0, 15.5f, (HW + 42.5f) * zs), concrete);
		// roof slab tilted towards the pitch + trusses
		float ry = 33.f;
		box(root, Vector3(length + 4.f, 0.8f, 24.f), Vector3(0, ry, (HW + 32.f) * zs), roof, Vector3(-0.09f * zs, 0, 0));
		box(root, Vector3(length + 4.f, 0.6f, 0.6f), Vector3(0, ry - 1.1f, (HW + 20.5f) * zs), steel);
		for (float x = -64.f; x <= 64.f; x += 16.f) {
			box(root, Vector3(0.35f, 0.35f, 24.f), Vector3(x, ry - 0.9f, (HW + 32.f) * zs), steel, Vector3(-0.09f * zs, 0, 0));
			box(root, Vector3(0.6f, ry, 0.6f), Vector3(x, ry * 0.5f, (HW + 43.f) * zs), steel);
		}
		box(root, Vector3(length + 4.f, 0.3f, 0.3f), Vector3(0, ry - 1.6f, (HW + 20.6f) * zs), strip);
	}
	for (float xs : { -1.f, 1.f }) {
		slope(root, crowd_material(xs < 0 ? 13 : 17, xs < 0 ? home : away, xs < 0 ? away : home), HL + 9.f, HL + 30.f, 1.2f, 16.f, 84.f, xs, true);
		box(root, Vector3(2.f, 1.2f, 84.f), Vector3((HL + 8.f) * xs, 0.6f, 0), concrete);
		box(root, Vector3(1.f, 22.f, 84.f), Vector3((HL + 30.5f) * xs, 11.f, 0), concrete);
		box(root, Vector3(18.f, 0.7f, 88.f), Vector3((HL + 24.f) * xs, 24.f, 0), roof, Vector3(0, 0, 0.08f * xs));
	}
}

static MeshInstance3D *halo(Node3D *root, Vector3 pos, float size) {
	Ref<StandardMaterial3D> m;
	m.instantiate();
	Ref<Gradient> gr;
	gr.instantiate();
	gr->set_color(0, Color(1.f, 0.98f, 0.9f, 0.9f));
	gr->set_color(1, Color(1.f, 0.95f, 0.8f, 0.f));
	gr->add_point(0.25f, Color(1.f, 0.97f, 0.88f, 0.45f));
	Ref<GradientTexture2D> gt;
	gt.instantiate();
	gt->set_gradient(gr);
	gt->set_fill(GradientTexture2D::FILL_RADIAL);
	gt->set_fill_from(Vector2(0.5f, 0.5f));
	gt->set_fill_to(Vector2(1.f, 0.5f));
	m->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, gt);
	m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	m->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD);
	m->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	m->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
	m->set_depth_draw_mode(BaseMaterial3D::DEPTH_DRAW_DISABLED);
	return quad(root, Vector2(size, size), pos, Vector3(), m);
}

static void lights(Node3D *root) {
	Ref<StandardMaterial3D> pole = mat(Color::html("6b7484"), false, 0.4f);
	Ref<StandardMaterial3D> frame = mat(Color::html("2b3240"));
	Ref<StandardMaterial3D> lamp = mat(Color::html("fffbea"), true);
	for (float xs : { -1.f, 1.f })
		for (float zs : { -1.f, 1.f }) {
			Vector3 p((HL + 26.f) * xs, 0, (HW + 46.f) * zs);
			mesh(root, cyl(0.7f, 50.f, 10), pole, p + Vector3(0, 25.f, 0));
			Node3D *head = memnew(Node3D);
			root->add_child(head);
			head->set_position(p + Vector3(0, 52.f, 0));
			head->look_at_from_position(p + Vector3(0, 52.f, 0), Vector3(0, 0, 0), Vector3(0, 1, 0));
			box(head, Vector3(10.f, 5.f, 0.6f), Vector3(0, 0, 0.2f), frame);
			for (int r = 0; r < 3; r++)
				for (int c = 0; c < 6; c++) {
					Vector3 lp(-4.f + c * 1.6f, -1.6f + r * 1.6f, -0.15f);
					box(head, Vector3(1.2f, 1.2f, 0.2f), lp, lamp);
				}
			halo(head, Vector3(0, 0, -1.f), 26.f);
		}
}

static void corner_flags(Node3D *root, Color home, Color away) {
	Ref<StandardMaterial3D> pole = mat(Color(0.95f, 0.95f, 0.95f));
	for (float xs : { -1.f, 1.f })
		for (float zs : { -1.f, 1.f }) {
			Vector3 p(HL * xs, 0, HW * zs);
			mesh(root, cyl(0.025f, 1.6f, 6), pole, p + Vector3(0, 0.8f, 0));
			Ref<StandardMaterial3D> fm = mat(xs < 0 ? home : away);
			fm->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
			quad(root, Vector2(0.45f, 0.32f), p + Vector3(0.24f * -xs, 1.42f, 0), Vector3(0, 0, 0), fm);
		}
}

static void dugouts(Node3D *root, Color home, Color away) {
	Ref<StandardMaterial3D> base = mat(Color::html("1f2937"));
	Ref<StandardMaterial3D> glass = mat(Color(0.7f, 0.85f, 1.f, 0.35f));
	glass->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	for (float xs : { -1.f, 1.f }) {
		Vector3 c(9.f * xs, 0, -(HW + 2.4f));
		box(root, Vector3(7.f, 0.5f, 1.4f), c + Vector3(0, 0.25f, 0), base);
		box(root, Vector3(7.f, 0.08f, 1.6f), c + Vector3(0, 1.6f, 0.1f), glass);
		box(root, Vector3(7.f, 1.6f, 0.08f), c + Vector3(0, 0.8f, -0.7f), glass);
		Ref<StandardMaterial3D> seats = mat((xs < 0 ? home : away).darkened(0.2f));
		for (int i = 0; i < 6; i++) box(root, Vector3(0.45f, 0.9f, 0.4f), c + Vector3(-2.8f + i * 1.1f, 0.95f, -0.2f), seats);
	}
}

static void big_screen(Node3D *root) {
	Ref<StandardMaterial3D> frame = mat(Color::html("111827"));
	Ref<StandardMaterial3D> screen = mat(Color::html("0b1a33"), true);
	Vector3 c(HL + 22.f, 21.f, -26.f);
	box(root, Vector3(0.6f, 7.5f, 16.f), c, frame);
	box(root, Vector3(0.2f, 6.6f, 15.f), c + Vector3(-0.35f, 0, 0), screen);
	Label3D *l = memnew(Label3D);
	l->set_name("BigScreen");
	l->set_text("PITCH MASTERS");
	l->set_font_size(160);
	l->set_pixel_size(0.022f);
	l->set_modulate(Color(1, 1, 1));
	l->set_outline_size(0);
	l->set_position(c + Vector3(-0.5f, 0, 0));
	l->set_rotation_degrees(Vector3(0, -90, 0));
	root->add_child(l);
}

static const char *LED_SHADER = R"(
shader_type spatial;
render_mode unshaded;
uniform vec3 base : source_color = vec3(0.1, 0.2, 0.6);
varying vec3 wp;
void vertex() { wp = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz; }
void fragment() {
	float sweep = smoothstep(0.0, 0.08, fract(wp.x * 0.012 + wp.z * 0.012 - TIME * 0.18)) * (1.0 - smoothstep(0.08, 0.2, fract(wp.x * 0.012 + wp.z * 0.012 - TIME * 0.18)));
	float px = step(0.25, fract(wp.x * 22.0)) * step(0.25, fract(wp.y * 22.0));
	ALBEDO = base * (0.75 + 0.5 * sweep) * (0.85 + 0.15 * px);
}
)";

static Ref<ShaderMaterial> led(Color c) {
	if (g_led_shader.is_null()) {
		g_led_shader.instantiate();
		g_led_shader->set_code(LED_SHADER);
	}
	Ref<ShaderMaterial> m;
	m.instantiate();
	m->set_shader(g_led_shader);
	m->set_shader_parameter("base", c);
	return m;
}

void set_excitement(float e) {
	if (std::fabs(e - g_excite) < 0.01f) return;
	g_excite = e;
	for (auto &m : g_crowd)
		if (m.is_valid()) m->set_shader_parameter("excite", e);
}

void release() {
	g_crowd.clear();
	g_crowd_shader.unref();
	g_led_shader.unref();
}

void build(Node3D *root, const std::vector<String> &board_words, Color home, Color away) {
	environment(root);
	grass(root);
	lines(root);
	Ref<StandardMaterial3D> net = net_material();
	goal(root, -1.f, net);
	goal(root, 1.f, net);
	g_crowd.clear();
	g_excite = 0.f;
	boards(root, board_words);
	stands(root, home, away);
	lights(root);
	corner_flags(root, home, away);
	dugouts(root, home, away);
	big_screen(root);
}

} // namespace stadium
} // namespace pm
