#include "app.h"
#include "screens.h"
#include "sfx.h"
#include "match/match.h"

#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/style_box_empty.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/grid_container.hpp>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture2d.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/classes/callback_tweener.hpp>
#include <godot_cpp/classes/interval_tweener.hpp>

using namespace godot;

namespace pm {

Node *make_screen(const String &name, const Dictionary &params) {
	Node *n = nullptr;
	if (name == "title") n = memnew(PMTitle);
	else if (name == "menu") n = memnew(PMMainMenu);
	else if (name == "squad") n = memnew(PMSquadScreen);
	else if (name == "packs") n = memnew(PMPacksScreen);
	else if (name == "league") n = memnew(PMLeagueScreen);
	else if (name == "pvp") n = memnew(PMPvpScreen);
	else if (name == "settings") n = memnew(PMSettingsScreen);
	else if (name == "patches") n = memnew(PMPatchScreen);
	else if (name == "result") n = memnew(PMResultScreen);
	else if (name == "match") {
		PMMatch *m = memnew(PMMatch);
		m->params = params;
		return m;
	}
	PMScreen *s = Object::cast_to<PMScreen>(n);
	if (s) s->params = params;
	return n;
}

// ================================================================== title
void PMTitle::_ready() {
	set_mouse_filter(MOUSE_FILTER_STOP);
	ColorRect *dim = memnew(ColorRect);
	dim->set_color(Color(0.0f, 0.02f, 0.08f, 0.45f));
	dim->set_mouse_filter(MOUSE_FILTER_IGNORE);
	add_child(dim);
	kit::full_rect(dim);
	VBoxContainer *v = kit::vbox(6);
	v->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	v->set_mouse_filter(MOUSE_FILTER_IGNORE);
	add_child(v);
	kit::full_rect(v);
	Label *t = kit::label("PITCH MASTERS", 100, Color(1, 1, 1), HORIZONTAL_ALIGNMENT_CENTER);
	t->add_theme_font_override("font", kit::font_bold());
	t->add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.45f));
	t->add_theme_constant_override("shadow_offset_y", 6);
	v->add_child(t);
	Label *y = kit::label(S("С Е З О Н   2 0 2 7"), 34, kit::LIME, HORIZONTAL_ALIGNMENT_CENTER);
	v->add_child(y);
	GameData &g = GameData::get();
	if (g.patch.active) v->add_child(kit::label(S("Патч: ") + g.patch.name, 24, kit::ACCENT, HORIZONTAL_ALIGNMENT_CENTER));
	Control *gap = memnew(Control);
	gap->set_custom_minimum_size(Vector2(0, 70));
	v->add_child(gap);
	press = kit::label(S("НАЖМИТЕ, ЧТОБЫ НАЧАТЬ"), 34, Color(1, 1, 1), HORIZONTAL_ALIGNMENT_CENTER);
	v->add_child(press);
	Label *ver = kit::label(S("v3.0 • C++"), 18, kit::MUTED, HORIZONTAL_ALIGNMENT_RIGHT);
	add_child(ver);
	ver->set_anchors_and_offsets_preset(PRESET_BOTTOM_RIGHT);
	ver->set_position(ver->get_position() + Vector2(-24, -16));
}

void PMTitle::_process(double delta) {
	t += delta;
	if (press) press->set_modulate(Color(1, 1, 1, 0.45f + 0.55f * (0.5f + 0.5f * std::sin((float)t * 3.f))));
}

void PMTitle::_gui_input(const Ref<InputEvent> &e) {
	Ref<InputEventMouseButton> mb = e;
	if (mb.is_valid() && mb->is_pressed()) {
		PMSfx::p("whistle", -8.f);
		PMApp::go("menu");
	}
}

void PMTitle::on_back() { get_tree()->quit(); }

// ================================================================== main menu
void PMMainMenu::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open_missions"), &PMMainMenu::open_missions);
	ClassDB::bind_method(D_METHOD("open_daily"), &PMMainMenu::open_daily);
}

void PMMainMenu::on_back() { PMApp::go("title"); }

