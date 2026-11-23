#pragma once
#include "util.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace pm {

class PMNet;
class PMSfx;
struct Footballer;

// Root node: screen manager, 3D stadium backdrop for menus, owns Net and Sfx.
class PMApp : public godot::Node {
	GDCLASS(PMApp, godot::Node)

	godot::Node *current = nullptr;
	godot::Node3D *backdrop = nullptr;
	godot::Camera3D *bd_cam = nullptr;
	godot::TextureRect *bg_image = nullptr;
	double bd_t = 0.0;
	godot::Node3D *showcase = nullptr;
	Footballer *hero = nullptr;
	float hero_t = 0.f, hero_next = 7.f;
	bool hero_mode = false, hero_snap = false, dbg_cam = false;
	godot::Vector3 dbg_eye, dbg_look;
	void build_showcase();
	void free_hero();
	bool started = false;

protected:
	static void _bind_methods();

public:
	static PMApp *singleton;
	String current_name;
	PMNet *net = nullptr;
	PMSfx *sfx = nullptr;

	static void go(const String &name, const godot::Dictionary &params = godot::Dictionary());
	static void apply_quality();
	void show_screen(const String &name, const godot::Dictionary &params);
	godot::Node *get_current() const { return current; }
	void rebuild_backdrop();
	// debug / test helpers (bound to scripting)
	godot::Dictionary debug_info() const;
	bool debug_photo(int card_id, const String &path, bool open_crop);
	void debug_cam(godot::Vector3 eye, godot::Vector3 look);
	godot::Array debug_list_patches() const;
	String debug_install_patch(const String &path);
	String debug_activate_patch(const String &id);
	String debug_install_example();
	void _ready() override;
	void _process(double delta) override;
	void _notification(int p_what);
	~PMApp();
};

} // namespace pm
