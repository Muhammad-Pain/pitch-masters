#include "match.h"
#include "../app.h"
#include "../net.h"
#include "../sfx.h"
#include "../ui_kit.h"
#include "hud.h"
#include "stadium.h"
#include "charlib.h"

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture2d.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/scene_tree_timer.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <algorithm>

using namespace godot;

namespace pm {

void PMMatch::_bind_methods() {
	ClassDB::bind_method(D_METHOD("on_back"), &PMMatch::on_back);
	ClassDB::bind_method(D_METHOD("get_score"), &PMMatch::get_score);
	ClassDB::bind_method(D_METHOD("get_clock"), &PMMatch::get_clock);
	ClassDB::bind_method(D_METHOD("get_state"), &PMMatch::get_state);
	ClassDB::bind_method(D_METHOD("get_local_team"), &PMMatch::get_local_team);
	ClassDB::bind_method(D_METHOD("get_ctrl", "t"), &PMMatch::get_ctrl);
	ClassDB::bind_method(D_METHOD("get_autoplay", "t"), &PMMatch::get_autoplay);
	ClassDB::bind_method(D_METHOD("get_ball_pos"), &PMMatch::get_ball_pos);
	ClassDB::bind_method(D_METHOD("get_in_move", "t"), &PMMatch::get_in_move);
	ClassDB::bind_method(D_METHOD("get_player_pos", "gid"), &PMMatch::get_player_pos);
	ClassDB::bind_method(D_METHOD("get_dbg"), &PMMatch::get_dbg);
	ClassDB::bind_method(D_METHOD("force_fulltime"), &PMMatch::force_fulltime);
	ClassDB::bind_method(D_METHOD("debug_goal", "t"), &PMMatch::debug_goal);
	ClassDB::bind_method(D_METHOD("toggle_auto"), &PMMatch::toggle_auto);
	ClassDB::bind_method(D_METHOD("local_has_ball"), &PMMatch::local_has_ball);
	ClassDB::bind_method(D_METHOD("local_action", "a", "value"), &PMMatch::local_action, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("hud_press", "name"), &PMMatch::hud_press);
	ClassDB::bind_method(D_METHOD("hud_release", "name"), &PMMatch::hud_release);
	ClassDB::bind_method(D_METHOD("hud_joy", "v"), &PMMatch::hud_joy);
	ClassDB::bind_method(D_METHOD("hud_hold", "name", "on"), &PMMatch::hud_hold);
	ClassDB::bind_method(D_METHOD("_leave_after_disconnect"), &PMMatch::_leave_after_disconnect);
	ADD_SIGNAL(MethodInfo("sim_finished", PropertyInfo(Variant::DICTIONARY, "result")));
}

PMMatch::~PMMatch() {
	for (auto *p : players) delete p;
	players.clear();
}

static float randf() { return (float)rng().randf(); }
static float randr(float a, float b) { return (float)rng().range(a, b); }
static float randn(float dev) { return (float)rng().gauss(0.0, dev); }

// ================================================================== setup
void PMMatch::_ready() {
	GameData &g = GameData::get();
	mode = params.get("mode", "quick");
	net_role = params.get("net_role", "");
	is_client = net_role == "client";
	is_host = net_role == "host";
	sim = mode == "sim";
	float minutes = (float)(double)params.get("minutes", g.settings.minutes);
	half_len = minutes * 30.f;
	if (!params.has("home")) params["home"] = sim ? g.build_ai_team(g.base_team_pool()[0]).to_dict() : g.build_my_team().to_dict();
	if (!params.has("away")) params["away"] = g.build_ai_team(g.base_team_pool()[3 % g.base_team_pool().size()]).to_dict();
	teams[0] = MatchTeam::from_dict(params.get("home", Dictionary()));
	teams[1] = MatchTeam::from_dict(params.get("away", Dictionary()));
	int diff = clampi((int)params.get("difficulty", g.settings.difficulty), 0, 2);
	if (mode == "pvp") {
		local_team = is_host ? 0 : 1;
		human[0] = human[1] = true;
	} else if (!sim) {
		local_team = (bool)params.get("user_home", true) ? 0 : 1;
		human[local_team] = true;
	}
	static const float rs[3] = { 0.6f, 0.38f, 0.24f }, sk[3] = { 0.78f, 1.0f, 1.18f };
	for (int t = 0; t < 2; t++) {
		if (human[t] || sim) {
			react[t] = 0.34f;
			ai_skill[t] = 1.f;
		} else {
			react[t] = rs[diff];
			ai_skill[t] = sk[diff];
		}
	}
	dbg["saves"] = 0;
	dbg["passes"] = 0;
	dbg["tackles"] = 0;
	dbg["outs"] = 0;
	pick_colors();
	stadium::build(this, g.board_texts(), team_colors[0], team_colors[1]);
	make_materials();
	spawn_players();
	make_ball();
	cam = memnew(Camera3D);
	cam->set_near(0.5f);
	cam->set_far(400.f);
	add_child(cam);
	cam->set_current(true);
	update_camera(1.f);
	make_hud();
	if (mode == "pvp" && PMNet::singleton) {
		PMNet *net = PMNet::singleton;
		net->connect("net_disconnected", callable_mp(this, &PMMatch::_on_net_disconnected));
		if (is_host) {
			net->connect("remote_input", callable_mp(this, &PMMatch::_on_remote_input));
			net->connect("remote_action", callable_mp(this, &PMMatch::_on_remote_action));
		} else {
			net->connect("snapshot_received", callable_mp(this, &PMMatch::_on_snapshot));
			net->connect("match_event", callable_mp(this, &PMMatch::_on_net_event));
			net->connect("match_over", callable_mp(this, &PMMatch::_on_net_over));
		}
	}
	net_targets.assign(players.size(), Vector3());
	replay_enabled = !sim && g.settings.replays;
	frames.resize(240);
	do_kickoff(0);
	if (net_role != "client") PMSfx::p("whistle");
}

void PMMatch::pick_colors() {
	Color c0 = teams[0].shirt, c1 = teams[1].shirt;
	float d = std::fabs(c0.r - c1.r) + std::fabs(c0.g - c1.g) + std::fabs(c0.b - c1.b);
	if (d < 0.55f) {
		c1 = c0.get_luminance() > 0.5f ? Color::html("111827") : Color::html("f1f5f9");
		teams[1].shorts = c1.get_luminance() > 0.5f ? Color::html("111827") : Color::html("e5e7eb");
	}
	team_colors[0] = c0;
	team_colors[1] = c1;
}

void PMMatch::make_materials() {
	for (int t = 0; t < 2; t++) kits[t] = CharLib::team_kit(team_colors[t], teams[t].shorts, (int)teams[t].name.hash());
	shadow_mat.instantiate();
	Ref<Gradient> gr;
	gr.instantiate();
	gr->set_color(0, Color(0, 0, 0, 0.35f));
	gr->set_color(1, Color(0, 0, 0, 0.0f));
	Ref<GradientTexture2D> gt;
	gt.instantiate();
	gt->set_gradient(gr);
	gt->set_fill(GradientTexture2D::FILL_RADIAL);
	gt->set_fill_from(Vector2(0.5f, 0.5f));
	gt->set_fill_to(Vector2(1.0f, 0.5f));
	gt->set_width(64);
	gt->set_height(64);
	shadow_mat->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, gt);
	shadow_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	shadow_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	shadow_mat->set_depth_draw_mode(BaseMaterial3D::DEPTH_DRAW_DISABLED);
	for (int t = 0; t < 2; t++) {
		Color mc = (t == local_team || !human[t]) ? Color::html("ffe14d") : Color::html("ff5e7a");
		marker_mats[t].instantiate();
		marker_mats[t]->set_albedo(mc);
		marker_mats[t]->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	}
}

void PMMatch::spawn_players() {
	Rng r((uint64_t)(int64_t)params.get("seed", 12345));
	for (int t = 0; t < 2; t++) {
		const auto &slots = GameData::formation(teams[t].formation);
		for (int i = 0; i < 11; i++) {
			const MatchPlayer &mp = teams[t].players[i];
			Footballer *f = new Footballer();
			f->team = t;
			f->idx = i;
			f->gid = t * 11 + i;
			f->data = mp.c;
			f->slot_pos = slots[i].pos;
			f->role = GameData::group(f->slot_pos);
			f->rid = f->role == "GK" ? R_GK : (f->role == "DEF" ? R_DEF : (f->role == "FWD" ? R_FWD : R_MID));
			f->base = Vector2(slots[i].x, slots[i].z);
			if (f->rid == R_GK) f->max_speed = 6.6f + mp.c.gk / 99.f * 1.6f;
			else f->max_speed = 5.6f + mp.c.pace / 99.f * 2.9f;
			KitColors km = kits[t];
			if (f->rid == R_GK) {
				static const Color gkc[2][2] = { { Color(0.64f, 0.9f, 0.21f), Color(0.1f, 0.12f, 0.16f) }, { Color(0.98f, 0.57f, 0.24f), Color(0.12f, 0.1f, 0.1f) } };
				km.shirt = gkc[t][0];
				km.shirt2 = gkc[t][1];
				km.shorts = gkc[t][1];
				km.socks = gkc[t][0];
				km.pattern = 0;
				km.keeper = true;
				km.gloves = Color(0.95f, 0.95f, 0.95f);
			}
			km.skin = CharLib::skin_tone(r.irange(0, 5));
			km.hair = CharLib::hair_tone(r.irange(0, 4));
			f->setup(this, km, mp.c.num, shadow_mat);
			f->tag->set_text(I(mp.c.num) + " " + kit::short_name(mp.c.name));
			players.push_back(f);
			squads[t].push_back(f);
		}
	}
}

