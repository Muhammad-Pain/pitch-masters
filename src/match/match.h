#pragma once
#include <godot_cpp/classes/label3d.hpp>
#include "../game_data.h"
#include "footballer.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>

#include <set>

namespace pm {

class PMHud;

// Match controller: rules, ball physics, team AI, human input, PvP sync.
class PMMatch : public godot::Node3D {
	GDCLASS(PMMatch, godot::Node3D)

public:
	enum State { KICKOFF, PLAY, GOAL, OUT, HALF, END };
	static constexpr float HL = 52.5f, HW = 34.f, GW = 3.66f, GH = 2.44f, BR = 0.14f, G = 9.8f, POST_R = 0.07f;

	Dictionary params;
	String mode = "quick", net_role;
	bool is_client = false, is_host = false;
	bool sim = false;
	MatchTeam teams[2];
	Color team_colors[2];
	std::vector<Footballer *> players;
	std::vector<Footballer *> squads[2];
	float dir[2] = { 1.f, -1.f };
	bool human[2] = { false, false }, autoplay[2] = { false, false };
	int ctrl[2] = { -1, -1 };
	Vector3 in_move[2], eff_move[2];
	bool in_sprint[2] = { false, false }, in_press[2] = { false, false }, in_tpress[2] = { false, false };
	float press_burst[2] = { 0.f, 0.f };
	float crowd_excite = 0.f, screen_t = 0.f;
	godot::Label3D *big_screen = nullptr;
	float idle_t[2] = { 0, 0 }, ai_skill[2] = { 1, 1 }, react[2] = { 0.35f, 0.35f };
	int local_team = 0;

	Vector3 ball_pos = Vector3(0, BR, 0), ball_vel;
	Footballer *holder = nullptr, *last_kicker = nullptr, *pass_target = nullptr;
	Footballer *last_passer = nullptr, *assist_cand = nullptr, *scorer = nullptr;
	int last_team = 0;
	bool shot_live = false, gk_hands = false, in_net = false;
	std::set<int> tried, gk_tried;
	float last_shot_d = 0.f;

	State state = KICKOFF;
	float state_t = 0.f, clock = 0.f, half_len = 180.f;
	int half = 1, score[2] = { 0, 0 }, first_kickoff = 0, last_scored = 0;
	Array scorers;
	struct SetPiece {
		bool active = false;
		String type;
		int team = 0;
		Vector3 spot;
		Footballer *taker = nullptr;
	} setpiece;
	bool paused = false, finished = false, leaving = false;
	float poss_time[2] = { 0, 0 };
	int shots[2] = { 0, 0 };
	Dictionary dbg;
	// goal replay
	struct Frame {
		Vector3 ball;
		float px[22], pz[22], pf[22];
		uint8_t kick[22];
	};
	std::vector<Frame> frames; // ring buffer
	int frame_head = 0, frame_count = 0;
	float rec_t = 0.f;
	bool replay = false, replay_enabled = true, replay_done = false, replay_skipped = false;
	float goal_clock = 0.f;
	State prev_state = KICKOFF;
	Vector3 celebrate_spot;
	void skip_replay();
	float replay_t = 0.f, replay_len = 0.f;
	int replay_start = 0, replay_frames = 0, goal_team = 0;
	void record_frame();
	void start_replay();
	void replay_render(float delta);

	godot::Camera3D *cam = nullptr;
	Vector3 cam_look;
	godot::MeshInstance3D *ball_node = nullptr, *ball_shadow = nullptr;
	PMHud *hud = nullptr;
	godot::Ref<godot::StandardMaterial3D> marker_mats[2], shadow_mat;
	KitColors kits[2];
	int marked[2] = { -2, -2 };

	// networking
	float snap_timer = 0.f, input_send_t = 0.f;
	bool have_snap = false;
	int client_ctrl[2] = { -1, -1 }, client_owner = -1;
	bool client_auto[2] = { false, false };
	Vector3 client_ball = Vector3(0, BR, 0), client_move;
	bool client_sprint = false, client_press = false;
	std::vector<Vector3> net_targets;

protected:
	static void _bind_methods();

public:
	~PMMatch();
	void _ready() override;
	void _physics_process(double delta) override;
	void _process(double delta) override;

	// HUD / input API
	void local_move(Vector2 v);
	void local_sprint(bool b);
	void local_press(bool b);
	void local_action(const String &a, float value = 0.f);
	void toggle_auto();
	bool local_auto() const;
	bool local_has_ball() const;
	float view_side() const { return local_team == 0 ? 1.f : -1.f; }
	String clock_text() const;
	bool can_pause() const { return mode != "pvp"; }
	void set_paused(bool p) { paused = p; }
	void exit_match();
	void on_back();
	Vector3 opp_goal(int t) const { return Vector3(HL * dir[t], 0, 0); }
	Vector3 own_goal(int t) const { return Vector3(-HL * dir[t], 0, 0); }