// simple vector icons for the bottom navigation
static Control *nav_icon(int kind, Color c) {
	Control *ic = memnew(Control);
	ic->set_custom_minimum_size(Vector2(30, 30));
	ic->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	ic->connect("draw", Fn(ic, [ic, kind, c] {
		Vector2 o = ic->get_size() * 0.5f;
		switch (kind) {
			case 0: { // home
				PackedVector2Array roof;
				roof.push_back(o + Vector2(-13, -1));
				roof.push_back(o + Vector2(0, -13));
				roof.push_back(o + Vector2(13, -1));
				ic->draw_colored_polygon(roof, c);
				ic->draw_rect(Rect2(o + Vector2(-9, -2), Vector2(18, 14)), c);
				ic->draw_rect(Rect2(o + Vector2(-3, 4), Vector2(6, 8)), Color(0.04f, 0.06f, 0.13f));
			} break;
			case 1: { // ball
				ic->draw_circle(o, 13, c);
				PackedVector2Array pent;
				for (int i = 0; i < 5; i++) {
					float a = -Math_PI * 0.5f + i * Math_TAU / 5.f;
					pent.push_back(o + Vector2(std::cos(a), std::sin(a)) * 5.5f);
				}
				ic->draw_colored_polygon(pent, Color(0.04f, 0.06f, 0.13f));
				for (int i = 0; i < 5; i++) {
					float a = -Math_PI * 0.5f + i * Math_TAU / 5.f;
					ic->draw_line(o + Vector2(std::cos(a), std::sin(a)) * 5.5f, o + Vector2(std::cos(a), std::sin(a)) * 12.f, Color(0.04f, 0.06f, 0.13f), 2.f);
				}
			} break;
			case 2: { // shirt
				PackedVector2Array sh;
				Vector2 pts[] = { { -5, -12 }, { -14, -7 }, { -11, 0 }, { -8, -2 }, { -8, 12 }, { 8, 12 }, { 8, -2 }, { 11, 0 }, { 14, -7 }, { 5, -12 }, { 0, -8 } };
				for (auto &pt : pts) sh.push_back(o + pt);
				ic->draw_colored_polygon(sh, c);
			} break;
			case 3: { // shopping bag
				ic->draw_rect(Rect2(o + Vector2(-11, -5), Vector2(22, 17)), c);
				ic->draw_arc(o + Vector2(0, -5), 6, Math_PI, Math_TAU, 12, c, 2.5f);
			} break;
			default: // more
				for (int i = -1; i <= 1; i++) ic->draw_circle(o + Vector2(i * 9.f, 0), 3.2f, c);
				break;
		}
	}));
	return ic;
}