void PMMatch::make_ball() {
	Ref<SphereMesh> sm;
	sm.instantiate();
	sm->set_radius(BR);
	sm->set_height(BR * 2);
	sm->set_radial_segments(16);
	sm->set_rings(8);
	ball_node = memnew(MeshInstance3D);
	ball_node->set_mesh(sm);
	Ref<Shader> sh;
	sh.instantiate();
	sh->set_code(R"(shader_type spatial;
varying vec3 lp;
void vertex() { lp = VERTEX; }
void fragment() {
	vec3 n = normalize(lp);
	float a = atan(n.z, n.x);
	float b = acos(clamp(n.y, -1.0, 1.0));
	float c = step(0.5, fract(a * 0.95));
	float d = step(0.5, fract(b * 1.9));
	float k = abs(c - d);
	ALBEDO = mix(vec3(0.97), vec3(0.08, 0.09, 0.12), k * 0.9);
	ROUGHNESS = 0.45;
})");
	Ref<ShaderMaterial> mat;
	mat.instantiate();
	mat->set_shader(sh);
	ball_node->set_material_override(mat);
	ball_node->set_scale(Vector3(1.35f, 1.35f, 1.35f));
	add_child(ball_node);
	ball_shadow = memnew(MeshInstance3D);
	Ref<QuadMesh> q;
	q.instantiate();
	q->set_size(Vector2(0.5f, 0.5f));
	ball_shadow->set_mesh(q);
	ball_shadow->set_material_override(shadow_mat);
	ball_shadow->set_rotation_degrees(Vector3(-90, 0, 0));
	add_child(ball_shadow);
}

void PMMatch::make_hud() {
	CanvasLayer *layer = memnew(CanvasLayer);
	add_child(layer);
	hud = memnew(PMHud);
	hud->m = this;
	layer->add_child(hud);
}

// ================================================================== helpers
Footballer *PMMatch::nearest_field(int t, const Vector3 &pt, const Footballer *exclude) const {
	Footballer *best = nullptr;
	float bd = 1e9f;
	for (auto *p : squads[t]) {
		if (p->rid == R_GK || p == exclude) continue;
		float d = fdist(p->pos, pt);
		if (d < bd) {
			bd = d;
			best = p;
		}
	}
	return best;
}

float PMMatch::nearest_opp_dist_pt(const Vector3 &pt, int t) const {
	float bd = 1e9f;
	for (auto *o : squads[1 - t]) bd = std::min(bd, fdist(o->pos, pt));
	return bd;
}

static float seg_dist(const Vector3 &p, const Vector3 &a, const Vector3 &b) {
	Vector2 ab(b.x - a.x, b.z - a.z), ap(p.x - a.x, p.z - a.z);
	float l2 = ab.length_squared();
	if (l2 < 0.0001f) return ap.length();
	float t = clampf(ap.dot(ab) / l2, 0.f, 1.f);
	return (ap - ab * t).length();
}

float PMMatch::lane_clear(const Vector3 &a, const Vector3 &b, int t) const {
	float bd = 1e9f;
	for (auto *o : squads[1 - t]) bd = std::min(bd, seg_dist(o->pos, a, b));
	return bd;
}

int PMMatch::minute() const {
	int m = (int)(clock / half_len * 45.f);
	return clampi(m, 0, 45) + (half == 2 ? 45 : 0) + 1;
}

Array PMMatch::get_score() const {
	Array a;
	a.push_back(score[0]);
	a.push_back(score[1]);
	return a;
}

Vector3 PMMatch::get_player_pos(int gid) const {
	return gid >= 0 && gid < (int)players.size() ? players[gid]->pos : Vector3();
}

// ================================================================== flow
void PMMatch::do_kickoff(int team) {
	state = KICKOFF;
	state_t = 0.f;
	holder = nullptr;
	pass_target = nullptr;
	shot_live = false;
	setpiece = SetPiece();
	ball_pos = Vector3(0, BR, 0);
	ball_vel = Vector3();
	in_net = false;
	replay = false;
	replay_done = false;
	replay_skipped = false;
	scorer = nullptr;
	assist_cand = nullptr;
	last_passer = nullptr;
	for (auto *p : players) {
		p->pos = kickoff_pos(p, team);
		p->vel = Vector3();
		p->target = p->pos;
		p->facing = dir[p->team] > 0 ? 0.f : (float)Math_PI;
		p->stun = p->kick_cd = p->protect = 0.f;
	}
	Footballer *taker = nullptr;
	float bz = 9.f;
	for (auto *p : squads[team])
		if (p->rid == R_FWD && std::fabs(p->base.y) < bz) {
			bz = std::fabs(p->base.y);
			taker = p;
		}
	if (!taker) taker = squads[team][10];
	taker->pos = Vector3(-0.55f * dir[team], 0, 0.15f);
	gain(taker);
	taker->protect = 1.8f;
	for (int t = 0; t < 2; t++)
		if (human[t] && !autoplay[t]) ctrl[t] = t == team ? taker->gid : nearest_field(t, Vector3())->gid;
}

Vector3 PMMatch::kickoff_pos(const Footballer *p, int kick_team) const {
	float fx = std::min(p->base.x * 1.05f, -0.05f);
	if (p->rid == R_GK) fx = -0.96f;
	Vector3 w = to_world(p->team, fx, p->base.y);
	if (p->team != kick_team && Vector2(w.x, w.z).length() < 10.f) w.x = -std::sqrt(std::max(0.f, 105.f - w.z * w.z)) * dir[p->team];
	return w;
}

void PMMatch::begin_play() {
	state = PLAY;
	state_t = 0.f;
	setpiece = SetPiece();
}

void PMMatch::step(float delta) {
	state_t += delta;
	for (auto *p : players) {
		p->tackle_cd -= delta;
		p->stun -= delta;
		p->kick_cd -= delta;
		p->protect -= delta;
	}
	switch (state) {
		case KICKOFF:
			if (state_t > 1.4f) begin_play();
			break;
		case PLAY:
			clock += delta;
			if (clock >= half_len) {
				if (half == 1) halftime();
				else fulltime();
			}
			break;
		case GOAL: {
			float need = replay_enabled ? 2.2f + 4.f / 0.6f + 0.5f : 3.f;
			if (replay_skipped && state_t > 0.5f) need = std::min(need, state_t);
			if (state_t > need) {
				replay = false;
				do_kickoff(1 - last_scored);
			}
			if (scorer && state_t > 0.9f && state_t < 1.0f) scorer->play_action("Dance", 1.6f);
			break;
		}
		case OUT:
			setpiece_step();
			break;
		case HALF:
			if (state_t > 3.f) {
				half = 2;
				clock = 0.f;
				do_kickoff(1 - first_kickoff);
				PMSfx::p("whistle");
				event("whistle", "", Color(1, 1, 1));
			}
			break;
		case END:
			if (state_t > 2.6f && !finished) finish();
			break;
	}
	if (finished) return;
	if (state == PLAY || state == OUT || state == KICKOFF) {
		rec_t += delta;
		if (rec_t >= 1.f / 30.f) {
			rec_t = 0.f;
			record_frame();
		}
	}
	if (replay) return;
	update_controls(delta);
	ai(delta);
	move_players(delta);
	update_ball(delta);
	if (state == PLAY) check_rules();
	if (holder) poss_time[holder->team] += delta;
}

void PMMatch::halftime() {
	for (auto *p : players) p->stamina = std::min(1.f, p->stamina + 0.35f);
	state = HALF;
	state_t = 0.f;
	holder = nullptr;
	ball_vel = Vector3();
	event("banner", S("ПЕРЕРЫВ"), Color(1, 1, 1));
	PMSfx::p("whistle_long");
}

void PMMatch::fulltime() {
	state = END;
	state_t = 0.f;
	holder = nullptr;
	ball_vel *= 0.2f;
	event("banner", S("КОНЕЦ МАТЧА"), kit::GOLD);
	PMSfx::p("whistle_long");
}

void PMMatch::event(const String &kind, const String &text, Color c) {
	if (kind == "banner" && hud) hud->banner(text, c, 2.4f);
	if (is_host && PMNet::singleton) {
		Dictionary info;
		info["text"] = text;
		info["color"] = c.to_html(false);
		PMNet::singleton->send_event(kind, info);
	}
}

void PMMatch::finish() {
	finished = true;
	float total = std::max(poss_time[0] + poss_time[1], 0.01f);
	Dictionary r;
	r["score"] = get_score();
	Array names, shorts, colors, logos, poss, sh;
	for (int t = 0; t < 2; t++) {
		names.push_back(teams[t].name);
		shorts.push_back(teams[t].shrt);
		colors.push_back(team_colors[t].to_html(false));
		logos.push_back(teams[t].logo);
		poss.push_back((int)std::round(poss_time[t] / total * 100.f));
		sh.push_back(shots[t]);
	}
	r["names"] = names;
	r["shorts"] = shorts;
	r["colors"] = colors;
	r["logos"] = logos;
	r["scorers"] = scorers.duplicate(true);
	r["poss"] = poss;
	r["shots"] = sh;
	Array ratings;
	float best = -1.f;
	int motm = -1;
	for (auto *p : players) {
		Dictionary pr;
		float rt = p->st.rating();
		pr["name"] = p->data.name;
		pr["team"] = p->team;
		pr["id"] = p->data.id;
		pr["pos"] = p->slot_pos;
		pr["rating"] = std::round(rt * 10.f) / 10.f;
		pr["goals"] = p->st.goals;
		pr["assists"] = p->st.assists;
		ratings.push_back(pr);
		if (rt > best) {
			best = rt;
			motm = p->gid;
		}
	}
	r["ratings"] = ratings;
	r["motm"] = motm;
	Array pk;
	for (int t = 0; t < 2; t++) {
		int n = 0;
		for (auto *p : squads[t]) n += p->st.passes_ok;
		pk.push_back(n);
	}
	r["passes_ok"] = pk;
	if (sim) {
		emit_signal("sim_finished", r);
		return;
	}
	if (is_host && PMNet::singleton) PMNet::singleton->send_over(r);
	show_result(r);
}

