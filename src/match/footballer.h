#pragma once
#include "../game_data.h"
#include "charlib.h"

#include <godot_cpp/classes/animation_player.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/skeleton3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>

namespace pm {

enum RoleId { R_GK = 0, R_DEF = 1, R_MID = 2, R_FWD = 3 };

struct PlayerStats {
	int goals = 0, assists = 0, shots = 0, passes = 0, passes_ok = 0, tackles = 0, saves = 0, lost = 0;
	float rating() const;
};

// One player on the pitch: simulation state + skinned, animated 3D footballer.
struct Footballer {
	int team = 0, idx = 0, gid = 0;
	Card data;
	String role = "MID", slot_pos = "CMF";
	int rid = R_MID;
	Vector2 base;

	Vector3 pos, vel, target, plan;
	float facing = 0.f, max_speed = 7.f, urgency = 0.5f, stamina = 1.f;
	float tackle_cd = 0.f, stun = 0.f, kick_cd = 0.f, protect = 0.f, think = 0.f, hold = 0.f;
	float slide_t = 0.f, slide_anim = 0.f, touch_phase = 0.f;
	Vector3 slide_dir;
	bool slide_won = false;
	float kick_anim = 0.f, dive_anim = 0.f, dive_side = 0.f, net_facing = 0.f, anim_phase = 0.f;
	String action; // one-off / celebration animation
	float action_t = 0.f;
	PlayerStats st;

	godot::Node3D *node = nullptr, *model = nullptr;
	godot::Skeleton3D *skel = nullptr;
	godot::AnimationPlayer *anim = nullptr;
	String cur_anim;
	bool simple = false; // capsule fallback when the model is missing
	godot::Node3D *leg_l = nullptr, *leg_r = nullptr, *arm_l = nullptr, *arm_r = nullptr;
	godot::MeshInstance3D *ring = nullptr, *marker = nullptr;
	godot::Label3D *tag = nullptr;

	Vector3 forward() const { return Vector3(std::cos(facing), 0.f, std::sin(facing)); }
	void setup(godot::Node3D *parent, const KitColors &kit, int number, const godot::Ref<godot::Material> &shadow);
	void set_marker(bool on, const godot::Ref<godot::Material> &m);
	void play_action(const String &a, float t) {
		action = a;
		action_t = t;
	}
	void animate(float delta, float speed);

private:
	void setup_simple(const KitColors &kit);
	void animate_simple(float delta, float speed);
	void rotate_bone_world(int bone, const Vector3 &axis, float angle);
};

} // namespace pm
