// League, PvP lobby, settings, patches and result screens.
#include "app.h"
#include "net.h"
#include "screens.h"
#include "sfx.h"

#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <algorithm>

using namespace godot;

namespace pm {

// ================================================================== league
void PMLeagueScreen::_ready() {
	VBoxContainer *v = root();
	GameData &g = GameData::get();
	header(v, g.league.name + S(" • Сезон ") + I(g.league.season), [] { PMApp::go("menu"); });
	body = kit::hbox(20);
	body->set_v_size_flags(SIZE_EXPAND_FILL);
	v->add_child(body);
	build();
}

void PMLeagueScreen::build() {
	for (int i = 0; i < body->get_child_count(); i++) body->get_child(i)->queue_free();
	GameData &g = GameData::get();
	League &lg = g.league;
	PanelContainer *tp = kit::panel(kit::PANEL, 8, 16);
	tp->set_h_size_flags(SIZE_EXPAND_FILL);
	tp->set_stretch_ratio(1.45f);
	body->add_child(tp);
	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(10);
	grid->add_theme_constant_override("h_separation", 14);
	grid->add_theme_constant_override("v_separation", 6);
	tp->add_child(grid);
	for (const char *hname : { "#", "", "Команда", "И", "В", "Н", "П", "Мячи", "РМ", "О" }) grid->add_child(kit::label(S(hname), 20, kit::MUTED));
	auto order = g.league_standings();
	for (size_t pos = 0; pos < order.size(); pos++) {
		int i = order[pos];
		TeamInfo &t = lg.teams[i];
		TableRow &r = lg.table[i];
		Color col = t.user ? kit::LIME : kit::TEXT;
		grid->add_child(kit::label(I((int)pos + 1), 22, col));
		String logo = t.user && g.patch.active && g.patch.has_my_team ? g.patch.my_team.logo : t.logo;
		grid->add_child(kit::badge(logo, html(t.shirt), t.shrt, 30));
		Label *nm = kit::label(t.name, 22, col);
		nm->set_h_size_flags(SIZE_EXPAND_FILL);
		grid->add_child(nm);
		for (int v : { r.p, r.w, r.d, r.l }) grid->add_child(kit::label(I(v), 22, col));
		grid->add_child(kit::label(I(r.gf) + ":" + I(r.ga), 22, col));
		int gd = r.gf - r.ga;
		grid->add_child(kit::label((gd > 0 ? "+" : "") + I(gd), 22, col));
		grid->add_child(kit::label(I(r.pts), 24, col));
	}
	VBoxContainer *right = kit::vbox(14);
	right->set_h_size_flags(SIZE_EXPAND_FILL);
	body->add_child(right);
	if (g.league_finished()) {
		int place = g.league_user_place();
		PanelContainer *p = kit::panel(Color(0.23f, 0.12f, 0.42f, 0.92f), 8, 22);
		VBoxContainer *pv = kit::vbox(12);
		p->add_child(pv);
		pv->add_child(kit::label(S("Сезон завершён!"), 34, kit::GOLD, HORIZONTAL_ALIGNMENT_CENTER));
		pv->add_child(kit::label(S("Ваше место: ") + I(place), 30, kit::TEXT, HORIZONTAL_ALIGNMENT_CENTER));
		pv->add_child(kit::label(S("Награда: ") + kit::fmt_int(GameData::league_reward(place)) + S(" монет"), 26, kit::GOLD, HORIZONTAL_ALIGNMENT_CENTER));
		Button *b = kit::button(S("Забрать и начать новый сезон"), kit::GREEN.darkened(0.2f), 24, Vector2(0, 66));
		b->connect("pressed", Fn(b, [] {
			GameData::get().league_next_season();
			PMApp::go("league");
		}));
		pv->add_child(b);
		right->add_child(p);
	} else {
		auto m = g.league_user_fixture();
		TeamInfo &home = lg.teams[m[0]];
		TeamInfo &away = lg.teams[m[1]];
		PanelContainer *p2 = kit::panel(Color(0.08f, 0.15f, 0.35f, 0.92f), 8, 22);
		VBoxContainer *pv = kit::vbox(10);
		p2->add_child(pv);
		pv->add_child(kit::label(S("Тур ") + I(lg.round + 1) + S(" из ") + I((int)lg.fixtures.size()), 24, kit::ACCENT, HORIZONTAL_ALIGNMENT_CENTER));
		HBoxContainer *vs = kit::hbox(16);
		vs->set_alignment(BoxContainer::ALIGNMENT_CENTER);
		auto logo_of = [&](TeamInfo &t) { return t.user && g.patch.active && g.patch.has_my_team ? g.patch.my_team.logo : t.logo; };
		vs->add_child(kit::badge(logo_of(home), html(home.shirt), home.shrt, 72));
		vs->add_child(kit::label("VS", 34, kit::LIME));
		vs->add_child(kit::badge(logo_of(away), html(away.shirt), away.shrt, 72));
		pv->add_child(vs);
		pv->add_child(kit::label(home.name + S("  —  ") + away.name, 26, kit::TEXT, HORIZONTAL_ALIGNMENT_CENTER));
		TeamInfo &opp = home.user ? away : home;
		pv->add_child(kit::label(S("Рейтинг соперника: ") + I(opp.strength) + S(" • Ваш: ") + I(g.team_ovr()), 20, kit::MUTED, HORIZONTAL_ALIGNMENT_CENTER));
		Button *play = kit::button(S("Играть матч"), kit::GREEN.darkened(0.2f), 28, Vector2(0, 70));
		play->connect("pressed", Fn(play, [] {
			GameData &gd = GameData::get();
			bool user_home = true;
			MatchTeam opp_t = gd.league_opponent_team(user_home);
			MatchTeam me = gd.build_my_team();
			if (gd.patch.active && gd.patch.has_my_team) me.logo = gd.patch.my_team.logo;
			Dictionary p;
			p["mode"] = "league";
			p["home"] = user_home ? me.to_dict() : opp_t.to_dict();
			p["away"] = user_home ? opp_t.to_dict() : me.to_dict();
			p["user_home"] = user_home;
			p["minutes"] = gd.settings.minutes;
			p["difficulty"] = gd.settings.difficulty;
			p["seed"] = (int64_t)(rng().next() & 0x7fffffff);
			PMApp::go("match", p);
		}));
		pv->add_child(play);
		Button *simb = kit::button(S("Симулировать (без игры)"), kit::PANEL2, 22, Vector2(0, 56));
		simb->connect("pressed", Fn(simb, [this] { simulate(); }));
		pv->add_child(simb);
		right->add_child(p2);
	}
	if (!lg.last.empty()) {
		PanelContainer *lp = kit::panel(kit::PANEL, 8, 16);
		VBoxContainer *lv = kit::vbox(2);
		lp->add_child(lv);
		lv->add_child(kit::label(S("Результаты прошлого тура"), 22, kit::MUTED));
		for (auto &r : lg.last) {
			TeamInfo &a = lg.teams[r[0]];
			TeamInfo &b = lg.teams[r[1]];
			lv->add_child(kit::label(a.shrt + " " + I(r[2]) + " : " + I(r[3]) + " " + b.shrt, 22, (a.user || b.user) ? kit::LIME : kit::TEXT));
		}
		right->add_child(lp);
	}
}

void PMLeagueScreen::simulate() {
	GameData &g = GameData::get();
	auto m = g.league_user_fixture();
	int opp = m[0] == g.league.user ? m[1] : m[0];
	static const int bonus[3] = { -4, 0, 4 };
	auto s = g.sim_score(g.team_ovr(), g.league.teams[opp].strength + bonus[g.settings.difficulty]);
	g.league_record_user(s[0], s[1]);
	int coins = g.match_reward(s[0], s[1], "league") / 2;
	g.add_coins(coins);
	kit::toast(this, S("Счёт ") + I(s[0]) + ":" + I(s[1]) + S(" • +") + I(coins) + S(" монет"));
	build();
}

// ================================================================== PvP lobby
void PMPvpScreen::_ready() {
	VBoxContainer *v = root();
	header(v, S("PvP по Wi-Fi"), [this] { leave(); });
	body = kit::vbox(16);
	body->set_v_size_flags(SIZE_EXPAND_FILL);
	v->add_child(body);
	PMNet *net = PMNet::singleton;
	net->connect("status_changed", callable_mp(this, &PMPvpScreen::_on_status));
	net->connect("lobby_changed", callable_mp(this, &PMPvpScreen::_on_lobby));
	if (net->active) mode = net->is_host ? "host" : "join";
	build();
}

void PMPvpScreen::on_back() { leave(); }

void PMPvpScreen::leave() {
	PMNet::singleton->close();
	PMApp::go("menu");
}

void PMPvpScreen::_on_status(const String &t) {
	if (status) status->set_text(t);
}

void PMPvpScreen::_on_lobby() { build(); }

void PMPvpScreen::build() {
	for (int i = 0; i < body->get_child_count(); i++) body->get_child(i)->queue_free();
	status = nullptr;
	PMNet *net = PMNet::singleton;
	if (!net->active && mode == "host") mode = "";
	Label *info = kit::label(S("Оба телефона должны быть в одной Wi-Fi сети (или включите точку доступа на одном и подключите второй)."), 22, kit::MUTED);
	info->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	body->add_child(info);
	if (mode.is_empty()) {
		HBoxContainer *h = kit::hbox(20);
		h->set_v_size_flags(SIZE_EXPAND_FILL);
		body->add_child(h);
		Button *a = kit::button(S("Создать матч\nВы — хост. Сообщите свой IP сопернику"), Color::html("b91c1c"), 30);
		a->set_h_size_flags(SIZE_EXPAND_FILL);
		a->set_v_size_flags(SIZE_EXPAND_FILL);
		a->connect("pressed", Fn(a, [this] {
			if (PMNet::singleton->host() == OK) mode = "host";
			build();
		}));
		h->add_child(a);
		Button *b = kit::button(S("Подключиться\nВведите IP хоста"), Color::html("1d4ed8"), 30);
		b->set_h_size_flags(SIZE_EXPAND_FILL);
		b->set_v_size_flags(SIZE_EXPAND_FILL);
		b->connect("pressed", Fn(b, [this] {
			mode = "join";
			build();
		}));
		h->add_child(b);
		return;
	}
	PanelContainer *p = kit::panel(kit::PANEL, 8, 22);
	VBoxContainer *v = kit::vbox(12);
	p->add_child(v);
	body->add_child(p);
	if (mode == "host") {
		PackedStringArray ips = net->local_ips();
		String ip_txt = ips.size() > 0 ? String(", ").join(ips) : S("не найден (включите Wi-Fi или точку доступа)");
		v->add_child(kit::label(S("Ваш IP: ") + ip_txt, 34, kit::GOLD));
		status = kit::label(net->connected ? S("Соперник подключён") : S("Ожидание соперника…"), 26, kit::ACCENT);
		v->add_child(status);
		if (net->has_remote)
			v->add_child(kit::label(S("Соперник: ") + net->remote_team.name + S(" (рейтинг ") + I(net->remote_team.ovr) + S(", схема ") + net->remote_team.formation + ")", 26, kit::TEXT));
		HBoxContainer *h = kit::hbox(14);
		h->add_child(kit::label(S("Длительность матча"), 24, kit::MUTED));
		std::vector<String> mt;
		for (int m : GameData::minutes_options()) mt.push_back(I(m) + S(" мин"));
		OptionButton *mo = kit::option(mt, minutes_sel);
		mo->connect("item_selected", FnV(mo, [this](const Variant &i) { minutes_sel = (int)i; }));
		h->add_child(mo);
		v->add_child(h);
		Button *start = kit::button(S("Начать матч"), kit::GREEN.darkened(0.2f), 30, Vector2(0, 74));
		start->set_disabled(!net->connected || !net->has_remote);
		start->connect("pressed", Fn(start, [this] { PMNet::singleton->start_match(GameData::minutes_options()[minutes_sel]); }));
		v->add_child(start);
		Button *cancel = kit::button(S("Отменить"), kit::PANEL2, 22);
		cancel->connect("pressed", Fn(cancel, [this] {
			PMNet::singleton->close();
			mode = "";
			build();
		}));
		v->add_child(cancel);
		v->add_child(kit::label(S("В матче у каждого есть кнопка «АВТО» — ИИ играет за вашу команду, пока она включена."), 20, kit::MUTED));
		return;
	}
	if (net->active && net->connected) {
		v->add_child(kit::label(S("Подключено!"), 34, kit::GREEN));
		if (net->has_remote) v->add_child(kit::label(S("Хост: ") + net->remote_team.name + S(" (рейтинг ") + I(net->remote_team.ovr) + ")", 26, kit::TEXT));
		status = kit::label(S("Ждём, пока хост начнёт матч…"), 26, kit::ACCENT);
		v->add_child(status);
		Button *leave_b = kit::button(S("Отключиться"), kit::PANEL2, 22);
		leave_b->connect("pressed", Fn(leave_b, [this] {
			PMNet::singleton->close();
			mode = "";
			build();
		}));
		v->add_child(leave_b);
		return;
	}
	v->add_child(kit::label(S("IP-адрес хоста"), 26, kit::TEXT));
	LineEdit *ip = memnew(LineEdit);
	ip->set_text(GameData::get().settings.last_ip);
	ip->set_custom_minimum_size(Vector2(0, 64));
	ip->add_theme_font_size_override("font_size", 32);
	ip->set_virtual_keyboard_type(LineEdit::KEYBOARD_TYPE_NUMBER_DECIMAL);
	ip->add_theme_stylebox_override("normal", kit::sb(kit::PANEL2, 10));
	v->add_child(ip);
	status = kit::label(net->active ? S("Подключение…") : net->last_status, 24, kit::ACCENT);
	v->add_child(status);
	HBoxContainer *h = kit::hbox(14);
	Button *back = kit::button(S("Назад"), kit::PANEL2, 24);
	back->set_h_size_flags(SIZE_EXPAND_FILL);
	back->connect("pressed", Fn(back, [this] {
		PMNet::singleton->close();
		mode = "";
		build();
	}));
	Button *go = kit::button(S("Подключиться"), kit::BLUE, 26);
	go->set_h_size_flags(SIZE_EXPAND_FILL);
	go->connect("pressed", Fn(go, [ip] {
		GameData::get().settings.last_ip = ip->get_text().strip_edges();
		GameData::get().save();
		PMNet::singleton->join(ip->get_text());
	}));
	h->add_child(back);
	h->add_child(go);
	v->add_child(h);
}

// ================================================================== settings
void PMSettingsScreen::_ready() {
	VBoxContainer *v = root();
	header(v, S("Настройки"), [] { PMApp::go("menu"); });
	GameData &g = GameData::get();
	PanelContainer *p = kit::panel(kit::PANEL, 8, 24);
	p->set_v_size_flags(SIZE_EXPAND_FILL);
	v->add_child(p);
	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(2);
	grid->add_theme_constant_override("h_separation", 30);
	grid->add_theme_constant_override("v_separation", 14);
	p->add_child(grid);
	auto row = [&](const char *name, Control *c) {
		grid->add_child(kit::label(S(name), 26, kit::TEXT));
		grid->add_child(c);
	};
	OptionButton *d = kit::option({ S("Лёгкий"), S("Средний"), S("Сложный") }, g.settings.difficulty, 24);
	d->set_custom_minimum_size(Vector2(380, 54));
	d->connect("item_selected", FnV(d, [](const Variant &i) { GameData::get().settings.difficulty = (int)i; GameData::get().save(); }));
	row("Сложность ИИ", d);
	std::vector<String> mt;
	int msel = 0;
	for (size_t i = 0; i < GameData::minutes_options().size(); i++) {
		mt.push_back(I(GameData::minutes_options()[i]) + S(" мин"));
		if (GameData::minutes_options()[i] == g.settings.minutes) msel = (int)i;
	}
	OptionButton *mo = kit::option(mt, msel, 24);
	mo->connect("item_selected", FnV(mo, [](const Variant &i) { GameData::get().settings.minutes = GameData::minutes_options()[(int)i]; GameData::get().save(); }));
	row("Длительность матча", mo);
	OptionButton *co = kit::option({ S("Широкая"), S("Близкая") }, g.settings.camera, 24);
	co->connect("item_selected", FnV(co, [](const Variant &i) { GameData::get().settings.camera = (int)i; GameData::get().save(); }));
	row("Камера", co);
	OptionButton *so = kit::option({ S("Выкл"), S("Вкл") }, g.settings.sound ? 1 : 0, 24);
	so->connect("item_selected", FnV(so, [](const Variant &i) { GameData::get().settings.sound = (int)i == 1; GameData::get().save(); }));
	row("Звук", so);
	OptionButton *ro = kit::option({ S("Выкл"), S("Вкл") }, g.settings.replays ? 1 : 0, 24);
	ro->connect("item_selected", FnV(ro, [](const Variant &i) { GameData::get().settings.replays = (int)i == 1; GameData::get().save(); }));
	row("Повтор гола", ro);
	OptionButton *ctl = kit::option({ S("Кнопки"), S("Кнопки + жесты"), S("Жесты (пальцем)") }, g.settings.controls, 24);
	ctl->connect("item_selected", FnV(ctl, [](const Variant &i) { GameData::get().settings.controls = (int)i; GameData::get().save(); }));
	row("Управление", ctl);
	OptionButton *qo = kit::option({ S("Авто"), S("Низкая"), S("Средняя"), S("Высокая"), S("Ультра") }, g.settings.quality + 1, 24);
	qo->connect("item_selected", FnV(qo, [](const Variant &i) {
		GameData::get().settings.quality = (int)i - 1;
		GameData::get().save();
		PMApp::apply_quality();
	}));
	row("Графика", qo);
	OptionButton *fo = kit::option({ S("30 кадров"), S("60 кадров") }, g.settings.fps == 30 ? 0 : 1, 24);
	fo->connect("item_selected", FnV(fo, [](const Variant &i) {
		GameData::get().settings.fps = (int)i == 0 ? 30 : 60;
		GameData::get().save();
		PMApp::apply_quality();
	}));
	row("Частота", fo);
	HBoxContainer *nh = kit::hbox(10);
	LineEdit *le = memnew(LineEdit);
	le->set_text(g.team_name);
	le->set_max_length(20);
	le->set_custom_minimum_size(Vector2(260, 54));
	le->add_theme_font_size_override("font_size", 24);
	le->add_theme_stylebox_override("normal", kit::sb(kit::PANEL2, 10));
	nh->add_child(le);
	Button *save = kit::button(S("Сохранить"), kit::BLUE, 22, Vector2(150, 54));
	save->connect("pressed", Fn(save, [this, le] {
		GameData::get().set_team_name(le->get_text());
		kit::toast(this, S("Сохранено"));
	}));
	nh->add_child(save);
	row("Название клуба", nh);
	// ---- patches right inside settings
	HBoxContainer *ph = kit::hbox(10);
	Button *add = kit::button(S("Добавить файл патча"), Color::html("0891b2"), 22, Vector2(270, 54));
	add->connect("pressed", Fn(add, [] {
		Dictionary p;
		p["import"] = true;
		PMApp::go("patches", p);
	}));
	ph->add_child(add);
	Button *manage = kit::button(S("Управление патчами"), kit::PANEL2, 22, Vector2(250, 54));
	manage->connect("pressed", Fn(manage, [] { PMApp::go("patches"); }));
	ph->add_child(manage);
	row(g.patch.active ? "Патч (активен)" : "Патч", ph);
	Button *reset = kit::button(S("Начать заново"), kit::RED.darkened(0.3f), 22, Vector2(260, 54));
	reset->connect("pressed", Fn(reset, [this] {
		VBoxContainer *m = kit::modal(this, 600);
		m->add_child(kit::label(S("Удалить весь прогресс?"), 32, kit::TEXT, HORIZONTAL_ALIGNMENT_CENTER));
		m->add_child(kit::label(S("Карточки, монеты и лига будут сброшены. Патчи останутся."), 22, kit::MUTED, HORIZONTAL_ALIGNMENT_CENTER));
		HBoxContainer *h = kit::hbox(14);
		Button *no = kit::button(S("Отмена"), kit::PANEL2, 24);
		no->set_h_size_flags(SIZE_EXPAND_FILL);
		no->connect("pressed", Fn(no, [m] { kit::close_modal(m); }));
		Button *yes = kit::button(S("Сбросить"), kit::RED, 24);
		yes->set_h_size_flags(SIZE_EXPAND_FILL);
		yes->connect("pressed", Fn(yes, [] {
			GameData::get().reset_progress();
			PMApp::go("menu");
		}));
		h->add_child(no);
		h->add_child(yes);
		m->add_child(h);
	}));
	row("Прогресс", reset);
	Label *help = kit::label(S("Управление: левый джойстик — бег. ПАС, РАЗРЕЗ (пас на ход), НАВЕС, УДАР (держите для силы), РЫВОК. В защите: СМЕНА, ПРЕССИНГ (держать), ОТБОР. «АВТО» — автоигра."), 20, kit::MUTED);
	help->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	v->add_child(help);
}

// ================================================================== patches
void PMPatchScreen::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_file", "path"), &PMPatchScreen::_on_file);
	ClassDB::bind_method(D_METHOD("install_file", "path"), &PMPatchScreen::_on_file);
}