void PMMatch::show_result(const Dictionary &r) {
	Dictionary p;
	p["result"] = r;
	p["mode"] = mode;
	p["local_team"] = local_team;
	if (mode == "league") p["user_home"] = params.get("user_home", true);
	if (mode == "quick") p["rematch"] = params.duplicate(true);
	PMApp::go("result", p);
}

void PMMatch::exit_match() {
	leaving = true;
	if (mode == "pvp") {
		if (PMNet::singleton) PMNet::singleton->close();
		PMApp::go("pvp");
	} else if (mode == "league") {
		PMApp::go("league");
	} else {
		PMApp::go("menu");
	}
}

void PMMatch::on_back() {
	if (hud) hud->on_back();
}

// ================================================================== loops
void PMMatch::_physics_process(double d) {
	float delta = (float)d;
	if (is_client) {
		input_send_t += delta;
		if (input_send_t >= 1.f / 30.f && PMNet::singleton) {
			input_send_t = 0.f;
			PMNet::singleton->send_input(Vector2(client_move.x, client_move.z), client_sprint, client_press);
		}
		return;
	}
	if (paused || finished) return;
	step(delta);
	if (is_host) {
		snap_timer += delta;
		if (snap_timer >= 1.f / 30.f) {
			snap_timer = 0.f;
			send_snapshot();
		}
	}
}

void PMMatch::_process(double d) {
	float delta = (float)d;
	if (state == GOAL && prev_state != GOAL) {
		goal_clock = 0.f;
		replay_done = false;
	}
	prev_state = state;
	if (state == GOAL) {
		goal_clock += delta;
		if (replay_enabled && !replay && !replay_done && !replay_skipped && goal_clock > 2.2f) start_replay();
	} else if (replay) {
		replay = false;
	}
	if (replay) {
		replay_render(delta);
		return;
	}
	if (is_client) {
		client_render(delta);
	} else {
		for (auto *p : players) p->animate(delta, Vector2(p->vel.x, p->vel.z).length());
		ball_visual(delta);
		update_markers(ctrl, autoplay);
	}
	update_camera(delta);
}

void PMMatch::record_frame() {
	if (frames.empty()) return;
	Frame &f = frames[frame_head];
	f.ball = ball_pos;
	for (int i = 0; i < 22 && i < (int)players.size(); i++) {
		Footballer *p = players[i];
		Vector3 pp = is_client ? p->target : p->pos;
		f.px[i] = pp.x;
		f.pz[i] = pp.z;
		f.pf[i] = is_client ? p->net_facing : p->facing;
		f.kick[i] = p->kick_anim > 0.2f ? 1 : 0;
	}
	if (is_client) f.ball = client_ball;
	frame_head = (frame_head + 1) % (int)frames.size();
	frame_count = std::min(frame_count + 1, (int)frames.size());
}

void PMMatch::start_replay() {
	replay_frames = std::min(frame_count, 120);
	if (replay_frames < 20) {
		replay_done = true;
		return;
	}
	int n = (int)frames.size();
	replay_start = ((frame_head - replay_frames) % n + n) % n;
	replay = true;
	replay_t = 0.f;
	replay_len = replay_frames / 30.f;
	for (auto *p : players) {
		p->set_marker(false, Ref<Material>());
		p->tag->set_visible(false);
	}
	marked[0] = marked[1] = -2;
	cam_look = frames[replay_start].ball;
}

void PMMatch::skip_replay() {
	if (!replay) return;
	replay = false;
	replay_done = true;
	replay_skipped = true;
}

void PMMatch::replay_render(float delta) {
	replay_t += delta * 0.6f;
	if (replay_t >= replay_len) {
		replay = false;
		replay_done = true;
		return;
	}
	int n = (int)frames.size();
	float fi = replay_t * 30.f;
	int a = (int)fi;
	float k = fi - a;
	const Frame &fa = frames[(replay_start + a) % n];
	const Frame &fb = frames[(replay_start + std::min(a + 1, replay_frames - 1)) % n];
	for (int i = 0; i < 22 && i < (int)players.size(); i++) {
		Footballer *p = players[i];
		Vector3 prev = p->pos;
		p->pos = Vector3(lerpf(fa.px[i], fb.px[i], k), 0, lerpf(fa.pz[i], fb.pz[i], k));
		p->facing = Math::lerp_angle(fa.pf[i], fb.pf[i], k);
		if (fa.kick[i] && p->kick_anim <= 0.f) p->kick_anim = 0.3f;
		float sp = (p->pos - prev).length() / std::max(delta * 0.6f, 0.001f);
		p->animate(delta * 0.6f, std::min(sp, 9.f));
	}
	Vector3 b = fa.ball.lerp(fb.ball, k);
	Vector3 bprev = ball_pos;
	ball_pos = b;
	ball_vel = (b - bprev) / std::max(delta * 0.6f, 0.001f);
	ball_visual(delta * 0.6f);
	// cinematic camera: low, from the corner of the goal that was scored in
	float gx = HL * dir[goal_team];
	float side = view_side();
	cam_look = cam_look.lerp(b, 1.f - std::exp(-4.f * delta));
	Vector3 want(gx - dir[goal_team] * 13.f, 3.4f, side * 13.f + b.z * 0.4f);
	cam->set_position(cam->get_position().lerp(want, 1.f - std::exp(-2.5f * delta)));
	cam->set_fov(34.f);
	cam->look_at(cam_look + Vector3(0, 0.6f, 0), Vector3(0, 1, 0));
}

void PMMatch::update_markers(const int *c, const bool *a) {
	for (int t = 0; t < 2; t++) {
		int cur = human[t] && !a[t] ? c[t] : -1;
		if (marked[t] == cur) continue;
		int prev = marked[t];
		if (prev >= 0 && prev < (int)players.size()) {
			players[prev]->set_marker(false, Ref<Material>());
			players[prev]->tag->set_visible(false);
		}
		if (cur >= 0 && cur < (int)players.size()) {
			players[cur]->set_marker(true, marker_mats[t]);
			players[cur]->tag->set_visible(t == local_team);
		}
		marked[t] = cur;
	}
}

void PMMatch::update_camera(float delta) {
	Vector3 b = ball_pos;
	bool close = GameData::get().settings.camera == 1;
	float dz = close ? 30.f : 42.f, h = close ? 11.5f : 16.f;
	float lim = HL - (close ? 12.f : 18.f);
	float side = view_side();
	// look a little ahead of the ball in the direction of play
	Vector3 lead(clampf(ball_vel.x * 0.35f, -6.f, 6.f), 0.f, 0.f);
	Vector3 look(clampf(b.x + lead.x, -lim, lim), 0.f, b.z * 0.82f);
	cam_look = cam_look.lerp(look, 1.f - std::exp(-2.6f * delta));
	float depth = cam_look.z * side;
	float base_fov = close ? 37.f : 35.f;
	cam->set_fov(base_fov - 7.f * clampf(-depth / 30.f, 0.f, 1.f) + 3.f * clampf(depth / 30.f, 0.f, 1.f));
	cam->set_position(Vector3(cam_look.x, h, side * dz + cam_look.z * 0.3f));
	cam->look_at(cam_look, Vector3(0, 1, 0));
}

void PMMatch::ball_visual(float delta) {
	ball_node->set_position(ball_pos);
	Vector3 hv(ball_vel.x, 0.f, ball_vel.z);
	if (hv.length() > 0.2f) {
		Vector3 axis = Vector3(0, 1, 0).cross(hv).normalized();
		ball_node->rotate(axis, hv.length() / BR * delta);
	}
	ball_shadow->set_position(Vector3(ball_pos.x + ball_pos.y * 0.25f, 0.025f, ball_pos.z + ball_pos.y * 0.12f));
	float s = clampf(1.f - ball_pos.y * 0.07f, 0.35f, 1.f);
	ball_shadow->set_scale(Vector3(s, s, s));
}

String PMMatch::clock_text() const {
	if (state == END) return "90'";
	if (state == HALF) return "45'";
	return I(minute()) + "'";
}

// ================================================================== human input
void PMMatch::update_controls(float delta) {
	for (int t = 0; t < 2; t++) {
		eff_move[t] = Vector3();
		if (!human[t] || autoplay[t]) {
			ctrl[t] = -1;
			continue;
		}
		Vector3 mv = in_move[t];
		if (mv.length() < 0.1f) idle_t[t] += delta;
		else idle_t[t] = 0.f;
		Footballer *c = ctrl[t] >= 0 ? players[ctrl[t]] : nullptr;
		if (holder && holder->team == t && holder->rid != R_GK) {
			ctrl[t] = holder->gid;
			c = holder;
		} else if (!holder || holder->team != t) {
			if (pass_target && pass_target->team == t && !holder) {
				ctrl[t] = pass_target->gid;
				c = pass_target;
			} else {
				Footballer *near = nearest_field(t, ball_ground());
				if (!c || c->rid == R_GK) {
					c = near;
					ctrl[t] = near->gid;
				} else if (near != c && idle_t[t] > 0.3f && fdist(c->pos, ball_pos) > fdist(near->pos, ball_pos) + 4.f) {
					c = near;
					ctrl[t] = near->gid;
				}
			}
		}
		if (!c) {
			c = nearest_field(t, ball_ground());
			ctrl[t] = c->gid;
		}
		Vector3 em = mv;
		if (in_press[t] && holder && holder->team != t && state == PLAY) {
			Vector3 tg = holder->pos + (own_goal(t) - holder->pos).normalized() * 0.5f;
			Vector3 d = tg - c->pos;
			d.y = 0;
			em = d.length() > 0.3f ? d.normalized() : Vector3();
			try_tackle(c, false, 0.07f);
		} else if (mv.length() < 0.1f && !holder && pass_target == c) {
			Vector3 d2 = ball_ground() + Vector3(ball_vel.x, 0, ball_vel.z) * 0.25f - c->pos;
			d2.y = 0;
			em = d2.normalized() * std::min(1.f, d2.length() / 2.f);
		}
		eff_move[t] = em;
	}
}