void PMMainMenu::_ready() {
	GameData &g = GameData::get();
	set_mouse_filter(MOUSE_FILTER_IGNORE);
	// left-to-right gradient so the 3D player on the right stays visible
	TextureRect *shade = memnew(TextureRect);
	Ref<Gradient> gr;
	gr.instantiate();
	gr->set_color(0, Color(0.02f, 0.03f, 0.09f, 0.94f));
	gr->set_color(1, Color(0.02f, 0.03f, 0.09f, 0.0f));
	gr->add_point(0.55f, Color(0.02f, 0.03f, 0.09f, 0.72f));
	Ref<GradientTexture2D> gt;
	gt.instantiate();
	gt->set_gradient(gr);
	gt->set_fill_from(Vector2(0, 0));
	gt->set_fill_to(Vector2(1, 0));
	gt->set_width(256);
	gt->set_height(4);
	shade->set_texture(gt);
	shade->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	shade->set_stretch_mode(TextureRect::STRETCH_SCALE);
	shade->set_mouse_filter(MOUSE_FILTER_IGNORE);
	add_child(shade);
	kit::full_rect(shade);

	MarginContainer *m = memnew(MarginContainer);
	m->add_theme_constant_override("margin_left", 30);
	m->add_theme_constant_override("margin_right", 30);
	m->add_theme_constant_override("margin_top", 18);
	m->add_theme_constant_override("margin_bottom", 12);
	m->set_mouse_filter(MOUSE_FILTER_IGNORE);
	add_child(m);
	kit::full_rect(m);
	VBoxContainer *v = kit::vbox(12);
	v->set_mouse_filter(MOUSE_FILTER_IGNORE);
	m->add_child(v);

	// ---- top bar
	HBoxContainer *top = kit::hbox(14);
	top->set_mouse_filter(MOUSE_FILTER_IGNORE);
	VBoxContainer *logo = kit::vbox(-8);
	logo->set_mouse_filter(MOUSE_FILTER_IGNORE);
	Label *t = kit::label("PITCH MASTERS", 34, Color(1, 1, 1));
	t->add_theme_font_override("font", kit::font_bold());
	logo->add_child(t);
	logo->add_child(kit::label(S("СЕЗОН 2027") + (g.patch.active ? S("  •  ") + g.patch.name.to_upper() : String()), 16, kit::LIME));
	top->add_child(logo);
	top->add_child(kit::spacer());
	// club chip
	PanelContainer *club = kit::glass(10);
	HBoxContainer *ch = kit::hbox(10);
	String my_logo = g.patch.active && g.patch.has_my_team ? g.patch.my_team.logo : String();
	ch->add_child(kit::badge(my_logo, html(g.shirt), g.team_short, 40));
	VBoxContainer *cv = kit::vbox(-6);
	cv->add_child(kit::label(g.team_name, 20, kit::TEXT));
	cv->add_child(kit::label(S("Рейтинг ") + I(g.team_ovr()) + S("  •  ") + I(g.stats.won) + "-" + I(g.stats.drawn) + "-" + I(g.stats.lost), 15, kit::MUTED));
	ch->add_child(cv);
	club->add_child(ch);
	top->add_child(club);
	// coins
	PanelContainer *cp = kit::glass(12);
	HBoxContainer *coh = kit::hbox(8);
	Control *coin = memnew(Control);
	coin->set_custom_minimum_size(Vector2(22, 22));
	coin->connect("draw", Fn(coin, [coin] {
		Vector2 o = coin->get_size() * 0.5f;
		coin->draw_circle(o, 10, kit::GOLD);
		coin->draw_circle(o, 6.5f, kit::GOLD.darkened(0.25f));
	}));
	coh->add_child(coin);
	coins_label = kit::label(kit::fmt_int(g.coins), 22, kit::GOLD);
	coins_label->add_theme_font_override("font", kit::font_bold());
	coh->add_child(coins_label);
	cp->add_child(coh);
	seen_rev = g.coins_rev;
	top->add_child(cp);
	// gift (daily reward + missions)
	Button *gift = kit::button("", Color(0.08f, 0.11f, 0.22f, 0.8f), 20, Vector2(56, 56));
	Control *gi = memnew(Control);
	gi->set_mouse_filter(MOUSE_FILTER_IGNORE);
	gift->add_child(gi);
	kit::full_rect(gi);
	gi->connect("draw", Fn(gi, [gi] {
		Vector2 o = gi->get_size() * 0.5f + Vector2(0, 2);
		gi->draw_rect(Rect2(o + Vector2(-12, -6), Vector2(24, 16)), kit::LIME);
		gi->draw_rect(Rect2(o + Vector2(-14, -11), Vector2(28, 6)), kit::LIME.lightened(0.2f));
		gi->draw_rect(Rect2(o + Vector2(-2, -11), Vector2(4, 21)), Color(0.9f, 0.2f, 0.3f));
		gi->draw_arc(o + Vector2(-5, -13), 4, Math_PI * 0.2f, Math_PI * 1.6f, 10, kit::LIME, 2.f);
		gi->draw_arc(o + Vector2(5, -13), 4, -Math_PI * 0.6f, Math_PI * 0.8f, 10, kit::LIME, 2.f);
	}));
	PanelContainer *dot = kit::panel(Color::html("ef4444"), 11, 2);
	dot->set_mouse_filter(MOUSE_FILTER_IGNORE);
	gift_count = kit::label("1", 14, Color(1, 1, 1), HORIZONTAL_ALIGNMENT_CENTER);
	gift_count->set_custom_minimum_size(Vector2(18, 0));
	dot->add_child(gift_count);
	gift->add_child(dot);
	dot->set_position(Vector2(36, -6));
	gift_dot = dot;
	gift->connect("pressed", Fn(gift, [this] { missions_popup(); }));
	top->add_child(gift);
	v->add_child(top);

	// ---- content (left ~58% of the screen; the 3D player stands on the right)
	HBoxContainer *body = kit::hbox(0);
	body->set_v_size_flags(SIZE_EXPAND_FILL);
	body->set_mouse_filter(MOUSE_FILTER_IGNORE);
	content = kit::vbox(12);
	content->set_h_size_flags(SIZE_EXPAND_FILL);
	content->set_stretch_ratio(1.4f);
	content->set_mouse_filter(MOUSE_FILTER_IGNORE);
	body->add_child(content);
	Control *right = memnew(Control);
	right->set_h_size_flags(SIZE_EXPAND_FILL);
	right->set_mouse_filter(MOUSE_FILTER_IGNORE);
	body->add_child(right);
	// star player caption
	const Card *best = nullptr;
	for (auto &c : g.cards)
		if (!best || c.ovr > best->ovr) best = &c;
	if (best) {
		PanelContainer *capp = kit::glass(14);
		capp->set_mouse_filter(MOUSE_FILTER_IGNORE);
		right->add_child(capp);
		capp->set_anchors_and_offsets_preset(PRESET_BOTTOM_RIGHT);
		capp->set_offset(SIDE_LEFT, -330);
		capp->set_offset(SIDE_TOP, -118);
		capp->set_offset(SIDE_RIGHT, 0);
		capp->set_offset(SIDE_BOTTOM, -6);
		VBoxContainer *cap = kit::vbox(-4);
		cap->set_mouse_filter(MOUSE_FILTER_IGNORE);
		capp->add_child(cap);
		cap->add_child(kit::label(S("ЗВЕЗДА КОМАНДЫ"), 15, kit::LIME, HORIZONTAL_ALIGNMENT_RIGHT));
		Label *bn = kit::label(best->name, 28, Color(1, 1, 1), HORIZONTAL_ALIGNMENT_RIGHT);
		bn->add_theme_font_override("font", kit::font_bold());
		cap->add_child(bn);
		cap->add_child(kit::label(GameData::pos_ru(best->pos) + S("  •  ") + I(best->ovr) + (best->lvl > 0 ? S("  •  ур. ") + I(best->lvl) : String()), 17, kit::MUTED, HORIZONTAL_ALIGNMENT_RIGHT));
	}
	v->add_child(body);

	// ---- bottom navigation
	PanelContainer *nav = kit::glass(6);
	tabs_row = kit::hbox(4);
	nav->add_child(tabs_row);
	HBoxContainer *navwrap = kit::hbox(0);
	navwrap->set_mouse_filter(MOUSE_FILTER_IGNORE);
	navwrap->add_child(nav);
	navwrap->add_child(kit::spacer());
	v->add_child(navwrap);
	if (params.has("tab")) tab = clampi((int)params["tab"], 0, 4);
	build_tabs();
	build_content();
	refresh_gift();
	// daily reward popup once per day
	static bool shown_this_session = false;
	bool new_day = g.refresh_daily();
	if (new_day || (!g.daily.reward_claimed && !shown_this_session)) {
		shown_this_session = true;
		Ref<Tween> tw = create_tween();
		tw->tween_interval(0.45);
		tw->tween_callback(Fn(this, [this] { daily_popup(); }));
	}
}