void PMPatchScreen::on_back() { PMApp::go("menu"); }

void PMPatchScreen::_ready() {
	VBoxContainer *v = root();
	header(v, S("Патчи"), [] { PMApp::go("menu"); });
	HBoxContainer *actions = kit::hbox(12);
	Button *add = kit::button(S("+ Добавить файл патча"), Color::html("0891b2"), 24, Vector2(320, 62));
	add->connect("pressed", Fn(add, [this] { import_dialog(); }));
	actions->add_child(add);
	Button *ex = kit::button(S("Установить пример «Сезон 2027»"), kit::PANEL2, 22, Vector2(0, 62));
	ex->connect("pressed", Fn(ex, [this] {
		String id;
		String err = GameData::get().install_example_patch(id);
		if (!err.is_empty()) kit::toast(this, err, kit::RED);
		else {
			kit::toast(this, S("Пример установлен"), kit::LIME);
			build_list();
		}
	}));
	actions->add_child(ex);
	Button *tpl = kit::button(S("Шаблон для своего патча"), kit::PANEL2, 22, Vector2(0, 62));
	tpl->connect("pressed", Fn(tpl, [this] {
		String path;
		String err = GameData::get().export_template(path);
		VBoxContainer *m = kit::modal(this, 820);
		m->add_child(kit::label(err.is_empty() ? S("Шаблон сохранён") : err, 30, err.is_empty() ? kit::LIME : kit::RED, HORIZONTAL_ALIGNMENT_CENTER));
		Label *l = kit::label(path + S("\n\nОткройте файл, впишите свои клубы, цвета, игроков и тексты щитов. Логотипы — PNG в папке logos. "
										"Сложите patch.json и logos в zip, переименуйте в .pmpatch и добавьте через «Добавить файл патча»."), 22, kit::TEXT);
		l->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		l->set_custom_minimum_size(Vector2(760, 0));
		m->add_child(l);
		Button *ok = kit::button("OK", kit::BLUE, 24);
		ok->connect("pressed", Fn(ok, [m] { kit::close_modal(m); }));
		m->add_child(ok);
	}));
	actions->add_child(tpl);
	v->add_child(actions);
	Label *hint = kit::label(S("Патч меняет названия и цвета клубов, эмблемы, составы, силу команд, тексты на щитах, название лиги и фон меню. Форматы: .pmpatch, .zip, .json"), 20, kit::MUTED);
	hint->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	v->add_child(hint);
	ScrollContainer *sc = memnew(ScrollContainer);
	sc->set_v_size_flags(SIZE_EXPAND_FILL);
	sc->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	v->add_child(sc);
	list = kit::vbox(10);
	list->set_h_size_flags(SIZE_EXPAND_FILL);
	sc->add_child(list);
	build_list();
	if ((bool)params.get("import", false)) call_deferred("_on_file", String()); // opens dialog below
}

