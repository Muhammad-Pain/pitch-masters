#pragma once
#include "../util.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/input_event.hpp>

#include <map>

namespace pm {

class PMMatch;

// Match HUD: scoreboard with badges, radar, multi-touch joystick and action buttons.
class PMHud : public godot::Control {
	GDCLASS(PMHud, godot::Control)

	struct Btn {
		Vector2 pos;
		float r;
		Color color;
		String label, alt;
	};
	std::map<String, Btn> buttons;
	std::map<String, int> btn_touch;
	Vector2 last_size;
	int joy_index = -1;
	Vector2 joy_center, joy_vec;
	bool test_joy = false;
	float shoot_hold = -1.f, press_hold = -1.f;
	bool tpress_on = false;
	int controls = 1; // 0 buttons, 1 buttons + gestures, 2 gestures only
	int ges_index = -1;
	Vector2 ges_start, ges_cur;
	uint64_t ges_t0 = 0;
	Vector2 fx_pos;
	float fx_t = 0.f;
	bool fx_swipe = false;
	Vector2 fx_from;
	bool screen_to_pitch(Vector2 sp, Vector3 &out) const;
	void finish_gesture(Vector2 p);
	bool button_visible(const String &name) const;
	String banner_text;
	float banner_t = 0.f;
	Color banner_color;
	godot::Control *overlay = nullptr;
	godot::Ref<godot::Font> font, bold;
	static constexpr float JOY_R = 95.f;

	void layout();
	godot::Rect2 pause_rect() const;
	godot::Rect2 auto_rect() const;
	void touch(int index, Vector2 p, bool pressed);
	void drag(int index, Vector2 p);
	void keyboard();
	void text_c(Vector2 p, const String &t, int fs, Color c);
	void text_b(Vector2 p, const String &t, int fs, Color c, float max_w = 0.f);
	void rrect(godot::Rect2 r, Color c, float radius, Color border = Color(), float bw = 0.f);
	void draw_badge(Vector2 c, float r, int team);
	void open_pause();
	void confirm_exit();

protected:
	static void _bind_methods() {}

public:
	PMMatch *m = nullptr;
	void _ready() override;
	void _process(double delta) override;
	void _physics_process(double delta) override;
	void _input(const godot::Ref<godot::InputEvent> &e) override;
	void _unhandled_key_input(const godot::Ref<godot::InputEvent> &e) override;
	void _draw() override;
	void banner(const String &t, Color c, float dur);
	void set_banner_time(float t) { banner_t = t; }
	void on_back();
	void press(const String &name);
	void release(const String &name);
	void set_test_joy(Vector2 v) { test_joy = true; joy_vec = v; }
	void set_hold(const String &name, bool on) {
		if (on) btn_touch[name] = 900;
		else btn_touch.erase(name);
	}
};

} // namespace pm