void PMMainMenu::refresh_gift() {
	if (!gift_dot) return;
	int n = GameData::get().missions_ready();
	gift_dot->set_visible(n > 0);
	gift_count->set_text(I(n));
}

void PMMainMenu::_process(double delta) {
	if (coins_label && seen_rev != GameData::get().coins_rev) {
		seen_rev = GameData::get().coins_rev;
		coins_label->set_text(kit::fmt_int(GameData::get().coins));
		refresh_gift();
	}
}

void PMMainMenu::build_tabs() {
	for (int i = 0; i < tabs_row->get_child_count(); i++) tabs_row->get_child(i)->queue_free();
	const char *names[5] = { "Главная", "Играть", "Клуб", "Магазин", "Ещё" };
	for (int i = 0; i < 5; i++) {
		bool sel = i == tab;
		Button *b = memnew(Button);
		b->set_focus_mode(FOCUS_NONE);
		b->set_custom_minimum_size(Vector2(118, 62));
		Ref<StyleBoxFlat> st = kit::sb(sel ? Color(1, 1, 1, 0.1f) : Color(0, 0, 0, 0), 14);
		Ref<StyleBoxFlat> hv = kit::sb(Color(1, 1, 1, 0.06f), 14);
		b->add_theme_stylebox_override("normal", st);
		b->add_theme_stylebox_override("hover", hv);
		b->add_theme_stylebox_override("pressed", st);
		b->add_theme_stylebox_override("focus", memnew(StyleBoxEmpty));
		VBoxContainer *bv = kit::vbox(0);
		bv->set_mouse_filter(MOUSE_FILTER_IGNORE);
		bv->set_alignment(BoxContainer::ALIGNMENT_CENTER);
		b->add_child(bv);
		kit::full_rect(bv);
		Color c = sel ? kit::LIME : kit::MUTED;
		CenterContainer *icc = memnew(CenterContainer);
		icc->set_mouse_filter(MOUSE_FILTER_IGNORE);
		icc->add_child(nav_icon(i, c));
		bv->add_child(icc);
		Label *l = kit::label(S(names[i]), 14, sel ? Color(1, 1, 1) : kit::MUTED, HORIZONTAL_ALIGNMENT_CENTER);
		if (sel) l->add_theme_font_override("font", kit::font_bold());
		bv->add_child(l);
		b->connect("pressed", Fn(b, [this, i] {
			if (tab == i) return;
			PMSfx::p("click", -6.f);
			tab = i;
			build_tabs();
			build_content();
		}));
		tabs_row->add_child(b);
	}
}

