#include "footballer.h"

#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/bone_attachment3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>

using namespace godot;

namespace pm {

float PlayerStats::rating() const {
	float r = 6.0f + goals * 1.2f + assists * 0.8f + shots * 0.12f + saves * 0.45f + tackles * 0.25f;
	if (passes > 0) r += clampf((float)passes_ok / passes - 0.7f, -0.3f, 0.3f) * 2.0f + std::min(passes_ok, 25) * 0.03f;
	r -= lost * 0.08f;
	return clampf(r, 4.0f, 10.0f);
}

static MeshInstance3D *mk(Node3D *parent, const Ref<Mesh> &m, const Ref<Material> &mat, Vector3 p) {
	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(m);
	mi->set_material_override(mat);
	mi->set_position(p);
	mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	parent->add_child(mi);
	return mi;
}

void Footballer::setup(Node3D *parent, const KitColors &kit, int number, const Ref<Material> &shadow) {
	node = memnew(Node3D);
	parent->add_child(node);
	CharLib &lib = CharLib::get();
	if (lib.init()) {
		Node *inst = lib.scene->instantiate();
		model = Object::cast_to<Node3D>(inst);
	}
	if (model) {
		model->set_rotation(Vector3(0, (float)Math_PI * 0.5f, 0));
		node->add_child(model);
		skel = Object::cast_to<Skeleton3D>(model->find_child("Skeleton3D", true, false));
		anim = Object::cast_to<AnimationPlayer>(model->find_child("AnimationPlayer", true, false));
		MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(model->find_child("Mannequin", true, false));
		if (mi) {
			mi->set_mesh(lib.mesh);
			mi->set_material_override(lib.material(kit));
			mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_ON);
		}
		if (anim) {
			anim->set_callback_mode_process(AnimationMixer::ANIMATION_CALLBACK_MODE_PROCESS_MANUAL);
			anim->play("Idle");
			cur_anim = "Idle";
			anim->seek(rng().randf() * 2.0, true);
		}
		if (skel && lib.bone_spine >= 0) {
			BoneAttachment3D *ba = memnew(BoneAttachment3D);
			skel->add_child(ba);
			ba->set_bone_idx(lib.bone_spine);
			Label3D *num = memnew(Label3D);
			num->set_text(I(number));
			num->set_font_size(64);
			num->set_pixel_size(0.0042f);
			num->set_outline_size(10);
			num->set_modulate(kit.shirt2);
			num->set_outline_modulate(kit.shirt.darkened(0.5f));
			// place on the back in rest pose, expressed in the bone's local space
			Transform3D want(Basis(Vector3(0, 1, 0), (float)Math_PI), Vector3(0, 1.30f, -0.155f));
			num->set_transform(lib.spine_rest.affine_inverse() * want);
			num->set_draw_flag(Label3D::FLAG_DOUBLE_SIDED, false);
			ba->add_child(num);
		}
	} else {
		simple = true;
		model = memnew(Node3D);
		node->add_child(model);
		setup_simple(kit);
	}

	Ref<QuadMesh> sh;
	sh.instantiate();
	sh->set_size(Vector2(0.9f, 0.9f));
	MeshInstance3D *s = mk(node, sh, shadow, Vector3(0.05f, 0.02f, 0.f));
	s->set_rotation_degrees(Vector3(-90, 0, 0));

	ring = memnew(MeshInstance3D);
	Ref<TorusMesh> tm;
	tm.instantiate();
	tm->set_inner_radius(0.5f);
	tm->set_outer_radius(0.64f);
	tm->set_rings(32);
	tm->set_ring_segments(4);
	ring->set_mesh(tm);
	ring->set_scale(Vector3(1, 0.06f, 1));
	ring->set_position(Vector3(0, 0.03f, 0));
	ring->set_visible(false);
	ring->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	node->add_child(ring);

	marker = memnew(MeshInstance3D);
	Ref<CylinderMesh> cone;
	cone.instantiate();
	cone->set_top_radius(0.16f);
	cone->set_bottom_radius(0.0f);
	cone->set_height(0.3f);
	cone->set_radial_segments(3);
	cone->set_rings(1);
	marker->set_mesh(cone);
	marker->set_position(Vector3(0, 2.25f, 0));
	marker->set_visible(false);
	marker->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	node->add_child(marker);

	tag = memnew(Label3D);
	tag->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
	tag->set_draw_flag(Label3D::FLAG_FIXED_SIZE, true);
	tag->set_draw_flag(Label3D::FLAG_DISABLE_DEPTH_TEST, true);
	tag->set_pixel_size(0.0008f);
	tag->set_font_size(26);
	tag->set_outline_size(10);
	tag->set_position(Vector3(0, 2.6f, 0));
	tag->set_visible(false);
	node->add_child(tag);
	node->set_scale(Vector3(1.04f, 1.04f, 1.04f));
}

void Footballer::set_marker(bool on, const Ref<Material> &m) {
	ring->set_visible(on);
	marker->set_visible(on);
	if (on) {
		ring->set_material_override(m);
		marker->set_material_override(m);
	}
}

void Footballer::rotate_bone_world(int bone, const Vector3 &axis, float angle) {
	if (!skel || bone < 0) return;
	Transform3D g = skel->get_bone_global_pose(bone);
	int parent = skel->get_bone_parent(bone);
	Transform3D pg = parent >= 0 ? skel->get_bone_global_pose(parent) : Transform3D();
	Transform3D want(Basis(axis, angle) * g.basis, g.origin);
	Transform3D local = pg.affine_inverse() * want;
	skel->set_bone_pose_rotation(bone, local.basis.get_rotation_quaternion());
}

