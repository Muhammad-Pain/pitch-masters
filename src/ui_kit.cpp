#include "ui_kit.h"
#include "app.h"
#include "sfx.h"

#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/grid_container.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/popup_menu.hpp>
#include <godot_cpp/classes/style_box_empty.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/classes/interval_tweener.hpp>
#include <godot_cpp/classes/callback_tweener.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <map>
#include <memory>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/input_event_magnify_gesture.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/file_dialog.hpp>
#include <godot_cpp/classes/font_file.hpp>
#include <godot_cpp/classes/font_variation.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture2d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>

using namespace godot;

namespace pm {
namespace kit {

const Color BG = Color(5/255.f, 12/255.f, 31/255.f);
const Color PANEL = Color(0.07f, 0.1f, 0.2f, 0.78f);
const Color PANEL2 = Color(21/255.f, 39/255.f, 90/255.f);
const Color ACCENT = Color(25/255.f, 211/255.f, 255/255.f);
const Color LIME = Color(198/255.f, 255/255.f, 61/255.f);
const Color BLUE = Color(29/255.f, 91/255.f, 234/255.f);
const Color GOLD = Color(255/255.f, 200/255.f, 61/255.f);
const Color TEXT = Color(238/255.f, 243/255.f, 255/255.f);
const Color MUTED = Color(147/255.f, 164/255.f, 204/255.f);
const Color GREEN = Color(31/255.f, 180/255.f, 86/255.f);
const Color RED = Color(226/255.f, 59/255.f, 74/255.f);

Ref<StyleBoxFlat> sb(Color c, int radius, int border, Color bc, float margin, float skew) {
	Ref<StyleBoxFlat> s;
	s.instantiate();
	s->set_bg_color(c);
	s->set_corner_radius_all(radius);
	if (border > 0) {
		s->set_border_width_all(border);
		s->set_border_color(bc);
	}
	s->set_content_margin(SIDE_LEFT, margin);
	s->set_content_margin(SIDE_RIGHT, margin);
	s->set_content_margin(SIDE_TOP, margin * 0.6f);
	s->set_content_margin(SIDE_BOTTOM, margin * 0.6f);
	s->set_anti_aliased(true);
	if (skew != 0.0f) s->set_skew(Vector2(skew, 0));
	return s;
}

void press_anim(Button *b) {
	b->connect("button_down", Fn(b, [b] {
		b->set_pivot_offset(b->get_size() * 0.5f);
		Ref<Tween> tw = b->create_tween();
		tw->tween_property(b, "scale", Vector2(0.96f, 0.96f), 0.06);
	}));
	b->connect("button_up", Fn(b, [b] {
		Ref<Tween> tw = b->create_tween();
		tw->tween_property(b, "scale", Vector2(1, 1), 0.1);
	}));
}

Button *button(const String &text, Color c, int fs, Vector2 min_size) {
	Button *b = memnew(Button);
	b->set_text(text);
	b->set_custom_minimum_size(min_size);
	b->set_focus_mode(Control::FOCUS_NONE);
	b->add_theme_font_size_override("font_size", fs);
	Color base = Color(c.r, c.g, c.b, std::min(c.a, 0.92f));
	b->add_theme_stylebox_override("normal", sb(base, 14, 1, Color(1, 1, 1, 0.1f)));
	b->add_theme_stylebox_override("hover", sb(base.lightened(0.1f), 14, 1, Color(1, 1, 1, 0.25f)));
	b->add_theme_stylebox_override("pressed", sb(base.darkened(0.15f), 14, 2, ACCENT));
	b->add_theme_stylebox_override("disabled", sb(Color(base.darkened(0.45f), 0.6f), 14));
	b->add_theme_stylebox_override("focus", memnew(StyleBoxEmpty));
	b->add_theme_color_override("font_color", TEXT);
	b->add_theme_color_override("font_hover_color", Color(1, 1, 1));
	b->add_theme_color_override("font_pressed_color", LIME);
	b->add_theme_color_override("font_disabled_color", MUTED.darkened(0.3f));
	b->connect("pressed", Fn(b, [] { PMSfx::p("click", -6.f); }));
	press_anim(b);
	return b;
}

Label *label(const String &text, int size, Color c, HorizontalAlignment align) {
	Label *l = memnew(Label);
	l->set_text(text);
	l->set_horizontal_alignment(align);
	l->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	l->add_theme_font_size_override("font_size", size);
	l->add_theme_color_override("font_color", c);
	l->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	return l;
}

PanelContainer *glass(float margin) {
	PanelContainer *p = memnew(PanelContainer);
	Ref<StyleBoxFlat> st = sb(Color(0.08f, 0.11f, 0.22f, 0.74f), 20, 1, Color(1, 1, 1, 0.09f), margin);
	st->set_shadow_color(Color(0, 0, 0, 0.35f));
	st->set_shadow_size(14);
	st->set_shadow_offset(Vector2(0, 6));
	p->add_theme_stylebox_override("panel", st);
	return p;
}

Button *primary(const String &text, int fs) {
	Button *b = memnew(Button);
	b->set_text(text);
	b->set_custom_minimum_size(Vector2(0, 66));
	b->set_focus_mode(Control::FOCUS_NONE);
	b->add_theme_font_size_override("font_size", fs);
	b->add_theme_font_override("font", font_bold());
	Color c = LIME;
	Ref<StyleBoxFlat> n = sb(c, 16, 0, Color(), 16);
	n->set_shadow_color(Color(c.r, c.g, c.b, 0.35f));
	n->set_shadow_size(10);
	b->add_theme_stylebox_override("normal", n);
	b->add_theme_stylebox_override("hover", sb(c.lightened(0.15f), 16, 0, Color(), 16));
	b->add_theme_stylebox_override("pressed", sb(c.darkened(0.15f), 16, 0, Color(), 16));
	b->add_theme_stylebox_override("disabled", sb(Color(0.3f, 0.35f, 0.4f, 0.6f), 16));
	b->add_theme_stylebox_override("focus", memnew(StyleBoxEmpty));
	Color dark(0.03f, 0.05f, 0.1f);
	b->add_theme_color_override("font_color", dark);
	b->add_theme_color_override("font_hover_color", dark);
	b->add_theme_color_override("font_pressed_color", dark);
	b->add_theme_color_override("font_disabled_color", Color(0.75f, 0.78f, 0.85f));
	b->connect("pressed", Fn(b, [] { PMSfx::p("click", -6.f); }));
	press_anim(b);
	return b;
}

static Ref<FontVariation> g_bold;
static std::map<String, Ref<ImageTexture>> g_photos;
void release_fonts() {
	g_bold.unref();
	g_photos.clear();
}

Ref<Texture2D> photo_tex(const String &path) {
	if (path.is_empty()) return Ref<Texture2D>();
	auto it = g_photos.find(path);
	if (it != g_photos.end()) return it->second;
	Ref<Image> img = Image::load_from_file(path);
	if (img.is_null() || img->is_empty()) return Ref<Texture2D>();
	img->convert(Image::FORMAT_RGBA8);
	int w = img->get_width(), h = img->get_height();
	float r = std::min(w, h) * 0.5f;
	Vector2 c(w * 0.5f, h * 0.5f);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			float d = Vector2(x + 0.5f, y + 0.5f).distance_to(c);
			float a = clampf(r - d, 0.f, 1.f);
			if (a < 1.f) {
				Color px = img->get_pixel(x, y);
				px.a *= a;
				img->set_pixel(x, y, px);
			}
		}
	img->generate_mipmaps();
	Ref<ImageTexture> t = ImageTexture::create_from_image(img);
	g_photos[path] = t;
	return t;
}

Control *portrait(const Card &c, float size, Color ring) {
	Control *p = memnew(Control);
	p->set_custom_minimum_size(Vector2(size, size));
	p->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	Ref<Texture2D> tex = photo_tex(c.photo);
	p->connect("draw", Fn(p, [p, tex, ring] {
		Vector2 s = p->get_size();
		float d = std::min(s.x, s.y);
		Vector2 o((s.x - d) * 0.5f, (s.y - d) * 0.5f);
		Vector2 ctr = o + Vector2(d, d) * 0.5f;
		if (tex.is_valid()) {
			p->draw_circle(ctr, d * 0.5f, Color(0.05f, 0.08f, 0.16f));
			p->draw_texture_rect(tex, Rect2(o, Vector2(d, d)), false);
		} else {
			// generic silhouette: head and shoulders
			p->draw_circle(ctr, d * 0.5f, Color(ring.darkened(0.55f), 0.9f));
			Color sil = Color(ring.lightened(0.15f), 0.55f);
			p->draw_circle(ctr + Vector2(0, -d * 0.1f), d * 0.17f, sil);
			PackedVector2Array body;
			int n = 16;
			for (int i = 0; i <= n; i++) {
				float a = Math_PI + Math_PI * i / n;
				body.push_back(ctr + Vector2(std::cos(a) * d * 0.3f, d * 0.36f + std::sin(a) * d * 0.22f));
			}
			// clip the shoulders to the circle by keeping them inside the bottom
			p->draw_colored_polygon(body, sil);
		}
		p->draw_arc(ctr, d * 0.5f - 1.f, 0, Math_TAU, 48, ring, 2.f, true);
	}));
	return p;
}

Ref<Image> image_from_file(const String &path) {
	PackedByteArray b = FileAccess::get_file_as_bytes(path);
	Ref<Image> img;
	img.instantiate();
	Error e = ERR_FILE_UNRECOGNIZED;
	if (b.size() > 12) {
		const uint8_t *d = b.ptr();
		if (d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G') e = img->load_png_from_buffer(b);
		else if (d[0] == 0xFF && d[1] == 0xD8) e = img->load_jpg_from_buffer(b);
		else if (d[0] == 'R' && d[1] == 'I' && d[2] == 'F' && d[3] == 'F' && d[8] == 'W' && d[9] == 'E') e = img->load_webp_from_buffer(b);
		else if (d[0] == 'B' && d[1] == 'M') e = img->load_bmp_from_buffer(b);
	}
	if (e != OK) {
		Ref<Image> f = Image::load_from_file(path);
		if (f.is_valid() && !f->is_empty()) return f;
		return Ref<Image>();
	}
	return img;
}

String save_photo(int card_id, const Ref<Image> &square) {
	DirAccess::make_dir_recursive_absolute("user://photos");
	Ref<Image> out = square->duplicate();
	out->convert(Image::FORMAT_RGB8);
	out->resize(320, 320, Image::INTERPOLATE_LANCZOS);
	String path = "user://photos/card_" + I(card_id) + "_" + String::num_int64((int64_t)Time::get_singleton()->get_ticks_msec() + (int64_t)Time::get_singleton()->get_unix_time_from_system() % 100000) + ".png";
	if (out->save_png(path) != OK) return String();
	return path;
}

void crop_photo(Control *host, int card_id, const Ref<Image> &src, std::function<void()> done) {
	struct St {
		Ref<Image> img;
		Ref<ImageTexture> tex;
		float zoom = 1.f;
		Vector2 pan;
		bool drag = false;
	};
	auto st = std::make_shared<St>();
	st->img = src->duplicate();
	st->img->convert(Image::FORMAT_RGB8);
	int mx = std::max(st->img->get_width(), st->img->get_height());
	if (mx > 1600) {
		float k = 1600.f / mx;
		st->img->resize(std::max(1, (int)(st->img->get_width() * k)), std::max(1, (int)(st->img->get_height() * k)), Image::INTERPOLATE_BILINEAR);
	}
	st->tex = ImageTexture::create_from_image(st->img);
	const float V = 360.f;
	VBoxContainer *v = modal(host, 620);
	Label *t = label(S("Фото на карточку"), 30, TEXT, HORIZONTAL_ALIGNMENT_CENTER);
	t->add_theme_font_override("font", font_bold());
	v->add_child(t);
	v->add_child(label(S("Передвиньте фото пальцем, чтобы лицо было в круге"), 18, MUTED, HORIZONTAL_ALIGNMENT_CENTER));
	CenterContainer *cc = memnew(CenterContainer);
	v->add_child(cc);
	Control *view = memnew(Control);
	view->set_custom_minimum_size(Vector2(V, V));
	view->set_clip_contents(true);
	view->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	cc->add_child(view);
	auto layout = [st, V](Vector2 &pos, float &s) {
		float w = (float)st->img->get_width(), h = (float)st->img->get_height();
		s = V / std::min(w, h) * st->zoom;
		float ex = std::max(0.f, (w * s - V) * 0.5f), ey = std::max(0.f, (h * s - V) * 0.5f);
		st->pan.x = clampf(st->pan.x, -ex, ex);
		st->pan.y = clampf(st->pan.y, -ey, ey);
		pos = Vector2(V, V) * 0.5f + st->pan - Vector2(w, h) * s * 0.5f;
	};
	view->connect("draw", Fn(view, [view, st, layout, V] {
		Vector2 pos;
		float s;
		layout(pos, s);
		view->draw_texture_rect(st->tex, Rect2(pos, Vector2((float)st->img->get_width(), (float)st->img->get_height()) * s), false);
		// dim the corners outside the circle
		Vector2 c(V * 0.5f, V * 0.5f);
		float r = V * 0.5f;
		const int n = 48;
		for (int q = 0; q < 4; q++) {
			float a0 = Math_PI * 0.5f * q;
			Vector2 corner = c + Vector2(q == 0 || q == 3 ? r : -r, q < 2 ? r : -r);
			PackedVector2Array poly;
			poly.push_back(corner);
			for (int i = 0; i <= n / 4; i++) {
				float a = a0 + Math_PI * 0.5f * i / (n / 4);
				poly.push_back(c + Vector2(std::cos(a), std::sin(a)) * r);
			}
			view->draw_colored_polygon(poly, Color(0, 0, 0, 0.55f));
		}
		view->draw_arc(c, r - 1.5f, 0, Math_TAU, 64, Color(1, 1, 1, 0.9f), 3.f, true);
	}));
	view->connect("gui_input", FnV(view, [view, st](const Variant &ev) {
		Ref<InputEventMouseButton> mb = ev;
		if (mb.is_valid()) {
			if (mb->get_button_index() == MOUSE_BUTTON_LEFT) st->drag = mb->is_pressed();
			else if (mb->is_pressed() && mb->get_button_index() == MOUSE_BUTTON_WHEEL_UP) st->zoom = clampf(st->zoom * 1.1f, 1.f, 4.f);
			else if (mb->is_pressed() && mb->get_button_index() == MOUSE_BUTTON_WHEEL_DOWN) st->zoom = clampf(st->zoom / 1.1f, 1.f, 4.f);
			view->queue_redraw();
			return;
		}
		Ref<InputEventMouseMotion> mm = ev;
		if (mm.is_valid() && st->drag) {
			st->pan += mm->get_relative();
			view->queue_redraw();
			return;
		}
		Ref<InputEventMagnifyGesture> mg = ev;
		if (mg.is_valid()) {
			st->zoom = clampf(st->zoom * (float)mg->get_factor(), 1.f, 4.f);
			view->queue_redraw();
		}
	}));
	HBoxContainer *zh = hbox(12);
	zh->add_child(label(S("Масштаб"), 18, MUTED));
	HSlider *zs = memnew(HSlider);
	zs->set_min(1.0);
	zs->set_max(4.0);
	zs->set_step(0.01);
	zs->set_value(1.0);
	zs->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	zs->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	zs->set_custom_minimum_size(Vector2(0, 40));
	zs->connect("value_changed", FnV(zs, [view, st](const Variant &val) {
		st->zoom = (float)(double)val;
		view->queue_redraw();
	}));
	zh->add_child(zs);
	Button *rot = button(S("Повернуть"), PANEL2, 20, Vector2(150, 50));
	rot->connect("pressed", Fn(rot, [view, st] {
		st->img->rotate_90(CLOCKWISE);
		st->tex = ImageTexture::create_from_image(st->img);
		st->pan = Vector2();
		view->queue_redraw();
	}));
	zh->add_child(rot);
	v->add_child(zh);
	HBoxContainer *h = hbox(12);
	Button *cancel = button(S("Отмена"), PANEL2, 22, Vector2(0, 60));
	cancel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	cancel->connect("pressed", Fn(cancel, [v] { close_modal(v); }));
	h->add_child(cancel);
	Button *ok = primary(S("Сохранить"), 24);
	ok->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	ok->connect("pressed", Fn(ok, [host, v, st, layout, card_id, done, V] {
		Vector2 pos;
		float s;
		layout(pos, s);
		int side = std::max(8, (int)std::round(V / s));
		int x = clampi((int)std::round(-pos.x / s), 0, std::max(0, st->img->get_width() - side));
		int y = clampi((int)std::round(-pos.y / s), 0, std::max(0, st->img->get_height() - side));
		side = std::min(side, std::min(st->img->get_width() - x, st->img->get_height() - y));
		Ref<Image> sq = st->img->get_region(Rect2i(x, y, side, side));
		String path = save_photo(card_id, sq);
		close_modal(v);
		if (path.is_empty()) {
			toast(host, S("Не удалось сохранить фото"), RED);
			return;
		}
		GameData::get().set_card_photo(card_id, path);
		toast(host, S("Фото добавлено на карточку"), LIME);
		if (done) done();
	}));
	h->add_child(ok);
	v->add_child(h);
}

void pick_photo(Control *host, int card_id, std::function<void()> done) {
	FileDialog *fd = memnew(FileDialog);
	fd->set_file_mode(FileDialog::FILE_MODE_OPEN_FILE);
	fd->set_access(FileDialog::ACCESS_FILESYSTEM);
	PackedStringArray filters;
	filters.push_back(S("*.jpg, *.jpeg, *.png, *.webp ; Фото ; image/jpeg, image/png, image/webp"));
	fd->set_filters(filters);
	fd->set_use_native_dialog(true);
	fd->set_title(S("Выберите фото"));
	String pics = OS::get_singleton()->get_system_dir(OS::SYSTEM_DIR_PICTURES);
	if (!pics.is_empty()) fd->set_current_dir(pics);
	fd->connect("file_selected", FnV(fd, [host, fd, card_id, done](const Variant &p) {
		String path = p;
		fd->queue_free();
		Ref<Image> img = image_from_file(path);
		if (img.is_null()) {
			toast(host, S("Не удалось открыть фото (нужен JPG, PNG или WEBP)"), RED);
			return;
		}
		crop_photo(host, card_id, img, done);
	}));
	fd->connect("canceled", Fn(fd, [fd] { fd->queue_free(); }));
	host->add_child(fd);
	fd->popup_centered_ratio(0.85f);
}

void rename_dialog(Control *host, int card_id, std::function<void()> done) {
	Card *c = GameData::get().card_by_id(card_id);
	if (!c) return;
	VBoxContainer *v = modal(host, 620);
	Label *t = label(S("Имя игрока"), 30, TEXT, HORIZONTAL_ALIGNMENT_CENTER);
	t->add_theme_font_override("font", font_bold());
	v->add_child(t);
	LineEdit *le = memnew(LineEdit);
	le->set_text(c->name);
	le->set_max_length(22);
	le->set_placeholder(S("Например: М. Пайн"));
	le->set_custom_minimum_size(Vector2(0, 60));
	le->add_theme_font_size_override("font_size", 26);
	le->set_select_all_on_focus(true);
	v->add_child(le);
	HBoxContainer *h = hbox(12);
	Button *cancel = button(S("Отмена"), PANEL2, 22, Vector2(0, 60));
	cancel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	cancel->connect("pressed", Fn(cancel, [v] { close_modal(v); }));
	h->add_child(cancel);
	Button *ok = primary(S("Готово"), 24);
	ok->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	ok->connect("pressed", Fn(ok, [host, v, le, card_id, done] {
		if (!GameData::get().rename_card(card_id, le->get_text())) {
			toast(host, S("Имя слишком короткое"), RED);
			return;
		}
		close_modal(v);
		if (done) done();
	}));
	h->add_child(ok);
	v->add_child(h);
	le->call_deferred("grab_focus");
}

Ref<Font> font_bold() {
	if (g_bold.is_null()) {
		g_bold.instantiate();
		Ref<FontFile> base = ResourceLoader::get_singleton()->load("res://assets/fonts/Montserrat.ttf");
		if (base.is_valid()) {
			g_bold->set_base_font(base);
			Dictionary v;
			v[(int64_t)0x77676874] = 800;
			g_bold->set_variation_opentype(v);
		}
	}
	return g_bold;
}

Control *progress(float frac, Color c, float h) {
	Control *bar = memnew(Control);
	bar->set_custom_minimum_size(Vector2(0, h));
	bar->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	bar->connect("draw", Fn(bar, [bar, frac, c] {
		Vector2 s = bar->get_size();
		bar->draw_rect(Rect2(Vector2(), s), Color(1, 1, 1, 0.1f));
		bar->draw_rect(Rect2(Vector2(), Vector2(s.x * clampf(frac, 0.f, 1.f), s.y)), c);
	}));
	return bar;
}

Control *stat_bar(const String &name, int a, int b, bool percent) {
	VBoxContainer *v = vbox(2);
	v->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	HBoxContainer *h = hbox(6);
	h->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	h->add_child(label(I(a) + (percent ? "%" : ""), 18, TEXT));
	h->add_child(spacer());
	h->add_child(label(name, 16, MUTED, HORIZONTAL_ALIGNMENT_CENTER));
	h->add_child(spacer());
	h->add_child(label(I(b) + (percent ? "%" : ""), 18, TEXT, HORIZONTAL_ALIGNMENT_RIGHT));
	v->add_child(h);
	Control *bar = memnew(Control);
	bar->set_custom_minimum_size(Vector2(0, 6));
	bar->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	float fa = (a + b) > 0 ? (float)a / (a + b) : 0.5f;
	bar->connect("draw", Fn(bar, [bar, fa] {
		Vector2 s = bar->get_size();
		bar->draw_rect(Rect2(Vector2(), Vector2(s.x * fa - 2, s.y)), ACCENT);
		bar->draw_rect(Rect2(Vector2(s.x * fa + 2, 0), Vector2(s.x * (1 - fa) - 2, s.y)), Color(1, 1, 1, 0.35f));
	}));
	v->add_child(bar);
	return v;
}

void fade_in(Control *c, float delay) {
	c->set_modulate(Color(1, 1, 1, 0));
	Ref<Tween> tw = c->create_tween();
	if (delay > 0.f) tw->tween_interval(delay);
	tw->tween_property(c, "modulate:a", 1.0, 0.25);
}

PanelContainer *panel(Color c, int radius, float margin) {
	PanelContainer *p = memnew(PanelContainer);
	p->add_theme_stylebox_override("panel", sb(c, radius, 0, Color(), margin));
	return p;
}

OptionButton *option(const std::vector<String> &items, int selected, int fs) {
	OptionButton *o = memnew(OptionButton);
	for (auto &it : items) o->add_item(it);
	o->select(selected);
	o->set_focus_mode(Control::FOCUS_NONE);
	o->set_custom_minimum_size(Vector2(0, 52));
	o->add_theme_font_size_override("font_size", fs);
	o->add_theme_stylebox_override("normal", sb(PANEL2, 10));
	o->add_theme_stylebox_override("hover", sb(PANEL2.lightened(0.1f), 10, 2, LIME));
	o->add_theme_stylebox_override("pressed", sb(PANEL2.darkened(0.2f), 10));
	o->add_theme_stylebox_override("focus", memnew(StyleBoxEmpty));
	o->get_popup()->add_theme_font_size_override("font_size", fs + 4);
	return o;
}

HBoxContainer *hbox(int sep) {
	HBoxContainer *h = memnew(HBoxContainer);
	h->add_theme_constant_override("separation", sep);
	return h;
}

VBoxContainer *vbox(int sep) {
	VBoxContainer *v = memnew(VBoxContainer);
	v->add_theme_constant_override("separation", sep);
	return v;
}

Control *spacer(bool vertical) {
	Control *c = memnew(Control);
	if (vertical) c->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	else c->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	c->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	return c;
}

void full_rect(Control *c) { c->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT); }

Control *badge(const String &logo_path, Color shirt, const String &shrt, float size) {
	Ref<Texture2D> tex = GameData::get().logo_texture(logo_path);
	if (tex.is_valid()) {
		TextureRect *tr = memnew(TextureRect);
		tr->set_texture(tex);
		tr->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
		tr->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
		tr->set_custom_minimum_size(Vector2(size, size));
		tr->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		return tr;
	}
	Control *c = memnew(Control);
	c->set_custom_minimum_size(Vector2(size, size));
	c->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	String t = shrt.left(3);
	c->connect("draw", Fn(c, [c, shirt, t, size] {
		Vector2 ctr = c->get_size() * 0.5f;
		c->draw_circle(ctr, size * 0.48f, Color(0.03f, 0.05f, 0.1f));
		c->draw_circle(ctr, size * 0.42f, shirt);
		Ref<Font> f = c->get_theme_default_font();
		int fs = (int)(size * 0.3f);
		Vector2 ts = f->get_string_size(t, HORIZONTAL_ALIGNMENT_LEFT, -1, fs);
		Color tc = shirt.get_luminance() > 0.6f ? Color(0.05f, 0.05f, 0.1f) : Color(1, 1, 1);
		c->draw_string(f, Vector2(ctr.x - ts.x * 0.5f, ctr.y + fs * 0.36f), t, HORIZONTAL_ALIGNMENT_LEFT, -1, fs, tc);
	}));
	return c;
}

String short_name(const String &n) {
	PackedStringArray parts = n.split(" ");
	if (parts.size() >= 2 && n.length() > 13) return parts[0].left(1) + ". " + parts[parts.size() - 1];
	return n;
}

PanelContainer *card(const Card &c, Vector2 size, const String &slot_pos) {
	GameData &g = GameData::get();
	String r = GameData::rarity(c.ovr);
	Color col = GameData::rarity_color(r);
	PanelContainer *p = memnew(PanelContainer);
	p->set_custom_minimum_size(size);
	Ref<StyleBoxFlat> st = sb(col.darkened(0.6f), 10, 3, col, 8);
	if (r == "legend") {
		st->set_bg_color(Color::html("2a0f4a"));
		st->set_shadow_color(Color(0.7f, 0.4f, 1.0f, 0.5f));
		st->set_shadow_size(8);
	}
	p->add_theme_stylebox_override("panel", st);
	VBoxContainer *v = vbox(0);
	v->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	p->add_child(v);
	HBoxContainer *top = hbox(4);
	top->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	int ov = c.ovr;
	Color oc = TEXT;
	if (!slot_pos.is_empty()) {
		ov = g.effective_ovr(c, slot_pos);
		if (ov < c.ovr - 1) oc = Color::html("fca5a5");
	}
	top->add_child(label(I(ov), (int)(size.y * 0.2f), oc));
	top->add_child(spacer());
	top->add_child(label(GameData::pos_ru(c.pos), (int)(size.y * 0.11f), col.lightened(0.3f), HORIZONTAL_ALIGNMENT_RIGHT));
	v->add_child(top);
	if (size.y >= 150) {
		CenterContainer *pc = memnew(CenterContainer);
		pc->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		pc->add_child(portrait(c, std::round(size.y * (c.photo.is_empty() ? 0.26f : 0.32f)), col));
		v->add_child(pc);
	}
	Label *nm = label(short_name(c.name), (int)(size.y * 0.085f), TEXT, HORIZONTAL_ALIGNMENT_CENTER);
	nm->set_clip_text(true);
	nm->set_custom_minimum_size(Vector2(size.x - 20, 0));
	v->add_child(nm);
	if (size.y >= 180) {
		GridContainer *grid = memnew(GridContainer);
		grid->set_columns(2);
		grid->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		grid->add_theme_constant_override("h_separation", 6);
		std::vector<String> keys = { "pace", "sho", "pas", "dri", "def", "phy" };
		if (c.pos == "GK") keys = { "gk", "pas", "phy", "pace" };
		for (auto &k : keys)
			grid->add_child(label(I(c.stat(k)) + " " + GameData::stat_ru(k), (int)(size.y * 0.068f), MUTED.lightened(0.3f)));
		v->add_child(grid);
	}
	v->add_child(label(GameData::rarity_name(r), (int)(size.y * 0.06f), col, HORIZONTAL_ALIGNMENT_CENTER));
	return p;
}

VBoxContainer *modal(Control *parent, float width) {
	ColorRect *dim = memnew(ColorRect);
	dim->set_color(Color(0, 0, 0, 0.75f));
	dim->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	parent->add_child(dim);
	full_rect(dim);
	CenterContainer *cc = memnew(CenterContainer);
	dim->add_child(cc);
	full_rect(cc);
	PanelContainer *p = memnew(PanelContainer);
	p->add_theme_stylebox_override("panel", sb(Color::html("0b1736"), 16, 2, ACCENT.darkened(0.3f), 26));
	p->set_custom_minimum_size(Vector2(width, 0));
	cc->add_child(p);
	VBoxContainer *v = vbox(14);
	p->add_child(v);
	v->set_meta("modal_root", dim);
	return v;
}

void close_modal(Control *v) {
	if (v && v->has_meta("modal_root")) {
		Object *o = v->get_meta("modal_root");
		Node *n = Object::cast_to<Node>(o);
		if (n) n->queue_free();
	}
}

void toast(Control *parent, const String &text, Color c) {
	PanelContainer *p = panel(Color(0, 0, 0, 0.85f), 12, 20);
	p->add_child(label(text, 28, c, HORIZONTAL_ALIGNMENT_CENTER));
	p->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	p->set_custom_minimum_size(Vector2(560, 0));
	parent->add_child(p);
	p->set_position(Vector2(parent->get_size().x * 0.5f - 280, 96));
	Ref<Tween> tw = p->create_tween();
	tw->tween_interval(1.8);
	tw->tween_property(p, "modulate:a", 0.0, 0.4);
	tw->tween_callback(Callable(p, "queue_free"));
}

void clickable(Control *c, std::function<void()> cb) {
	c->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	c->connect("gui_input", FnV(c, [cb](const Variant &e) {
		Ref<InputEventMouseButton> mb = e;
		if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MOUSE_BUTTON_LEFT) {
			PMSfx::p("click", -6.f);
			cb();
		}
	}));
}