Control *PMMainMenu::card(const String &kicker, const String &title, const String &sub, Color accent, std::function<void()> cb, float min_h, float progress, const String &cta) {
	Button *b = memnew(Button);
	b->set_focus_mode(FOCUS_NONE);
	b->set_h_size_flags(SIZE_EXPAND_FILL);
	b->set_custom_minimum_size(Vector2(0, min_h));
	Ref<StyleBoxFlat> n = kit::sb(Color(0.07f, 0.1f, 0.21f, 0.82f), 20, 1, Color(1, 1, 1, 0.08f));
	n->set_border_width(SIDE_LEFT, 5);
	n->set_border_color(accent);
	n->set_shadow_color(Color(0, 0, 0, 0.3f));
	n->set_shadow_size(12);
	n->set_shadow_offset(Vector2(0, 5));
	Ref<StyleBoxFlat> h = n->duplicate();
	h->set_bg_color(Color(0.1f, 0.14f, 0.28f, 0.9f));
	h->set_border_color(accent.lightened(0.2f));
	b->add_theme_stylebox_override("normal", n);
	b->add_theme_stylebox_override("hover", h);
	b->add_theme_stylebox_override("pressed", h);
	b->add_theme_stylebox_override("focus", memnew(StyleBoxEmpty));
	// diagonal accent glow
	Control *glow = memnew(Control);
	glow->set_mouse_filter(MOUSE_FILTER_IGNORE);
	b->add_child(glow);
	kit::full_rect(glow);
	glow->connect("draw", Fn(glow, [glow, accent] {
		Vector2 s = glow->get_size();
		PackedVector2Array poly;
		poly.push_back(Vector2(s.x * 0.62f, 0));
		poly.push_back(Vector2(s.x - 18, 0));
		poly.push_back(Vector2(s.x - 18, s.y));
		poly.push_back(Vector2(s.x * 0.48f, s.y));
		PackedColorArray cols;
		cols.push_back(Color(accent, 0.0f));
		cols.push_back(Color(accent, 0.22f));
		cols.push_back(Color(accent, 0.12f));
		cols.push_back(Color(accent, 0.0f));
		glow->draw_polygon(poly, cols);
	}));
	VBoxContainer *v = kit::vbox(2);
	v->set_mouse_filter(MOUSE_FILTER_IGNORE);
	b->add_child(v);
	kit::full_rect(v);
	v->set_offset(SIDE_LEFT, 26);
	v->set_offset(SIDE_TOP, 14);
	v->set_offset(SIDE_RIGHT, -20);
	v->set_offset(SIDE_BOTTOM, -14);
	v->add_child(kit::label(kicker, 14, accent.lightened(0.35f)));
	v->add_child(kit::spacer(true));
	Label *tl = kit::label(title, min_h > 200 ? 40 : 24, Color(1, 1, 1));
	tl->add_theme_font_override("font", kit::font_bold());
	v->add_child(tl);
	if (!sub.is_empty()) {
		Label *sl = kit::label(sub, min_h > 200 ? 19 : 16, kit::MUTED);
		sl->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		v->add_child(sl);
	}
	if (progress >= 0.f) {
		Control *pg = kit::progress(progress, accent, 6);
		v->add_child(pg);
	}
	if (!cta.is_empty()) {
		HBoxContainer *ch = kit::hbox(0);
		ch->set_mouse_filter(MOUSE_FILTER_IGNORE);
		PanelContainer *cp = kit::panel(kit::LIME, 12, 10);
		cp->set_mouse_filter(MOUSE_FILTER_IGNORE);
		Label *cl = kit::label(cta, 20, Color(0.03f, 0.05f, 0.1f));
		cl->add_theme_font_override("font", kit::font_bold());
		cp->add_child(cl);
		ch->add_child(cp);
		v->add_child(kit::spacer(true));
		v->add_child(ch);
	}
	b->connect("pressed", Fn(b, [cb] {
		PMSfx::p("click", -6.f);
		cb();
	}));
	kit::press_anim(b);
	return b;
}

