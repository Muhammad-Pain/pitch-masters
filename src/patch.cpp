// Patch system: .pmpatch (zip with patch.json + images) or plain .json files.
// A patch can rename and recolour clubs, set strengths and full squads, add logos,
// change ad boards, league/stadium names, the menu background and your own club.

#include "game_data.h"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/zip_reader.hpp>
#include <godot_cpp/classes/zip_packer.hpp>

using namespace godot;

namespace pm {

static const char *DEFAULT_TEAMS[][4] = {
	{ "Памир Юнайтед", "ПАМ", "#1f4fd1", "#ffffff" }, { "Золотые Львы", "ЗЛВ", "#f2b705", "#1a1a1a" },
	{ "Красный Маяк", "КРМ", "#d62839", "#ffffff" }, { "Стальные Волки", "СТВ", "#6b7280", "#111827" },
	{ "Горные Орлы", "ГОР", "#0f766e", "#f1f5f9" }, { "Речной Порт", "РПТ", "#0ea5e9", "#0c4a6e" },
	{ "Звезда Востока", "ЗВВ", "#7c3aed", "#facc15" }, { "Северный Ветер", "СВВ", "#f8fafc", "#1e3a8a" },
	{ "Атлас Сити", "АТС", "#ea580c", "#1c1917" }, { "Шёлковый Путь", "ШПТ", "#16a34a", "#ffffff" },
};

String GameData::patches_dir() const { return "user://patches"; }

std::vector<TeamInfo> GameData::base_team_pool() const {
	std::vector<TeamInfo> out;
	if (patch.active)
		for (auto &t : patch.teams) out.push_back(t);
	int i = 0;
	while (out.size() < 7 && i < 10) {
		TeamInfo t;
		t.id = String("default_") + I(i);
		t.name = S(DEFAULT_TEAMS[i][0]);
		t.shrt = S(DEFAULT_TEAMS[i][1]);
		t.shirt = DEFAULT_TEAMS[i][2];
		t.shorts = DEFAULT_TEAMS[i][3];
		t.strength = 66;
		bool dup = false;
		for (auto &o : out) dup = dup || o.name == t.name;
		if (!dup) out.push_back(t);
		i++;
	}
	if (!patch.active) {
		out.clear();
		for (int k = 0; k < 10; k++) {
			TeamInfo t;
			t.id = String("default_") + I(k);
			t.name = S(DEFAULT_TEAMS[k][0]);
			t.shrt = S(DEFAULT_TEAMS[k][1]);
			t.shirt = DEFAULT_TEAMS[k][2];
			t.shorts = DEFAULT_TEAMS[k][3];
			out.push_back(t);
		}
	}
	for (auto &t : out) apply_edits(t);
	return out;
}

std::vector<String> GameData::board_texts() const {
	if (patch.active && !patch.boards.empty()) return patch.boards;
	return { "PITCH MASTERS", S("ФУТБОЛ 27"), S("СУПЕРЛИГА"), "PITCH MASTERS", S("ГОЛ!"), S("ЧЕМПИОНАТ") };
}

Ref<Texture2D> GameData::logo_texture(const String &path) {
	if (path.is_empty()) return Ref<Texture2D>();
	auto it = logo_cache.find(path);
	if (it != logo_cache.end()) return it->second;
	Ref<Texture2D> tex;
	if (FileAccess::file_exists(path)) {
		Ref<Image> img;
		img.instantiate();
		if (img->load(path) == OK && img->get_width() > 0) {
			if (img->get_width() > 256 || img->get_height() > 256) img->resize(128, 128 * img->get_height() / img->get_width());
			tex = ImageTexture::create_from_image(img);
		}
	}
	logo_cache[path] = tex;
	return tex;
}

static Variant read_json(const String &path) {
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
	if (f.is_null()) return Variant();
	return JSON::parse_string(f->get_as_text());
}

static bool safe_rel(const String &p) {
	if (p.is_empty() || p.begins_with("/") || p.contains("..") || p.contains(":") || p.contains("\\")) return false;
	return true;
}

static String patch_id_for(const Dictionary &d) {
	String base = String(d.get("name", "patch")) + "|" + String(d.get("version", "1"));
	uint32_t h = (uint32_t)base.hash();
	return String("patch_") + String::num_uint64(h, 16);
}

std::vector<PatchInfo> GameData::list_patches() const {
	std::vector<PatchInfo> out;
	Ref<DirAccess> d = DirAccess::open(patches_dir());
	if (d.is_null()) return out;
	PackedStringArray dirs = d->get_directories();
	for (int i = 0; i < dirs.size(); i++) {
		String dir = patches_dir() + "/" + dirs[i];
		Variant v = read_json(dir + "/patch.json");
		if (v.get_type() != Variant::DICTIONARY) continue;
		Dictionary j = v;
		PatchInfo pi;
		pi.id = dirs[i];
		pi.dir = dir;
		pi.name = String(j.get("name", dirs[i])).left(40);
		pi.author = String(j.get("author", "")).left(40);
		pi.version = String(j.get("version", "")).left(16);
		pi.description = String(j.get("description", "")).left(200);
		Array teams = j.get("teams", Array());
		pi.teams = teams.size();
		for (int k = 0; k < teams.size(); k++)
			if (teams[k].get_type() == Variant::DICTIONARY && !String(Dictionary(teams[k]).get("logo", "")).is_empty()) pi.has_logos = true;
		out.push_back(pi);
	}
	return out;
}

static bool validate_patch_dict(const Dictionary &j, String &err) {
	if (!j.has("name")) {
		err = S("В patch.json нет поля \"name\"");
		return false;
	}
	Variant t = j.get("teams", Array());
	if (t.get_type() != Variant::ARRAY) {
		err = S("Поле \"teams\" должно быть списком");
		return false;
	}
	if (Array(t).size() > 300) {
		err = S("Слишком много клубов в патче (максимум 300)");
		return false;
	}
	return true;
}

String GameData::install_patch(const String &path, String &out_id) {
	if (!FileAccess::file_exists(path)) return S("Файл не найден: ") + path;
	String low = path.to_lower();
	DirAccess::make_dir_recursive_absolute(patches_dir());
	if (low.ends_with(".json")) {
		Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
		if (f.is_null()) return S("Не удалось открыть файл");
		String text = f->get_as_text();
		Variant v = JSON::parse_string(text);
		if (v.get_type() != Variant::DICTIONARY) return S("Файл не похож на патч: ошибка в JSON");
		String err;
		if (!validate_patch_dict(v, err)) return err;
		out_id = patch_id_for(v);
		String dir = patches_dir() + "/" + out_id;
		DirAccess::make_dir_recursive_absolute(dir);
		Ref<FileAccess> w = FileAccess::open(dir + "/patch.json", FileAccess::WRITE);
		if (w.is_null()) return S("Не удалось сохранить патч");
		w->store_string(text);
		return "";
	}
	// zip / .pmpatch
	Ref<ZIPReader> z;
	z.instantiate();
	if (z->open(path) != OK) return S("Не удалось открыть архив. Нужен .pmpatch, .zip или .json");
	PackedStringArray files = z->get_files();
	String prefix;
	bool found = false;
	for (int i = 0; i < files.size(); i++) {
		String f = files[i];
		if (f == "patch.json" || f.ends_with("/patch.json")) {
			String pre = f.substr(0, f.length() - String("patch.json").length());
			if (!found || pre.length() < prefix.length()) prefix = pre;
			found = true;
		}
	}
	if (!found) {
		z->close();
		return S("В архиве нет файла patch.json");
	}
	PackedByteArray jb = z->read_file(prefix + "patch.json");
	Variant v = JSON::parse_string(jb.get_string_from_utf8());
	if (v.get_type() != Variant::DICTIONARY) {
		z->close();
		return S("Ошибка в patch.json внутри архива");
	}
	String err;
	if (!validate_patch_dict(v, err)) {
		z->close();
		return err;
	}
	out_id = patch_id_for(v);
	String dir = patches_dir() + "/" + out_id;
	DirAccess::make_dir_recursive_absolute(dir);
	int64_t total = 0;
	for (int i = 0; i < files.size(); i++) {
		String f = files[i];
		if (!f.begins_with(prefix) || f.ends_with("/")) continue;
		String rel = f.substr(prefix.length());
		String ext = rel.get_extension().to_lower();
		if (!safe_rel(rel)) continue;
		if (ext != "json" && ext != "png" && ext != "jpg" && ext != "jpeg" && ext != "webp") continue;
		PackedByteArray data = z->read_file(f);
		total += data.size();
		if (total > 60 * 1024 * 1024) break;
		String out = dir + "/" + rel;
		DirAccess::make_dir_recursive_absolute(out.get_base_dir());
		Ref<FileAccess> w = FileAccess::open(out, FileAccess::WRITE);
		if (w.is_valid()) w->store_buffer(data);
	}
	z->close();
	return "";
}

static void remove_recursive(const String &dir) {
	Ref<DirAccess> d = DirAccess::open(dir);
	if (d.is_null()) return;
	PackedStringArray sub = d->get_directories();
	for (int i = 0; i < sub.size(); i++) remove_recursive(dir.path_join(sub[i]));
	PackedStringArray files = d->get_files();
	for (int i = 0; i < files.size(); i++) DirAccess::remove_absolute(dir.path_join(files[i]));
	DirAccess::remove_absolute(dir);
}

bool GameData::remove_patch(const String &id) {
	if (!safe_rel(id)) return false;
	if (settings.active_patch == id) activate_patch("");
	remove_recursive(patches_dir() + "/" + id);
	return true;
}

void GameData::load_active_patch() {
	patch = PatchData();
	logo_cache.clear();
	if (settings.active_patch.is_empty() || !safe_rel(settings.active_patch)) return;
	String dir = patches_dir() + "/" + settings.active_patch;
	Variant v = read_json(dir + "/patch.json");
	if (v.get_type() != Variant::DICTIONARY) {
		settings.active_patch = "";
		return;
	}
	Dictionary j = v;
	patch.active = true;
	patch.id = settings.active_patch;
	patch.name = String(j.get("name", "")).left(40);
	patch.league_name = String(j.get("league_name", "")).left(30);
	patch.stadium_name = String(j.get("stadium_name", "")).left(30);
	auto resolve = [&](const Variant &rel) -> String {
		String r = rel.get_type() == Variant::STRING ? String(rel) : String();
		if (!safe_rel(r)) return "";
		String p = dir + "/" + r;
		return FileAccess::file_exists(p) ? p : String();
	};
	Array boards = j.get("boards", Array());
	for (int i = 0; i < boards.size() && i < 12; i++) patch.boards.push_back(String(boards[i]).left(24));
	patch.menu_background = resolve(j.get("menu_background", ""));
	Array teams = j.get("teams", Array());
	for (int i = 0; i < teams.size() && i < 300; i++) {
		if (teams[i].get_type() != Variant::DICTIONARY) continue;
		Dictionary td = teams[i];
		TeamInfo t = TeamInfo::from_dict(td);
		t.logo = resolve(td.get("logo", ""));
		if (t.id.is_empty()) t.id = String("p") + I(i);
		t.user = false;
		patch.teams.push_back(t);
	}
	if (j.get("my_team", Variant()).get_type() == Variant::DICTIONARY) {
		Dictionary md = j["my_team"];
		patch.my_team = TeamInfo::from_dict(md);
		patch.my_team.logo = resolve(md.get("logo", ""));
		patch.has_my_team = true;
	}
}

String GameData::activate_patch(const String &id) {
	settings.active_patch = id;
	load_active_patch();
	if (!id.is_empty() && !patch.active) return S("Патч повреждён или удалён");
	if (patch.active && patch.has_my_team) {
		team_name = patch.my_team.name;
		team_short = patch.my_team.shrt;
		shirt = patch.my_team.shirt;
		shorts = patch.my_team.shorts;
	}
	new_league(league.season);
	save();
	return "";
}

// ------------------------------------------------------------------ template & example
static Dictionary player_dict(const Card &c) {
	Dictionary d;
	d["name"] = c.name; d["pos"] = c.pos; d["ovr"] = c.ovr; d["num"] = c.num;
	d["pace"] = c.pace; d["sho"] = c.sho; d["pas"] = c.pas; d["dri"] = c.dri; d["def"] = c.def; d["phy"] = c.phy; d["gk"] = c.gk;
	return d;
}

static Dictionary template_dict(bool example) {
	GameData &g = GameData::get();
	Dictionary d;
	d["format"] = "pitchmasters-patch-1";
	d["name"] = example ? S("Пример: Сезон 2027") : S("Мой патч");
	d["author"] = example ? String("Pitch Masters") : S("Ваше имя");
	d["version"] = "1.0";
	d["description"] = example ? S("Пример патча: 10 выдуманных клубов с эмблемами, составами, щитами и названием лиги.")
							   : S("Описание патча");
	d["league_name"] = example ? S("Премьер-лига 2027") : S("Название лиги");
	d["stadium_name"] = example ? S("Арена Памир") : S("Название стадиона");
	Array boards;
	for (const char *b : { "СЕЗОН 2027", "PITCH MASTERS", "ПРЕМЬЕР-ЛИГА", "ФУТБОЛ", "ГОЛ!", "ФИНАЛ" }) boards.push_back(S(b));
	d["boards"] = boards;
	Dictionary me;
	me["name"] = example ? S("ФК Душанбе") : S("Мой клуб");
	me["short"] = example ? S("ДУШ") : S("МОЙ");
	me["shirt"] = "#0b3d91";
	me["shorts"] = "#ffffff";
	me["logo"] = example ? "logos/me.png" : "";
	d["my_team"] = me;
	static const char *ex[][5] = {
		{ "Памир Юнайтед", "ПАМ", "#1f4fd1", "#ffffff", "82" }, { "Золотые Львы", "ЗЛВ", "#f2b705", "#1a1a1a", "80" },
		{ "Красный Маяк", "КРМ", "#d62839", "#ffffff", "78" }, { "Стальные Волки", "СТВ", "#6b7280", "#111827", "76" },
		{ "Горные Орлы", "ГОР", "#0f766e", "#f1f5f9", "75" }, { "Речной Порт", "РПТ", "#0ea5e9", "#0c4a6e", "73" },
		{ "Звезда Востока", "ЗВВ", "#7c3aed", "#facc15", "72" }, { "Северный Ветер", "СВВ", "#f8fafc", "#1e3a8a", "70" },
		{ "Атлас Сити", "АТС", "#ea580c", "#1c1917", "69" }, { "Шёлковый Путь", "ШПТ", "#16a34a", "#ffffff", "68" },
	};
	Array teams;
	int n = example ? 10 : 2;
	Rng r(2027);
	for (int i = 0; i < n; i++) {
		Dictionary t;
		t["id"] = String("club_") + I(i + 1);
		t["name"] = example ? S(ex[i][0]) : S("Клуб ") + I(i + 1);
		t["short"] = example ? S(ex[i][1]) : String("CL") + I(i + 1);
		t["shirt"] = ex[i][2];
		t["shorts"] = ex[i][3];
		t["strength"] = String(ex[i][4]).to_int();
		t["logo"] = example ? String("logos/club_") + I(i + 1) + ".png" : String("logos/club_1.png");
		Array ps;
		const char *lineup[] = { "GK", "LB", "CB", "CB", "RB", "DMF", "CMF", "AMF", "LWF", "CF", "RWF", "GK", "CB", "CMF", "CF" };
		int count = example ? 15 : 2;
		for (int k = 0; k < count; k++) {
			Card c = g.make_card(String(ex[i][4]).to_int() + r.irange(-5, 4), lineup[k], r);
			ps.push_back(player_dict(c));
		}
		t["players"] = ps;
		teams.push_back(t);
	}
	d["teams"] = teams;
	if (!example) {
		Dictionary help;
		help["teams"] = S("Список клубов. strength — сила клуба 40..95. players — необязательно: имя, позиция (GK CB LB RB DMF CMF AMF LMF RMF LWF RWF CF), ovr и параметры.");
		help["logo"] = S("Путь к PNG внутри архива .pmpatch (например logos/club_1.png). Квадрат 128x128.");
		help["boards"] = S("Тексты на рекламных щитах вокруг поля.");
		help["menu_background"] = S("Необязательно: PNG-фон главного меню, например images/menu.png.");
		help["pack"] = S("Сложите patch.json и папку logos в zip-архив и переименуйте его в .pmpatch (или оставьте .zip).");
		d["_help"] = help;
	}
	return d;
}

// Procedural club badge (shield with a stripe) so the example patch has real image files.
static PackedByteArray badge_png(const Color &a, const Color &b, int style) {
	const int N = 128;
	Ref<Image> img = Image::create(N, N, false, Image::FORMAT_RGBA8);
	img->fill(Color(0, 0, 0, 0));
	for (int y = 0; y < N; y++) {
		for (int x = 0; x < N; x++) {
			float u = (x + 0.5f) / N * 2.f - 1.f;
			float v = (y + 0.5f) / N;
			float half = v < 0.55f ? 0.82f : 0.82f * std::sqrt(std::max(0.f, 1.f - std::pow((v - 0.55f) / 0.45f, 2.f)));
			bool inside = v > 0.06f && v < 0.99f && std::fabs(u) < half;
			if (!inside) continue;
			bool edge = std::fabs(u) > half - 0.09f || v < 0.12f;
			Color c = a;
			if (style == 0 && std::fabs(u - (v - 0.5f)) < 0.22f) c = b;
			if (style == 1 && std::fmod(std::fabs(u) * 3.f + 0.0f, 1.f) < 0.4f) c = b;
			if (style == 2 && v > 0.42f && v < 0.62f) c = b;
			if (style == 3 && u < 0) c = b;
			float d = std::sqrt(u * u + (v - 0.5f) * (v - 0.5f) * 4.f);
			if (d < 0.22f) c = Color(1, 1, 1);
			if (d < 0.14f) c = a.darkened(0.2f);
			if (edge) c = Color(0.06f, 0.07f, 0.1f);
			img->set_pixel(x, y, c);
		}
	}
	return img->save_png_to_buffer();
}

String GameData::export_template(String &out_path) const {
	Dictionary d = template_dict(false);
	String text = JSON::stringify(d, "  ", false);
	String dl = OS::get_singleton()->get_system_dir(OS::SYSTEM_DIR_DOWNLOADS);
	std::vector<String> targets;
	if (!dl.is_empty()) targets.push_back(dl + "/PitchMasters_patch_template.json");
	targets.push_back("user://PitchMasters_patch_template.json");
	for (auto &t : targets) {
		Ref<FileAccess> f = FileAccess::open(t, FileAccess::WRITE);
		if (f.is_valid()) {
			f->store_string(text);
			out_path = t.begins_with("user://") ? OS::get_singleton()->get_user_data_dir() + t.substr(6) : t;
			return "";
		}
	}
	return S("Не удалось сохранить шаблон");
}

String GameData::install_example_patch(String &out_id) {
	Dictionary d = template_dict(true);
	String tmp = "user://example_2027.pmpatch";
	Ref<ZIPPacker> zp;
	zp.instantiate();
	if (zp->open(tmp) != OK) return S("Не удалось создать пример");
	zp->start_file("patch.json");
	zp->write_file(JSON::stringify(d, "  ", false).to_utf8_buffer());
	zp->close_file();
	Array teams = d["teams"];
	for (int i = 0; i < teams.size(); i++) {
		Dictionary t = teams[i];
		zp->start_file(t["logo"]);
		zp->write_file(badge_png(html(t["shirt"]), html(t["shorts"]), i % 4));
		zp->close_file();
	}
	zp->start_file("logos/me.png");
	zp->write_file(badge_png(html("#0b3d91"), html("#ffffff"), 2));
	zp->close_file();
	zp->close();
	String err = install_patch(tmp, out_id);
	DirAccess::remove_absolute(tmp);
	return err;
}

} // namespace pm