String fmt_int(int n) {
	String s = I(n < 0 ? -n : n);
	String out;
	while (s.length() > 3) {
		out = " " + s.right(3) + out;
		s = s.left(s.length() - 3);
	}
	return (n < 0 ? "-" : "") + s + out;
}

String coins_text() { return fmt_int(GameData::get().coins) + S(" монет"); }

} // namespace kit

// ------------------------------------------------------------------ PMScreen
VBoxContainer *PMScreen::root() {
	TextureRect *dim = memnew(TextureRect);
	Ref<Gradient> g;
	g.instantiate();
	g->set_color(0, Color(0.02f, 0.03f, 0.09f, 0.6f));
	g->set_color(1, Color(0.02f, 0.03f, 0.08f, 0.9f));
	Ref<GradientTexture2D> gt;
	gt.instantiate();
	gt->set_gradient(g);
	gt->set_fill_from(Vector2(0, 0));
	gt->set_fill_to(Vector2(0, 1));
	gt->set_width(8);
	gt->set_height(128);
	dim->set_texture(gt);
	dim->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	dim->set_stretch_mode(TextureRect::STRETCH_SCALE);
	dim->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	add_child(dim);
	kit::full_rect(dim);
	MarginContainer *m = memnew(MarginContainer);
	m->add_theme_constant_override("margin_left", 34);
	m->add_theme_constant_override("margin_right", 34);
	m->add_theme_constant_override("margin_top", 20);
	m->add_theme_constant_override("margin_bottom", 20);
	add_child(m);
	kit::full_rect(m);
	VBoxContainer *v = kit::vbox(14);
	m->add_child(v);
	kit::fade_in(m);
	return v;
}

Label *PMScreen::header(VBoxContainer *parent, const String &title, std::function<void()> on_back) {
	HBoxContainer *bar = kit::hbox(16);
	Button *back = kit::button(S("‹ Назад"), kit::PANEL2, 24, Vector2(150, 56));
	back->connect("pressed", Fn(back, on_back));
	bar->add_child(back);
	Label *t = kit::label(title, 34, kit::TEXT);
	t->add_theme_font_override("font", kit::font_bold());
	t->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	bar->add_child(t);
	PanelContainer *cp = kit::panel(kit::PANEL, 10, 14);
	coins_label = kit::label(kit::coins_text(), 26, kit::GOLD, HORIZONTAL_ALIGNMENT_RIGHT);
	cp->add_child(coins_label);
	bar->add_child(cp);
	parent->add_child(bar);
	seen_rev = GameData::get().coins_rev;
	return coins_label;
}

void PMScreen::on_back() { PMApp::go("menu"); }

void PMScreen::_process(double) {
	if (coins_label && seen_rev != GameData::get().coins_rev) {
		seen_rev = GameData::get().coins_rev;
		coins_label->set_text(kit::coins_text());
	}
}

} // namespace pm