void PMMatch::human_action(int t, const String &action, float value) {
	if (!human[t] || autoplay[t] || finished || ctrl[t] < 0) return;
	Footballer *c = players[ctrl[t]];
	bool has = holder == c;
	bool restart = state == KICKOFF || state == OUT;
	if (restart) {
		if (!has) return;
	} else if (state != PLAY) {
		return;
	}
	if (action == "pass") {
		if (has) human_pass(t, c, "ground");
		else switch_player(t);
	} else if (action == "through") {
		if (has) human_pass(t, c, "through");
	} else if (action == "lob") {
		if (has) human_pass(t, c, "lob");
	} else if (action == "shoot") {
		if (has) {
			if (restart) {
				human_pass(t, c, "lob");
			} else {
				float aim = 0.f;
				bool ha = human_aim(t, aim);
				shoot(c, value > 0.f ? value : 0.8f, ha, aim, true);
			}
		} else {
			try_tackle(c, true, 1.f);
		}
	}
	if (restart && holder != c) begin_play();
}

void PMMatch::switch_player(int t) {
	Footballer *cur = ctrl[t] >= 0 ? players[ctrl[t]] : nullptr;
	Footballer *near = nearest_field(t, ball_ground(), cur);
	if (near) {
		ctrl[t] = near->gid;
		idle_t[t] = 0.f;
	}
}

bool PMMatch::human_aim(int t, float &aim) const {
	Vector3 d = in_move[t];
	if (d.length() < 0.25f) return false;
	aim = clampf(d.z * 1.6f, -1.f, 1.f) * (GW - 0.35f);
	return true;
}

void PMMatch::human_pass(int t, Footballer *c, const String &kind) {
	Vector3 d = in_move[t];
	if (d.length() < 0.2f) d = c->forward();
	d = d.normalized();
	Footballer *best = nullptr;
	float bs = -99.f;
	for (auto *m : squads[t]) {
		if (m == c) continue;
		Vector3 v = m->pos - c->pos;
		v.y = 0;
		float dist = v.length();
		if (dist < 3.f || dist > 55.f) continue;
		float a = v.normalized().dot(d);
		if (a < 0.45f) continue;
		float s = a * 2.2f - dist / 35.f + std::min(lane_clear(c->pos, m->pos, t), 4.f) / 8.f;
		if (m->rid == R_GK) s -= 0.8f;
		if (s > bs) {
			bs = s;
			best = m;
		}
	}
	bool thr = state == OUT && setpiece.active && setpiece.type == "throw";
	if (!best) {
		Vector3 tg = c->pos + d * (kind == "lob" ? 26.f : 14.f);
		tg.x = clampf(tg.x, -HL - 2.f, HL + 2.f);
		tg.z = clampf(tg.z, -HW + 0.5f, HW - 0.5f);
		kick_to(c, tg, thr ? "throw" : (kind == "lob" ? "lob" : "ground"), (float)c->data.pas);
		return;
	}
	if (thr) {
		kick_to(c, best->pos + best->vel * 0.3f, "throw", (float)c->data.pas);
		pass_target = best;
		ctrl[t] = best->gid;
		return;
	}
	Vector3 lead = (attack_vec(t) * 0.6f + d * 0.4f).normalized();
	bool lob = kind == "lob" || (kind == "ground" && fdist(c->pos, best->pos) > 34.f);
	do_pass(c, best, kind == "through", lob, lead);
}

void PMMatch::local_move(Vector2 v) {
	float side = view_side();
	Vector3 w(v.x * side, 0.f, -v.y * side);
	if (w.length() > 1.f) w = w.normalized();
	if (is_client) client_move = w;
	else in_move[local_team] = w;
}

void PMMatch::local_sprint(bool b) {
	if (is_client) client_sprint = b;
	else in_sprint[local_team] = b;
}

void PMMatch::local_press(bool b) {
	if (is_client) client_press = b;
	else in_press[local_team] = b;
}

void PMMatch::local_action(const String &a, float value) {
	if (is_client) {
		if (PMNet::singleton) PMNet::singleton->send_action(a, value);
	} else if (!sim) {
		human_action(local_team, a, value);
	}
}

void PMMatch::toggle_auto() {
	bool on = !local_auto();
	if (is_client) {
		client_auto[local_team] = on;
		if (PMNet::singleton) PMNet::singleton->send_action("auto", on ? 1.f : 0.f);
	} else {
		autoplay[local_team] = on;
		if (!on) ctrl[local_team] = -1;
	}
	if (hud) hud->banner(on ? S("АВТОИГРА ВКЛ") : S("АВТОИГРА ВЫКЛ"), on ? Color::html("4ade80") : Color(1, 1, 1), 1.f);
}

bool PMMatch::local_auto() const {
	if (is_client) return client_auto[local_team];
	return autoplay[local_team];
}

bool PMMatch::local_has_ball() const {
	if (is_client) return client_owner >= 0 && client_owner == client_ctrl[local_team];
	return holder && holder->team == local_team && holder->rid != R_GK;
}

void PMMatch::hud_press(const String &n) {
	if (hud) hud->press(n);
}
void PMMatch::hud_release(const String &n) {
	if (hud) hud->release(n);
}
void PMMatch::hud_joy(Vector2 v) {
	if (hud) hud->set_test_joy(v);
}
void PMMatch::hud_hold(const String &n, bool on) {
	if (hud) hud->set_hold(n, on);
}

// ================================================================== AI
void PMMatch::ai(float delta) {
	for (int t = 0; t < 2; t++) {
		int poss = poss_team();
		Footballer *chaser = nullptr, *cover = nullptr;
		if (poss != t && state == PLAY) {
			Vector3 focus = holder ? holder->pos : predict(0.4f);
			std::vector<Footballer *> list;
			for (auto *p : squads[t])
				if (p->rid != R_GK) list.push_back(p);
			std::sort(list.begin(), list.end(), [&](Footballer *a, Footballer *b) { return fdist(a->pos, focus) < fdist(b->pos, focus); });
			chaser = list[0];
			cover = list[1];
		}
		for (auto *p : squads[t]) {
			if (is_human_ctrl(p) && state != GOAL) {
				if (p == holder) p->target = p->pos;
				continue;
			}
			if (p == holder) {
				if (state == PLAY) ai_carrier(p, delta);
				else p->target = p->pos;
				continue;
			}
			if (p->rid == R_GK) {
				ai_gk(p);
				continue;
			}
			if (state == OUT) {
				p->target = setpiece_target(p);
				p->urgency = 0.75f;
				continue;
			}
			if (state == GOAL && scorer) {
				if (p == scorer) {
					p->target = celebrate_spot;
					p->urgency = 0.95f;
				} else if (p->team == scorer->team && p->rid != R_GK) {
					float a = p->idx * 0.9f;
					p->target = scorer->pos + Vector3(std::cos(a), 0, std::sin(a)) * (1.6f + (p->idx % 3) * 0.6f);
					p->urgency = 0.85f;
				} else {
					p->target = formation_target(p);
					p->urgency = 0.35f;
				}
				continue;
			}
			if (state != PLAY) {
				p->target = formation_target(p);
				p->urgency = 0.45f;
				continue;
			}
			if (p == pass_target && !holder) {
				p->target = ball_ground() + Vector3(ball_vel.x, 0, ball_vel.z) * 0.3f;
				p->urgency = 0.9f;
				continue;
			}
			if (p == chaser) {
				if (!holder) {
					p->target = intercept(p);
					p->urgency = 1.f;
				} else {
					Vector3 gl = own_goal(t);
					p->target = holder->pos + (gl - holder->pos).normalized() * 0.7f;
					p->urgency = fdist(p->pos, holder->pos) > 3.f ? 1.f : 0.8f;
					try_tackle(p, false, 0.05f * ai_skill[t]);
				}
				continue;
			}
			if (p == cover && holder && holder->team == 1 - t) {
				Vector3 g2 = own_goal(t);
				int press = teams[t].press;
				if (press == 2) {
					p->target = holder->pos + (g2 - holder->pos).normalized() * 2.f;
					p->urgency = 0.9f;
					try_tackle(p, false, 0.03f * ai_skill[t]);
				} else {
					p->target = holder->pos + (g2 - holder->pos).normalized() * (7.f - press * 2.f);
					p->urgency = 0.75f;
				}
				continue;
			}
			p->target = formation_target(p);
			p->urgency = fdist(p->pos, p->target) < 4.f ? 0.5f : 0.8f;
		}
	}
}

Vector3 PMMatch::predict(float t) const {
	Vector3 v(ball_vel.x, 0, ball_vel.z);
	float k = std::max(0.25f, 1.f - 0.2f * t);
	Vector3 p = ball_ground() + v * t * k;
	p.x = clampf(p.x, -HL - 1.f, HL + 1.f);
	p.z = clampf(p.z, -HW - 1.f, HW + 1.f);
	return p;
}

Vector3 PMMatch::intercept(const Footballer *p) const {
	for (int i = 1; i < 25; i++) {
		float t = i * 0.08f;
		Vector3 bp = predict(t);
		if (fdist(p->pos, bp) <= p->max_speed * t + 0.6f) return bp;
	}
	return predict(2.f);
}

