// Editor ("edit mode" like classic football games): rename clubs, short names, kit colours and players.
// Everything stays on the phone; the player can name teams however they like.
#include "app.h"
#include "game_data.h"
#include "screens.h"
#include "sfx.h"
#include "ui_kit.h"

#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/v_box_container.hpp>

using namespace godot;

namespace pm {

static const char *PALETTE[16] = { "#ffffff", "#0f172a", "#1d4ed8", "#38bdf8", "#dc2626", "#7f1d1d", "#16a34a", "#065f46",
	"#facc15", "#f97316", "#7c3aed", "#db2777", "#6b7280", "#a16207", "#14b8a6", "#e5e7eb" };

void PMEditScreen::_bind_methods() {}

void PMEditScreen::on_back() {
	Dictionary p;
	p["tab"] = 4;
	PMApp::go("menu", p);
}

void PMEditScreen::_ready() {
	VBoxContainer *v = root();
	header(v, S("Редактор"), [this] { on_back(); });
	Label *info = kit::label(S("Назовите клубы и игроков как хотите — изменения хранятся только на вашем телефоне."), 17, kit::MUTED);
	info->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	v->add_child(info);
	HBoxContainer *h = kit::hbox(14);
	h->set_v_size_flags(SIZE_EXPAND_FILL);
	v->add_child(h);
	ScrollContainer *ls = memnew(ScrollContainer);
	ls->set_custom_minimum_size(Vector2(330, 0));
	ls->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	h->add_child(ls);
	list = kit::vbox(6);
	list->set_h_size_flags(SIZE_EXPAND_FILL);
	ls->add_child(list);
	ScrollContainer *fs = memnew(ScrollContainer);
	fs->set_h_size_flags(SIZE_EXPAND_FILL);
	fs->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	h->add_child(fs);
	form = kit::vbox(10);
	form->set_h_size_flags(SIZE_EXPAND_FILL);
	fs->add_child(form);
	build_list();
	build_form();
}

void PMEditScreen::build_list() {
	for (int i = 0; i < list->get_child_count(); i++) list->get_child(i)->queue_free();
	GameData &g = GameData::get();
	std::vector<TeamInfo> pool = g.base_team_pool();
	ids.clear();
	ids.push_back("__my");
	for (auto &t : pool) ids.push_back(t.id);
	for (int i = 0; i < (int)ids.size(); i++) {
		String name, shrt, logo;
		Color c;
		if (i == 0) {
			name = g.team_name;
			shrt = g.team_short;
			c = html(g.shirt);
		} else {
			const TeamInfo &t = pool[i - 1];
			name = t.name;
			shrt = t.shrt;
			c = html(t.shirt);
			logo = t.logo;
		}
		bool sel = i == current;
		Button *b = kit::button("", sel ? Color(0.16f, 0.24f, 0.45f) : Color(0.08f, 0.11f, 0.22f), 18, Vector2(0, 58));
		HBoxContainer *bh = kit::hbox(10);
		bh->set_mouse_filter(MOUSE_FILTER_IGNORE);
		b->add_child(bh);
		kit::full_rect(bh);
		bh->set_offset(SIDE_LEFT, 10);
		bh->add_child(kit::badge(logo, c, shrt, 38));
		Label *l = kit::label(name, 19, sel ? kit::LIME : kit::TEXT);
		l->set_mouse_filter(MOUSE_FILTER_IGNORE);
		bh->add_child(l);
		if (i > 0 && g.edits.has(ids[i])) bh->add_child(kit::label(S("  изм."), 14, kit::ACCENT));
		b->connect("pressed", Fn(b, [this, i] {
			current = i;
			build_list();
			build_form();
		}));
		list->add_child(b);
	}
}

static Control *swatches(String *target, std::function<void()> changed) {
	HBoxContainer *row = kit::hbox(6);
	for (int i = 0; i < 16; i++) {
		String col = PALETTE[i];
		bool sel = target->to_lower() == col;
		Button *b = memnew(Button);
		b->set_focus_mode(Control::FOCUS_NONE);
		b->set_custom_minimum_size(Vector2(38, 38));
		Ref<StyleBoxFlat> st = kit::sb(html(col), 19, sel ? 3 : 1, sel ? kit::LIME : Color(1, 1, 1, 0.3f));
		b->add_theme_stylebox_override("normal", st);
		b->add_theme_stylebox_override("hover", st);
		b->add_theme_stylebox_override("pressed", st);
		b->connect("pressed", Fn(b, [target, col, changed] {
			*target = col;
			changed();
		}));
		row->add_child(b);
	}
	return row;
}

static LineEdit *field(const String &text, int max_len, float min_w) {
	LineEdit *le = memnew(LineEdit);
	le->set_text(text);
	le->set_max_length(max_len);
	le->set_custom_minimum_size(Vector2(min_w, 50));
	le->add_theme_font_size_override("font_size", 21);
	le->set_select_all_on_focus(true);
	return le;
}

void PMEditScreen::build_form() {
	for (int i = 0; i < form->get_child_count(); i++) form->get_child(i)->queue_free();
	GameData &g = GameData::get();
	if (current < 0 || current >= (int)ids.size()) current = 0;
	bool mine = current == 0;
	TeamInfo t;
	if (!mine) {
		std::vector<TeamInfo> pool = g.base_team_pool();
		t = pool[current - 1];
		roster = g.team_roster(t);
	} else {
		roster.clear();
	}
	shirt = mine ? g.shirt : t.shirt;
	shorts = mine ? g.shorts : t.shorts;
	PanelContainer *p = kit::glass(16);
	VBoxContainer *pv = kit::vbox(10);
	p->add_child(pv);
	form->add_child(p);
	HBoxContainer *r1 = kit::hbox(10);
	r1->add_child(kit::label(S("Название"), 18, kit::MUTED));
	name_edit = field(mine ? g.team_name : t.name, 28, 300);
	name_edit->set_h_size_flags(SIZE_EXPAND_FILL);
	r1->add_child(name_edit);
	r1->add_child(kit::label(S("Сокр."), 18, kit::MUTED));
	short_edit = field(mine ? g.team_short : t.shrt, 4, 96);
	r1->add_child(short_edit);
	pv->add_child(r1);
	auto refresh = [this] {
		// keep typed text when the colours change
		String n = name_edit->get_text(), s = short_edit->get_text();
		std::vector<String> names, nums;
		for (size_t i = 0; i < player_edits.size(); i++) {
			names.push_back(player_edits[i]->get_text());
			nums.push_back(num_edits[i]->get_text());
		}
		String c1 = shirt, c2 = shorts;
		build_form();
		shirt = c1;
		shorts = c2;
		name_edit->set_text(n);
		short_edit->set_text(s);
		for (size_t i = 0; i < player_edits.size() && i < names.size(); i++) {
			player_edits[i]->set_text(names[i]);
			num_edits[i]->set_text(nums[i]);
		}
	};
	pv->add_child(kit::label(S("Цвет формы"), 17, kit::MUTED));
	pv->add_child(swatches(&shirt, [this, refresh] { pending_refresh = true; }));
	pv->add_child(kit::label(S("Цвет шорт"), 17, kit::MUTED));
	pv->add_child(swatches(&shorts, [this, refresh] { pending_refresh = true; }));
	refresh_fn = refresh;
	player_edits.clear();
	num_edits.clear();
	if (!mine) {
		PanelContainer *pp = kit::glass(14);
		VBoxContainer *ppv = kit::vbox(6);
		pp->add_child(ppv);
		ppv->add_child(kit::label(S("Игроки"), 20, kit::TEXT));
		for (size_t i = 0; i < roster.size(); i++) {
			const Card &c = roster[i];
			HBoxContainer *rh = kit::hbox(8);
			Label *pl = kit::label(GameData::pos_ru(c.pos), 16, kit::ACCENT);
			pl->set_custom_minimum_size(Vector2(40, 0));
			rh->add_child(pl);
			LineEdit *ne = field(c.name, 22, 240);
			ne->set_h_size_flags(SIZE_EXPAND_FILL);
			rh->add_child(ne);
			LineEdit *nu = field(I(c.num), 2, 64);
			nu->set_virtual_keyboard_type(LineEdit::KEYBOARD_TYPE_NUMBER);
			rh->add_child(nu);
			rh->add_child(kit::label(I(c.ovr), 18, kit::GOLD));
			ppv->add_child(rh);
			player_edits.push_back(ne);
			num_edits.push_back(nu);
		}
		form->add_child(pp);
	} else {
		form->add_child(kit::label(S("Имена своих игроков меняются в «Составе» или в «Коллекции»."), 16, kit::MUTED));
	}
	HBoxContainer *bh = kit::hbox(12);
	Button *save = kit::primary(S("Сохранить"));
	save->set_h_size_flags(SIZE_EXPAND_FILL);
	save->connect("pressed", Fn(save, [this] { save_current(); }));
	bh->add_child(save);
	if (!mine && g.edits.has(ids[current])) {
		Button *rs = kit::button(S("Сбросить"), kit::PANEL2, 22, Vector2(200, 66));
		rs->connect("pressed", Fn(rs, [this] {
			GameData::get().reset_team_edit(ids[current]);
			kit::toast(this, S("Изменения сброшены"));
			build_list();
			build_form();
		}));
		bh->add_child(rs);
	}
	form->add_child(bh);
}

void PMEditScreen::_process(double) {
	if (pending_refresh && refresh_fn) {
		pending_refresh = false;
		refresh_fn();
	}
}

void PMEditScreen::save_current() {
	GameData &g = GameData::get();
	String n = name_edit->get_text().strip_edges();
	String s = short_edit->get_text().strip_edges().to_upper();
	if (n.length() < 2) {
		kit::toast(this, S("Название слишком короткое"), kit::RED);
		return;
	}
	if (current == 0) {
		g.set_team_name(n);
		if (s.length() >= 2) g.team_short = s.left(4);
		g.shirt = shirt;
		g.shorts = shorts;
		g.save();
	} else {
		Dictionary e;
		e["name"] = n.left(28);
		e["shrt"] = (s.length() >= 2 ? s : n.left(3).to_upper()).left(4);
		e["shirt"] = shirt;
		e["shorts"] = shorts;
		Array ps;
		for (size_t i = 0; i < roster.size() && i < player_edits.size(); i++) {
			Card c = roster[i];
			String pn = player_edits[i]->get_text().strip_edges().left(22);
			if (pn.length() >= 2) c.name = pn;
			int num = num_edits[i]->get_text().to_int();
			if (num >= 1 && num <= 99) c.num = num;
			ps.push_back(c.to_dict());
		}
		e["players"] = ps;
		g.save_team_edit(ids[current], e);
	}
	PMSfx::p("click");
	kit::toast(this, S("Сохранено"), kit::LIME);
	build_list();
	build_form();
}

} // namespace pm