void PMPatchScreen::import_dialog() {
	FileDialog *fd = memnew(FileDialog);
	fd->set_file_mode(FileDialog::FILE_MODE_OPEN_FILE);
	fd->set_access(FileDialog::ACCESS_FILESYSTEM);
	PackedStringArray filters;
	filters.push_back(S("*.pmpatch, *.zip, *.json ; Патчи Pitch Masters"));
	fd->set_filters(filters);
	fd->set_use_native_dialog(true);
	fd->set_title(S("Выберите файл патча"));
	String dl = OS::get_singleton()->get_system_dir(OS::SYSTEM_DIR_DOWNLOADS);
	if (!dl.is_empty()) fd->set_current_dir(dl);
	fd->connect("file_selected", callable_mp(this, &PMPatchScreen::_on_file));
	add_child(fd);
	fd->popup_centered_ratio(0.85f);
}

void PMPatchScreen::_on_file(const String &path) {
	if (path.is_empty()) {
		import_dialog();
		return;
	}
	String id;
	String err = GameData::get().install_patch(path, id);
	if (!err.is_empty()) {
		kit::toast(this, err, kit::RED);
		return;
	}
	build_list();
	String name = id;
	for (auto &p : GameData::get().list_patches())
		if (p.id == id) name = p.name;
	confirm_activate(id, name);
}

