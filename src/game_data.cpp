#include "game_data.h"
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/dir_access.hpp>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;

namespace pm {

Rng &rng() {
	static Rng r(godot::Time::get_singleton()->get_ticks_usec() ^ 0xA5A5F00DULL);
	return r;
}

static const char *SAVE_PATH = "user://save_cpp.json";
static const int SAVE_VERSION = 1;

// ------------------------------------------------------------------ constants
const std::vector<String> &GameData::formation_names() {
	static std::vector<String> v = { "4-3-3", "4-4-2", "4-2-3-1", "3-5-2", "5-3-2" };
	return v;
}

const std::vector<Slot> &GameData::formation(const String &name) {
	static std::map<String, std::vector<Slot>> f = {
		{ "4-3-3", { { "GK", -0.96f, 0.f }, { "LB", -0.62f, -0.68f }, { "CB", -0.68f, -0.24f }, { "CB", -0.68f, 0.24f }, { "RB", -0.62f, 0.68f },
						   { "CMF", -0.38f, -0.30f }, { "DMF", -0.48f, 0.f }, { "CMF", -0.38f, 0.30f },
						   { "LWF", -0.10f, -0.64f }, { "CF", -0.03f, 0.f }, { "RWF", -0.10f, 0.64f } } },
		{ "4-4-2", { { "GK", -0.96f, 0.f }, { "LB", -0.62f, -0.68f }, { "CB", -0.68f, -0.24f }, { "CB", -0.68f, 0.24f }, { "RB", -0.62f, 0.68f },
						   { "LMF", -0.30f, -0.64f }, { "CMF", -0.38f, -0.20f }, { "CMF", -0.38f, 0.20f }, { "RMF", -0.30f, 0.64f },
						   { "CF", -0.04f, -0.17f }, { "CF", -0.04f, 0.17f } } },
		{ "4-2-3-1", { { "GK", -0.96f, 0.f }, { "LB", -0.62f, -0.68f }, { "CB", -0.68f, -0.24f }, { "CB", -0.68f, 0.24f }, { "RB", -0.62f, 0.68f },
							 { "DMF", -0.48f, -0.18f }, { "DMF", -0.48f, 0.18f }, { "LMF", -0.24f, -0.62f }, { "AMF", -0.22f, 0.f }, { "RMF", -0.24f, 0.62f },
							 { "CF", -0.03f, 0.f } } },
		{ "3-5-2", { { "GK", -0.96f, 0.f }, { "CB", -0.68f, -0.40f }, { "CB", -0.70f, 0.f }, { "CB", -0.68f, 0.40f },
						   { "LMF", -0.34f, -0.72f }, { "CMF", -0.36f, -0.26f }, { "DMF", -0.48f, 0.f }, { "CMF", -0.36f, 0.26f }, { "RMF", -0.34f, 0.72f },
						   { "CF", -0.04f, -0.17f }, { "CF", -0.04f, 0.17f } } },
		{ "5-3-2", { { "GK", -0.96f, 0.f }, { "LB", -0.58f, -0.74f }, { "CB", -0.68f, -0.36f }, { "CB", -0.70f, 0.f }, { "CB", -0.68f, 0.36f }, { "RB", -0.58f, 0.74f },
						   { "CMF", -0.40f, -0.30f }, { "DMF", -0.46f, 0.f }, { "CMF", -0.40f, 0.30f },
						   { "CF", -0.05f, -0.17f }, { "CF", -0.05f, 0.17f } } },
	};
	auto it = f.find(name);
	return it != f.end() ? it->second : f["4-3-3"];
}

const std::vector<String> &GameData::pos_list() {
	static std::vector<String> v = { "GK", "CB", "LB", "RB", "DMF", "CMF", "AMF", "LMF", "RMF", "LWF", "RWF", "CF" };
	return v;
}

static const int POS_WEIGHTS[12] = { 2, 4, 2, 2, 2, 3, 2, 1, 1, 1, 1, 3 };

String GameData::group(const String &p) {
	if (p == "GK") return "GK";
	if (p == "CB" || p == "LB" || p == "RB") return "DEF";
	if (p == "LWF" || p == "RWF" || p == "CF") return "FWD";
	return "MID";
}

String GameData::pos_ru(const String &p) {
	static std::map<String, const char *> m = { { "GK", "ВР" }, { "CB", "ЦЗ" }, { "LB", "ЛЗ" }, { "RB", "ПЗ" }, { "DMF", "ОП" },
		{ "CMF", "ЦП" }, { "AMF", "АП" }, { "LMF", "ЛП" }, { "RMF", "ПП" }, { "LWF", "ЛФ" }, { "RWF", "ПФ" }, { "CF", "ЦФ" } };
	auto it = m.find(p);
	return it != m.end() ? S(it->second) : p;
}

String GameData::stat_ru(const String &k) {
	static std::map<String, const char *> m = { { "pace", "СКО" }, { "sho", "УДР" }, { "pas", "ПАС" }, { "dri", "ДРБ" },
		{ "def", "ЗАЩ" }, { "phy", "ФИЗ" }, { "gk", "ВРТ" } };
	auto it = m.find(k);
	return it != m.end() ? S(it->second) : k;
}

const std::vector<PackDef> &GameData::packs() {
	static std::vector<PackDef> v = {
		{ "std", "Стандартный", "3 карточки. Шанс золота 10%", 600, 3, 0, { { 0.60f, 54, 65 }, { 0.30f, 65, 74 }, { 0.09f, 75, 84 }, { 0.01f, 85, 92 } } },
		{ "prem", "Премиум", "3 карточки. Гарантированное золото", 1800, 3, 75, { { 0.25f, 60, 68 }, { 0.45f, 68, 76 }, { 0.26f, 76, 85 }, { 0.04f, 85, 93 } } },
		{ "leg", "Легендарный", "3 карточки. Гарантированная легенда", 5000, 3, 86, { { 0.0f, 70, 75 }, { 0.55f, 75, 82 }, { 0.30f, 82, 86 }, { 0.15f, 86, 95 } } },
	};
	return v;
}

const char *GameData::difficulty_name(int i) {
	static const char *n[3] = { "Лёгкий", "Средний", "Сложный" };
	return n[clampi(i, 0, 2)];
}

const std::vector<int> &GameData::minutes_options() {
	static std::vector<int> v = { 4, 6, 8, 10 };
	return v;
}

static const char *FIRST[] = { "Алекс", "Тимур", "Рустам", "Фаррух", "Диего", "Марко", "Лукас", "Иван", "Амир", "Карим",
	"Николас", "Сантьяго", "Матео", "Жоао", "Эмиль", "Даниэль", "Юсуф", "Омар", "Лео", "Андрей", "Шахзод",
	"Бехруз", "Феликс", "Тео", "Рафаэль", "Хуго", "Адам", "Ноа", "Самир", "Илья", "Артём", "Мурод", "Джамшед",
	"Кенджи", "Хироши", "Мин-Хо", "Пабло", "Энцо", "Оскар", "Виктор", "Стефан", "Кофи", "Яя", "Уго", "Нико",
	"Бруно", "Тиаго", "Арда", "Эмре", "Миро", "Далер", "Сухроб", "Фирдавс", "Комрон", "Исмоил", "Зафар" };
static const char *LAST[] = { "Сафаров", "Рахимов", "Каримов", "Нурматов", "Сильвейра", "Перейра", "Мартинес", "Гарсия", "Россетти",
	"Бьянко", "Мюллер", "Шнайдер", "Новак", "Ковач", "Петров", "Волков", "Орлов", "Дюпон", "Лефевр", "Андерсен",
	"Ларсен", "Ямада", "Танака", "Ким", "Пак", "Окафор", "Менса", "Диалло", "Траоре", "Ибрагимов", "Султанов",
	"Хасанов", "Мирзоев", "Фернандес", "Лопес", "Морено", "Коста", "Рибейро", "Яблонски", "Хорват", "Эрдоган",
	"Йылмаз", "Бергман", "Линд", "Вега", "Кастро", "Ромеро", "Соса", "Бенитес", "Арсланов", "Назаров", "Шарипов",
	"Давлатов", "Олимов", "Юсупов", "Холов" };

// offsets for pace, sho, pas, dri, def, phy
static int profile(const String &pos, int k) {
	static std::map<String, std::array<int, 6>> p = {
		{ "GK", { -18, -40, -14, -30, -35, 0 } }, { "CB", { -6, -22, -8, -14, 6, 6 } }, { "LB", { 5, -18, -2, -4, 0, -2 } },
		{ "RB", { 5, -18, -2, -4, 0, -2 } }, { "DMF", { -4, -10, 3, -4, 3, 4 } }, { "CMF", { 0, -4, 6, 2, -6, 0 } },
		{ "AMF", { 2, 2, 7, 6, -25, -6 } }, { "LMF", { 6, -4, 3, 4, -18, -4 } }, { "RMF", { 6, -4, 3, 4, -18, -4 } },
		{ "LWF", { 8, 2, -2, 7, -30, -6 } }, { "RWF", { 8, 2, -2, 7, -30, -6 } }, { "CF", { 3, 7, -5, 3, -32, 4 } },
	};
	auto it = p.find(pos);
	return it != p.end() ? it->second[k] : 0;
}

// ------------------------------------------------------------------ Card / teams (de)serialisation
int Card::stat(const String &k) const {
	if (k == "pace") return pace;
	if (k == "sho") return sho;
	if (k == "pas") return pas;
	if (k == "dri") return dri;
	if (k == "def") return def;
	if (k == "phy") return phy;
	if (k == "gk") return gk;
	return ovr;
}

Dictionary Card::to_dict() const {
	Dictionary d;
	d["id"] = id; d["name"] = name; d["pos"] = pos; d["ovr"] = ovr; d["pace"] = pace; d["sho"] = sho;
	d["pas"] = pas; d["dri"] = dri; d["def"] = def; d["phy"] = phy; d["gk"] = gk; d["num"] = num;
	d["lvl"] = lvl; d["xp"] = xp;
	if (!photo.is_empty()) d["photo"] = photo;
	return d;
}

static int di(const Dictionary &d, const char *k, int def, int lo, int hi) {
	Variant v = d.get(k, def);
	int x = def;
	if (v.get_type() == Variant::INT || v.get_type() == Variant::FLOAT) x = (int)(double)v;
	else if (v.get_type() == Variant::STRING) x = String(v).to_int();
	return clampi(x, lo, hi);
}

static String ds(const Dictionary &d, const char *k, const String &def, int maxlen = 40) {
	Variant v = d.get(k, def);
	String s = v.get_type() == Variant::STRING ? String(v) : def;
	return s.strip_edges().left(maxlen);
}

Card Card::from_dict(const Dictionary &d) {
	Card c;
	c.id = di(d, "id", 0, 0, 1 << 30);
	c.name = ds(d, "name", S("Игрок"), 32);
	c.pos = ds(d, "pos", "CMF", 4).to_upper();
	bool ok = false;
	for (auto &p : GameData::pos_list()) ok = ok || p == c.pos;
	if (!ok) c.pos = "CMF";
	c.ovr = di(d, "ovr", 60, 30, 99);
	c.pace = di(d, "pace", c.ovr, 10, 99);
	c.sho = di(d, "sho", c.ovr, 10, 99);
	c.pas = di(d, "pas", c.ovr, 10, 99);
	c.dri = di(d, "dri", c.ovr, 10, 99);
	c.def = di(d, "def", c.ovr, 10, 99);
	c.phy = di(d, "phy", c.ovr, 10, 99);
	c.gk = di(d, "gk", c.pos == "GK" ? c.ovr : 15, 10, 99);
	c.num = di(d, "num", 10, 1, 99);
	c.lvl = di(d, "lvl", 0, 0, 10);
	c.xp = di(d, "xp", 0, 0, 100000);
	String ph = ds(d, "photo", "", 120);
	if (ph.begins_with("user://photos/") && !ph.contains("..") && FileAccess::file_exists(ph)) c.photo = ph;
	return c;
}

Dictionary MatchTeam::to_dict() const {
	Dictionary d;
	d["name"] = name; d["short"] = shrt; d["shirt"] = shirt.to_html(false); d["shorts"] = shorts.to_html(false);
	d["formation"] = formation; d["line"] = line; d["press"] = press; d["style"] = style; d["ovr"] = ovr;
	d["logo"] = logo;
	Array ps;
	for (auto &p : players) {
		Dictionary pd = p.c.to_dict();
		pd["slot_pos"] = p.slot_pos;
		ps.push_back(pd);
	}
	d["players"] = ps;
	return d;
}

MatchTeam MatchTeam::from_dict(const Dictionary &d) {
	MatchTeam t;
	t.name = ds(d, "name", S("Соперник"), 24);
	t.shrt = ds(d, "short", S("СОП"), 4);
	t.shirt = html(ds(d, "shirt", "#d62839"), Color(0.84f, 0.16f, 0.22f));
	t.shorts = html(ds(d, "shorts", "#ffffff"));
	t.formation = ds(d, "formation", "4-3-3", 8);
	bool ok = false;
	for (auto &f : GameData::formation_names()) ok = ok || f == t.formation;
	if (!ok) t.formation = "4-3-3";
	t.line = di(d, "line", 1, 0, 2);
	t.press = di(d, "press", 1, 0, 2);
	t.style = di(d, "style", 1, 0, 2);
	t.ovr = di(d, "ovr", 60, 1, 99);
	t.logo = ds(d, "logo", "", 400);
	if (!t.logo.begins_with("user://patches/")) t.logo = "";
	Variant pv = d.get("players", Array());
	Array ps = pv.get_type() == Variant::ARRAY ? Array(pv) : Array();
	const auto &slots = GameData::formation(t.formation);
	for (int i = 0; i < 11; i++) {
		Dictionary pd = (i < ps.size() && ps[i].get_type() == Variant::DICTIONARY) ? Dictionary(ps[i]) : Dictionary();
		MatchPlayer mp;
		mp.c = Card::from_dict(pd);
		mp.slot_pos = slots[i].pos;
		t.players.push_back(mp);
	}
	return t;
}

Dictionary TeamInfo::to_dict() const {
	Dictionary d;
	d["id"] = id; d["name"] = name; d["short"] = shrt; d["shirt"] = shirt; d["shorts"] = shorts;
	d["strength"] = strength; d["user"] = user; d["logo"] = logo;
	Array ps;
	for (auto &c : players) ps.push_back(c.to_dict());
	d["players"] = ps;
	return d;
}

TeamInfo TeamInfo::from_dict(const Dictionary &d) {
	TeamInfo t;
	t.id = ds(d, "id", "", 32);
	t.name = ds(d, "name", S("Клуб"), 24);
	t.shrt = ds(d, "short", t.name.left(3).to_upper(), 4);
	t.shirt = ds(d, "shirt", "#1d4ed8", 9);
	t.shorts = ds(d, "shorts", "#ffffff", 9);
	if (!Color::html_is_valid(t.shirt)) t.shirt = "#1d4ed8";
	if (!Color::html_is_valid(t.shorts)) t.shorts = "#ffffff";
	t.strength = di(d, "strength", 65, 40, 95);
	t.user = (bool)d.get("user", false);
	t.logo = ds(d, "logo", "", 400);
	Variant pv = d.get("players", Array());
	if (pv.get_type() == Variant::ARRAY) {
		Array ps = pv;
		for (int i = 0; i < ps.size() && i < 30; i++)
			if (ps[i].get_type() == Variant::DICTIONARY) t.players.push_back(Card::from_dict(ps[i]));
	}
	return t;
}

// ------------------------------------------------------------------ singleton & save
GameData &GameData::get() {
	static GameData g;
	return g;
}

Dictionary GameData::to_dict() const {
	Dictionary d;
	d["version"] = SAVE_VERSION;
	d["coins"] = coins; d["next_id"] = next_id; d["packs_opened"] = packs_opened;
	d["team_name"] = team_name; d["team_short"] = team_short; d["shirt"] = shirt; d["shorts"] = shorts;
	Array cs;
	for (auto &c : cards) cs.push_back(c.to_dict());
	d["cards"] = cs;
	Dictionary sq;
	sq["formation"] = squad.formation; sq["line"] = squad.line; sq["press"] = squad.press; sq["style"] = squad.style;
	Array sl;
	for (int id : squad.slots) sl.push_back(id);
	sq["slots"] = sl;
	d["squad"] = sq;
	Dictionary st;
	st["difficulty"] = settings.difficulty; st["minutes"] = settings.minutes; st["camera"] = settings.camera;
	st["sound"] = settings.sound; st["last_ip"] = settings.last_ip; st["active_patch"] = settings.active_patch;
	st["replays"] = settings.replays;
	st["cam_v3"] = true;
	d["settings"] = st;
	Dictionary dy;
	dy["date"] = daily.date; dy["last_login"] = daily.last_login; dy["streak"] = daily.streak; dy["reward_claimed"] = daily.reward_claimed;
	Array ms;
	for (auto &m : daily.missions) {
		Dictionary md;
		md["kind"] = m.kind; md["text"] = m.text; md["goal"] = m.goal; md["progress"] = m.progress; md["reward"] = m.reward; md["claimed"] = m.claimed;
		ms.push_back(md);
	}
	dy["missions"] = ms;
	d["daily"] = dy;
	Dictionary ss;
	ss["played"] = stats.played; ss["won"] = stats.won; ss["drawn"] = stats.drawn; ss["lost"] = stats.lost;
	ss["gf"] = stats.gf; ss["ga"] = stats.ga;
	d["stats"] = ss;
	Dictionary lg;
	lg["name"] = league.name; lg["season"] = league.season; lg["user"] = league.user; lg["round"] = league.round;
	Array teams;
	for (auto &t : league.teams) teams.push_back(t.to_dict());
	lg["teams"] = teams;
	Array fx;
	for (auto &r : league.fixtures) {
		Array ra;
		for (auto &m : r) {
			Array ma;
			ma.push_back(m[0]); ma.push_back(m[1]);
			ra.push_back(ma);
		}
		fx.push_back(ra);
	}
	lg["fixtures"] = fx;
	Array tb;
	for (auto &r : league.table) {
		Array ra;
		for (int v : { r.p, r.w, r.d, r.l, r.gf, r.ga, r.pts }) ra.push_back(v);
		tb.push_back(ra);
	}
	lg["table"] = tb;
	Array la;
	for (auto &l : league.last) {
		Array a;
		for (int v : l) a.push_back(v);
		la.push_back(a);
	}
	lg["last"] = la;
	d["league"] = lg;
	return d;
}

void GameData::from_dict(const Dictionary &d) {
	coins = di(d, "coins", 3000, 0, 1 << 30);
	next_id = di(d, "next_id", 1, 1, 1 << 30);
	packs_opened = di(d, "packs_opened", 0, 0, 1 << 30);
	team_name = ds(d, "team_name", S("Мой клуб"), 20);
	team_short = ds(d, "team_short", S("МОЙ"), 4);
	shirt = ds(d, "shirt", "#1d4ed8", 9);
	shorts = ds(d, "shorts", "#ffffff", 9);
	cards.clear();
	Array cs = d.get("cards", Array());
	for (int i = 0; i < cs.size(); i++) cards.push_back(Card::from_dict(cs[i]));
	Dictionary sq = d.get("squad", Dictionary());
	squad.formation = ds(sq, "formation", "4-3-3", 8);
	squad.line = di(sq, "line", 1, 0, 2);
	squad.press = di(sq, "press", 1, 0, 2);
	squad.style = di(sq, "style", 1, 0, 2);
	squad.slots.clear();
	Array sl = sq.get("slots", Array());
	for (int i = 0; i < sl.size(); i++) squad.slots.push_back((int)(double)sl[i]);
	Dictionary st = d.get("settings", Dictionary());
	settings.difficulty = di(st, "difficulty", 1, 0, 2);
	settings.minutes = di(st, "minutes", 6, 2, 20);
	settings.camera = di(st, "camera", 1, 0, 1);
	if (!st.has("cam_v3")) settings.camera = 1; // 3.1: everyone starts on the closer camera once
	settings.sound = (bool)st.get("sound", true);
	settings.last_ip = ds(st, "last_ip", "192.168.43.1", 40);
	settings.active_patch = ds(st, "active_patch", "", 80);
	settings.replays = (bool)st.get("replays", true);
	Dictionary dy = d.get("daily", Dictionary());
	daily = Daily();
	daily.date = ds(dy, "date", "", 12);
	daily.last_login = ds(dy, "last_login", "", 12);
	daily.streak = di(dy, "streak", 0, 0, 100000);
	daily.reward_claimed = (bool)dy.get("reward_claimed", false);
	Array ms = dy.get("missions", Array());
	for (int i = 0; i < ms.size() && i < 6; i++) {
		Dictionary md = ms[i];
		Mission m;
		m.kind = ds(md, "kind", "play", 16);
		m.text = ds(md, "text", "", 80);
		m.goal = di(md, "goal", 1, 1, 1000);
		m.progress = di(md, "progress", 0, 0, 100000);
		m.reward = di(md, "reward", 200, 0, 100000);
		m.claimed = (bool)md.get("claimed", false);
		daily.missions.push_back(m);
	}
	Dictionary ss = d.get("stats", Dictionary());
	stats.played = di(ss, "played", 0, 0, 1 << 30);
	stats.won = di(ss, "won", 0, 0, 1 << 30);
	stats.drawn = di(ss, "drawn", 0, 0, 1 << 30);
	stats.lost = di(ss, "lost", 0, 0, 1 << 30);
	stats.gf = di(ss, "gf", 0, 0, 1 << 30);
	stats.ga = di(ss, "ga", 0, 0, 1 << 30);
	Dictionary lg = d.get("league", Dictionary());
	league = League();
	league.name = ds(lg, "name", S("Суперлига"), 30);
	league.season = di(lg, "season", 1, 1, 999);
	league.user = di(lg, "user", 0, 0, 7);
	league.round = di(lg, "round", 0, 0, 99);
	Array teams = lg.get("teams", Array());
	for (int i = 0; i < teams.size(); i++) league.teams.push_back(TeamInfo::from_dict(teams[i]));
	Array fx = lg.get("fixtures", Array());
	for (int i = 0; i < fx.size(); i++) {
		Array ra = fx[i];
		std::vector<std::array<int, 2>> r;
		for (int j = 0; j < ra.size(); j++) {
			Array m = ra[j];
			if (m.size() >= 2) r.push_back({ (int)(double)m[0], (int)(double)m[1] });
		}
		league.fixtures.push_back(r);
	}
	Array tb = lg.get("table", Array());
	for (int i = 0; i < tb.size(); i++) {
		Array a = tb[i];
		TableRow r;
		if (a.size() >= 7) {
			r.p = (int)(double)a[0]; r.w = (int)(double)a[1]; r.d = (int)(double)a[2]; r.l = (int)(double)a[3];
			r.gf = (int)(double)a[4]; r.ga = (int)(double)a[5]; r.pts = (int)(double)a[6];
		}
		league.table.push_back(r);
	}
	Array la = lg.get("last", Array());
	for (int i = 0; i < la.size(); i++) {
		Array a = la[i];
		if (a.size() >= 4) league.last.push_back({ (int)(double)a[0], (int)(double)a[1], (int)(double)a[2], (int)(double)a[3] });
	}
	if (league.teams.size() != 8 || league.table.size() != 8 || league.fixtures.empty()) new_league();
	validate_squad();
}

void GameData::load() {
	if (FileAccess::file_exists(SAVE_PATH)) {
		Ref<FileAccess> f = FileAccess::open(SAVE_PATH, FileAccess::READ);
		if (f.is_valid()) {
			Variant v = JSON::parse_string(f->get_as_text());
			if (v.get_type() == Variant::DICTIONARY && (int)(double)Dictionary(v).get("version", 0) == SAVE_VERSION) {
				from_dict(v);
				load_active_patch();
				return;
			}
		}
	}
	new_game();
}

void GameData::save() {
	Ref<FileAccess> f = FileAccess::open(SAVE_PATH, FileAccess::WRITE);
	if (f.is_valid()) f->store_string(JSON::stringify(to_dict()));
}

void GameData::new_game() {
	coins = 3000;
	next_id = 1;
	packs_opened = 0;
	team_name = S("Мой клуб");
	team_short = S("МОЙ");
	shirt = "#1d4ed8";
	shorts = "#ffffff";
	cards.clear();
	squad = Squad();
	stats = Stats();
	String keep_patch = settings.active_patch;
	settings = Settings();
	settings.active_patch = keep_patch;
	Rng &r = rng();
	for (const char *p : { "GK", "LB", "CB", "CB", "RB", "CMF", "DMF", "CMF", "LWF", "CF", "RWF" })
		squad.slots.push_back(add_card(make_card(r.irange(61, 69), p, r)).id);
	for (const char *p : { "GK", "CB", "RB", "CMF", "AMF", "LMF", "CF" }) add_card(make_card(r.irange(56, 64), p, r));
	load_active_patch();
	new_league();
	save();
}

void GameData::reset_progress() {
	new_game();
	coins_rev++;
}

// ------------------------------------------------------------------ coins
void GameData::add_coins(int n) {
	coins = std::max(0, coins + n);
	coins_rev++;
	save();
}

bool GameData::spend(int n) {
	if (coins < n) return false;
	coins -= n;
	coins_rev++;
	save();
	return true;
}

// ------------------------------------------------------------------ cards
String GameData::rarity(int o) {
	if (o >= 85) return "legend";
	if (o >= 75) return "gold";
	if (o >= 65) return "silver";
	return "bronze";
}

Color GameData::rarity_color(const String &r) {
	if (r == "legend") return Color::html("b26bff");
	if (r == "gold") return Color::html("f2c94c");
	if (r == "silver") return Color::html("c3ccd9");
	return Color::html("c07a45");
}

String GameData::rarity_name(const String &r) {
	if (r == "legend") return S("Легенда");
	if (r == "gold") return S("Золото");
	if (r == "silver") return S("Серебро");
	return S("Бронза");
}

int GameData::sell_value(const Card &c) {
	if (c.ovr >= 85) return 2000 + (c.ovr - 85) * 300;
	if (c.ovr >= 75) return 400 + (c.ovr - 75) * 60;
	if (c.ovr >= 65) return 120 + (c.ovr - 65) * 10;
	return 50;
}

Card GameData::make_card(int ovr, String pos, Rng &r) const {
	if (pos.is_empty()) {
		int total = 0;
		for (int w : POS_WEIGHTS) total += w;
		int x = r.irange(1, total);
		for (int i = 0; i < 12; i++) {
			x -= POS_WEIGHTS[i];
			if (x <= 0) { pos = pos_list()[i]; break; }
		}
	}
	Card c;
	c.pos = pos;
	c.ovr = clampi(ovr, 40, 99);
	c.name = S(FIRST[r.irange(0, (int)(sizeof(FIRST) / sizeof(FIRST[0])) - 1)]) + " " +
			S(LAST[r.irange(0, (int)(sizeof(LAST) / sizeof(LAST[0])) - 1)]);
	int *st[6] = { &c.pace, &c.sho, &c.pas, &c.dri, &c.def, &c.phy };
	for (int i = 0; i < 6; i++) *st[i] = clampi(c.ovr + profile(pos, i) + r.irange(-5, 5), 25, 99);
	c.gk = pos == "GK" ? clampi(c.ovr + r.irange(-1, 2), 40, 99) : r.irange(10, 25);
	static std::map<String, std::vector<int>> nums = { { "GK", { 1, 12, 13 } }, { "CB", { 4, 5, 6, 15 } }, { "LB", { 3, 21 } },
		{ "RB", { 2, 22 } }, { "DMF", { 6, 16, 18 } }, { "CMF", { 8, 14, 20 } }, { "AMF", { 10, 19 } }, { "LMF", { 11, 17 } },
		{ "RMF", { 7, 23 } }, { "LWF", { 11, 17, 77 } }, { "RWF", { 7, 27 } }, { "CF", { 9, 99, 29 } } };
	auto &o = nums[pos];
	c.num = o[r.irange(0, (int)o.size() - 1)];
	return c;
}

Card &GameData::add_card(Card c) {
	c.id = next_id++;
	cards.push_back(c);
	return cards.back();
}

Card *GameData::card_by_id(int id) {
	for (auto &c : cards)
		if (c.id == id) return &c;
	return nullptr;
}

bool GameData::is_starter(int id) const {
	return std::find(squad.slots.begin(), squad.slots.end(), id) != squad.slots.end();
}

int GameData::sell_card(int id) {
	if (is_starter(id)) return 0;
	for (size_t i = 0; i < cards.size(); i++) {
		if (cards[i].id == id) {
			int v = sell_value(cards[i]);
			cards.erase(cards.begin() + i);
			add_coins(v);
			return v;
		}
	}
	return 0;
}

std::vector<Card> GameData::open_pack(const String &id) {
	std::vector<Card> out;
	const PackDef *pk = nullptr;
	for (auto &p : packs())
		if (id == p.id) pk = &p;
	if (!pk || !spend(pk->price)) return out;
	Rng &r = rng();
	for (int i = 0; i < pk->count; i++) {
		double x = r.randf();
		int ovr = 60;
		bool found = false;
		for (auto &o : pk->odds) {
			x -= o[0];
			if (x <= 0.0) { ovr = r.irange((int)o[1], (int)o[2]); found = true; break; }
		}
		if (!found) ovr = r.irange((int)pk->odds[3][1], (int)pk->odds[3][2]);
		if (i == 0 && pk->guarantee > 0) ovr = std::max(ovr, r.irange(pk->guarantee, pk->guarantee + 5));
		Card c = make_card(ovr, "", r);
		out.push_back(add_card(c));
	}
	std::sort(out.begin(), out.end(), [](const Card &a, const Card &b) { return a.ovr < b.ovr; });
	packs_opened++;
	for (auto &m : daily.missions)
		if (m.kind == "pack" && !m.claimed) m.progress = std::min(m.goal, m.progress + 1);
	save();
	return out;
}

// ------------------------------------------------------------------ squad
float GameData::compat(const String &cp, const String &sp) {
	if (cp == sp) return 1.0f;
	if (cp == "GK" || sp == "GK") return 0.3f;
	static const char *nb[][2] = { { "LB", "LMF" }, { "RB", "RMF" }, { "LMF", "LWF" }, { "RMF", "RWF" }, { "AMF", "CF" }, { "DMF", "CB" },
		{ "AMF", "CMF" }, { "DMF", "CMF" }, { "LWF", "CF" }, { "RWF", "CF" } };
	for (auto &n : nb)
		if ((cp == n[0] && sp == n[1]) || (cp == n[1] && sp == n[0])) return 0.92f;
	if (group(cp) == group(sp)) return 0.88f;
	return 0.7f;
}

std::vector<String> GameData::slot_positions(const String &f) const {
	std::vector<String> out;
	for (auto &s : formation(f)) out.push_back(s.pos);
	return out;
}

int GameData::effective_ovr(const Card &c, const String &sp) const {
	if (sp == "GK") return c.pos == "GK" ? c.ovr : c.gk;
	return (int)std::round(c.ovr * lerpf(0.75f, 1.0f, compat(c.pos, sp)));
}

int GameData::team_ovr() {
	auto sp = slot_positions(squad.formation);
	int total = 0;
	for (int i = 0; i < 11 && i < (int)squad.slots.size(); i++) {
		Card *c = card_by_id(squad.slots[i]);
		if (c) total += effective_ovr(*c, sp[i]);
	}
	return (int)std::round(total / 11.0);
}

void GameData::validate_squad() {
	bool valid = squad.slots.size() == 11;
	std::vector<int> seen;
	for (int id : squad.slots) {
		if (!card_by_id(id) || std::find(seen.begin(), seen.end(), id) != seen.end()) valid = false;
		seen.push_back(id);
	}
	bool fok = false;
	for (auto &f : formation_names()) fok = fok || f == squad.formation;
	if (!fok) { squad.formation = "4-3-3"; valid = false; }
	if (!valid) auto_squad();
}

void GameData::auto_squad() {
	auto sp = slot_positions(squad.formation);
	std::vector<int> slots(11, -1);
	std::vector<int> used;
	for (int i = 0; i < 11; i++) {
		int best = -1, bv = -1;
		for (auto &c : cards) {
			if (std::find(used.begin(), used.end(), c.id) != used.end()) continue;
			int v = effective_ovr(c, sp[i]);
			if (v > bv) { bv = v; best = c.id; }
		}
		slots[i] = best;
		used.push_back(best);
	}
	squad.slots = slots;
	save();
}

void GameData::set_slot(int slot, int card_id) {
	auto it = std::find(squad.slots.begin(), squad.slots.end(), card_id);
	if (it != squad.slots.end()) *it = squad.slots[slot];
	squad.slots[slot] = card_id;
	save();
}

MatchTeam GameData::build_my_team() {
	MatchTeam t;
	t.name = team_name;
	t.shrt = team_short;
	t.shirt = html(shirt, Color(0.11f, 0.31f, 0.85f));
	t.shorts = html(shorts);
	t.formation = squad.formation;
	t.line = squad.line; t.press = squad.press; t.style = squad.style;
	auto sp = slot_positions(squad.formation);
	for (int i = 0; i < 11; i++) {
		Card *cp = card_by_id(squad.slots[i]);
		MatchPlayer mp;
		mp.c = cp ? *cp : make_card(55, sp[i], rng());
		float f = lerpf(0.82f, 1.0f, compat(mp.c.pos, sp[i]));
		int *st[6] = { &mp.c.pace, &mp.c.sho, &mp.c.pas, &mp.c.dri, &mp.c.def, &mp.c.phy };
		for (int k = 0; k < 6; k++) *st[k] = (int)std::round(*st[k] * f);
		mp.slot_pos = sp[i];
		t.players.push_back(mp);
	}
	t.ovr = team_ovr();
	if (patch.active && patch.has_my_team) t.logo = patch.my_team.logo;
	return t;
}

MatchTeam GameData::build_ai_team(const TeamInfo &ti, int bonus) {
	Rng r((uint64_t)ti.name.hash() * 31u + (uint64_t)league.season);
	MatchTeam t;
	t.name = ti.name;
	t.shrt = ti.shrt;
	t.logo = ti.logo;
	t.shirt = html(ti.shirt, Color(0.84f, 0.16f, 0.22f));
	t.shorts = html(ti.shorts);
	t.formation = formation_names()[r.irange(0, (int)formation_names().size() - 1)];
	t.line = r.irange(0, 2); t.press = r.irange(0, 2); t.style = r.irange(0, 2);
	int strength = ti.strength + bonus;
	auto sp = slot_positions(t.formation);
	std::vector<bool> used(ti.players.size(), false);
	for (int i = 0; i < 11; i++) {
		MatchPlayer mp;
		mp.slot_pos = sp[i];
		// players supplied by a patch: best fit for the slot
		int best = -1, bv = -1;
		for (size_t k = 0; k < ti.players.size(); k++) {
			if (used[k]) continue;
			int v = effective_ovr(ti.players[k], sp[i]);
			if (v > bv) { bv = v; best = (int)k; }
		}
		if (best >= 0 && bv >= 40) {
			used[best] = true;
			mp.c = ti.players[best];
			if (bonus != 0) {
				int *st[6] = { &mp.c.pace, &mp.c.sho, &mp.c.pas, &mp.c.dri, &mp.c.def, &mp.c.phy };
				for (int k = 0; k < 6; k++) *st[k] = clampi(*st[k] + bonus, 20, 99);
				mp.c.gk = clampi(mp.c.gk + bonus, 10, 99);
			}
		} else {
			mp.c = make_card(strength + r.irange(-4, 4), sp[i], r);
		}
		t.players.push_back(mp);
	}
	t.ovr = strength;
	return t;
}

// ------------------------------------------------------------------ league
static std::vector<std::vector<std::array<int, 2>>> round_robin(int n) {
	std::vector<int> ids;
	for (int i = 0; i < n; i++) ids.push_back(i);
	std::vector<std::vector<std::array<int, 2>>> rounds;
	for (int r = 0; r < n - 1; r++) {
		std::vector<std::array<int, 2>> g;
		for (int i = 0; i < n / 2; i++) {
			int a = ids[i], b = ids[n - 1 - i];
			if ((r + i) % 2 == 0) g.push_back({ a, b });
			else g.push_back({ b, a });
		}
		rounds.push_back(g);
		int last = ids.back();
		ids.pop_back();
		ids.insert(ids.begin() + 1, last);
	}
	return rounds;
}

void GameData::new_league(int season) {
	Rng &r = rng();
	std::vector<TeamInfo> pool = base_team_pool();
	for (size_t i = pool.size(); i > 1; i--) std::swap(pool[i - 1], pool[r.irange(0, (int)i - 1)]);
	league = League();
	league.season = season;
	league.name = patch.active && !patch.league_name.is_empty() ? patch.league_name : S("Суперлига");
	int base = 60 + (season - 1) * 2;
	for (int i = 0; i < 7 && i < (int)pool.size(); i++) {
		TeamInfo t = pool[i];
		if (patch.active)
			t.strength = clampi(t.strength + (season - 1) * 2, 45, 95);
		else
			t.strength = clampi(base + i * 2 + r.irange(-2, 2), 50, 92);
		t.user = false;
		league.teams.push_back(t);
	}
	TeamInfo me;
	me.id = "user";
	me.name = team_name;
	me.shrt = team_short;
	me.shirt = shirt;
	me.shorts = shorts;
	me.strength = cards.empty() ? 65 : team_ovr();
	me.user = true;
	if (patch.active && patch.has_my_team) me.logo = patch.my_team.logo;
	league.teams.push_back(me);
	for (size_t i = league.teams.size(); i > 1; i--) std::swap(league.teams[i - 1], league.teams[r.irange(0, (int)i - 1)]);
	for (size_t i = 0; i < league.teams.size(); i++)
		if (league.teams[i].user) league.user = (int)i;
	league.table.assign(league.teams.size(), TableRow());
	league.fixtures = round_robin((int)league.teams.size());
}

bool GameData::league_finished() const { return league.round >= (int)league.fixtures.size(); }

std::array<int, 2> GameData::league_user_fixture() const {
	if (league_finished()) return { -1, -1 };
	for (auto &m : league.fixtures[league.round])
		if (m[0] == league.user || m[1] == league.user) return m;
	return { -1, -1 };
}

static int poisson(double l, Rng &r) {
	double L = std::exp(-l), p = 1.0;
	int k = 0;
	while (true) {
		p *= r.randf();
		if (p <= L || k > 8) break;
		k++;
	}
	return k;
}

std::array<int, 2> GameData::sim_score(int sa, int sb) {
	double la = 1.35 * std::exp((sa - sb) / 14.0) + 0.1;
	double lb = 1.1 * std::exp((sb - sa) / 14.0) + 0.1;
	return { poisson(la, rng()), poisson(lb, rng()) };
}

static void table_add(TableRow &r, int gf, int ga) {
	r.p++; r.gf += gf; r.ga += ga;
	if (gf > ga) { r.w++; r.pts += 3; }
	else if (gf == ga) { r.d++; r.pts += 1; }
	else r.l++;
}

void GameData::league_record_user(int ug, int og) {
	if (league_finished()) return;
	league.teams[league.user].strength = team_ovr();
	league.last.clear();
	for (auto &m : league.fixtures[league.round]) {
		int a = m[0], b = m[1], ga, gb;
		if (a == league.user) { ga = ug; gb = og; }
		else if (b == league.user) { ga = og; gb = ug; }
		else {
			auto s = sim_score(league.teams[a].strength, league.teams[b].strength);
			ga = s[0]; gb = s[1];
		}
		table_add(league.table[a], ga, gb);
		table_add(league.table[b], gb, ga);
		league.last.push_back({ a, b, ga, gb });
	}
	league.round++;
	save();
}

std::vector<int> GameData::league_standings() const {
	std::vector<int> idx;
	for (size_t i = 0; i < league.teams.size(); i++) idx.push_back((int)i);
	const auto &t = league.table;
	std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
		if (t[a].pts != t[b].pts) return t[a].pts > t[b].pts;
		int ga = t[a].gf - t[a].ga, gb = t[b].gf - t[b].ga;
		if (ga != gb) return ga > gb;
		return t[a].gf > t[b].gf;
	});
	return idx;
}

