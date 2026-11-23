#pragma once
// Widget helpers + base class for menu screens.

#include "game_data.h"

#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/font.hpp>

namespace pm {

namespace kit {
extern const Color BG, PANEL, PANEL2, ACCENT, LIME, BLUE, GOLD, TEXT, MUTED, GREEN, RED;

godot::Ref<godot::StyleBoxFlat> sb(Color c, int radius = 14, int border = 0, Color border_color = Color(0, 0, 0, 0), float margin = 12, float skew = 0.0f);
godot::Button *button(const String &text, Color c = PANEL2, int font_size = 26, Vector2 min_size = Vector2(0, 64));
godot::Label *label(const String &text, int size = 24, Color c = TEXT, godot::HorizontalAlignment align = godot::HORIZONTAL_ALIGNMENT_LEFT);
godot::PanelContainer *panel(Color c = PANEL, int radius = 18, float margin = 16);
godot::PanelContainer *glass(float margin = 18);
godot::Button *primary(const String &text, int font_size = 26);
godot::Ref<godot::Font> font_bold();
void release_fonts();
// ---- personal photos on cards
godot::Ref<godot::Texture2D> photo_tex(const String &path); // round-masked, cached
godot::Control *portrait(const Card &c, float size, Color ring);
void pick_photo(godot::Control *host, int card_id, std::function<void()> done);
void crop_photo(godot::Control *host, int card_id, const godot::Ref<godot::Image> &img, std::function<void()> done);
godot::Ref<godot::Image> image_from_file(const String &path);
String save_photo(int card_id, const godot::Ref<godot::Image> &square);
void rename_dialog(godot::Control *host, int card_id, std::function<void()> done);
godot::Control *stat_bar(const String &name, int a, int b, bool percent);
godot::Control *progress(float frac, Color c, float h = 8);
void fade_in(godot::Control *c, float delay = 0.f);
void press_anim(godot::Button *b);
godot::OptionButton *option(const std::vector<String> &items, int selected = 0, int font_size = 22);
godot::HBoxContainer *hbox(int sep = 14);
godot::VBoxContainer *vbox(int sep = 12);
godot::Control *spacer(bool vertical = false);
godot::Control *badge(const String &logo_path, Color shirt, const String &shrt, float size);
godot::PanelContainer *card(const Card &c, Vector2 size = Vector2(150, 206), const String &slot_pos = "");
godot::VBoxContainer *modal(godot::Control *parent, float width = 640);
void close_modal(godot::Control *v);
void toast(godot::Control *parent, const String &text, Color c = GOLD);
void clickable(godot::Control *c, std::function<void()> cb);
void full_rect(godot::Control *c);
String coins_text();
String fmt_int(int n);
String short_name(const String &n);
} // namespace kit

// Base class for menu screens: full-rect Control with header + coin counter polling.
class PMScreen : public godot::Control {
	GDCLASS(PMScreen, godot::Control)

protected:
	static void _bind_methods() {}
	godot::Label *coins_label = nullptr;
	int seen_rev = -1;
	godot::VBoxContainer *root(); // background dim + margins
	godot::Label *header(godot::VBoxContainer *parent, const String &title, std::function<void()> on_back);

public:
	Dictionary params;
	virtual void on_back();
	void _process(double delta) override;
};

} // namespace pm