void PMMainMenu::build_content() {
	for (int i = 0; i < content->get_child_count(); i++) content->get_child(i)->queue_free();
	GameData &g = GameData::get();
	auto row = [this]() {
		HBoxContainer *h = kit::hbox(12);
		h->set_mouse_filter(MOUSE_FILTER_IGNORE);
		content->add_child(h);
		return h;
	};
	int done = 0;
	for (auto &mi : g.daily.missions) done += mi.done() ? 1 : 0;
	int nm = std::max<int>(1, (int)g.daily.missions.size());
	String league_sub = g.league.name + S(" • тур ") + I(std::min(g.league.round + 1, 7)) + S("/7 • ") + I(g.league_user_place()) + S(" место");
	switch (tab) {
		case 0: {
			content->add_child(card(S("БЫСТРАЯ ИГРА"), S("Товарищеский матч"), S("Против ИИ • ") + S(GameData::difficulty_name(g.settings.difficulty)) + S(" • ") + I(g.settings.minutes) + S(" мин"),
					Color::html("3b82f6"), [this] { quick_match(); }, 236, -1.f, S("ИГРАТЬ  ›")));
			HBoxContainer *h = row();
			h->add_child(card(S("ТУРНИР"), S("Лига"), league_sub, Color::html("8b5cf6"), [] { PMApp::go("league"); }, 132, g.league.round / 7.f));
			h->add_child(card(S("ОНЛАЙН"), S("PvP"), S("Wi-Fi • кнопка АВТО"), Color::html("ef4444"), [] { PMApp::go("pvp"); }, 132));
			h->add_child(card(S("ЕЖЕДНЕВНО"), S("Задания"), I(done) + "/" + I(nm) + S(" выполнено"), Color::html("22c55e"), [this] { missions_popup(); }, 132, (float)done / nm));
		} break;
		case 1: {
			content->add_child(card(S("МАТЧ"), S("Товарищеский матч"), S("Выберите соперника, сложность и длительность"), Color::html("3b82f6"), [this] { quick_match(); }, 200, -1.f, S("ИГРАТЬ  ›")));
			HBoxContainer *h = row();
			h->add_child(card(S("ТУРНИР"), S("Лига"), league_sub, Color::html("8b5cf6"), [] { PMApp::go("league"); }, 168, g.league.round / 7.f));
			h->add_child(card(S("ОНЛАЙН"), S("PvP по Wi-Fi"), S("Двое на двух телефонах, у каждого кнопка АВТО"), Color::html("ef4444"), [] { PMApp::go("pvp"); }, 168));
		} break;
		case 2: {
			int lv = 0;
			for (auto &c : g.cards) lv += c.lvl;
			content->add_child(card(S("МОЙ КЛУБ"), S("Состав и тактика"), S("Схема ") + g.squad.formation + S(" • рейтинг ") + I(g.team_ovr()), Color::html("14b8a6"), [] { PMApp::go("squad"); }, 200, -1.f, S("ОТКРЫТЬ  ›")));
			HBoxContainer *h = row();
			h->add_child(card(S("КОЛЛЕКЦИЯ"), I((int)g.cards.size()) + S(" карточек"), S("Уровни игроков: ") + I(lv) + S(" • опыт за оценки в матчах"), Color::html("f59e0b"), [] { PMApp::go("squad"); }, 168));
			h->add_child(card(S("СТАТИСТИКА"), I(g.stats.played) + S(" матчей"), S("Победы ") + I(g.stats.won) + S(" • голы ") + I(g.stats.gf) + ":" + I(g.stats.ga), Color::html("64748b"), [] {}, 168));
		} break;
		case 3: {
			content->add_child(card(S("МАГАЗИН"), S("Паки и карточки"), S("Открывайте паки и усиливайте состав"), Color::html("f59e0b"), [] { PMApp::go("packs"); }, 200, -1.f, S("К ПАКАМ  ›")));
			HBoxContainer *h = row();
			h->add_child(card(S("БОНУС"), S("Ежедневная награда"), g.daily.reward_claimed ? S("Получено • серия ") + I(g.daily.streak) + S(" дн.") : S("Доступно: ") + kit::fmt_int(g.daily_reward_amount()) + S(" монет"),
					Color::html("22c55e"), [this] { daily_popup(); }, 168));
			h->add_child(card(S("ЗАДАНИЯ"), I(done) + "/" + I(nm), S("Выполняйте задания дня и получайте монеты"), Color::html("06b6d4"), [this] { missions_popup(); }, 168, (float)done / nm));
		} break;
		default: {
			content->add_child(card(S("РЕДАКТИРОВАНИЕ"), S("Патчи"), g.patch.active ? S("Активен: ") + g.patch.name : S("Добавьте файл патча: команды, игроки, эмблемы"), Color::html("06b6d4"), [] { PMApp::go("patches"); }, 200, -1.f, S("ОТКРЫТЬ  ›")));
			HBoxContainer *h = row();
			h->add_child(card(S("СИСТЕМА"), S("Настройки"), S("Сложность, камера, звук, повторы"), Color::html("64748b"), [] { PMApp::go("settings"); }, 168));
			h->add_child(card(S("ЭКРАН"), S("Титульный экран"), S("Вернуться к заставке"), Color::html("334155"), [] { PMApp::go("title"); }, 168));
		} break;
	}
	content->add_child(kit::spacer(true));
	for (int i = 0; i < content->get_child_count(); i++) {
		Control *c = Object::cast_to<Control>(content->get_child(i));
		if (c) kit::fade_in(c, 0.04f * i);
	}
}