int GameData::league_user_place() const {
	auto s = league_standings();
	for (size_t i = 0; i < s.size(); i++)
		if (s[i] == league.user) return (int)i + 1;
	return 8;
}

int GameData::league_reward(int place) {
	switch (place) {
		case 1: return 6000;
		case 2: return 3500;
		case 3: return 2000;
		default: return 600;
	}
}

int GameData::league_next_season() {
	int reward = league_reward(league_user_place());
	add_coins(reward);
	new_league(league.season + 1);
	save();
	return reward;
}

MatchTeam GameData::league_opponent_team(bool &user_home) {
	auto m = league_user_fixture();
	user_home = m[0] == league.user;
	int opp = user_home ? m[1] : m[0];
	static const int bonus[3] = { -4, 0, 4 };
	return build_ai_team(league.teams[opp], bonus[settings.difficulty]);
}

// ------------------------------------------------------------------ results
int GameData::match_reward(int gf, int ga, const String &mode) const {
	int base = gf > ga ? 300 : (gf == ga ? 150 : 75);
	if (mode == "pvp") base = gf > ga ? 400 : (gf == ga ? 200 : 100);
	static const float mult[3] = { 0.8f, 1.0f, 1.5f };
	float m = mode == "pvp" ? 1.0f : mult[settings.difficulty];
	return (int)std::round((base + gf * 25) * m);
}