void Footballer::animate(float delta, float speed) {
	node->set_position(pos);
	node->set_rotation(Vector3(0, -facing, 0));
	if (simple) {
		animate_simple(delta, speed);
		return;
	}
	if (!anim) return;
	// pick animation
	String want;
	float scale = 1.f;
	if (action_t > 0.f) {
		action_t -= delta;
		want = action;
	} else if (rid == R_GK && speed < 2.5f && dive_anim <= 0.f) {
		want = speed < 0.4f ? "Crouch_Idle" : "Crouch_Fwd";
		scale = speed < 0.4f ? 1.f : clampf(speed / 1.6f, 0.7f, 1.8f);
	} else if (speed < 0.35f) {
		want = "Idle";
	} else if (speed < (cur_anim == "Walk" ? 2.4f : 2.0f)) {
		want = "Walk";
		scale = clampf(speed / 1.45f, 0.5f, 1.7f);
	} else if (speed < (cur_anim == "Sprint" ? 5.2f : 5.7f)) {
		want = "Jog_Fwd";
		scale = clampf(speed / 3.9f, 0.6f, 1.5f);
	} else {
		want = "Sprint";
		scale = clampf(speed / 6.8f, 0.8f, 1.35f);
	}
	if (dive_anim > 0.f) want = "Jump";
	if (want != cur_anim) {
		anim->play(want, 0.22);
		cur_anim = want;
	}
	anim->set_speed_scale(scale);
	anim->advance(delta);
	// procedural layers on top of the clip
	Vector3 side(1, 0, 0); // model space: +X = left, +Z = forward
	if (kick_anim > 0.f) {
		kick_anim -= delta;
		float k = clampf(1.f - kick_anim / 0.3f, 0.f, 1.f);
		float th, kn;
		if (k < 0.35f) {
			float a = k / 0.35f;
			th = 0.7f * a;
			kn = 1.3f * a;
		} else if (k < 0.7f) {
			float a = (k - 0.35f) / 0.35f;
			th = lerpf(0.7f, -1.25f, a);
			kn = lerpf(1.3f, 0.1f, a);
		} else {
			float a = (k - 0.7f) / 0.3f;
			th = lerpf(-1.25f, 0.f, a);
			kn = lerpf(0.1f, 0.f, a);
		}
		CharLib &lib = CharLib::get();
		rotate_bone_world(lib.bone_thigh_r, side, th);
		rotate_bone_world(lib.bone_shin_r, side, kn);
		rotate_bone_world(lib.bone_upper_arm_l, Vector3(0, 0, 1), -0.5f * std::sin(k * (float)Math_PI));
	}
	Vector3 mr;
	Vector3 mp;
	if (dive_anim > 0.f) {
		dive_anim -= delta;
		float d = std::sin(clampf(1.f - dive_anim / 0.8f, 0.f, 1.f) * (float)Math_PI * 0.5f);
		mr.x = dive_side * 1.3f * d;
		mp.y = 0.3f * d;
	}
	model->set_position(mp);
	model->set_rotation(Vector3(mr.x, (float)Math_PI * 0.5f, 0));
}

// ------------------------------------------------------------------ capsule fallback
static Ref<CapsuleMesh> capsule(float r, float h) {
	Ref<CapsuleMesh> c;
	c.instantiate();
	c->set_radius(r);
	c->set_height(h);
	c->set_radial_segments(10);
	c->set_rings(3);
	return c;
}

static Ref<StandardMaterial3D> flat(Color c) {
	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_albedo(c);
	return m;
}

static Node3D *limb(Node3D *model, Vector3 pivot, float r, float len, const Ref<Material> &m) {
	Node3D *n = memnew(Node3D);
	n->set_position(pivot);
	model->add_child(n);
	mk(n, capsule(r, len), m, Vector3(0, -len * 0.5f, 0));
	return n;
}

void Footballer::setup_simple(const KitColors &kit) {
	Ref<StandardMaterial3D> shirt = flat(kit.shirt), shorts = flat(kit.shorts), skin = flat(kit.skin), socks = flat(kit.socks);
	MeshInstance3D *torso = mk(model, capsule(0.25f, 0.78f), shirt, Vector3(0, 1.22f, 0));
	torso->set_scale(Vector3(0.85f, 1.f, 1.15f));
	mk(model, capsule(0.26f, 0.5f), shorts, Vector3(0, 0.88f, 0));
	Ref<SphereMesh> head;
	head.instantiate();
	head->set_radius(0.13f);
	head->set_height(0.27f);
	mk(model, head, skin, Vector3(0, 1.76f, 0));
	leg_l = limb(model, Vector3(0, 0.8f, -0.11f), 0.085f, 0.8f, socks);
	leg_r = limb(model, Vector3(0, 0.8f, 0.11f), 0.085f, 0.8f, socks);
	arm_l = limb(model, Vector3(0, 1.5f, -0.3f), 0.065f, 0.62f, shirt);
	arm_r = limb(model, Vector3(0, 1.5f, 0.3f), 0.065f, 0.62f, shirt);
}

void Footballer::animate_simple(float delta, float speed) {
	anim_phase += delta * (3.f + speed * 1.5f);
	float amp = clampf(speed / 7.5f, 0.f, 1.f) * 0.85f;
	float s = std::sin(anim_phase);
	leg_l->set_rotation(Vector3(0, 0, s * amp));
	leg_r->set_rotation(Vector3(0, 0, -s * amp));
	arm_l->set_rotation(Vector3(0, 0, -s * amp * 0.8f));
	arm_r->set_rotation(Vector3(0, 0, s * amp * 0.8f));
	if (kick_anim > 0.f) kick_anim -= delta;
	if (dive_anim > 0.f) dive_anim -= delta;
	if (action_t > 0.f) action_t -= delta;
}

} // namespace pm