Vector3 PMMatch::formation_target(const Footballer *p) const {
	int t = p->team;
	int poss = poss_team();
	bool attacking = poss == t || (poss == -1 && last_team == t);
	float bx = clampf(tx(t, ball_pos), -1.f, 1.f);
	float bz = clampf(tz(t, ball_pos), -1.f, 1.f);
	const MatchTeam &tm = teams[t];
	float fx = p->base.x + bx * 0.5f + (tm.line - 1) * 0.06f;
	if (attacking) {
		fx += 0.22f + (tm.style - 1) * 0.05f;
		if (p->rid == R_FWD) fx += 0.12f;
		else if (p->rid == R_MID) fx += 0.04f;
		fx += 0.04f * std::sin(clock * 0.6f + p->gid);
	} else if (p->rid == R_DEF) {
		fx = std::min(fx, bx - 0.05f);
	}
	fx = clampf(fx, -0.9f, 0.86f);
	float fz = p->base.y * (attacking ? 1.f : 0.78f) + bz * 0.28f;
	fz = clampf(fz, -0.92f, 0.92f);
	if (state == KICKOFF) fx = std::min(fx, -0.04f);
	return to_world(t, fx, fz);
}

Vector3 PMMatch::gk_home(const Footballer *p) const {
	Vector3 g = own_goal(p->team);
	Vector3 tb = ball_ground() - g;
	float d = tb.length();
	float off = clampf(d * 0.1f, 0.8f, 4.5f);
	Vector3 tg = d > 0.1f ? g + tb.normalized() * off : g + attack_vec(p->team);
	tg.z = clampf(tg.z, -GW + 0.3f, GW - 0.3f);
	return tg;
}

void PMMatch::ai_gk(Footballer *p) {
	int t = p->team;
	Vector3 g = own_goal(t);
	Vector3 tg = gk_home(p);
	p->urgency = 0.7f;
	if (!holder && state == PLAY) {
		float vt = -ball_vel.x * dir[t];
		if (vt > 6.f) {
			float tt = (std::fabs(ball_pos.x - g.x) - 0.5f) / vt;
			if (tt > 0.f && tt < 1.8f) {
				float cz = ball_pos.z + ball_vel.z * tt;
				if (std::fabs(cz) < GW + 1.5f) {
					tg = Vector3(g.x + dir[t] * 0.6f, 0, clampf(cz, -GW + 0.2f, GW - 0.2f));
					p->urgency = 1.f;
				}
			}
		}
		bool in_box = std::fabs(ball_pos.x - g.x) < 16.5f && std::fabs(ball_pos.z) < 20.16f;
		if (in_box && ball_vel.length() < 8.f && ball_pos.y < 1.5f) {
			float gd = fdist(p->pos, ball_pos);
			float od = nearest_opp_dist_pt(ball_pos, t);
			if (gd < od + 1.f) {
				tg = ball_ground();
				p->urgency = 1.f;
			}
		}
	}
	p->target = tg;
}

void PMMatch::ai_carrier(Footballer *p, float delta) {
	int t = p->team;
	if (p->rid == R_GK) {
		p->hold += delta;
		p->target = p->pos;
		if (p->hold > 1.2f) gk_distribute(p);
		return;
	}
	p->think -= delta;
	if (p->think > 0.f) {
		p->target = p->pos + p->plan * 5.f;
		return;
	}
	p->think = react[t] * randr(0.75f, 1.3f);
	Vector3 goal = opp_goal(t);
	Vector3 tg = goal - p->pos;
	tg.y = 0;
	float dg = tg.length();
	float press = nearest_opp_dist(p);
	if (dg < 27.f && std::fabs(p->pos.z) < 22.f) {
		float block = shot_block(p);
		float q = clampf((28.f - dg) / 17.f, 0.f, 1.f) * (1.f - block * 0.5f);
		if (dg < 12.5f || randf() < q * 0.7f) {
			shoot(p, randr(0.7f, 0.95f), false, 0.f, false);
			return;
		}
	}
	PassOpt best = best_pass(p);
	if (best.mate) {
		float thr = 1.45f + randf() * 0.6f;
		if ((press < 2.8f && best.score > 0.1f) || best.score > thr) {
			do_pass(p, best.mate, best.through, best.lob);
			return;
		}
	}
	Vector3 d = tg.normalized();
	if (dg < 30.f && std::fabs(p->pos.z) > 12.f) d.z -= signf(p->pos.z) * 0.6f;
	for (auto *o : squads[1 - t]) {
		Vector3 od = o->pos - p->pos;
		od.y = 0;
		float l = od.length();
		if (l < 7.f && l > 0.01f && od.normalized().dot(d) > -0.2f) d -= od.normalized() * (1.f - l / 7.f) * 1.4f;
	}
	if (std::fabs(p->pos.z) > HW - 3.f) d.z -= signf(p->pos.z) * 1.f;
	if (std::fabs(p->pos.x) > HL - 4.f && std::fabs(p->pos.z) > 10.f) d.x -= signf(p->pos.x) * 0.6f;
	d.y = 0;
	if (d.length() < 0.05f) d = attack_vec(t);
	p->plan = d.normalized();
	p->target = p->pos + p->plan * 5.f;
	p->urgency = press > 5.f ? 1.f : 0.82f;
}

float PMMatch::shot_block(const Footballer *p) const {
	Vector3 goal = opp_goal(p->team);
	float b = 0.f;
	for (auto *o : squads[1 - p->team]) {
		if (o->rid == R_GK) continue;
		if (seg_dist(o->pos, p->pos, goal) < 1.4f) b += 0.35f;
	}
	return clampf(b, 0.f, 1.f);
}

PMMatch::PassOpt PMMatch::best_pass(const Footballer *p) const {
	int t = p->team;
	PassOpt best;
	float px = tx(t, p->pos);
	bool pressed = nearest_opp_dist(p) < 3.f;
	for (auto *m : squads[t]) {
		if (m == p) continue;
		if (m->rid == R_GK && !pressed) continue;
		Vector3 v = m->pos - p->pos;
		v.y = 0;
		float d = v.length();
		if (d < 5.f || d > 42.f) continue;
		float prog = (tx(t, m->pos) - px) * HL / 14.f;
		float lane = lane_clear(p->pos, m->pos, t);
		bool lob = false;
		if (lane < 1.3f) {
			if (d > 18.f) {
				lob = true;
				lane = 2.f;
			} else {
				continue;
			}
		}
		float free = nearest_opp_dist_pt(m->pos, t);
		float s = prog * 0.85f + std::min(lane, 5.f) / 5.f + std::min(free, 7.f) / 7.f - d / 50.f;
		if (lob) s -= 0.35f;
		if (m->rid == R_GK) s -= 1.f;
		bool through = false;
		float mx = tx(t, m->pos);
		if (m->rid != R_DEF && m->rid != R_GK && mx > px && free > 3.f && mx < 0.85f) {
			if (nearest_opp_dist_pt(m->pos + attack_vec(t) * 5.f, t) > 3.5f) {
				through = true;
				s += 0.35f;
			}
		}
		if (s > best.score) best = { m, s, through, lob };
	}
	return best;
}

void PMMatch::do_pass(Footballer *p, Footballer *mate, bool through, bool lob, Vector3 lead) {
	int t = p->team;
	Vector3 tg = mate->pos + mate->vel * 0.35f;
	if (through) {
		Vector3 lv = lead.length() > 0.1f ? lead : attack_vec(t);
		tg = mate->pos + lv * 5.f + mate->vel * 0.5f;
	}
	tg.x = clampf(tg.x, -HL + 1.f, HL - 1.f);
	tg.z = clampf(tg.z, -HW + 1.f, HW - 1.f);
	kick_to(p, tg, lob ? "lob" : "ground", (float)p->data.pas);
	pass_target = mate;
	last_passer = p;
	p->st.passes++;
	dbg["passes"] = (int)dbg["passes"] + 1;
	if (human[t] && !autoplay[t]) ctrl[t] = mate->gid;
}

void PMMatch::kick_to(Footballer *p, Vector3 tg, const String &kind, float skill) {
	Vector3 flat = tg - ball_pos;
	flat.y = 0;
	float d = std::max(flat.length(), 0.5f);
	float err = (100.f - skill) / 100.f * 0.11f / ai_skill[p->team];
	flat = flat.rotated(Vector3(0, 1, 0), randn(err));
	Vector3 dv = flat.normalized();
	if (kind == "ground") {
		float spd = clampf((d * 1.3f + 13.f) / 2.05f, 8.f, 28.f);
		ball_vel = dv * spd;
		ball_vel.y = 0;
	} else if (kind == "lob") {
		float sh = clampf(11.f + d * 0.32f, 13.f, 26.f);
		float tt = d / sh;
		ball_vel = dv * sh;
		ball_vel.y = std::min(0.5f * G * tt, 17.f);
	} else {
		float sh2 = clampf(6.f + d * 0.5f, 7.f, 16.f);
		float t2 = d / sh2;
		ball_vel = dv * sh2;
		ball_vel.y = 0.5f * G * t2;
		ball_pos.y = 1.6f;
	}
	release(p);
}

void PMMatch::release(Footballer *p) {
	holder = nullptr;
	gk_hands = false;
	last_kicker = p;
	last_team = p->team;
	p->kick_cd = 0.35f;
	p->kick_anim = 0.28f;
	p->protect = 0.f;
	tried.clear();
	gk_tried.clear();
	shot_live = false;
	pass_target = nullptr;
	ball_pos.y = std::max(ball_pos.y, BR);
	PMSfx::p("kick", -4.f);
}