	// test helpers (bound)
	Array get_score() const;
	float get_clock() const { return clock; }
	int get_state() const { return (int)state; }
	int get_local_team() const { return local_team; }
	int get_ctrl(int t) const { return ctrl[t]; }
	bool get_autoplay(int t) const { return autoplay[t]; }
	Vector3 get_ball_pos() const { return ball_pos; }
	Vector3 get_in_move(int t) const { return in_move[t]; }
	Vector3 get_player_pos(int gid) const;
	Dictionary get_dbg() const { return dbg; }
	void force_fulltime() { fulltime(); }
	void debug_goal(int t);
	void hud_press(const String &name);
	void hud_release(const String &name);
	void hud_joy(Vector2 v);
	void hud_hold(const String &name, bool on);

private:
	void pick_colors();
	void make_materials();
	void spawn_players();
	void make_ball();
	void make_hud();

	Vector3 to_world(int t, float fx, float fz) const { return Vector3(fx * HL * dir[t], 0, fz * HW * dir[t]); }
	float tx(int t, const Vector3 &p) const { return p.x / HL * dir[t]; }
	float tz(int t, const Vector3 &p) const { return p.z / HW * dir[t]; }
	Vector3 attack_vec(int t) const { return Vector3(dir[t], 0, 0); }
	Vector3 ball_ground() const { return Vector3(ball_pos.x, 0, ball_pos.z); }
	int poss_team() const { return holder ? holder->team : -1; }
	bool is_human_ctrl(const Footballer *p) const { return human[p->team] && !autoplay[p->team] && ctrl[p->team] == p->gid; }
	Footballer *nearest_field(int t, const Vector3 &pt, const Footballer *exclude = nullptr) const;
	float nearest_opp_dist_pt(const Vector3 &pt, int t) const;
	float nearest_opp_dist(const Footballer *p) const { return nearest_opp_dist_pt(p->pos, p->team); }
	float lane_clear(const Vector3 &a, const Vector3 &b, int t) const;
	int minute() const;

	void do_kickoff(int team);
	Vector3 kickoff_pos(const Footballer *p, int kick_team) const;
	void begin_play();
	void step(float delta);
	void halftime();
	void fulltime();
	void event(const String &kind, const String &text, Color c);
	void finish();
	void show_result(const Dictionary &r);
	void update_markers(const int *c, const bool *a);
	void update_camera(float delta);
	void ball_visual(float delta);

	void update_controls(float delta);
	void human_action(int t, const String &a, float value);
	void switch_player(int t);
	bool human_aim(int t, float &aim) const;
	void do_slide(Footballer *c, Vector3 dir);
	void gesture_tap(int t, Footballer *c, bool has, Vector3 w);
	void gesture_swipe(int t, Footballer *c, bool has, Vector3 dir, float power);
	void human_pass(int t, Footballer *c, const String &kind);

	void ai(float delta);
	Vector3 predict(float t) const;
	Vector3 intercept(const Footballer *p) const;
	Vector3 formation_target(const Footballer *p) const;
	Vector3 gk_home(const Footballer *p) const;
	void ai_gk(Footballer *p);
	void ai_carrier(Footballer *p, float delta);
	float shot_block(const Footballer *p) const;
	struct PassOpt {
		Footballer *mate = nullptr;
		float score = -99.f;
		bool through = false, lob = false;
	};
	PassOpt best_pass(const Footballer *p) const;
	void do_pass(Footballer *p, Footballer *mate, bool through, bool lob, Vector3 lead = Vector3());
	void kick_to(Footballer *p, Vector3 tgt, const String &kind, float skill);
	void release(Footballer *p);
	void shoot(Footballer *p, float power, bool has_aim, float aim_z, bool human_shot);
	void gk_distribute(Footballer *gk);
	void try_tackle(Footballer *p, bool forced, float prob);
	void gain(Footballer *p);
	void gk_catch(Footballer *gk);

	void move_players(float delta);
	void update_ball(float delta);
	void goal_frame();
	void gk_logic();
	void pickup();
	void header(Footballer *p);

	void check_rules();
	void goal_scored(int st);
	Vector3 sp_dir() const;
	void start_setpiece(const String &type, int team);
	void setpiece_step();
	void ai_setpiece_kick(Footballer *taker);
	Vector3 setpiece_target(const Footballer *p) const;

	void send_snapshot();

public:
	void _on_remote_input(Vector2 move, bool sprint, bool press);
	void _on_remote_action(const String &a, float v);
	void _on_snapshot(const godot::PackedFloat32Array &a);
	void _on_net_event(const String &kind, const Dictionary &info);
	void _on_net_over(const Dictionary &r);
	void _on_net_disconnected();
	void _leave_after_disconnect();
	void client_render(float delta);
};

} // namespace pm