void PMPatchScreen::confirm_activate(const String &id, const String &name) {
	VBoxContainer *m = kit::modal(this, 680);
	m->add_child(kit::label(S("Включить патч «") + name + S("»?"), 30, kit::TEXT, HORIZONTAL_ALIGNMENT_CENTER));
	m->add_child(kit::label(S("Лига начнётся заново с клубами из патча. Карточки и монеты останутся."), 22, kit::MUTED, HORIZONTAL_ALIGNMENT_CENTER));
	HBoxContainer *h = kit::hbox(14);
	Button *no = kit::button(S("Позже"), kit::PANEL2, 24);
	no->set_h_size_flags(SIZE_EXPAND_FILL);
	no->connect("pressed", Fn(no, [m] { kit::close_modal(m); }));
	Button *yes = kit::button(S("Включить"), kit::GREEN.darkened(0.2f), 24);
	yes->set_h_size_flags(SIZE_EXPAND_FILL);
	yes->connect("pressed", Fn(yes, [this, m, id] {
		String err = GameData::get().activate_patch(id);
		kit::close_modal(m);
		PMApp::singleton->rebuild_backdrop();
		if (!err.is_empty()) kit::toast(this, err, kit::RED);
		else kit::toast(this, S("Патч включён"), kit::LIME);
		build_list();
	}));
	h->add_child(no);
	h->add_child(yes);
	m->add_child(h);
}