void PMMatch::shoot(Footballer *p, float power, bool has_aim, float aim_z, bool human_shot) {
	int t = p->team;
	Vector3 goal = opp_goal(t);
	Footballer *gk = squads[1 - t][0];
	if (!has_aim) {
		float side = randf() < 0.5f ? 1.f : -1.f;
		if (std::fabs(gk->pos.z) > 0.4f) side = -signf(gk->pos.z);
		aim_z = side * randr(1.2f, GW - 0.35f);
	}
	float skill = (float)p->data.sho;
	float aim_y = human_shot ? 0.35f + power * 1.3f + std::max(0.f, power - 0.88f) * 9.f : randr(0.25f, 1.9f);
	Vector3 tg(goal.x, aim_y, aim_z);
	Vector3 flat = tg - ball_pos;
	flat.y = 0;
	float d = std::max(flat.length(), 1.f);
	float err = ((100.f - skill) / 100.f * 0.085f + d / 300.f) / ai_skill[t];
	flat = flat.rotated(Vector3(0, 1, 0), randn(err));
	float spd = (18.f + skill / 99.f * 12.f) * lerpf(0.7f, 1.08f, clampf(power, 0.f, 1.05f));
	float tt = d / spd;
	ball_vel = flat.normalized() * spd;
	ball_vel.y = (aim_y - ball_pos.y) / tt + 0.5f * G * tt + randn(err * 4.f);
	last_shot_d = d;
	release(p);
	shot_live = true;
	shots[t]++;
	p->st.shots++;
}

void PMMatch::gk_distribute(Footballer *gk) {
	int t = gk->team;
	gk->protect = 0.f;
	PassOpt best = best_pass(gk);
	if (best.mate) do_pass(gk, best.mate, false, fdist(gk->pos, best.mate->pos) > 26.f);
	else kick_to(gk, to_world(t, 0.1f, randr(-0.5f, 0.5f)), "lob", (float)gk->data.pas);
}

void PMMatch::try_tackle(Footballer *p, bool forced, float prob) {
	if (!holder || holder->team == p->team || p->tackle_cd > 0.f || p->stun > 0.f) return;
	if (holder->protect > 0.f) return;
	float d = fdist(p->pos, holder->pos);
	if (d > (forced ? 1.9f : 1.45f)) return;
	if (!forced && randf() > prob) return;
	p->tackle_cd = forced ? 0.55f : 0.9f;
	float chance = clampf(0.36f + ((float)p->data.def - (float)holder->data.dri) / 85.f, 0.12f, 0.8f);
	if (!is_human_ctrl(p)) chance = std::min(chance * ai_skill[p->team], 0.85f);
	if (forced && d > 1.45f) chance *= 0.6f;
	if (randf() < chance) {
		Footballer *loser = holder;
		loser->stun = 0.55f;
		loser->play_action("Hit_Chest", 0.5f);
		loser->st.lost++;
		p->st.tackles++;
		dbg["tackles"] = (int)dbg["tackles"] + 1;
		if (randf() < 0.55f) {
			gain(p);
		} else {
			Vector3 away = p->pos - loser->pos;
			away.y = 0;
			Vector3 dv = (away.normalized() * 0.7f + Vector3(randr(-0.5f, 0.5f), 0, randr(-0.5f, 0.5f))).normalized();
			holder = nullptr;
			gk_hands = false;
			ball_vel = dv * randr(4.f, 7.f);
			last_team = p->team;
			last_kicker = p;
			tried.clear();
			p->kick_cd = 0.15f;
			loser->kick_cd = 0.4f;
		}
		PMSfx::p("kick", -6.f);
	} else {
		p->stun = forced ? 0.3f : 0.15f;
	}
}

void PMMatch::gain(Footballer *p) {
	if (last_kicker && last_kicker != p && last_kicker == last_passer) {
		if (last_kicker->team == p->team) {
			last_kicker->st.passes_ok++;
			assist_cand = last_kicker;
		} else {
			assist_cand = nullptr;
		}
	} else if (last_kicker && last_kicker->team != p->team) {
		assist_cand = nullptr;
	}
	last_passer = nullptr;
	holder = p;
	gk_hands = false;
	last_team = p->team;
	last_kicker = p;
	pass_target = nullptr;
	shot_live = false;
	p->protect = 0.3f;
	p->think = react[p->team] * 0.5f;
	p->hold = 0.f;
	ball_vel = Vector3();
	if (human[p->team] && !autoplay[p->team] && p->rid != R_GK) ctrl[p->team] = p->gid;
}

void PMMatch::gk_catch(Footballer *gk) {
	if (shot_live) {
		dbg["saves"] = (int)dbg["saves"] + 1;
		gk->st.saves++;
	}
	gain(gk);
	gk_hands = true;
	gk->hold = 0.f;
	gk->protect = 9.f;
	PMSfx::p("save", -4.f);
}

// ================================================================== movement & ball
void PMMatch::move_players(float delta) {
	for (auto *p : players) {
		int t = p->team;
		Vector3 desired;
		bool hm = is_human_ctrl(p) && (state == PLAY || (state == OUT && p != holder));
		if (state == GOAL) hm = false;
		float ms = p->max_speed * (0.78f + 0.22f * p->stamina);
		if (p->stun > 0.f) {
			desired = Vector3();
		} else if (hm) {
			desired = eff_move[t] * (ms * (in_sprint[t] ? 1.f : 0.74f));
		} else {
			Vector3 to = p->target - p->pos;
			to.y = 0;
			float d = to.length();
			if (d > 0.3f) {
				float spd = ms * lerpf(0.5f, 1.f, clampf(p->urgency, 0.f, 1.f));
				spd *= std::min(1.f, d / 2.f + 0.15f);
				desired = to / d * spd;
			}
		}
		if (p == holder) {
			desired *= 0.9f;
			if (state != PLAY) desired = Vector3();
		}
		float acc = desired.length() > p->vel.length() ? 20.f : 26.f;
		p->vel = p->vel.move_toward(desired, acc * delta);
		p->pos += p->vel * delta;
		float spn = Vector2(p->vel.x, p->vel.z).length();
		if (state == PLAY) {
			if (spn > p->max_speed * 0.8f) p->stamina -= 0.045f * delta * (1.2f - p->data.phy / 200.f);
			else p->stamina += (spn < 1.5f ? 0.05f : 0.022f) * delta;
		} else {
			p->stamina += 0.06f * delta;
		}
		p->stamina = clampf(p->stamina, 0.f, 1.f);
		p->pos.x = clampf(p->pos.x, -HL - 3.5f, HL + 3.5f);
		p->pos.z = clampf(p->pos.z, -HW - 2.5f, HW + 2.5f);
		p->pos.y = 0;
	}
	size_t n = players.size();
	for (size_t i = 0; i < n; i++) {
		Footballer *a = players[i];
		for (size_t j = i + 1; j < n; j++) {
			Footballer *b = players[j];
			float dx = b->pos.x - a->pos.x, dz = b->pos.z - a->pos.z;
			float d2 = dx * dx + dz * dz;
			if (d2 < 0.72f && d2 > 0.0001f) {
				float d = std::sqrt(d2);
				float push = (0.85f - d) * 0.5f;
				float nx = dx / d, nz = dz / d;
				a->pos.x -= nx * push;
				a->pos.z -= nz * push;
				b->pos.x += nx * push;
				b->pos.z += nz * push;
			}
		}
	}
	for (auto *p : players) {
		Vector3 fv(p->vel.x, 0, p->vel.z);
		float tf = p->facing;
		Vector3 em = eff_move[p->team];
		if (p == holder && state != PLAY) {
		} else if (p == holder && is_human_ctrl(p) && em.length() > 0.1f) {
			tf = std::atan2(em.z, em.x);
		} else if (fv.length() > 0.6f) {
			tf = std::atan2(fv.z, fv.x);
		} else {
			Vector3 tb = ball_pos - p->pos;
			tf = std::atan2(tb.z, tb.x);
		}
		p->facing = Math::lerp_angle(p->facing, tf, std::min(1.f, delta * (p == holder ? 14.f : 8.f)));
	}
}

void PMMatch::update_ball(float delta) {
	if (holder) {
		Vector3 fwd = holder->forward();
		Vector3 tg;
		if (holder->rid == R_GK && gk_hands) {
			tg = holder->pos + fwd * 0.35f + Vector3(0, 1.05f, 0);
		} else {
			tg = holder->pos + fwd * 0.62f;
			tg.y = BR;
		}
		ball_pos = ball_pos.lerp(tg, std::min(1.f, delta * 20.f));
		ball_vel = holder->vel;
		return;
	}
	ball_vel.y -= G * delta;
	ball_pos += ball_vel * delta;
	if (ball_pos.y < BR) {
		ball_pos.y = BR;
		if (ball_vel.y < -2.f) {
			ball_vel.y = -ball_vel.y * 0.48f;
			ball_vel.x *= 0.88f;
			ball_vel.z *= 0.88f;
		} else {
			ball_vel.y = 0;
		}
	}
	Vector2 h(ball_vel.x, ball_vel.z);
	float sp = h.length();
	if (ball_pos.y <= BR + 0.03f && std::fabs(ball_vel.y) < 0.5f) sp = std::max(0.f, sp - (2.5f + 0.35f * sp) * delta);
	else sp *= (1.f - 0.07f * delta);
	h = h.length() > 0.0001f ? h.normalized() * sp : Vector2();
	ball_vel.x = h.x;
	ball_vel.z = h.y;
	goal_frame();
	if (in_net && std::fabs(ball_pos.x) > HL) {
		float sx = signf(ball_pos.x);
		ball_pos.x = clampf(std::fabs(ball_pos.x), HL, HL + 1.9f) * sx;
		ball_pos.z = clampf(ball_pos.z, -GW + 0.12f, GW - 0.12f);
		ball_pos.y = std::min(ball_pos.y, GH - 0.15f);
		if (std::fabs(ball_pos.x) >= HL + 1.89f) ball_vel *= 0.2f;
	}
	if (state == PLAY) {
		gk_logic();
		if (!holder) pickup();
	}
}

