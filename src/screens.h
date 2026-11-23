#pragma once
#include "ui_kit.h"

#include <godot_cpp/classes/file_dialog.hpp>
#include <godot_cpp/classes/grid_container.hpp>
#include <godot_cpp/classes/line_edit.hpp>

namespace pm {

godot::Node *make_screen(const String &name, const Dictionary &params);

class PMTitle : public PMScreen {
	GDCLASS(PMTitle, PMScreen)
	godot::Label *press = nullptr;
	double t = 0.0;

protected:
	static void _bind_methods() {}

public:
	void _ready() override;
	void _process(double delta) override;
	void _gui_input(const godot::Ref<godot::InputEvent> &e) override;
	void on_back() override;
};

class PMMainMenu : public PMScreen {
	GDCLASS(PMMainMenu, PMScreen)
	int tab = 0;
	godot::HBoxContainer *tabs_row = nullptr;
	godot::VBoxContainer *content = nullptr;
	godot::Control *gift_dot = nullptr;
	godot::Label *gift_count = nullptr;
	void build_tabs();
	void build_content();
	godot::Control *card(const String &kicker, const String &title, const String &sub, Color accent, std::function<void()> cb, float min_h, float progress = -1.f, const String &cta = String());
	void quick_match();
	void daily_popup();
	void missions_popup();
	void refresh_gift();

protected:
	static void _bind_methods();

public:
	void _ready() override;
	void _process(double delta) override;
	void on_back() override;
	void open_missions() { missions_popup(); }
	void open_daily() { daily_popup(); }
};

class PMSquadScreen : public PMScreen {
	GDCLASS(PMSquadScreen, PMScreen)
	int selected = -1;
	godot::HBoxContainer *body = nullptr;
	void build();
	void place_slots(godot::Control *pitch);
	godot::Control *slot_chip(int i, const String &pos);

protected:
	static void _bind_methods() {}

public:
	void _ready() override;
};

class PMPacksScreen : public PMScreen {
	GDCLASS(PMPacksScreen, PMScreen)
	int tab = 0;
	godot::VBoxContainer *content = nullptr;
	void build();
	void shop();
	void collection();
	void open(const String &id);
	void card_dialog(int id);

protected:
	static void _bind_methods() {}

public:
	void _ready() override;
};

class PMLeagueScreen : public PMScreen {
	GDCLASS(PMLeagueScreen, PMScreen)
	godot::HBoxContainer *body = nullptr;
	void build();
	void simulate();

protected:
	static void _bind_methods() {}

public:
	void _ready() override;
};

class PMPvpScreen : public PMScreen {
	GDCLASS(PMPvpScreen, PMScreen)
	godot::VBoxContainer *body = nullptr;
	godot::Label *status = nullptr;
	String mode;
	int minutes_sel = 0;
	void build();
	void leave();

protected:
	static void _bind_methods() {}

public:
	void _ready() override;
	void on_back() override;
	void _on_status(const String &t);
	void _on_lobby();
};

class PMSettingsScreen : public PMScreen {
	GDCLASS(PMSettingsScreen, PMScreen)

protected:
	static void _bind_methods() {}

public:
	void _ready() override;
};

class PMEditScreen : public PMScreen {
	GDCLASS(PMEditScreen, PMScreen)
	godot::VBoxContainer *list = nullptr, *form = nullptr;
	std::vector<String> ids;
	int current = 0;
	std::vector<Card> roster;
	String shirt, shorts;
	godot::LineEdit *name_edit = nullptr, *short_edit = nullptr;
	std::vector<godot::LineEdit *> player_edits, num_edits;
	bool pending_refresh = false;
	std::function<void()> refresh_fn;
	void build_list();
	void build_form();
	void save_current();

protected:
	static void _bind_methods();

public:
	void _ready() override;
	void _process(double delta) override;
	void on_back() override;
};

class PMPatchScreen : public PMScreen {
	GDCLASS(PMPatchScreen, PMScreen)
	godot::VBoxContainer *list = nullptr;
	void build_list();
	void import_dialog();
	void confirm_activate(const String &id, const String &name);

protected:
	static void _bind_methods();

public:
	void _ready() override;
	void on_back() override;
	void _on_file(const String &path);
};

class PMResultScreen : public PMScreen {
	GDCLASS(PMResultScreen, PMScreen)

protected:
	static void _bind_methods() {}

public:
	void _ready() override;
	void on_back() override;
};

} // namespace pm