int GameData::record_result(int gf, int ga, const String &mode) {
	stats.played++;
	stats.gf += gf;
	stats.ga += ga;
	if (gf > ga) stats.won++;
	else if (gf == ga) stats.drawn++;
	else stats.lost++;
	int c = match_reward(gf, ga, mode);
	add_coins(c);
	return c;
}

// ------------------------------------------------------------------ personal photos
void GameData::set_card_photo(int id, const String &path) {
	Card *c = card_by_id(id);
	if (!c) return;
	if (!c->photo.is_empty() && c->photo != path && FileAccess::file_exists(c->photo))
		DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(c->photo));
	c->photo = path;
	save();
}

bool GameData::rename_card(int id, const String &n0) {
	Card *c = card_by_id(id);
	String n = n0.strip_edges().replace("\n", " ").left(22);
	if (!c || n.length() < 2) return false;
	c->name = n;
	save();
	return true;
}

const Card *GameData::photo_card() const {
	for (auto &c : cards)
		if (!c.photo.is_empty()) return &c;
	return nullptr;
}

// ------------------------------------------------------------------ daily
String GameData::today() { return godot::Time::get_singleton()->get_date_string_from_system(); }

bool GameData::refresh_daily() {
	String t = today();
	if (daily.date == t) return false;
	Time *tm = Time::get_singleton();
	if (!daily.last_login.is_empty()) {
		int64_t a = tm->get_unix_time_from_datetime_string(daily.last_login);
		int64_t b = tm->get_unix_time_from_datetime_string(t);
		daily.streak = (b - a <= 86400 + 3600) ? daily.streak + 1 : 1;
	} else {
		daily.streak = 1;
	}
	daily.date = t;
	daily.last_login = t;
	daily.reward_claimed = false;
	daily.missions.clear();
	struct Def { const char *kind, *text; int goal, reward; };
	static const Def pool[] = {
		{ "play", "Сыграйте 2 матча", 2, 250 }, { "win", "Выиграйте матч", 1, 350 }, { "goals", "Забейте 4 гола", 4, 300 },
		{ "clean", "Сыграйте на ноль", 1, 400 }, { "passes", "Сделайте 40 точных передач", 40, 300 }, { "league", "Сыграйте матч лиги", 1, 300 },
		{ "pack", "Откройте пак", 1, 200 }, { "bigwin", "Победите с разницей 2+ мяча", 1, 500 }, { "pvp", "Сыграйте PvP-матч", 1, 450 },
	};
	Rng r((uint64_t)t.hash());
	std::vector<int> idx;
	for (int i = 0; i < 9; i++) idx.push_back(i);
	for (int i = 8; i > 0; i--) std::swap(idx[i], idx[r.irange(0, i)]);
	for (int k = 0; k < 3; k++) {
		const Def &d = pool[idx[k]];
		Mission m;
		m.kind = d.kind;
		m.text = S(d.text);
		m.goal = d.goal;
		m.reward = d.reward;
		daily.missions.push_back(m);
	}
	save();
	return true;
}