void PMMatch::goal_frame() {
	for (float s : { -1.f, 1.f }) {
		float gx = HL * s;
		for (float zs : { -1.f, 1.f }) {
			if (ball_pos.y < GH + BR) {
				float dx = ball_pos.x - gx, dz = ball_pos.z - GW * zs;
				float d = std::sqrt(dx * dx + dz * dz);
				if (d < POST_R + BR && d > 0.0001f) {
					Vector3 nrm(dx / d, 0, dz / d);
					float vn = ball_vel.dot(nrm);
					if (vn < 0.f) {
						ball_vel -= nrm * vn * 1.7f;
						PMSfx::p("kick", -2.f);
					}
					ball_pos.x = gx + nrm.x * (POST_R + BR + 0.01f);
					ball_pos.z = GW * zs + nrm.z * (POST_R + BR + 0.01f);
				}
			}
		}
		if (std::fabs(ball_pos.z) < GW) {
			float dx2 = ball_pos.x - gx, dy = ball_pos.y - GH;
			float d2 = std::sqrt(dx2 * dx2 + dy * dy);
			if (d2 < POST_R + BR && d2 > 0.0001f) {
				Vector3 n2(dx2 / d2, dy / d2, 0);
				float vn2 = ball_vel.dot(n2);
				if (vn2 < 0.f) {
					ball_vel -= n2 * vn2 * 1.7f;
					PMSfx::p("kick", -2.f);
				}
				ball_pos.x = gx + n2.x * (POST_R + BR + 0.01f);
				ball_pos.y = GH + n2.y * (POST_R + BR + 0.01f);
			}
		}
		if (std::fabs(ball_pos.x) > HL && signf(ball_pos.x) == s && std::fabs(ball_pos.z) < GW + 0.05f && ball_pos.y < GH) {
			if (std::fabs(ball_pos.x) > HL + 1.9f) {
				ball_pos.x = (HL + 1.9f) * s;
				ball_vel.x *= -0.15f;
				ball_vel.z *= 0.4f;
				ball_vel.y *= 0.4f;
			}
			if (std::fabs(ball_pos.z) > GW - 0.12f) {
				ball_pos.z = (GW - 0.12f) * signf(ball_pos.z);
				ball_vel.z *= -0.2f;
				ball_vel.x *= 0.5f;
			}
			if (ball_pos.y > GH - 0.15f) {
				ball_pos.y = GH - 0.15f;
				ball_vel.y = -std::fabs(ball_vel.y) * 0.2f;
			}
		}
	}
}

void PMMatch::gk_logic() {
	if (holder) return;
	for (int t = 0; t < 2; t++) {
		Footballer *gk = squads[t][0];
		if (gk->stun > 0.f || gk->kick_cd > 0.f) continue;
		Vector3 g = own_goal(t);
		bool in_box = std::fabs(ball_pos.x - g.x) < 16.5f && std::fabs(ball_pos.z) < 20.16f;
		if (!in_box) continue;
		float dist = fdist(gk->pos, ball_pos);
		float sp = ball_vel.length();
		if (dist < 1.3f && ball_pos.y < 2.3f && sp < 9.f) {
			gk_catch(gk);
			return;
		}
		if (gk_tried.count(gk->gid)) continue;
		float reach = 1.f + gk->data.gk / 99.f * 1.35f;
		Vector3 rel = ball_pos - (gk->pos + Vector3(0, 1.f, 0));
		float horiz = Vector2(rel.x, rel.z).length();
		if (horiz < reach && ball_pos.y < 2.75f) {
			gk_tried.insert(gk->gid);
			bool toward = -ball_vel.x * dir[t] > 2.f;
			float chance = clampf(0.78f + (gk->data.gk - 70.f) / 60.f - (sp - 20.f) / 40.f - horiz / reach * 0.18f +
							clampf((last_shot_d - 12.f) / 40.f, 0.f, 0.25f),
					0.12f, 0.95f);
			if (!toward) chance = 0.97f;
			if (!human[t]) chance = clampf(chance * (0.85f + 0.15f * ai_skill[t]), 0.1f, 0.96f);
			float f = gk->facing;
			Vector3 right(-std::sin(f), 0, std::cos(f));
			gk->dive_side = signf(rel.dot(right));
			if (horiz > 0.7f) gk->dive_anim = 0.8f;
			if (randf() < chance) {
				if (sp < 21.f && randf() < 0.6f) {
					gk_catch(gk);
					return;
				}
				ball_vel = Vector3(std::fabs(ball_vel.x) * 0.3f * dir[t] + dir[t] * 3.f, randr(2.5f, 5.f), signf(rel.z + 0.001f) * randr(4.f, 9.f));
				last_team = t;
				last_kicker = gk;
				shot_live = false;
				tried.clear();
				gk->kick_cd = 0.6f;
				gk->st.saves++;
				dbg["saves"] = (int)dbg["saves"] + 1;
				PMSfx::p("save", -2.f);
			} else {
				gk->kick_cd = 0.4f;
			}
		}
	}
}

void PMMatch::pickup() {
	Footballer *best = nullptr;
	float bestd = 99.f;
	float sp = ball_vel.length();
	for (auto *p : players) {
		if (p->kick_cd > 0.f || p->stun > 0.f || p->rid == R_GK) continue;
		float d = fdist(p->pos, ball_pos);
		if (d > 1.05f) continue;
		if (ball_pos.y < 0.9f) {
			if (tried.count(p->gid)) continue;
			float chance = 1.f;
			if (sp > 11.f) chance = clampf(1.3f - sp / 24.f + (p->data.dri - 60.f) / 150.f, 0.2f, 1.f);
			if (p == pass_target) chance = std::max(chance, 0.95f);
			else if (pass_target && p->team != last_team) chance *= 0.85f;
			if (randf() > chance) {
				tried.insert(p->gid);
				ball_vel = ball_vel.rotated(Vector3(0, 1, 0), randr(-0.6f, 0.6f)) * 0.7f;
				last_team = p->team;
				continue;
			}
			if (d < bestd) {
				best = p;
				bestd = d;
			}
		} else if (ball_pos.y < 2.4f && d < 0.85f && !tried.count(p->gid)) {
			header(p);
			return;
		}
	}
	if (best) gain(best);
}

void PMMatch::header(Footballer *p) {
	int t = p->team;
	Vector3 goal = opp_goal(t);
	bool near_goal = std::fabs(p->pos.x - goal.x) < 17.f && std::fabs(p->pos.z) < 16.f;
	if (p == pass_target && !near_goal && ball_vel.y < 0.f && ball_pos.y < 1.8f) {
		gain(p);
		return;
	}
	if (near_goal) {
		Vector3 tg(goal.x, randr(0.3f, 1.6f), randr(-GW + 0.5f, GW - 0.5f));
		Vector3 flat = tg - ball_pos;
		flat.y = 0;
		float d = std::max(flat.length(), 1.f);
		float skill = (p->data.sho + p->data.phy) * 0.5f;
		flat = flat.rotated(Vector3(0, 1, 0), randn((100.f - skill) / 100.f * 0.16f / ai_skill[t]));
		float spd = 12.f + skill / 99.f * 7.f;
		float tt = d / spd;
		ball_vel = flat.normalized() * spd;
		ball_vel.y = (tg.y - ball_pos.y) / tt + 0.5f * G * tt;
		last_shot_d = d;
	} else {
		Vector3 dv = (attack_vec(t) + Vector3(0, 0, randr(-0.6f, 0.6f))).normalized();
		ball_vel = dv * randr(10.f, 14.f);
		ball_vel.y = randr(3.f, 6.f);
	}
	release(p);
	tried.insert(p->gid);
	if (near_goal) {
		shot_live = true;
		shots[t]++;
		p->st.shots++;
	}
}

// ================================================================== rules & set pieces
void PMMatch::check_rules() {
	if (std::fabs(ball_pos.x) > HL + BR) {
		if (std::fabs(ball_pos.z) < GW && ball_pos.y < GH) {
			goal_scored(ball_pos.x > 0.f ? 0 : 1);
		} else {
			int def_team = ball_pos.x < 0.f ? 0 : 1;
			if (last_team == def_team) start_setpiece("corner", 1 - def_team);
			else start_setpiece("goal_kick", def_team);
		}
		return;
	}
	if (std::fabs(ball_pos.z) > HW + BR) start_setpiece("throw", 1 - last_team);
}

void PMMatch::debug_goal(int t) {
	if (state != PLAY) return;
	float sx = t == 0 ? 1.f : -1.f;
	Footballer *fw = nullptr;
	for (auto *p : squads[t])
		if (p->rid == R_FWD) fw = p;
	if (!fw) fw = squads[t].back();
	fw->pos = Vector3(sx * (HL - 9.f), 0.f, 1.f);
	last_kicker = fw;
	ball_pos = Vector3(sx * (HL - 8.f), 0.3f, 0.8f);
	ball_vel = Vector3(sx * 24.f, 1.2f, -0.6f);
	holder = nullptr;
}

void PMMatch::goal_scored(int st) {
	score[st]++;
	goal_team = st;
	scorer = (last_kicker && last_kicker->team == st) ? last_kicker : nullptr;
	if (scorer) {
		scorer->st.goals++;
		if (assist_cand && assist_cand->team == st && assist_cand != scorer) assist_cand->st.assists++;
		float zs = scorer->pos.z >= 0 ? 1.f : -1.f;
		celebrate_spot = Vector3(clampf(scorer->pos.x, -HL + 6, HL - 6) - dir[st] * 2.f, 0, clampf(scorer->pos.z + zs * 9.f, -HW + 3, HW - 3));
	}
	String nm = scorer ? scorer->data.name : S("Автогол");
	Dictionary s;
	s["team"] = st;
	s["name"] = nm;
	s["min"] = minute();
	scorers.push_back(s);
	in_net = true;
	state = GOAL;
	state_t = 0.f;
	holder = nullptr;
	last_scored = st;
	event("banner", S("ГОЛ!"), team_colors[st].lightened(0.3f));
	event("goal", nm, team_colors[st]);
	PMSfx::p("whistle");
	PMSfx::p("cheer");
}

