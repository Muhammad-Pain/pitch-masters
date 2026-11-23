#include "hud.h"
#include "../game_data.h"
#include "../sfx.h"
#include "../ui_kit.h"
#include "match.h"

#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_screen_drag.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include "footballer.h"

using namespace godot;

namespace pm {

void PMHud::_ready() {
	kit::full_rect(this);
	set_mouse_filter(MOUSE_FILTER_IGNORE);
	font = get_theme_default_font();
	bold = kit::font_bold();
	if (bold.is_null()) bold = font;
	controls = GameData::get().settings.controls;
}

bool PMHud::button_visible(const String &name) const { return controls < 2 || name == "sprint"; }

bool PMHud::screen_to_pitch(Vector2 sp, Vector3 &out) const {
	if (!m || !m->cam) return false;
	Vector3 o = m->cam->project_ray_origin(sp);
	Vector3 n = m->cam->project_ray_normal(sp);
	if (n.y > -0.02f) return false;
	float t = -o.y / n.y;
	out = o + n * t;
	out.y = 0.f;
	return true;
}

void PMHud::finish_gesture(Vector2 p) {
	float dt = (float)(Time::get_singleton()->get_ticks_msec() - ges_t0) / 1000.f;
	Vector2 d = p - ges_start;
	ges_index = -1;
	if (!m) return;
	if (d.length() < 26.f && dt < 0.4f) {
		Vector3 w;
		if (screen_to_pitch(p, w)) {
			m->local_action("tap:" + String::num(w.x, 2) + ":" + String::num(w.z, 2));
			fx_pos = p;
			fx_t = 0.5f;
			fx_swipe = false;
		}
	} else if (d.length() > 55.f && dt < 0.7f) {
		Vector3 a, b;
		if (screen_to_pitch(ges_start, a) && screen_to_pitch(p, b)) {
			Vector3 dir = b - a;
			float speed = d.length() / std::max(dt, 0.05f);
			float power = clampf(0.5f + d.length() / (get_size().y * 0.9f) + speed / 9000.f, 0.5f, 1.05f);
			m->local_action("swipe:" + String::num(dir.x, 3) + ":" + String::num(dir.z, 3), power);
			fx_from = ges_start;
			fx_pos = p;
			fx_t = 0.45f;
			fx_swipe = true;
		}
	}
}

void PMHud::rrect(Rect2 r, Color c, float radius, Color border, float bw) {
	Ref<StyleBoxFlat> sb;
	sb.instantiate();
	sb->set_bg_color(c);
	sb->set_corner_radius_all((int)radius);
	sb->set_corner_detail(6);
	if (bw > 0.f) {
		sb->set_border_width_all((int)bw);
		sb->set_border_color(border);
	}
	sb->set_anti_aliased(true);
	draw_style_box(sb, r);
}

void PMHud::text_b(Vector2 p, const String &t, int fs, Color c, float max_w) {
	float w = bold->get_string_size(t, HORIZONTAL_ALIGNMENT_LEFT, -1, fs).x;
	while (max_w > 0.f && w > max_w && fs > 10) {
		fs -= 1;
		w = bold->get_string_size(t, HORIZONTAL_ALIGNMENT_LEFT, -1, fs).x;
	}
	draw_string(bold, Vector2(p.x - w * 0.5f, p.y + fs * 0.36f), t, HORIZONTAL_ALIGNMENT_LEFT, -1, fs, c);
}

void PMHud::layout() {
	Vector2 s = get_size();
	if (!buttons.empty() && s == last_size) return;
	last_size = s;
	// eFootball-style cluster: big shoot/press on the right, pass below, through above, lob left, sprint on top
	buttons["shoot"] = { Vector2(s.x - 110, s.y - 150), 62.f, Color::html("ff5a6a"), S("УДАР"), S("ПРЕССИНГ") };
	buttons["pass"] = { Vector2(s.x - 236, s.y - 74), 56.f, Color::html("60a5fa"), S("ПАС"), S("СМЕНА") };
	buttons["through"] = { Vector2(s.x - 252, s.y - 222), 50.f, Color::html("4ade80"), S("РАЗРЕЗ"), S("ПОДКАТ") };
	buttons["lob"] = { Vector2(s.x - 376, s.y - 112), 46.f, Color::html("22d3ee"), S("НАВЕС"), S("КОМ.ПРЕСС") };
	buttons["sprint"] = { Vector2(s.x - 112, s.y - 296), 46.f, Color::html("fbbf24"), S("РЫВОК"), S("РЫВОК") };
}

Rect2 PMHud::pause_rect() const { return Rect2(Vector2(get_size().x - 78, 16), Vector2(60, 52)); }
Rect2 PMHud::auto_rect() const { return Rect2(Vector2(get_size().x - 236, 16), Vector2(146, 52)); }

void PMHud::banner(const String &t, Color c, float dur) {
	banner_text = t;
	banner_color = c;
	banner_t = dur;
}

void PMHud::_process(double delta) {
	layout();
	if (banner_t > 0.f) banner_t -= (float)delta;
	keyboard();
	queue_redraw();
}

void PMHud::keyboard() {
	if (overlay || !m) return;
	Input *in = Input::get_singleton();
	Vector2 k;
	if (in->is_physical_key_pressed(KEY_A) || in->is_physical_key_pressed(KEY_LEFT)) k.x -= 1;
	if (in->is_physical_key_pressed(KEY_D) || in->is_physical_key_pressed(KEY_RIGHT)) k.x += 1;
	if (in->is_physical_key_pressed(KEY_W) || in->is_physical_key_pressed(KEY_UP)) k.y += 1;
	if (in->is_physical_key_pressed(KEY_S) || in->is_physical_key_pressed(KEY_DOWN)) k.y -= 1;
	if (joy_index == -1 && !test_joy) joy_vec = k.length() > 0 ? k.normalized() : Vector2();
	m->local_move(joy_vec);
	m->local_sprint(btn_touch.count("sprint") || in->is_physical_key_pressed(KEY_SHIFT));
	bool has = m->local_has_ball();
	m->local_press(!has && ((btn_touch.count("shoot") && press_hold > 0.15f) || in->is_physical_key_pressed(KEY_L)));
}

void PMHud::_unhandled_key_input(const Ref<InputEvent> &e) {
	if (!m || overlay) return;
	Ref<InputEventKey> k = e;
	if (k.is_null() || k->is_echo()) return;
	switch (k->get_physical_keycode()) {
		case KEY_J:
			if (k->is_pressed()) m->local_action("pass");
			break;
		case KEY_L:
			if (k->is_pressed()) m->local_action("through");
			break;
		case KEY_I:
			if (k->is_pressed()) m->local_action("lob");
			break;
		case KEY_K:
			if (k->is_pressed()) press("shoot");
			else release("shoot");
			break;
		case KEY_P:
		case KEY_ESCAPE:
			if (k->is_pressed()) open_pause();
			break;
		default:
			break;
	}
}

void PMHud::_input(const Ref<InputEvent> &e) {
	if (!m || overlay) return;
	Ref<InputEventScreenTouch> st = e;
	if (st.is_valid()) {
		touch(st->get_index(), st->get_position(), st->is_pressed());
		return;
	}
	Ref<InputEventScreenDrag> sd = e;
	if (sd.is_valid()) {
		drag(sd->get_index(), sd->get_position());
		return;
	}
	Ref<InputEventMouseButton> mb = e;
	if (mb.is_valid() && mb->get_device() != InputEvent::DEVICE_ID_EMULATION && mb->get_button_index() == MOUSE_BUTTON_LEFT) {
		touch(99, mb->get_position(), mb->is_pressed());
		return;
	}
	Ref<InputEventMouseMotion> mm = e;
	if (mm.is_valid() && mm->get_device() != InputEvent::DEVICE_ID_EMULATION && (joy_index == 99 || ges_index == 99)) drag(99, mm->get_position());
}

void PMHud::touch(int index, Vector2 p, bool pressed) {
	if (pressed && m->replay && m->mode != "pvp") {
		m->skip_replay();
		get_viewport()->set_input_as_handled();
		return;
	}
	if (pressed) {
		if (pause_rect().has_point(p)) {
			open_pause();
			get_viewport()->set_input_as_handled();
			return;
		}
		if (auto_rect().has_point(p)) {
			m->toggle_auto();
			PMSfx::p("click");
			get_viewport()->set_input_as_handled();
			return;
		}
		for (auto &kv : buttons) {
			if (!button_visible(kv.first)) continue;
			if (p.distance_to(kv.second.pos) < kv.second.r * 1.25f) {
				btn_touch[kv.first] = index;
				press(kv.first);
				get_viewport()->set_input_as_handled();
				return;
			}
		}
		if (controls >= 1 && ges_index == -1 && p.x > get_size().x * 0.42f && p.y > 90.f && !m->local_auto()) {
			ges_index = index;
			ges_start = p;
			ges_cur = p;
			ges_t0 = Time::get_singleton()->get_ticks_msec();
			get_viewport()->set_input_as_handled();
			return;
		}
		if (p.x < get_size().x * 0.5f && joy_index == -1 && p.y > get_size().y * 0.25f) {
			joy_index = index;
			joy_center = p;
			joy_vec = Vector2();
			test_joy = false;
			get_viewport()->set_input_as_handled();
		}
	} else {
		if (index == ges_index) finish_gesture(p);
		if (index == joy_index) {
			joy_index = -1;
			joy_vec = Vector2();
		}
		std::vector<String> rel;
		for (auto &kv : btn_touch)
			if (kv.second == index) rel.push_back(kv.first);
		for (auto &n : rel) {
			btn_touch.erase(n);
			release(n);
		}
	}
}

void PMHud::drag(int index, Vector2 p) {
	if (index == ges_index) {
		ges_cur = p;
		return;
	}
	if (index != joy_index) return;
	Vector2 d = p - joy_center;
	if (d.length() > JOY_R) {
		joy_center = p - d.normalized() * JOY_R;
		d = p - joy_center;
	}
	Vector2 v = d / JOY_R;
	v.y = -v.y;
	joy_vec = v.length() > 0.18f ? v : Vector2();
	get_viewport()->set_input_as_handled();
}

void PMHud::press(const String &name) {
	if (!m) return;
	bool has = m->local_has_ball();
	if (name == "shoot") {
		if (has) shoot_hold = 0.f;
		else press_hold = 0.f; // hold = pressing (keyboard() feeds local_press), quick tap = tackle
	} else if (name == "pass") {
		m->local_action("pass");
	} else if (name == "through") {
		m->local_action(has ? String("through") : String("slide"));
	} else if (name == "lob") {
		if (has) m->local_action("lob");
		else {
			tpress_on = true;
			m->local_action("tpress_on");
		}
	}
}

void PMHud::release(const String &name) {
	if (name == "lob" && tpress_on && m) {
		tpress_on = false;
		m->local_action("tpress_off");
	}
	if (name == "shoot" && press_hold >= 0.f && m) {
		if (press_hold < 0.22f) m->local_action("shoot");
		press_hold = -1.f;
	}
	if (name == "shoot" && shoot_hold >= 0.f && m) {
		float power = clampf(0.45f + shoot_hold * 0.75f, 0.45f, 1.05f);
		shoot_hold = -1.f;
		m->local_action("shoot", power);
	}
}

void PMHud::_physics_process(double delta) {
	if (press_hold >= 0.f) press_hold += (float)delta;
	if (fx_t > 0.f) fx_t -= (float)delta;
	if (shoot_hold >= 0.f) {
		shoot_hold += (float)delta;
		if (shoot_hold > 1.2f) release("shoot");
	}
}

void PMHud::open_pause() {
	if (overlay || !m) return;
	if (!m->can_pause()) {
		confirm_exit();
		return;
	}
	m->set_paused(true);
	VBoxContainer *v = kit::modal(this, 520);
	overlay = Object::cast_to<Control>((Object *)v->get_meta("modal_root"));
	v->add_child(kit::label(S("Пауза"), 40, kit::TEXT, HORIZONTAL_ALIGNMENT_CENTER));
	Button *b1 = kit::button(S("Продолжить"), kit::BLUE, 28);
	b1->connect("pressed", Fn(b1, [this, v] {
		kit::close_modal(v);
		overlay = nullptr;
		m->set_paused(false);
	}));
	v->add_child(b1);
	Button *b2 = kit::button(S("Выйти из матча"), kit::PANEL2, 26);
	b2->connect("pressed", Fn(b2, [this] { m->exit_match(); }));
	v->add_child(b2);
	v->add_child(kit::label(S("Джойстик слева, кнопки справа. Удар — держите для силы."), 20, kit::MUTED, HORIZONTAL_ALIGNMENT_CENTER));
}

void PMHud::confirm_exit() {
	VBoxContainer *v = kit::modal(this, 560);
	overlay = Object::cast_to<Control>((Object *)v->get_meta("modal_root"));
	v->add_child(kit::label(S("Покинуть PvP-матч?"), 34, kit::TEXT, HORIZONTAL_ALIGNMENT_CENTER));
	HBoxContainer *h = kit::hbox(14);
	Button *no = kit::button(S("Остаться"), kit::BLUE, 26);
	no->set_h_size_flags(SIZE_EXPAND_FILL);
	no->connect("pressed", Fn(no, [this, v] {
		kit::close_modal(v);
		overlay = nullptr;
	}));
	Button *yes = kit::button(S("Выйти"), kit::RED, 26);
	yes->set_h_size_flags(SIZE_EXPAND_FILL);
	yes->connect("pressed", Fn(yes, [this] { m->exit_match(); }));
	h->add_child(no);
	h->add_child(yes);
	v->add_child(h);
}

void PMHud::on_back() {
	if (!overlay) open_pause();
}

// ------------------------------------------------------------------ drawing
void PMHud::text_c(Vector2 p, const String &t, int fs, Color c) {
	float w = font->get_string_size(t, HORIZONTAL_ALIGNMENT_LEFT, -1, fs).x;
	draw_string(font, Vector2(p.x - w * 0.5f, p.y + fs * 0.35f), t, HORIZONTAL_ALIGNMENT_LEFT, -1, fs, c);
}

void PMHud::draw_badge(Vector2 c, float r, int team) {
	Ref<Texture2D> tex = GameData::get().logo_texture(m->teams[team].logo);
	if (tex.is_valid()) {
		draw_texture_rect(tex, Rect2(c - Vector2(r, r), Vector2(r * 2, r * 2)), false);
		return;
	}
	draw_circle(c, r, Color(0.02f, 0.04f, 0.1f));
	draw_circle(c, r * 0.85f, m->team_colors[team]);
}

void PMHud::_draw() {
	if (!m) return;
	Vector2 s = get_size();
	const Color glass(0.03f, 0.05f, 0.12f, 0.72f);
	// ---- scoreboard (top-left, broadcast style)
	{
		float x = 18, y = 16, h = 52;
		float tw = 86, sw = 92, cw = 74;
		float w = 8 + 40 + tw + sw + tw + 40 + 8;
		rrect(Rect2(Vector2(x, y), Vector2(w + cw + 6, h)), glass, 14, Color(1, 1, 1, 0.08f), 1);
		draw_rect(Rect2(Vector2(x + 6, y + h - 5), Vector2(w * 0.5f - 6, 3)), m->team_colors[0]);
		draw_rect(Rect2(Vector2(x + w * 0.5f, y + h - 5), Vector2(w * 0.5f - 6, 3)), m->team_colors[1]);
		draw_badge(Vector2(x + 28, y + h * 0.5f), 17, 0);
		text_b(Vector2(x + 48 + tw * 0.5f, y + h * 0.5f - 1), m->teams[0].shrt, 22, kit::TEXT, tw - 6);
		rrect(Rect2(Vector2(x + 48 + tw, y + 7), Vector2(sw, h - 14)), Color(1, 1, 1, 0.95f), 9);
		text_b(Vector2(x + 48 + tw + sw * 0.5f, y + h * 0.5f - 1), I(m->score[0]) + "  " + I(m->score[1]), 26, Color(0.04f, 0.06f, 0.12f));
		text_b(Vector2(x + 48 + tw + sw + tw * 0.5f, y + h * 0.5f - 1), m->teams[1].shrt, 22, kit::TEXT, tw - 6);
		draw_badge(Vector2(x + w - 28, y + h * 0.5f), 17, 1);
		draw_line(Vector2(x + w + 2, y + 10), Vector2(x + w + 2, y + h - 10), Color(1, 1, 1, 0.15f), 1.f);
		text_b(Vector2(x + w + 4 + cw * 0.5f, y + h * 0.5f - 1), m->clock_text(), 22, kit::LIME);
	}
	// ---- radar (bottom centre)
	float rw = 210.f, rh = 136.f;
	Rect2 rr(Vector2(s.x * 0.5f - rw * 0.5f, s.y - rh - 14), Vector2(rw, rh));
	rrect(rr.grow(4), Color(0.02f, 0.06f, 0.05f, 0.55f), 12, Color(1, 1, 1, 0.12f), 1);
	draw_rect(rr, Color(1, 1, 1, 0.28f), false, 1.f);
	draw_line(Vector2(rr.get_center().x, rr.position.y), Vector2(rr.get_center().x, rr.get_end().y), Color(1, 1, 1, 0.28f), 1.f);
	draw_arc(rr.get_center(), rh * 0.13f, 0, Math_TAU, 24, Color(1, 1, 1, 0.28f), 1.f);
	for (float sx : { -1.f, 1.f }) {
		float bx = sx < 0 ? rr.position.x : rr.get_end().x - rw * 0.157f;
		draw_rect(Rect2(Vector2(bx, rr.get_center().y - rh * 0.3f), Vector2(rw * 0.157f, rh * 0.6f)), Color(1, 1, 1, 0.28f), false, 1.f);
	}
	float flip = m->view_side();
	const int *cc = m->is_client ? m->client_ctrl : m->ctrl;
	Footballer *mine = nullptr;
	for (auto *p : m->players) {
		Vector2 v(p->pos.x * flip / 52.5f, p->pos.z * flip / 34.f);
		Vector2 sp = rr.get_center() + Vector2(v.x * rw * 0.5f, v.y * rh * 0.5f);
		bool ctl = p->team == m->local_team && cc[m->local_team] == p->gid;
		if (ctl) mine = p;
		Color col = p->team == m->local_team ? Color::html("38bdf8") : Color::html("f87171");
		draw_circle(sp, ctl ? 4.5f : 3.3f, col);
		if (ctl) draw_arc(sp, 7, 0, Math_TAU, 14, Color::html("ffe14d"), 2.f);
	}
	Vector3 bp = m->ball_pos;
	draw_circle(rr.get_center() + Vector2(bp.x * flip / 52.5f * rw * 0.5f, bp.z * flip / 34.f * rh * 0.5f), 3.f, Color(1, 1, 1));
	// ---- controlled player: stamina + power gauges under the feet
	bool on = m->local_auto();
	if (mine && m->cam && !m->replay && !overlay) {
		Vector3 wp = mine->pos;
		if (!m->cam->is_position_behind(wp)) {
			Vector2 sp = m->cam->unproject_position(wp) + Vector2(0, 14);
			float bw = 56;
			rrect(Rect2(sp - Vector2(bw * 0.5f + 2, 2), Vector2(bw + 4, 8)), Color(0, 0, 0, 0.55f), 4);
			float st = clampf(mine->stamina, 0.f, 1.f);
			Color sc = st > 0.5f ? Color::html("a3e635") : (st > 0.25f ? Color::html("facc15") : Color::html("ef4444"));
			rrect(Rect2(sp - Vector2(bw * 0.5f, 0), Vector2(bw * st, 4)), sc, 2);
			if (shoot_hold >= 0.f) {
				float pw = clampf(0.45f + shoot_hold * 0.75f, 0.45f, 1.05f);
				float frac = clampf((pw - 0.45f) / 0.6f, 0.f, 1.f);
				Vector2 pp = sp + Vector2(0, 12);
				float pwid = 90;
				rrect(Rect2(pp - Vector2(pwid * 0.5f + 2, 2), Vector2(pwid + 4, 12)), Color(0, 0, 0, 0.6f), 5);
				Color pc = frac < 0.7f ? Color::html("facc15") : (frac < 0.92f ? Color::html("fb923c") : Color::html("ef4444"));
				rrect(Rect2(pp - Vector2(pwid * 0.5f, 0), Vector2(pwid * frac, 8)), pc, 4);
			}
		}
	}
	// ---- your own player (card with a personal photo) under the scoreboard
	if (mine && !mine->data.photo.is_empty() && !m->replay) {
		Ref<Texture2D> ph = kit::photo_tex(mine->data.photo);
		if (ph.is_valid()) {
			Vector2 o(18, 76);
			float nw = bold->get_string_size(mine->data.name, HORIZONTAL_ALIGNMENT_LEFT, -1, 20).x;
			rrect(Rect2(o, Vector2(66 + nw + 18, 56)), glass, 28, Color(kit::LIME, 0.6f), 1.5f);
			draw_texture_rect(ph, Rect2(o + Vector2(4, 4), Vector2(48, 48)), false);
			draw_arc(o + Vector2(28, 28), 24, 0, Math_TAU, 32, kit::LIME, 2.f, true);
			draw_string(bold, o + Vector2(62, 35), mine->data.name, HORIZONTAL_ALIGNMENT_LEFT, -1, 20, Color(1, 1, 1));
		}
	}
	// ---- top-right: AUTO pill + pause
	Rect2 ar = auto_rect();
	rrect(ar, on ? Color(0.13f, 0.77f, 0.37f, 0.92f) : glass, 26, on ? Color::html("86efac") : Color(1, 1, 1, 0.18f), 1.5f);
	draw_circle(ar.position + Vector2(24, ar.size.y * 0.5f), 6, on ? Color(1, 1, 1) : Color(1, 1, 1, 0.4f));
	text_b(ar.get_center() + Vector2(10, 0), on ? S("АВТО ВКЛ") : S("АВТО"), 20, Color(1, 1, 1), ar.size.x - 46);
	Rect2 pr = pause_rect();
	rrect(pr, glass, 26, Color(1, 1, 1, 0.18f), 1.5f);
	draw_rect(Rect2(pr.get_center() + Vector2(-9, -11), Vector2(6, 22)), Color(1, 1, 1));
	draw_rect(Rect2(pr.get_center() + Vector2(3, -11), Vector2(6, 22)), Color(1, 1, 1));
	// ---- replay badge
	if (m->replay) {
		Rect2 rb(Vector2(s.x - 210, 82), Vector2(192, 42));
		rrect(rb, Color(0.86f, 0.12f, 0.2f, 0.9f), 10);
		draw_circle(rb.position + Vector2(22, 21), 6, Color(1, 1, 1));
		text_b(rb.get_center() + Vector2(10, 0), S("ПОВТОР"), 22, Color(1, 1, 1));
		if (m->mode != "pvp") text_b(Vector2(s.x * 0.5f, s.y - 190), S("Нажмите, чтобы пропустить"), 20, Color(1, 1, 1, 0.75f));
	}
	if (!overlay && !m->replay) {
		// ---- joystick
		Vector2 c = joy_index != -1 ? joy_center : Vector2(180, s.y - 160);
		bool active = joy_index != -1;
		draw_circle(c, JOY_R + 10, Color(0.02f, 0.04f, 0.1f, active ? 0.35f : 0.22f));
		draw_arc(c, JOY_R + 10, 0, Math_TAU, 48, Color(1, 1, 1, active ? 0.45f : 0.22f), 2.f);
		Vector2 kn = c + Vector2(joy_vec.x, -joy_vec.y) * JOY_R;
		if (joy_vec.length() > 0.01f) {
			float ang = Vector2(joy_vec.x, -joy_vec.y).angle();
			draw_arc(c, JOY_R + 10, ang - 0.5f, ang + 0.5f, 16, Color(0.3f, 0.85f, 1.f, 0.9f), 4.f);
		}
		draw_circle(kn, 40, Color(1, 1, 1, active ? 0.5f : 0.25f));
		draw_arc(kn, 40, 0, Math_TAU, 32, Color(1, 1, 1, 0.6f), 2.f);
		// ---- action buttons (translucent white like mobile football games, contextual labels)
		bool has = m->local_has_ball();
		for (auto &kv : buttons) {
			if (!button_visible(kv.first)) continue;
			const Btn &b = kv.second;
			bool pr2 = btn_touch.count(kv.first) > 0;
			float a = on ? 0.35f : 1.f;
			float r = pr2 ? b.r * 0.93f : b.r;
			Color tint = has ? b.color : Color::html("f8fafc");
			draw_circle(b.pos, r + 3, Color(0, 0, 0, 0.18f * a));
			draw_circle(b.pos, r, Color(1, 1, 1, (pr2 ? 0.42f : 0.16f) * a));
			draw_circle(b.pos, r * 0.78f, Color(tint, (pr2 ? 0.35f : 0.12f) * a));
			draw_arc(b.pos, r, 0, Math_TAU, 56, Color(1, 1, 1, 0.85f * a), 2.5f, true);
			draw_arc(b.pos, r - 6, -Math_PI * 0.85f, -Math_PI * 0.15f, 20, Color(tint, 0.9f * a), 3.f, true);
			String t = has ? b.label : b.alt;
			text_b(b.pos + Vector2(0, 2), t, kv.first == "shoot" ? 21 : 17, Color(1, 1, 1, a), r * 1.72f);
			if (!has && kv.first == "shoot") text_b(b.pos + Vector2(0, r * 0.52f), S("удерж."), 12, Color(1, 1, 1, 0.65f * a));
		}
		if (press_hold > 0.15f) {
			const Btn &sb = buttons["shoot"];
			draw_arc(sb.pos, sb.r + 9, 0, Math_TAU, 48, Color(1, 0.45f, 0.4f, 0.8f), 4.f, true);
		}
		// ---- gesture feedback
		if (ges_index != -1 && ges_cur.distance_to(ges_start) > 20.f) {
			draw_line(ges_start, ges_cur, Color(1, 1, 1, 0.55f), 6.f, true);
			draw_circle(ges_cur, 10, Color(1, 1, 1, 0.7f));
		}
		if (fx_t > 0.f) {
			float k = fx_t / 0.5f;
			if (fx_swipe) {
				draw_line(fx_from, fx_pos, Color(1, 0.9f, 0.3f, k), 8.f * k + 2.f, true);
			} else {
				draw_arc(fx_pos, 18 + (1.f - k) * 26.f, 0, Math_TAU, 32, Color(1, 1, 1, k), 3.f, true);
			}
		}
		if (controls == 2 && !on) text_b(Vector2(s.x * 0.72f, s.y - 40), S("Тап по игроку — пас • свайп к воротам — удар"), 15, Color(1, 1, 1, 0.55f));
		if (shoot_hold >= 0.f) {
			const Btn &sb = buttons["shoot"];
			float pw = clampf(0.45f + shoot_hold * 0.75f, 0.45f, 1.05f);
			float frac = clampf((pw - 0.45f) / 0.6f, 0.f, 1.f);
			draw_arc(sb.pos, sb.r + 10, -Math_PI * 0.5f, -Math_PI * 0.5f + Math_TAU * frac, 48, pw < 0.92f ? Color::html("facc15") : Color::html("ef4444"), 7.f);
		}
	}
	// ---- event banner
	if (banner_t > 0.f && !banner_text.is_empty()) {
		float a = clampf(banner_t * 2.f, 0.f, 1.f);
		int fs = 84;
		float tw = bold->get_string_size(banner_text, HORIZONTAL_ALIGNMENT_LEFT, -1, fs).x;
		Vector2 c(s.x * 0.5f, s.y * 0.36f);
		float slide = clampf((2.6f - banner_t) * 6.f, 0.f, 1.f);
		float bw = (tw + 160) * slide;
		rrect(Rect2(Vector2(c.x - bw * 0.5f, c.y - 58), Vector2(bw, 108)), Color(0.02f, 0.04f, 0.1f, 0.78f * a), 18);
		Color bc = banner_color.get_luminance() < 0.25f ? banner_color.lightened(0.45f) : banner_color;
		draw_rect(Rect2(Vector2(c.x - bw * 0.5f + 12, c.y + 42), Vector2(bw - 24, 6)), Color(bc, a));
		if (slide > 0.6f) {
			draw_string_outline(bold, Vector2(c.x - tw * 0.5f, c.y + 26), banner_text, HORIZONTAL_ALIGNMENT_LEFT, -1, fs, 8, Color(0, 0, 0, 0.6f * a));
			draw_string(bold, Vector2(c.x - tw * 0.5f, c.y + 26), banner_text, HORIZONTAL_ALIGNMENT_LEFT, -1, fs, Color(1, 1, 1, a));
		}
	}
}

} // namespace pm