void PMPatchScreen::build_list() {
	for (int i = 0; i < list->get_child_count(); i++) list->get_child(i)->queue_free();
	GameData &g = GameData::get();
	// default (no patch) entry
	{
		PanelContainer *p = kit::panel(g.patch.active ? kit::PANEL : Color(0.1f, 0.25f, 0.12f, 0.92f), 8, 16);
		HBoxContainer *h = kit::hbox(14);
		p->add_child(h);
		VBoxContainer *v = kit::vbox(2);
		v->set_h_size_flags(SIZE_EXPAND_FILL);
		v->add_child(kit::label(S("Без патча (стандартные клубы)"), 26, g.patch.active ? kit::TEXT : kit::LIME));
		v->add_child(kit::label(S("10 выдуманных клубов Pitch Masters"), 20, kit::MUTED));
		h->add_child(v);
		if (g.patch.active) {
			Button *b = kit::button(S("Включить"), kit::BLUE, 22, Vector2(170, 52));
			b->connect("pressed", Fn(b, [this] {
				GameData::get().activate_patch("");
				PMApp::singleton->rebuild_backdrop();
				build_list();
			}));
			h->add_child(b);
		} else {
			h->add_child(kit::label(S("АКТИВЕН"), 22, kit::LIME));
		}
		list->add_child(p);
	}
	auto patches = g.list_patches();
	if (patches.empty()) list->add_child(kit::label(S("Установленных патчей пока нет."), 22, kit::MUTED));
	for (auto &pi : patches) {
		bool active = g.settings.active_patch == pi.id;
		PanelContainer *p = kit::panel(active ? Color(0.1f, 0.25f, 0.12f, 0.92f) : kit::PANEL, 8, 16);
		HBoxContainer *h = kit::hbox(14);
		p->add_child(h);
		VBoxContainer *v = kit::vbox(2);
		v->set_h_size_flags(SIZE_EXPAND_FILL);
		v->add_child(kit::label(pi.name + (pi.version.is_empty() ? String() : "  v" + pi.version), 26, active ? kit::LIME : kit::TEXT));
		String meta = (pi.author.is_empty() ? String() : S("Автор: ") + pi.author + S("  •  ")) + S("Клубов: ") + I(pi.teams) + (pi.has_logos ? S("  •  с эмблемами") : String());
		v->add_child(kit::label(meta, 20, kit::MUTED));
		if (!pi.description.is_empty()) {
			Label *d = kit::label(pi.description, 18, kit::MUTED);
			d->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
			v->add_child(d);
		}
		h->add_child(v);
		String id = pi.id, name = pi.name;
		if (active) {
			h->add_child(kit::label(S("АКТИВЕН"), 22, kit::LIME));
		} else {
			Button *b = kit::button(S("Включить"), kit::BLUE, 22, Vector2(170, 52));
			b->connect("pressed", Fn(b, [this, id, name] { confirm_activate(id, name); }));
			h->add_child(b);
		}
		Button *del = kit::button(S("Удалить"), kit::RED.darkened(0.35f), 22, Vector2(150, 52));
		del->connect("pressed", Fn(del, [this, id] {
			GameData::get().remove_patch(id);
			PMApp::singleton->rebuild_backdrop();
			build_list();
		}));
		h->add_child(del);
		list->add_child(p);
	}
}