void PMMainMenu::daily_popup() {
	GameData &g = GameData::get();
	VBoxContainer *v = kit::modal(this, 820);
	Label *t = kit::label(S("Ежедневная награда"), 34, kit::TEXT, HORIZONTAL_ALIGNMENT_CENTER);
	t->add_theme_font_override("font", kit::font_bold());
	v->add_child(t);
	v->add_child(kit::label(S("Заходите каждый день — на 7-й день большой приз"), 18, kit::MUTED, HORIZONTAL_ALIGNMENT_CENTER));
	HBoxContainer *days = kit::hbox(8);
	int cur = ((std::max(1, g.daily.streak) - 1) % 7) + 1;
	for (int d = 1; d <= 7; d++) {
		int amount = d == 7 ? 2500 : 250 + d * 100;
		bool past = d < cur || (d == cur && g.daily.reward_claimed);
		bool today = d == cur && !g.daily.reward_claimed;
		Color bg = today ? Color(kit::LIME, 0.22f) : (past ? Color(1, 1, 1, 0.05f) : Color(1, 1, 1, 0.08f));
		PanelContainer *p = kit::panel(bg, 12, 8);
		if (today) {
			Ref<StyleBoxFlat> st = kit::sb(bg, 12, 2, kit::LIME, 8);
			p->add_theme_stylebox_override("panel", st);
		}
		p->set_h_size_flags(SIZE_EXPAND_FILL);
		VBoxContainer *dv = kit::vbox(2);
		dv->add_child(kit::label(S("День ") + I(d), 15, today ? kit::LIME : kit::MUTED, HORIZONTAL_ALIGNMENT_CENTER));
		Label *al = kit::label(past ? S("Есть") : kit::fmt_int(amount), d == 7 ? 22 : 19, past ? kit::LIME : kit::GOLD, HORIZONTAL_ALIGNMENT_CENTER);
		al->add_theme_font_override("font", kit::font_bold());
		dv->add_child(al);
		p->add_child(dv);
		days->add_child(p);
	}
	v->add_child(days);
	v->add_child(kit::label(S("Серия входов: ") + I(g.daily.streak) + S(" дн."), 18, kit::ACCENT, HORIZONTAL_ALIGNMENT_CENTER));
	HBoxContainer *h = kit::hbox(12);
	Button *close = kit::button(S("Закрыть"), kit::PANEL2, 22, Vector2(0, 60));
	close->set_h_size_flags(SIZE_EXPAND_FILL);
	close->connect("pressed", Fn(close, [v] { kit::close_modal(v); }));
	h->add_child(close);
	if (!g.daily.reward_claimed) {
		Button *get = kit::primary(S("Забрать ") + kit::fmt_int(g.daily_reward_amount()));
		get->set_h_size_flags(SIZE_EXPAND_FILL);
		get->connect("pressed", Fn(get, [this, v] {
			int c = GameData::get().claim_daily_reward();
			PMSfx::p("goal", -10.f);
			kit::close_modal(v);
			kit::toast(this, S("+") + kit::fmt_int(c) + S(" монет"));
			refresh_gift();
			build_content();
		}));
		h->add_child(get);
	}
	v->add_child(h);
}

void PMMainMenu::missions_popup() {
	GameData &g = GameData::get();
	g.refresh_daily();
	VBoxContainer *v = kit::modal(this, 760);
	Label *t = kit::label(S("Задания дня"), 34, kit::TEXT, HORIZONTAL_ALIGNMENT_CENTER);
	t->add_theme_font_override("font", kit::font_bold());
	v->add_child(t);
	if (!g.daily.reward_claimed) {
		Button *dr = kit::button(S("Ежедневная награда доступна — забрать"), Color(0.13f, 0.5f, 0.25f), 20, Vector2(0, 52));
		dr->connect("pressed", Fn(dr, [this, v] {
			kit::close_modal(v);
			daily_popup();
		}));
		v->add_child(dr);
	}
	for (int i = 0; i < (int)g.daily.missions.size(); i++) {
		const Mission &mi = g.daily.missions[i];
		PanelContainer *p = kit::panel(Color(1, 1, 1, 0.06f), 14, 12);
		HBoxContainer *h = kit::hbox(14);
		VBoxContainer *mv = kit::vbox(4);
		mv->set_h_size_flags(SIZE_EXPAND_FILL);
		mv->add_child(kit::label(mi.text, 21, kit::TEXT));
		HBoxContainer *ph = kit::hbox(10);
		Control *pg = kit::progress((float)mi.progress / mi.goal, mi.done() ? kit::LIME : kit::ACCENT, 8);
		pg->set_h_size_flags(SIZE_EXPAND_FILL);
		pg->set_v_size_flags(SIZE_SHRINK_CENTER);
		ph->add_child(pg);
		ph->add_child(kit::label(I(mi.progress) + "/" + I(mi.goal), 16, kit::MUTED));
		mv->add_child(ph);
		h->add_child(mv);
		if (mi.claimed) {
			h->add_child(kit::label(S("Получено"), 18, kit::LIME));
		} else if (mi.done()) {
			Button *cb = kit::primary(S("+") + kit::fmt_int(mi.reward), 20);
			cb->set_custom_minimum_size(Vector2(150, 52));
			cb->connect("pressed", Fn(cb, [this, v, i] {
				int c = GameData::get().claim_mission(i);
				PMSfx::p("goal", -10.f);
				kit::close_modal(v);
				kit::toast(this, S("+") + kit::fmt_int(c) + S(" монет"));
				refresh_gift();
				build_content();
			}));
			h->add_child(cb);
		} else {
			h->add_child(kit::label(kit::fmt_int(mi.reward), 20, kit::GOLD));
		}
		p->add_child(h);
		v->add_child(p);
	}
	v->add_child(kit::label(S("Новые задания каждый день"), 16, kit::MUTED, HORIZONTAL_ALIGNMENT_CENTER));
	Button *close = kit::button(S("Закрыть"), kit::PANEL2, 22, Vector2(0, 58));
	close->connect("pressed", Fn(close, [v] { kit::close_modal(v); }));
	v->add_child(close);
}

