#include "app.h"
#include "game_data.h"
#include "match/charlib.h"
#include "match/hud.h"
#include "ui_kit.h"
#include "match/match.h"
#include "net.h"
#include "screens.h"
#include "sfx.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

static void initialize_pm(ModuleInitializationLevel level) {
	if (level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
	GDREGISTER_CLASS(pm::PMSfx);
	GDREGISTER_CLASS(pm::PMNet);
	GDREGISTER_CLASS(pm::PMApp);
	GDREGISTER_CLASS(pm::PMScreen);
	GDREGISTER_CLASS(pm::PMTitle);
	GDREGISTER_CLASS(pm::PMMainMenu);
	GDREGISTER_CLASS(pm::PMSquadScreen);
	GDREGISTER_CLASS(pm::PMPacksScreen);
	GDREGISTER_CLASS(pm::PMLeagueScreen);
	GDREGISTER_CLASS(pm::PMPvpScreen);
	GDREGISTER_CLASS(pm::PMSettingsScreen);
	GDREGISTER_CLASS(pm::PMPatchScreen);
	GDREGISTER_CLASS(pm::PMResultScreen);
	GDREGISTER_CLASS(pm::PMHud);
	GDREGISTER_CLASS(pm::PMMatch);
}

static void uninitialize_pm(ModuleInitializationLevel level) {
	if (level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
	pm::GameData::get().shutdown();
	pm::CharLib::get().shutdown();
	pm::kit::release_fonts();
}

extern "C" {
GDExtensionBool GDE_EXPORT pitchmasters_init(GDExtensionInterfaceGetProcAddress get_proc, const GDExtensionClassLibraryPtr lib, GDExtensionInitialization *init) {
	godot::GDExtensionBinding::InitObject obj(get_proc, lib, init);
	obj.register_initializer(initialize_pm);
	obj.register_terminator(uninitialize_pm);
	obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
	return obj.init();
}
}