// ================================================================== result
void PMResultScreen::on_back() {
	if (String(params.get("mode", "")) == "pvp") PMNet::singleton->close();
	PMApp::go("menu");
}

void PMResultScreen::_ready() {
	GameData &g = GameData::get();
	Dictionary r = params.get("result", Dictionary());
	String mode = params.get("mode", "quick");
	int lt = (int)params.get("local_team", 0);
	Array sc = r.get("score", Array());
	int s0 = sc.size() > 0 ? (int)sc[0] : 0, s1 = sc.size() > 1 ? (int)sc[1] : 0;
	int gf = lt == 0 ? s0 : s1, ga = lt == 0 ? s1 : s0;
	if (mode == "league") g.league_record_user(gf, ga);
	int coins = g.record_result(gf, ga, mode == "pvp" ? String("pvp") : mode);
	if (mode == "league" && gf > ga) {
		g.add_coins(100);
		coins += 100;
	}
	// daily missions
	g.refresh_daily();
	g.mission_add("play", 1);
	if (gf > ga) g.mission_add("win", 1);
	if (gf - ga >= 2) g.mission_add("bigwin", 1);
	if (ga == 0) g.mission_add("clean", 1);
	g.mission_add("goals", gf);
	if (mode == "league") g.mission_add("league", 1);
	if (mode == "pvp") g.mission_add("pvp", 1);
	Array pk = r.get("passes_ok", Array());
	if (pk.size() > lt) g.mission_add("passes", (int)pk[lt]);
	// ratings, man of the match, card experience
	Array ratings = r.get("ratings", Array());
	int motm = (int)r.get("motm", -1);
	std::vector<std::pair<int, float>> xp;
	std::vector<Dictionary> mine;
	for (int i = 0; i < ratings.size(); i++) {
		Dictionary pr = ratings[i];
		if ((int)pr.get("team", -1) != lt) continue;
		mine.push_back(pr);
		int id = (int)pr.get("id", 0);
		if (id > 0 && g.card_by_id(id)) xp.push_back({ id, (float)(double)pr.get("rating", 6.0) });
	}
	std::vector<LevelUp> ups = g.award_xp(xp);
	std::sort(mine.begin(), mine.end(), [](const Dictionary &a, const Dictionary &b) { return (double)a.get("rating", 0.0) > (double)b.get("rating", 0.0); });

	VBoxContainer *v = root();
	String verdict = gf > ga ? S("ПОБЕДА") : (gf == ga ? S("НИЧЬЯ") : S("ПОРАЖЕНИЕ"));
	Color vc = gf > ga ? kit::LIME : (gf == ga ? kit::GOLD : kit::RED);
	Label *vl = kit::label(verdict, 50, vc, HORIZONTAL_ALIGNMENT_CENTER);
	vl->add_theme_font_override("font", kit::font_bold());
	v->add_child(vl);
	Array names = r.get("names", Array());
	Array colors = r.get("colors", Array());
	Array logos = r.get("logos", Array());
	Array shorts = r.get("shorts", Array());
	HBoxContainer *row = kit::hbox(22);
	row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	for (int t = 0; t < 2; t++) {
		String nm = names.size() > t ? String(names[t]) : String();
		Color c = colors.size() > t ? html(colors[t]) : Color(1, 1, 1);
		String lg = logos.size() > t ? String(logos[t]) : String();
		String sh = shorts.size() > t ? String(shorts[t]) : String();
		Label *l = kit::label(nm, 28, Color(1, 1, 1), t == 0 ? HORIZONTAL_ALIGNMENT_RIGHT : HORIZONTAL_ALIGNMENT_LEFT);
		l->set_custom_minimum_size(Vector2(300, 0));
		if (t == 0) {
			row->add_child(l);
			row->add_child(kit::badge(lg, c, sh, 58));
			Label *scl = kit::label(I(s0) + " : " + I(s1), 64, Color(1, 1, 1), HORIZONTAL_ALIGNMENT_CENTER);
			scl->add_theme_font_override("font", kit::font_bold());
			row->add_child(scl);
		} else {
			row->add_child(kit::badge(lg, c, sh, 58));
			row->add_child(l);
		}
	}
	v->add_child(row);
	HBoxContainer *mid = kit::hbox(16);
	mid->set_v_size_flags(SIZE_EXPAND_FILL);
	v->add_child(mid);
	// goals + stats
	PanelContainer *sp = kit::glass();
	sp->set_h_size_flags(SIZE_EXPAND_FILL);
	VBoxContainer *sv = kit::vbox(4);
	sp->add_child(sv);
	sv->add_child(kit::label(S("ГОЛЫ"), 18, kit::MUTED));
	Array scorers = r.get("scorers", Array());
	if (scorers.is_empty()) sv->add_child(kit::label(S("—"), 22, kit::TEXT));
	for (int i = 0; i < scorers.size() && i < 6; i++) {
		Dictionary s = scorers[i];
		int t = (int)s.get("team", 0);
		Color c = colors.size() > t ? html(colors[t]) : Color(1, 1, 1);
		sv->add_child(kit::label(I((int)s.get("min", 0)) + "'  " + String(s.get("name", "")), 20, c.lightened(0.45f)));
	}
	sv->add_child(kit::spacer(true));
	Array poss = r.get("poss", Array());
	Array shots = r.get("shots", Array());
	if (poss.size() >= 2) sv->add_child(kit::stat_bar(S("Владение"), (int)poss[0], (int)poss[1], true));
	if (shots.size() >= 2) sv->add_child(kit::stat_bar(S("Удары"), (int)shots[0], (int)shots[1], false));
	if (pk.size() >= 2) sv->add_child(kit::stat_bar(S("Точные передачи"), (int)pk[0], (int)pk[1], false));
	mid->add_child(sp);
	// ratings
	PanelContainer *rp = kit::glass();
	rp->set_h_size_flags(SIZE_EXPAND_FILL);
	VBoxContainer *rv = kit::vbox(3);
	rp->add_child(rv);
	rv->add_child(kit::label(S("ОЦЕНКИ ИГРОКОВ"), 18, kit::MUTED));
	if (motm >= 0 && motm < ratings.size()) {
		Dictionary m = ratings[motm];
		PanelContainer *mp = kit::panel(Color(1.f, 0.78f, 0.24f, 0.18f), 10, 10);
		HBoxContainer *mh = kit::hbox(10);
		if ((int)m.get("team", -1) == lt) {
			Card *mc = g.card_by_id((int)m.get("id", 0));
			if (mc && !mc->photo.is_empty()) mh->add_child(kit::portrait(*mc, 40, kit::GOLD));
		}
		mh->add_child(kit::label(S("★ Лучший игрок матча"), 18, kit::GOLD));
		mh->add_child(kit::spacer());
		mh->add_child(kit::label(String(m.get("name", "")) + "  " + String::num((double)m.get("rating", 6.0), 1), 20, Color(1, 1, 1)));
		mp->add_child(mh);
		rv->add_child(mp);
	}
	for (size_t i = 0; i < mine.size() && i < 6; i++) {
		Dictionary pr = mine[i];
		HBoxContainer *h = kit::hbox(8);
		Card *pcard = g.card_by_id((int)pr.get("id", 0));
		if (pcard && !pcard->photo.is_empty()) h->add_child(kit::portrait(*pcard, 26, kit::LIME));
		h->add_child(kit::label(GameData::pos_ru(pr.get("pos", "")), 16, kit::MUTED));
		Label *nm = kit::label(kit::short_name(pr.get("name", "")), 19, kit::TEXT);
		nm->set_h_size_flags(SIZE_EXPAND_FILL);
		h->add_child(nm);
		String extra;
		if ((int)pr.get("goals", 0) > 0) extra += S("гол ") + I((int)pr.get("goals", 0)) + " ";
		if ((int)pr.get("assists", 0) > 0) extra += S("пас ") + I((int)pr.get("assists", 0));
		if (!extra.is_empty()) h->add_child(kit::label(extra, 16, kit::LIME));
		double rt = pr.get("rating", 6.0);
		Color rc = rt >= 8.0 ? kit::LIME : (rt >= 6.5 ? kit::ACCENT : (rt >= 6.0 ? kit::TEXT : kit::RED));
		h->add_child(kit::label(String::num(rt, 1), 20, rc));
		rv->add_child(h);
	}
	mid->add_child(rp);
	// rewards
	PanelContainer *wp = kit::glass();
	wp->set_custom_minimum_size(Vector2(300, 0));
	VBoxContainer *wv = kit::vbox(6);
	wp->add_child(wv);
	wv->add_child(kit::label(S("НАГРАДЫ"), 18, kit::MUTED));
	Label *cl = kit::label("+" + kit::fmt_int(coins), 40, kit::GOLD);
	cl->add_theme_font_override("font", kit::font_bold());
	wv->add_child(cl);
	wv->add_child(kit::label(S("монет"), 18, kit::GOLD));
	if (!ups.empty()) {
		wv->add_child(kit::label(S("Уровень повышен:"), 18, kit::LIME));
		for (size_t i = 0; i < ups.size() && i < 4; i++)
			wv->add_child(kit::label(kit::short_name(ups[i].name) + S("  ур.") + I(ups[i].lvl) + "  (" + I(ups[i].ovr) + ")", 17, kit::TEXT));
	}
	int ready = g.missions_ready();
	if (ready > 0) wv->add_child(kit::label(S("Заданий выполнено: ") + I(ready) + S(" — заберите награду в меню"), 16, kit::ACCENT));
	mid->add_child(wp);

	HBoxContainer *h = kit::hbox(14);
	v->add_child(h);
	if (mode == "league") {
		Button *b = kit::primary(S("К таблице лиги"));
		b->set_h_size_flags(SIZE_EXPAND_FILL);
		b->connect("pressed", Fn(b, [] { PMApp::go("league"); }));
		h->add_child(b);
	} else if (mode == "pvp") {
		Button *b = kit::primary(S("В лобби"));
		b->set_h_size_flags(SIZE_EXPAND_FILL);
		b->connect("pressed", Fn(b, [] { PMApp::go("pvp"); }));
		h->add_child(b);
	} else if (params.has("rematch")) {
		Dictionary rp2 = Dictionary(params["rematch"]).duplicate(true);
		rp2["home"] = GameData::get().build_my_team().to_dict();
		rp2["seed"] = (int64_t)(rng().next() & 0x7fffffff);
		Button *b = kit::primary(S("Реванш"));
		b->set_h_size_flags(SIZE_EXPAND_FILL);
		b->connect("pressed", Fn(b, [rp2] { PMApp::go("match", rp2); }));
		h->add_child(b);
	}
	Button *menu = kit::button(S("Главное меню"), kit::PANEL2, 24, Vector2(0, 64));
	menu->set_h_size_flags(SIZE_EXPAND_FILL);
	menu->connect("pressed", Fn(menu, [mode] {
		if (mode == "pvp") PMNet::singleton->close();
		PMApp::go("menu");
	}));
	h->add_child(menu);
}

} // namespace pm
