// Squad & tactics screen and packs / collection screen.
#include "app.h"
#include "screens.h"
#include "sfx.h"

#include <godot_cpp/classes/aspect_ratio_container.hpp>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/classes/interval_tweener.hpp>
#include <godot_cpp/classes/callback_tweener.hpp>
#include <godot_cpp/classes/property_tweener.hpp>

#include <algorithm>
#include <map>

using namespace godot;

namespace pm {

// ================================================================== squad
void PMSquadScreen::_ready() {
	VBoxContainer *v = root();
	header(v, S("Состав и тактика"), [] { PMApp::go("menu"); });
	body = kit::hbox(20);
	body->set_v_size_flags(SIZE_EXPAND_FILL);
	v->add_child(body);
	build();
}

static void draw_pitch(Control *c) {
	Vector2 s = c->get_size();
	c->draw_rect(Rect2(Vector2(), s), Color::html("2f7a33"));
	for (int i = 0; i < 8; i += 2) c->draw_rect(Rect2(Vector2(0, s.y / 8 * i), Vector2(s.x, s.y / 8)), Color::html("37873a"));
	Color w(1, 1, 1, 0.7f);
	c->draw_rect(Rect2(Vector2(6, 6), s - Vector2(12, 12)), w, false, 2.f);
	float bw = s.x * 0.55f;
	c->draw_rect(Rect2(Vector2((s.x - bw) * 0.5f, s.y - 6 - s.y * 0.16f), Vector2(bw, s.y * 0.16f)), w, false, 2.f);
	c->draw_arc(Vector2(s.x * 0.5f, 8), s.x * 0.13f, 0, Math_PI, 24, w, 2.f);
}

void PMSquadScreen::build() {
	for (int i = 0; i < body->get_child_count(); i++) body->get_child(i)->queue_free();
	GameData &g = GameData::get();
	const auto &form = GameData::formation(g.squad.formation);

	AspectRatioContainer *box = memnew(AspectRatioContainer);
	box->set_ratio(0.82f);
	box->set_h_size_flags(SIZE_EXPAND_FILL);
	body->add_child(box);
	Control *pitch = memnew(Control);
	box->add_child(pitch);
	pitch->connect("draw", Fn(pitch, [pitch] { draw_pitch(pitch); }));
	for (int i = 0; i < 11; i++) pitch->add_child(slot_chip(i, form[i].pos));
	pitch->connect("resized", Fn(pitch, [this, pitch] { place_slots(pitch); }));

	VBoxContainer *right = kit::vbox(10);
	right->set_h_size_flags(SIZE_EXPAND_FILL);
	right->set_stretch_ratio(1.25f);
	body->add_child(right);
	GridContainer *tac = memnew(GridContainer);
	tac->set_columns(4);
	tac->add_theme_constant_override("h_separation", 10);
	tac->add_theme_constant_override("v_separation", 8);
	int fsel = 0;
	for (size_t i = 0; i < GameData::formation_names().size(); i++)
		if (GameData::formation_names()[i] == g.squad.formation) fsel = (int)i;
	tac->add_child(kit::label(S("Схема"), 20, kit::MUTED));
	OptionButton *fo = kit::option(GameData::formation_names(), fsel, 20);
	fo->connect("item_selected", FnV(fo, [this](const Variant &i) {
		GameData::get().squad.formation = GameData::formation_names()[(int)i];
		GameData::get().save();
		selected = -1;
		build();
	}));
	tac->add_child(fo);
	struct Opt { const char *name; int *val; std::vector<String> items; };
	std::vector<Opt> opts = {
		{ "Линия", &g.squad.line, { S("Низкая"), S("Средняя"), S("Высокая") } },
		{ "Прессинг", &g.squad.press, { S("Слабый"), S("Средний"), S("Сильный") } },
		{ "Стиль", &g.squad.style, { S("Оборона"), S("Баланс"), S("Атака") } },
	};
	for (auto &o : opts) {
		tac->add_child(kit::label(S(o.name), 20, kit::MUTED));
		OptionButton *ob = kit::option(o.items, *o.val, 20);
		int *val = o.val;
		ob->connect("item_selected", FnV(ob, [val](const Variant &i) {
			*val = (int)i;
			GameData::get().save();
		}));
		tac->add_child(ob);
	}
	for (int i = 0; i < tac->get_child_count(); i++) {
		Control *c = Object::cast_to<Control>(tac->get_child(i));
		if (Object::cast_to<OptionButton>(c)) c->set_h_size_flags(SIZE_EXPAND_FILL);
	}
	right->add_child(tac);
	HBoxContainer *h = kit::hbox(10);
	Label *ol = kit::label(S("Рейтинг команды: ") + I(g.team_ovr()), 26, kit::GOLD);
	ol->set_h_size_flags(SIZE_EXPAND_FILL);
	h->add_child(ol);
	Button *au = kit::button(S("Авто-состав"), kit::BLUE, 22, Vector2(200, 52));
	au->connect("pressed", Fn(au, [this] {
		GameData::get().auto_squad();
		selected = -1;
		build();
	}));
	h->add_child(au);
	right->add_child(h);
	String hint = S("Нажмите на игрока на поле, чтобы заменить его");
	String slot_pos;
	if (selected >= 0) {
		slot_pos = form[selected].pos;
		Card *c = g.card_by_id(g.squad.slots[selected]);
		hint = S("Замена: ") + GameData::pos_ru(slot_pos) + " (" + (c ? c->name : String()) + S(") — выберите игрока");
	}
	right->add_child(kit::label(hint, 20, kit::ACCENT));
	ScrollContainer *scroll = memnew(ScrollContainer);
	scroll->set_v_size_flags(SIZE_EXPAND_FILL);
	scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	right->add_child(scroll);
	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(4);
	grid->add_theme_constant_override("h_separation", 10);
	grid->add_theme_constant_override("v_separation", 10);
	scroll->add_child(grid);
	std::vector<Card> cards = g.cards;
	if (selected >= 0)
		std::sort(cards.begin(), cards.end(), [&](const Card &a, const Card &b) { return g.effective_ovr(a, slot_pos) > g.effective_ovr(b, slot_pos); });
	else
		std::sort(cards.begin(), cards.end(), [](const Card &a, const Card &b) { return a.ovr > b.ovr; });
	for (auto &c : cards) {
		PanelContainer *w = kit::card(c, Vector2(132, 184), slot_pos);
		if (g.is_starter(c.id)) w->set_modulate(Color(0.75f, 0.85f, 1.f, 0.85f));
		int cid = c.id;
		kit::clickable(w, [this, cid] {
			if (selected < 0) {
				kit::toast(this, S("Сначала выберите позицию на поле"), kit::ACCENT);
				return;
			}
			GameData::get().set_slot(selected, cid);
			selected = -1;
			build();
		});
		grid->add_child(w);
	}
}

Control *PMSquadScreen::slot_chip(int i, const String &pos) {
	GameData &g = GameData::get();
	Card *cp = g.card_by_id(g.squad.slots[i]);
	Card c = cp ? *cp : Card();
	int eff = g.effective_ovr(c, pos);
	Color col = GameData::rarity_color(GameData::rarity(c.ovr));
	PanelContainer *p = memnew(PanelContainer);
	p->set_custom_minimum_size(Vector2(108, 78));
	p->add_theme_stylebox_override("panel", kit::sb(Color(0.03f, 0.06f, 0.14f, 0.9f), 10, i == selected ? 4 : 2, i == selected ? kit::LIME : col, 6));
	VBoxContainer *v = kit::vbox(-4);
	v->set_mouse_filter(MOUSE_FILTER_IGNORE);
	HBoxContainer *top = kit::hbox(2);
	top->set_mouse_filter(MOUSE_FILTER_IGNORE);
	top->add_child(kit::label(I(eff), 26, eff < c.ovr - 1 ? Color::html("fca5a5") : Color(1, 1, 1)));
	top->add_child(kit::spacer());
	if (c.lvl > 0) top->add_child(kit::label(S("ур") + I(c.lvl), 13, kit::LIME));
	top->add_child(kit::label(GameData::pos_ru(pos), 18, col.lightened(0.3f)));
	v->add_child(top);
	PackedStringArray parts = c.name.split(" ");
	Label *n = kit::label(parts.size() ? parts[parts.size() - 1] : c.name, 17, kit::TEXT, HORIZONTAL_ALIGNMENT_CENTER);
	n->set_clip_text(true);
	n->set_custom_minimum_size(Vector2(92, 0));
	v->add_child(n);
	p->add_child(v);
	kit::clickable(p, [this, i] {
		selected = selected == i ? -1 : i;
		build();
	});
	return p;
}

void PMSquadScreen::place_slots(Control *pitch) {
	GameData &g = GameData::get();
	const auto &form = GameData::formation(g.squad.formation);
	static std::map<String, int> row_of = { { "GK", 0 }, { "CB", 1 }, { "LB", 1 }, { "RB", 1 }, { "DMF", 2 }, { "CMF", 3 }, { "LMF", 3 },
		{ "RMF", 3 }, { "AMF", 4 }, { "LWF", 5 }, { "RWF", 5 }, { "CF", 5 } };
	std::map<int, std::vector<int>> rows;
	for (int i = 0; i < 11; i++) rows[row_of[form[i].pos]].push_back(i);
	Vector2 s = pitch->get_size();
	int nrows = (int)rows.size(), ri = 0;
	for (auto &kv : rows) {
		auto idx = kv.second;
		std::sort(idx.begin(), idx.end(), [&](int a, int b) { return form[a].z < form[b].z; });
		float y = s.y - 50.f - (float)ri / std::max(1, nrows - 1) * (s.y - 100.f);
		for (size_t k = 0; k < idx.size(); k++) {
			int i = idx[k];
			float x = s.x * (k + 0.5f) / idx.size();
			if (idx.size() == 1) x = s.x * 0.5f + form[i].z * s.x * 0.35f;
			if (i < pitch->get_child_count()) {
				Control *chip = Object::cast_to<Control>(pitch->get_child(i));
				if (chip) chip->set_position(Vector2(x - 54.f, y - 39.f));
			}
		}
		ri++;
	}
}

// ================================================================== packs
void PMPacksScreen::_ready() {
	VBoxContainer *v = root();
	header(v, S("Паки и карточки"), [] { PMApp::go("menu"); });
	HBoxContainer *tabs = kit::hbox(12);
	Button *t1 = kit::button(S("Магазин паков"), kit::BLUE, 24, Vector2(260, 54));
	Button *t2 = kit::button(S("Коллекция"), kit::PANEL2, 24, Vector2(260, 54));
	t1->connect("pressed", Fn(t1, [this] { tab = 0; build(); }));
	t2->connect("pressed", Fn(t2, [this] { tab = 1; build(); }));
	tabs->add_child(t1);
	tabs->add_child(t2);
	v->add_child(tabs);
	content = kit::vbox(12);
	content->set_v_size_flags(SIZE_EXPAND_FILL);
	v->add_child(content);
	build();
}

void PMPacksScreen::build() {
	for (int i = 0; i < content->get_child_count(); i++) content->get_child(i)->queue_free();
	if (tab == 0) shop();
	else collection();
}

void PMPacksScreen::shop() {
	HBoxContainer *h = kit::hbox(20);
	h->set_v_size_flags(SIZE_EXPAND_FILL);
	content->add_child(h);
	Color colors[3] = { Color::html("1e40af"), Color::html("b45309"), Color::html("6d28d9") };
	for (size_t i = 0; i < GameData::packs().size(); i++) {
		const PackDef &pk = GameData::packs()[i];
		Color col = colors[i];
		PanelContainer *p = memnew(PanelContainer);
		p->set_h_size_flags(SIZE_EXPAND_FILL);
		p->add_theme_stylebox_override("panel", kit::sb(Color(col.darkened(0.4f), 0.92f), 8, 3, col.lightened(0.3f), 22, -0.05f));
		VBoxContainer *v = kit::vbox(12);
		p->add_child(v);
		v->add_child(kit::label(S(pk.name), 36, Color(1, 1, 1), HORIZONTAL_ALIGNMENT_CENTER));
		Control *art = memnew(Control);
		art->set_custom_minimum_size(Vector2(0, 170));
		art->connect("draw", Fn(art, [art, col] {
			float w = 120, hh = 160, x = art->get_size().x * 0.5f - w * 0.5f;
			for (int k = 0; k < 3; k++) {
				Vector2 off((k - 1) * 18.f, k * 2.f);
				art->draw_rect(Rect2(Vector2(x, 4) + off, Vector2(w, hh)), col.lightened(0.1f + k * 0.1f));
				art->draw_rect(Rect2(Vector2(x, 4) + off, Vector2(w, hh)), Color(1, 1, 1, 0.6f), false, 2.f);
			}
			Ref<Font> f = art->get_theme_default_font();
			art->draw_string(f, Vector2(x, 100), "PM", HORIZONTAL_ALIGNMENT_CENTER, w + 18, 56, Color(1, 1, 1, 0.9f));
		}));
		v->add_child(art);
		v->add_child(kit::label(S(pk.desc), 20, Color(1, 1, 1, 0.8f), HORIZONTAL_ALIGNMENT_CENTER));
		v->add_child(kit::spacer(true));
		Button *b = kit::button(S("Открыть • ") + kit::fmt_int(pk.price), kit::GOLD.darkened(0.35f), 26, Vector2(0, 66));
		b->set_disabled(GameData::get().coins < pk.price);
		String id = pk.id;
		b->connect("pressed", Fn(b, [this, id] { open(id); }));
		v->add_child(b);
		h->add_child(p);
	}
	content->add_child(kit::label(S("Монеты дают за матчи, победы в лиге и продажу лишних карточек."), 20, kit::MUTED, HORIZONTAL_ALIGNMENT_CENTER));
}

void PMPacksScreen::open(const String &id) {
	std::vector<Card> cards = GameData::get().open_pack(id);
	if (cards.empty()) {
		kit::toast(this, S("Не хватает монет"), kit::RED);
		return;
	}
	ColorRect *dim = memnew(ColorRect);
	dim->set_color(Color(0, 0, 0, 0.88f));
	dim->set_mouse_filter(MOUSE_FILTER_STOP);
	add_child(dim);
	kit::full_rect(dim);
	VBoxContainer *v = kit::vbox(26);
	v->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	dim->add_child(v);
	kit::full_rect(v);
	Label *title = kit::label(S("Открываем пак…"), 40, kit::GOLD, HORIZONTAL_ALIGNMENT_CENTER);
	v->add_child(title);
	HBoxContainer *row = kit::hbox(30);
	row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	v->add_child(row);
	std::vector<CenterContainer *> holders;
	for (size_t i = 0; i < cards.size(); i++) {
		CenterContainer *hold = memnew(CenterContainer);
		hold->set_custom_minimum_size(Vector2(200, 280));
		hold->set_pivot_offset(Vector2(100, 140));
		PanelContainer *back = memnew(PanelContainer);
		back->set_custom_minimum_size(Vector2(190, 262));
		back->add_theme_stylebox_override("panel", kit::sb(Color::html("0f1d40"), 12, 3, kit::ACCENT, 8));
		back->add_child(kit::label("?", 90, kit::ACCENT, HORIZONTAL_ALIGNMENT_CENTER));
		hold->add_child(back);
		row->add_child(hold);
		holders.push_back(hold);
	}
	Button *done = kit::button(S("Забрать"), kit::GREEN.darkened(0.2f), 28, Vector2(320, 66));
	done->set_visible(false);
	done->connect("pressed", Fn(done, [this, dim] {
		dim->queue_free();
		build();
	}));
	CenterContainer *dc = memnew(CenterContainer);
	dc->add_child(done);
	v->add_child(dc);
	int best = 0;
	for (auto &c : cards) best = std::max(best, c.ovr);
	Ref<Tween> tw = create_tween();
	for (size_t i = 0; i < cards.size(); i++) {
		CenterContainer *hold = holders[i];
		Card card = cards[i];
		tw->tween_interval(0.45);
		tw->tween_property(hold, "scale:x", 0.0, 0.18)->from(1.0);
		tw->tween_callback(Fn(hold, [hold, card] {
			for (int k = 0; k < hold->get_child_count(); k++) hold->get_child(k)->queue_free();
			hold->add_child(kit::card(card, Vector2(190, 262)));
			PMSfx::p(card.ovr >= 85 ? "legend" : "reveal", -4.f);
		}));
		tw->tween_property(hold, "scale:x", 1.0, 0.22);
	}
	tw->tween_callback(Fn(title, [title, done, best] {
		title->set_text(best >= 85 ? S("ЛЕГЕНДА!") : (best >= 75 ? S("Золото!") : S("Новые игроки")));
		title->add_theme_color_override("font_color", GameData::rarity_color(GameData::rarity(best)));
		done->set_visible(true);
	}));
}

void PMPacksScreen::collection() {
	GameData &g = GameData::get();
	content->add_child(kit::label(S("Всего карточек: ") + I((int)g.cards.size()) + S(". Нажмите на карточку, чтобы продать."), 20, kit::MUTED));
	ScrollContainer *scroll = memnew(ScrollContainer);
	scroll->set_v_size_flags(SIZE_EXPAND_FILL);
	scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	content->add_child(scroll);
	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(7);
	grid->add_theme_constant_override("h_separation", 12);
	grid->add_theme_constant_override("v_separation", 12);
	scroll->add_child(grid);
	std::vector<Card> cards = g.cards;
	std::sort(cards.begin(), cards.end(), [](const Card &a, const Card &b) { return a.ovr > b.ovr; });
	for (auto &c : cards) {
		PanelContainer *w = kit::card(c, Vector2(150, 206));
		int cid = c.id;
		kit::clickable(w, [this, cid] { card_dialog(cid); });
		grid->add_child(w);
	}
}

void PMPacksScreen::card_dialog(int cid) {
	GameData &g = GameData::get();
	Card *cp = g.card_by_id(cid);
	if (!cp) return;
	Card c = *cp;
	VBoxContainer *v = kit::modal(this, 600);
	HBoxContainer *h = kit::hbox(20);
	h->add_child(kit::card(c, Vector2(190, 262)));
	VBoxContainer *info = kit::vbox(4);
	info->add_child(kit::label(c.name, 28, kit::TEXT));
	info->add_child(kit::label(GameData::pos_ru(c.pos) + S(" • Рейтинг ") + I(c.ovr) + S(" • №") + I(c.num), 22, kit::MUTED));
	{
		int need = 100 + c.lvl * 60;
		HBoxContainer *lh = kit::hbox(10);
		lh->add_child(kit::label(S("Уровень ") + I(c.lvl) + "/10", 20, kit::LIME));
		Control *pg = kit::progress(c.lvl >= 10 ? 1.f : (float)c.xp / need, kit::LIME, 8);
		pg->set_h_size_flags(SIZE_EXPAND_FILL);
		pg->set_v_size_flags(SIZE_SHRINK_CENTER);
		lh->add_child(pg);
		lh->add_child(kit::label(c.lvl >= 10 ? S("МАКС") : I(c.xp) + "/" + I(need), 16, kit::MUTED));
		info->add_child(lh);
	}
	for (const char *k : { "pace", "sho", "pas", "dri", "def", "phy", "gk" })
		info->add_child(kit::label(GameData::stat_ru(k) + ": " + I(c.stat(k)), 20, kit::TEXT));
	h->add_child(info);
	v->add_child(h);
	HBoxContainer *row = kit::hbox(12);
	Button *close = kit::button(S("Закрыть"), kit::PANEL2, 24);
	close->set_h_size_flags(SIZE_EXPAND_FILL);
	close->connect("pressed", Fn(close, [v] { kit::close_modal(v); }));
	row->add_child(close);
	bool starter = g.is_starter(cid);
	Button *sell = kit::button(starter ? S("В основе") : S("Продать за ") + kit::fmt_int(GameData::sell_value(c)), kit::RED.darkened(0.3f), 24);
	sell->set_h_size_flags(SIZE_EXPAND_FILL);
	sell->set_disabled(starter || g.cards.size() <= 12);
	sell->connect("pressed", Fn(sell, [this, v, cid] {
		int got = GameData::get().sell_card(cid);
		kit::close_modal(v);
		kit::toast(this, "+" + kit::fmt_int(got) + S(" монет"));
		build();
	}));
	row->add_child(sell);
	v->add_child(row);
}

} // namespace pm