Vector3 PMMatch::sp_dir() const {
	int t = setpiece.team;
	Vector3 spot = setpiece.spot;
	if (setpiece.type == "throw") return Vector3(dir[t] * 0.5f, 0, -signf(spot.z)).normalized();
	if (setpiece.type == "corner") {
		Vector3 v = (opp_goal(t) - attack_vec(t) * 10.f) - spot;
		v.y = 0;
		return v.normalized();
	}
	return attack_vec(t);
}

void PMMatch::start_setpiece(const String &type, int team) {
	state = OUT;
	state_t = 0.f;
	holder = nullptr;
	pass_target = nullptr;
	shot_live = false;
	Vector3 spot;
	if (type == "throw") {
		spot = Vector3(clampf(ball_pos.x, -HL + 1.f, HL - 1.f), BR, signf(ball_pos.z) * (HW + 0.3f));
	} else if (type == "corner") {
		float gx = opp_goal(team).x;
		float zs = std::fabs(ball_pos.z) > 0.1f ? signf(ball_pos.z) : 1.f;
		spot = Vector3(gx - signf(gx) * 0.6f, BR, zs * (HW - 0.6f));
	} else {
		spot = Vector3(own_goal(team).x + dir[team] * 5.5f, BR, 0);
	}
	ball_pos = spot;
	ball_vel = Vector3();
	Footballer *taker = type == "goal_kick" ? squads[team][0] : nearest_field(team, spot);
	setpiece.active = true;
	setpiece.type = type;
	setpiece.team = team;
	setpiece.spot = spot;
	setpiece.taker = taker;
	if (human[team] && !autoplay[team] && taker->rid != R_GK) ctrl[team] = taker->gid;
	String txt = type == "throw" ? S("АУТ") : (type == "corner" ? S("УГЛОВОЙ") : S("ОТ ВОРОТ"));
	dbg["outs"] = (int)dbg["outs"] + 1;
	event("banner", txt, Color(1, 1, 1));
	if (hud) hud->set_banner_time(1.1f);
	PMSfx::p("whistle", -6.f);
}

void PMMatch::setpiece_step() {
	if (!setpiece.active) {
		begin_play();
		return;
	}
	Footballer *taker = setpiece.taker;
	Vector3 stand = setpiece.spot - sp_dir() * 0.6f;
	stand.y = 0;
	if (state_t < 0.9f) {
		taker->target = stand;
		taker->urgency = 1.f;
		return;
	}
	if (!holder) {
		taker->pos = stand;
		taker->vel = Vector3();
		Vector3 fd = sp_dir();
		taker->facing = std::atan2(fd.z, fd.x);
		gain(taker);
		taker->protect = 6.f;
		ball_pos = setpiece.spot;
	}
	float delay = is_human_ctrl(taker) ? 4.5f : 1.8f;
	if (state_t > delay) {
		ai_setpiece_kick(taker);
		begin_play();
	}
}

void PMMatch::ai_setpiece_kick(Footballer *taker) {
	int t = taker->team;
	taker->protect = 0.f;
	if (setpiece.type == "throw") {
		PassOpt best = best_pass(taker);
		if (best.mate && fdist(taker->pos, best.mate->pos) < 25.f) {
			Footballer *mate = best.mate;
			kick_to(taker, mate->pos + mate->vel * 0.3f, "throw", (float)taker->data.pas);
			pass_target = mate;
		} else {
			kick_to(taker, taker->pos + Vector3(dir[t] * 8.f, 0, -signf(taker->pos.z) * 6.f), "throw", (float)taker->data.pas);
		}
	} else if (setpiece.type == "corner") {
		Vector3 tg = opp_goal(t) + Vector3(-dir[t] * randr(5.f, 11.f), 0, randr(-6.f, 6.f));
		kick_to(taker, tg, "lob", (float)taker->data.pas);
	} else {
		gk_distribute(taker);
	}
}

Vector3 PMMatch::setpiece_target(const Footballer *p) const {
	if (!setpiece.active) return formation_target(p);
	if (p == setpiece.taker) {
		Vector3 st = setpiece.spot - sp_dir() * 0.6f;
		st.y = 0;
		return st;
	}
	if (setpiece.type == "corner") {
		int att = setpiece.team;
		Vector3 goal = opp_goal(att);
		static const Vector2 spots[7] = { Vector2(6, -4), Vector2(8, 3), Vector2(11, 0), Vector2(5, 5), Vector2(13, -6), Vector2(16, 4), Vector2(20, 0) };
		int k = p->idx % 7;
		if (p->team == att) {
			if (p->rid == R_DEF && p->base.x < -0.6f && std::fabs(p->base.y) > 0.5f) return formation_target(p);
			return goal + Vector3(-dir[att] * spots[k].x, 0, spots[k].y);
		}
		Vector2 s2 = spots[(k + 2) % 7];
		return goal + Vector3(-dir[att] * std::max(3.f, s2.x - 2.f), 0, s2.y * 0.8f);
	}
	return formation_target(p);
}

// ================================================================== networking
void PMMatch::_on_remote_input(Vector2 move, bool sprint, bool press) {
	in_move[1] = Vector3(move.x, 0, move.y);
	in_sprint[1] = sprint;
	in_press[1] = press;
}

void PMMatch::_on_remote_action(const String &a, float v) {
	if (a == "auto") {
		autoplay[1] = v > 0.5f;
		if (!autoplay[1]) ctrl[1] = -1;
	} else {
		human_action(1, a, v);
	}
}

void PMMatch::send_snapshot() {
	PackedFloat32Array a;
	a.resize(16 + 22 * 5);
	float *w = a.ptrw();
	w[0] = clock;
	w[1] = (float)state;
	w[2] = (float)score[0];
	w[3] = (float)score[1];
	w[4] = ball_pos.x;
	w[5] = ball_pos.y;
	w[6] = ball_pos.z;
	w[7] = holder ? (float)holder->gid : -1.f;
	w[8] = (float)ctrl[0];
	w[9] = (float)ctrl[1];
	w[10] = autoplay[0] ? 1.f : 0.f;
	w[11] = autoplay[1] ? 1.f : 0.f;
	w[12] = (float)half;
	w[13] = half_len;
	w[14] = ball_vel.x;
	w[15] = ball_vel.z;
	for (int i = 0; i < 22; i++) {
		Footballer *p = players[i];
		int b = 16 + i * 5;
		w[b] = p->pos.x;
		w[b + 1] = p->pos.z;
		w[b + 2] = p->facing;
		w[b + 3] = p->kick_anim;
		w[b + 4] = p->dive_anim * p->dive_side;
	}
	if (PMNet::singleton) PMNet::singleton->send_snapshot(a);
}

void PMMatch::_on_snapshot(const PackedFloat32Array &a) {
	if (a.size() < 16 + 22 * 5) return;
	have_snap = true;
	clock = a[0];
	state = (State)clampi((int)a[1], 0, 5);
	score[0] = (int)a[2];
	score[1] = (int)a[3];
	client_ball = Vector3(a[4], a[5], a[6]);
	client_owner = (int)a[7];
	client_ctrl[0] = (int)a[8];
	client_ctrl[1] = (int)a[9];
	client_auto[0] = a[10] > 0.5f;
	client_auto[1] = a[11] > 0.5f;
	half = (int)a[12];
	half_len = std::max(10.f, (float)a[13]);
	ball_vel = Vector3(a[14], 0, a[15]);

	for (int i = 0; i < 22; i++) {
		Footballer *p = players[i];
		int b = 16 + i * 5;
		p->target = Vector3(a[b], 0, a[b + 1]);
		p->net_facing = a[b + 2];
		if (a[b + 3] > 0.2f && p->kick_anim <= 0.f) p->kick_anim = 0.28f;
		if (std::fabs(a[b + 4]) > 0.5f && p->dive_anim <= 0.f) {
			p->dive_anim = 0.8f;
			p->dive_side = signf(a[b + 4]);
		}
	}
	if (state == PLAY || state == OUT || state == KICKOFF) record_frame();
}

void PMMatch::client_render(float delta) {
	if (!have_snap) return;
	float k = 1.f - std::exp(-18.f * delta);
	for (auto *p : players) {
		Vector3 prev = p->pos;
		p->pos = p->pos.lerp(p->target, k);
		float sp = (p->pos - prev).length() / std::max(delta, 0.001f);
		p->facing = Math::lerp_angle(p->facing, p->net_facing, k);
		p->animate(delta, sp);
	}
	ball_pos = ball_pos.lerp(client_ball, 1.f - std::exp(-25.f * delta));
	ball_visual(delta);
	update_markers(client_ctrl, client_auto);
}

void PMMatch::_on_net_event(const String &kind, const Dictionary &info) {
	Color c = html(info.get("color", "#ffffff"));
	String txt = info.get("text", "");
	if (kind == "banner") {
		if (hud) hud->banner(txt, c, txt.length() > 4 ? 2.4f : 1.1f);
		if (txt == S("ПЕРЕРЫВ") || txt == S("КОНЕЦ МАТЧА")) PMSfx::p("whistle_long");
		else PMSfx::p("whistle", -6.f);
	} else if (kind == "goal") {
		PMSfx::p("whistle");
		PMSfx::p("cheer");
	} else if (kind == "whistle") {
		PMSfx::p("whistle");
	}
}

void PMMatch::_on_net_over(const Dictionary &r) {
	finished = true;
	show_result(r);
}

void PMMatch::_on_net_disconnected() {
	if (finished || leaving) return;
	finished = true;
	if (hud) hud->banner(S("СОПЕРНИК ОТКЛЮЧИЛСЯ"), kit::RED, 3.f);
	Ref<SceneTreeTimer> tm = get_tree()->create_timer(2.5);
	tm->connect("timeout", Callable(this, "_leave_after_disconnect"));
}

void PMMatch::_leave_after_disconnect() {
	if (PMNet::singleton) PMNet::singleton->close();
	PMApp::go("pvp");
}

} // namespace pm