void PMMainMenu::quick_match() {
	GameData &g = GameData::get();
	VBoxContainer *v = kit::modal(this, 780);
	v->add_child(kit::label(S("Товарищеский матч"), 36, kit::TEXT, HORIZONTAL_ALIGNMENT_CENTER));
	std::vector<TeamInfo> pool;
	if (g.patch.active && !g.patch.teams.empty()) pool = g.patch.teams;
	else
		for (auto &t : g.league.teams)
			if (!t.user) pool.push_back(t);
	std::vector<String> names;
	for (auto &t : pool) names.push_back(t.name + " (" + I(t.strength) + ")");
	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(2);
	grid->add_theme_constant_override("h_separation", 16);
	grid->add_theme_constant_override("v_separation", 12);
	grid->add_child(kit::label(S("Соперник"), 24, kit::MUTED));
	OptionButton *opp = kit::option(names, (int)(rng().next() % std::max<size_t>(1, names.size())));
	opp->set_h_size_flags(SIZE_EXPAND_FILL);
	grid->add_child(opp);
	grid->add_child(kit::label(S("Сложность"), 24, kit::MUTED));
	OptionButton *diff = kit::option({ S("Лёгкий"), S("Средний"), S("Сложный") }, g.settings.difficulty);
	grid->add_child(diff);
	grid->add_child(kit::label(S("Длительность"), 24, kit::MUTED));
	std::vector<String> mt;
	int msel = 0;
	for (size_t i = 0; i < GameData::minutes_options().size(); i++) {
		mt.push_back(I(GameData::minutes_options()[i]) + S(" мин"));
		if (GameData::minutes_options()[i] == g.settings.minutes) msel = (int)i;
	}
	OptionButton *mins = kit::option(mt, msel);
	grid->add_child(mins);
	v->add_child(grid);
	HBoxContainer *h = kit::hbox(14);
	Button *cancel = kit::button(S("Отмена"), kit::PANEL2, 26);
	cancel->set_h_size_flags(SIZE_EXPAND_FILL);
	cancel->connect("pressed", Fn(cancel, [v] { kit::close_modal(v); }));
	Button *go = kit::button(S("Начать матч"), kit::GREEN.darkened(0.2f), 28);
	go->set_h_size_flags(SIZE_EXPAND_FILL);
	go->connect("pressed", Fn(go, [pool, opp, diff, mins] {
		GameData &gd = GameData::get();
		gd.settings.difficulty = diff->get_selected();
		gd.settings.minutes = GameData::minutes_options()[mins->get_selected()];
		gd.save();
		static const int bonus[3] = { -4, 0, 4 };
		int idx = clampi(opp->get_selected(), 0, (int)pool.size() - 1);
		MatchTeam ai = gd.build_ai_team(pool[idx], bonus[gd.settings.difficulty]);
		Dictionary p;
		p["mode"] = "quick";
		p["home"] = gd.build_my_team().to_dict();
		p["away"] = ai.to_dict();
		p["away_logo"] = pool[idx].logo;
		p["user_home"] = true;
		p["minutes"] = gd.settings.minutes;
		p["difficulty"] = gd.settings.difficulty;
		p["seed"] = (int64_t)(rng().next() & 0x7fffffff);
		PMApp::go("match", p);
	}));
	h->add_child(cancel);
	h->add_child(go);
	v->add_child(h);
}

} // namespace pm
