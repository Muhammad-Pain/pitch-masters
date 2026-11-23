#pragma once
// Game state: cards, squad, league, settings, saves and patches.

#include "util.h"

#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <array>
#include <map>
#include <vector>

namespace pm {

using godot::Array;
using godot::Dictionary;
using godot::Ref;
using godot::Texture2D;

struct Card {
	int id = 0;
	String name;
	String pos = "CMF";
	int ovr = 60, pace = 60, sho = 60, pas = 60, dri = 60, def = 60, phy = 60, gk = 20, num = 1;
	int lvl = 0, xp = 0;
	String photo; // user://photos/... — the player's own picture on the card
	Dictionary to_dict() const;
	static Card from_dict(const Dictionary &d);
	int stat(const String &k) const;
};

// A player prepared for a match (card + the slot he plays in).
struct MatchPlayer {
	Card c;
	String slot_pos;
};

struct MatchTeam {
	String name, shrt, logo;
	Color shirt = Color(0.1f, 0.3f, 0.9f), shorts = Color(1, 1, 1);
	String formation = "4-3-3";
	int line = 1, press = 1, style = 1, ovr = 60;
	std::vector<MatchPlayer> players;
	Dictionary to_dict() const;
	static MatchTeam from_dict(const Dictionary &d); // sanitising: safe for network input
};

struct TeamInfo {
	String id, name, shrt, shirt = "#1d4ed8", shorts = "#ffffff", logo; // logo: absolute path or ""
	int strength = 65;
	bool user = false;
	std::vector<Card> players; // optional (from a patch)
	Dictionary to_dict() const;
	static TeamInfo from_dict(const Dictionary &d);
};

struct TableRow {
	int p = 0, w = 0, d = 0, l = 0, gf = 0, ga = 0, pts = 0;
};

struct League {
	String name;
	int season = 1, user = 0, round = 0;
	std::vector<TeamInfo> teams;
	std::vector<std::vector<std::array<int, 2>>> fixtures;
	std::vector<TableRow> table;
	std::vector<std::array<int, 4>> last;
};

struct Settings {
	int difficulty = 1, minutes = 6, camera = 1;
	bool sound = true, replays = true;
	int controls = 1;  // 0 buttons, 1 buttons + gestures, 2 gestures (flick)
	int quality = -1;  // -1 auto, 0 low, 1 medium, 2 high, 3 ultra
	int fps = 60;
	String last_ip = "192.168.43.1";
	String active_patch;
};

struct Squad {
	String formation = "4-3-3";
	std::vector<int> slots;
	int line = 1, press = 1, style = 1;
};

struct Stats {
	int played = 0, won = 0, drawn = 0, lost = 0, gf = 0, ga = 0;
};

struct Slot {
	const char *pos;
	float x, z;
};

struct PatchInfo {
	String id, name, author, version, description, dir;
	int teams = 0;
	bool has_logos = false;
};

struct PatchData { // contents of the active patch
	bool active = false;
	String id, name, league_name, stadium_name;
	std::vector<TeamInfo> teams;
	std::vector<String> boards;
	String menu_background; // absolute path
	TeamInfo my_team;
	bool has_my_team = false;
};

struct Mission {
	String kind, text;
	int goal = 1, progress = 0, reward = 200;
	bool claimed = false;
	bool done() const { return progress >= goal; }
};

struct Daily {
	String date, last_login;
	int streak = 0;
	bool reward_claimed = false;
	std::vector<Mission> missions;
};

struct LevelUp {
	String name;
	int lvl, ovr;
};

struct PackDef {
	const char *id, *name, *desc;
	int price, count, guarantee;
	float odds[4][3];
};

class GameData {
public:
	static GameData &get();