int GameData::daily_reward_amount() const {
	int day = ((std::max(1, daily.streak) - 1) % 7) + 1;
	return day == 7 ? 2500 : 250 + day * 100;
}

int GameData::claim_daily_reward() {
	if (daily.reward_claimed) return 0;
	daily.reward_claimed = true;
	int c = daily_reward_amount();
	add_coins(c);
	return c;
}

void GameData::mission_add(const String &kind, int amount) {
	for (auto &m : daily.missions)
		if (m.kind == kind && !m.claimed) m.progress = std::min(m.goal, m.progress + amount);
	save();
}

int GameData::claim_mission(int i) {
	if (i < 0 || i >= (int)daily.missions.size()) return 0;
	Mission &m = daily.missions[i];
	if (!m.done() || m.claimed) return 0;
	m.claimed = true;
	add_coins(m.reward);
	return m.reward;
}

int GameData::missions_ready() const {
	int n = 0;
	for (auto &m : daily.missions) n += (m.done() && !m.claimed) ? 1 : 0;
	return n + (daily.reward_claimed ? 0 : 1);
}

std::vector<LevelUp> GameData::award_xp(const std::vector<std::pair<int, float>> &ratings) {
	std::vector<LevelUp> ups;
	for (auto &pr : ratings) {
		Card *c = card_by_id(pr.first);
		if (!c) continue;
		c->xp += (int)std::round(40.f + std::max(0.f, pr.second - 5.5f) * 30.f);
		int need = 100 + c->lvl * 60;
		while (c->lvl < 10 && c->xp >= need) {
			c->xp -= need;
			c->lvl++;
			c->ovr = std::min(99, c->ovr + 1);
			int *st[7] = { &c->pace, &c->sho, &c->pas, &c->dri, &c->def, &c->phy, &c->gk };
			for (int k = 0; k < 7; k++) *st[k] = std::min(99, *st[k] + 1);
			ups.push_back({ c->name, c->lvl, c->ovr });
			need = 100 + c->lvl * 60;
		}
	}
	save();
	return ups;
}

void GameData::set_team_name(const String &n0) {
	String n = n0.strip_edges().left(20);
	if (n.is_empty()) return;
	team_name = n;
	team_short = n.left(3).to_upper();
	if (league.user < (int)league.teams.size()) {
		league.teams[league.user].name = team_name;
		league.teams[league.user].shrt = team_short;
	}
	save();
}

} // namespace pm
