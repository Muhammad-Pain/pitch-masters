#include "hud.h"
#include "../game_data.h"
#include "../sfx.h"
#include "../ui_kit.h"
#include "match.h"

#include <godot_cpp/classes/input.hpp>
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
	buttons["shoot"] = { Vector2(s.x - 112, s.y - 112), 64.f, Color::html("ff4d5e"), S("УДАР"), S("ОТБОР") };
	buttons["pass"] = { Vector2(s.x - 262, s.y - 78), 52.f, Color::html("3b82f6"), S("ПАС"), S("СМЕНА") };
	buttons["through"] = { Vector2(s.x - 236, s.y - 214), 48.f, Color::html("22c55e"), S("РАЗРЕЗ"), S("ПРЕСС") };
	buttons["sprint"] = { Vector2(s.x - 96, s.y - 262), 44.f, Color::html("f59e0b"), S("РЫВОК"), S("РЫВОК") };
	buttons["lob"] = { Vector2(s.x - 388, s.y - 64), 40.f, Color::html("06b6d4"), S("НАВЕС"), S("НАВЕС") };
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
	m->local_press(!has && (btn_touch.count("through") || in->is_physical_key_pressed(KEY_L)));
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
	if (mm.is_valid() && mm->get_device() != InputEvent::DEVICE_ID_EMULATION && joy_index == 99) drag(99, mm->get_position());
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
			if (p.distance_to(kv.second.pos) < kv.second.r * 1.3f) {
				btn_touch[kv.first] = index;
				press(kv.first);
				get_viewport()->set_input_as_handled();
				return;
			}
		}
		if (p.x < get_size().x * 0.5f && joy_index == -1 && p.y > get_size().y * 0.25f) {
			joy_index = index;
			joy_center = p;
			joy_vec = Vector2();
			test_joy = false;
			get_viewport()->set_input_as_handled();
		}
	} else {
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
	if (name == "shoot") {
		if (m->local_has_ball()) shoot_hold = 0.f;
		else m->local_action("shoot");
	} else if (name == "pass") {
		m->local_action("pass");
	} else if (name == "through") {
		if (m->local_has_ball()) m->local_action("through");
	} else if (name == "lob") {
		m->local_action("lob");
	}
}

void PMHud::release(const String &name) {
	if (name == "shoot" && shoot_hold >= 0.f && m) {
		float power = clampf(0.45f + shoot_hold * 0.75f, 0.45f, 1.05f);
		shoot_hold = -1.f;
		m->local_action("shoot", power);
	}
}

void PMHud::_physics_process(double delta) {
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
		// ---- action buttons
		bool has = m->local_has_ball();
		for (auto &kv : buttons) {
			const Btn &b = kv.second;
			bool pr2 = btn_touch.count(kv.first) > 0;
			float a = on ? 0.4f : 1.f;
			float r = pr2 ? b.r * 0.94f : b.r;
			draw_circle(b.pos, r, Color(0.02f, 0.04f, 0.1f, (pr2 ? 0.75f : 0.5f) * a));
			draw_circle(b.pos, r - 5, Color(b.color, (pr2 ? 0.55f : 0.22f) * a));
			draw_arc(b.pos, r, 0, Math_TAU, 48, Color(b.color.lightened(0.25f), 0.95f * a), 3.f);
			String t = has ? b.label : b.alt;
			text_b(b.pos, t, kv.first == "shoot" ? 22 : 18, Color(1, 1, 1, a), r * 1.7f);
		}
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