	int coins = 3000, next_id = 1, packs_opened = 0;
	String team_name, team_short, shirt = "#1d4ed8", shorts = "#ffffff";
	std::vector<Card> cards;
	Squad squad;
	Settings settings;
	Stats stats;
	League league;
	PatchData patch;
	Daily daily;
	Dictionary edits; // editor: team id -> {name, shrt, shirt, shorts, players[]}
	int coins_rev = 0; // bumped whenever coins change (UI polls it)

	// ---- constants
	static const std::vector<String> &formation_names();
	static const std::vector<Slot> &formation(const String &name);
	static const std::vector<String> &pos_list();
	static String group(const String &pos);
	static String pos_ru(const String &pos);
	static String stat_ru(const String &k);
	static const std::vector<PackDef> &packs();
	static const char *difficulty_name(int i);
	static const std::vector<int> &minutes_options();

	// ---- lifecycle
	void load();
	void save();
	void new_game();
	void reset_progress();

	// ---- coins
	void add_coins(int n);
	bool spend(int n);

	// ---- cards
	static String rarity(int ovr);
	static Color rarity_color(const String &r);
	static String rarity_name(const String &r);
	static int sell_value(const Card &c);
	Card make_card(int ovr, String pos, Rng &r) const;
	Card &add_card(Card c);
	Card *card_by_id(int id);
	void set_card_photo(int id, const String &path);
	bool rename_card(int id, const String &name);
	const Card *photo_card() const; // first card with a personal photo, if any
	int eff_quality() const;        // resolved graphics preset 0..3
	bool is_starter(int id) const;
	int sell_card(int id);
	std::vector<Card> open_pack(const String &id);

	// ---- squad
	static float compat(const String &card_pos, const String &slot_pos);
	std::vector<String> slot_positions(const String &formation) const;
	int effective_ovr(const Card &c, const String &slot_pos) const;
	int team_ovr();
	void auto_squad();
	void set_slot(int slot, int card_id);
	MatchTeam build_my_team();
	MatchTeam build_ai_team(const TeamInfo &t, int bonus = 0);

	// ---- league
	void new_league(int season = 1);
	bool league_finished() const;
	std::array<int, 2> league_user_fixture() const;
	std::array<int, 2> sim_score(int sa, int sb);
	void league_record_user(int gf, int ga);
	std::vector<int> league_standings() const;
	int league_user_place() const;
	static int league_reward(int place);
	int league_next_season();
	MatchTeam league_opponent_team(bool &user_home);

	// ---- daily rewards & missions
	static String today();
	bool refresh_daily();          // true when a new day started
	int daily_reward_amount() const;
	int claim_daily_reward();
	void mission_add(const String &kind, int amount);
	int claim_mission(int i);
	int missions_ready() const;
	std::vector<LevelUp> award_xp(const std::vector<std::pair<int, float>> &ratings);

	// ---- results
	int match_reward(int gf, int ga, const String &mode) const;
	int record_result(int gf, int ga, const String &mode);
	void set_team_name(const String &n);

	// ---- patches (patch.cpp)
	String patches_dir() const;
	std::vector<PatchInfo> list_patches() const;
	String install_patch(const String &path, String &out_id); // returns error text or ""
	bool remove_patch(const String &id);
	String activate_patch(const String &id); // "" = default teams; returns error text or ""
	void load_active_patch();
	String export_template(String &out_path) const;
	String install_example_patch(String &out_id);
	Ref<Texture2D> logo_texture(const String &path);
	std::vector<TeamInfo> base_team_pool() const;
	void apply_edits(TeamInfo &t) const;
	std::vector<Card> team_roster(const TeamInfo &t) const;
	void save_team_edit(const String &id, const Dictionary &e);
	void reset_team_edit(const String &id);
	std::vector<String> board_texts() const;
	void shutdown() { logo_cache.clear(); patch = PatchData(); }

private:
	Dictionary to_dict() const;
	void from_dict(const Dictionary &d);
	void validate_squad();
	std::map<String, Ref<Texture2D>> logo_cache;
};

} // namespace pm
