#include "app.h"
#include "game_data.h"
#include "match/stadium.h"
#include "match/footballer.h"
#include "match/charlib.h"
#include "net.h"
#include "screens.h"
#include "sfx.h"
#include "ui_kit.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/multiplayer_api.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture2d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/spot_light3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace pm {

PMApp *PMApp::singleton = nullptr;

void PMApp::_bind_methods() {
	ClassDB::bind_method(D_METHOD("show_screen", "name", "params"), &PMApp::show_screen);
	ClassDB::bind_method(D_METHOD("get_current"), &PMApp::get_current);
	ClassDB::bind_method(D_METHOD("debug_info"), &PMApp::debug_info);
	ClassDB::bind_method(D_METHOD("debug_photo", "card_id", "path", "open_crop"), &PMApp::debug_photo);
	ClassDB::bind_method(D_METHOD("debug_cam", "eye", "look"), &PMApp::debug_cam);
	ClassDB::bind_method(D_METHOD("debug_list_patches"), &PMApp::debug_list_patches);
	ClassDB::bind_method(D_METHOD("debug_install_patch", "path"), &PMApp::debug_install_patch);
	ClassDB::bind_method(D_METHOD("debug_activate_patch", "id"), &PMApp::debug_activate_patch);
	ClassDB::bind_method(D_METHOD("debug_install_example"), &PMApp::debug_install_example);
}

void PMApp::debug_cam(Vector3 eye, Vector3 look) {
	dbg_cam = true;
	dbg_eye = eye;
	dbg_look = look;
}

bool PMApp::debug_photo(int card_id, const String &path, bool open_crop) {
	Ref<Image> img = kit::image_from_file(path);
	if (img.is_null()) return false;
	if (open_crop) {
		Control *host = Object::cast_to<Control>(current);
		if (!host) return false;
		kit::crop_photo(host, card_id, img, nullptr);
		return true;
	}
	int side = std::min(img->get_width(), img->get_height());
	Ref<Image> sq = img->get_region(Rect2i((img->get_width() - side) / 2, (img->get_height() - side) / 2, side, side));
	String p = kit::save_photo(card_id, sq);
	if (p.is_empty()) return false;
	GameData::get().set_card_photo(card_id, p);
	return true;
}

Dictionary PMApp::debug_info() const {
	GameData &g = GameData::get();
	Dictionary d;
	d["coins"] = g.coins;
	if (!g.cards.empty()) d["first_card_id"] = g.squad.slots.empty() ? g.cards[0].id : g.squad.slots[g.squad.slots.size() - 1];
	d["cards"] = (int)g.cards.size();
	d["team_name"] = g.team_name;
	d["league_name"] = g.league.name;
	d["round"] = g.league.round;
	d["patch_active"] = g.patch.active;
	d["patch_name"] = g.patch.name;
	d["team_ovr"] = g.team_ovr();
	if (net) {
		d["net_active"] = net->active;
		d["net_connected"] = net->connected;
		d["net_remote"] = net->has_remote;
		d["net_status"] = net->last_status;
		d["net_peers"] = get_multiplayer()->get_peers().size();
		d["net_unique"] = get_multiplayer()->get_unique_id();
		d["fps_frames"] = (int64_t)Engine::get_singleton()->get_process_frames();
	}
	Array teams;
	for (auto &t : g.league.teams) {
		Dictionary td;
		td["name"] = t.name;
		td["strength"] = t.strength;
		td["logo"] = t.logo;
		td["players"] = (int)t.players.size();
		teams.push_back(td);
	}
	d["teams"] = teams;
	Array boards;
	for (auto &b : g.board_texts()) boards.push_back(b);
	d["boards"] = boards;
	return d;
}

Array PMApp::debug_list_patches() const {
	Array a;
	for (auto &p : GameData::get().list_patches()) {
		Dictionary d;
		d["id"] = p.id;
		d["name"] = p.name;
		d["teams"] = p.teams;
		d["has_logos"] = p.has_logos;
		a.push_back(d);
	}
	return a;
}

String PMApp::debug_install_patch(const String &path) {
	String id;
	String err = GameData::get().install_patch(path, id);
	return err.is_empty() ? id : String("ERROR: ") + err;
}

String PMApp::debug_activate_patch(const String &id) {
	String err = GameData::get().activate_patch(id);
	rebuild_backdrop();
	return err;
}

String PMApp::debug_install_example() {
	String id;
	String err = GameData::get().install_example_patch(id);
	return err.is_empty() ? id : String("ERROR: ") + err;
}

void PMApp::go(const String &name, const Dictionary &params) {
	if (singleton) singleton->show_screen(name, params);
}

void PMApp::_ready() {
	singleton = this;
	GameData::get().load();
	sfx = memnew(PMSfx);
	sfx->set_name("Sfx");
	add_child(sfx);
	net = memnew(PMNet);
	net->set_name("Net");
	add_child(net);
	net->on_start_match = [](const Dictionary &p) { PMApp::go("match", p); };
	get_viewport()->set_embedding_subwindows(true);
	apply_quality();
	rebuild_backdrop();
	show_screen("title", Dictionary());
}

PMApp::~PMApp() { free_hero(); }

void PMApp::apply_quality() {
	if (!singleton || !singleton->is_inside_tree()) return;
	GameData &g = GameData::get();
	int q = g.eff_quality();
	Engine::get_singleton()->set_max_fps(g.settings.fps);
	Viewport *vp = singleton->get_viewport();
	const float scale[4] = { 0.62f, 0.8f, 1.f, 1.f };
	const float lod[4] = { 6.f, 3.f, 1.5f, 0.75f };
	vp->set_scaling_3d_scale(scale[q]);
	vp->set_msaa_3d(q >= 3 ? Viewport::MSAA_4X : (q == 2 ? Viewport::MSAA_2X : Viewport::MSAA_DISABLED));
	vp->set_mesh_lod_threshold(lod[q]);
	RenderingServer::get_singleton()->directional_shadow_atlas_set_size(q >= 3 ? 4096 : 2048, true);
}

void PMApp::free_hero() {
	if (hero) {
		delete hero;
		hero = nullptr;
	}
}

void PMApp::build_showcase() {
	free_hero();
	GameData &g = GameData::get();
	showcase = memnew(Node3D);
	showcase->set_name("Showcase");
	backdrop->add_child(showcase);
	// soft blob under the feet
	Ref<StandardMaterial3D> sh;
	sh.instantiate();
	Ref<Gradient> gr;
	gr.instantiate();
	gr->set_color(0, Color(0, 0, 0, 0.55f));
	gr->set_color(1, Color(0, 0, 0, 0));
	Ref<GradientTexture2D> gt;
	gt.instantiate();
	gt->set_gradient(gr);
	gt->set_fill(GradientTexture2D::FILL_RADIAL);
	gt->set_fill_from(Vector2(0.5f, 0.5f));
	gt->set_fill_to(Vector2(1.f, 0.5f));
	sh->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, gt);
	sh->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	sh->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	sh->set_depth_draw_mode(BaseMaterial3D::DEPTH_DRAW_DISABLED);
	hero = new Footballer();
	hero->rid = R_FWD;
	Color shirt = html(g.shirt), shorts = html(g.shorts);
	KitColors kit = CharLib::team_kit(shirt, shorts, (int)g.team_name.hash());
	int num = 10;
	const Card *best = nullptr;
	for (auto &c : g.cards)
		if (!best || c.ovr > best->ovr) best = &c;
	if (best) num = best->num;
	uint32_t hsh = best ? (uint32_t)best->name.hash() : 7u;
	kit.skin = CharLib::skin_tone((int)(hsh % 6));
	kit.hair = CharLib::hair_tone((int)((hsh >> 8) % 5));
	kit.hair_style = (int)((hsh >> 12) % 3);
	kit.beard = ((hsh >> 16) % 3) == 0;
	hero->setup(showcase, kit, num, sh);
	hero->pos = Vector3(0, 0, 0);
	hero->facing = (float)Math_PI * 0.5f - 0.38f; // three-quarter view towards the camera
	hero->animate(0.016f, 0.f);
	// key + rim lights
	SpotLight3D *key = memnew(SpotLight3D);
	key->set_position(Vector3(-2.5f, 5.5f, 4.5f));
	showcase->add_child(key);
	key->look_at(Vector3(0, 1.0f, 0), Vector3(0, 1, 0));
	key->set_param(Light3D::PARAM_ENERGY, 3.0f);
	key->set_param(Light3D::PARAM_RANGE, 14.f);
	key->set_param(Light3D::PARAM_SPOT_ANGLE, 28.f);
	key->set_color(Color(1.f, 0.96f, 0.9f));
	OmniLight3D *rim = memnew(OmniLight3D);
	rim->set_position(Vector3(1.8f, 2.2f, -1.8f));
	rim->set_param(Light3D::PARAM_ENERGY, 2.2f);
	rim->set_param(Light3D::PARAM_RANGE, 6.f);
	rim->set_color(Color(0.35f, 0.8f, 1.f));
	showcase->add_child(rim);
	hero_t = 0.f;
	hero_next = 6.f;
}

void PMApp::rebuild_backdrop() {
	free_hero();
	showcase = nullptr;
	if (backdrop) {
		backdrop->queue_free();
		backdrop = nullptr;
	}
	if (bg_image) {
		bg_image->queue_free();
		bg_image = nullptr;
	}
	backdrop = memnew(Node3D);
	backdrop->set_name("Backdrop");
	add_child(backdrop);
	move_child(backdrop, 0);
	stadium::build(backdrop, GameData::get().board_texts());
	bd_cam = memnew(Camera3D);
	bd_cam->set_fov(48);
	bd_cam->set_far(500);
	backdrop->add_child(bd_cam);
	bd_cam->set_current(true);
	build_showcase();
	const String &bg = GameData::get().patch.menu_background;
	if (!bg.is_empty()) {
		Ref<Image> img;
		img.instantiate();
		if (img->load(bg) == OK) {
			bg_image = memnew(TextureRect);
			bg_image->set_texture(ImageTexture::create_from_image(img));
			bg_image->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
			bg_image->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_COVERED);
			bg_image->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
			add_child(bg_image);
			move_child(bg_image, 1);
			kit::full_rect(bg_image);
		}
	}
	if (current && current_name == "match") {
		backdrop->set_visible(false);
		if (bg_image) bg_image->set_visible(false);
	}
}

void PMApp::show_screen(const String &name, const Dictionary &params) {
	if (current) {
		current->queue_free();
		current = nullptr;
	}
	Node *s = make_screen(name, params);
	if (!s) s = make_screen("menu", Dictionary());
	current = s;
	current_name = name;
	add_child(s);
	Control *c = Object::cast_to<Control>(s);
	if (c) kit::full_rect(c);
	bool is_match = name == "match";
	if (backdrop) {
		backdrop->set_visible(!is_match);
		if (!is_match && bd_cam) bd_cam->set_current(true);
	}
	hero_snap = (name == "menu") != hero_mode || name == "menu";
	hero_mode = name == "menu";
	if (showcase) showcase->set_visible(hero_mode || name == "title");
	if (name == "menu" && params.has("rebuild_hero")) rebuild_backdrop();
	if (bg_image) bg_image->set_visible(!is_match);
	if (sfx) sfx->crowd_on(is_match);
}

void PMApp::_process(double delta) {
	if (!backdrop || !backdrop->is_visible() || !bd_cam) return;
	bd_t += delta * 0.05;
	float a = (float)bd_t;
	if (hero && showcase && showcase->is_visible()) {
		hero_t += (float)delta;
		if (hero_t > hero_next) {
			const char *acts[3] = { "Dance", "Jump", "Dance" };
			hero->play_action(acts[(int)(hero_next) % 3], 1.8f);
			hero_next = hero_t + 7.f + rng().randf() * 4.f;
		}
		hero->animate((float)delta, 0.f);
	}
	if (dbg_cam) {
		bd_cam->set_fov(30);
		bd_cam->set_position(dbg_eye);
		bd_cam->look_at(dbg_look, Vector3(0, 1, 0));
		return;
	}
	if (hero_mode && hero) {
		// close-up on the club's star player: he stands on the right third of the screen
		float sway = std::sin(a * 3.f) * 0.35f;
		Vector3 eye(-2.15f + sway, 1.25f, 5.9f);
		bd_cam->set_fov(34);
		bd_cam->set_position(hero_snap ? eye : bd_cam->get_position().lerp(eye, 1.f - std::exp(-3.f * (float)delta)));
		hero_snap = false;
		bd_cam->look_at(Vector3(-1.75f + sway * 0.4f, 0.98f, 0.f), Vector3(0, 1, 0));
		return;
	}
	bd_cam->set_fov(48);
	bd_cam->set_position(Vector3(std::cos(a) * 62.f, 24.f + std::sin(a * 0.7f) * 4.f, std::sin(a) * 50.f));
	bd_cam->look_at(Vector3(0, 0, 0), Vector3(0, 1, 0));
}

void PMApp::_notification(int p_what) {
	if (p_what == NOTIFICATION_WM_GO_BACK_REQUEST) {
		PMScreen *sc = Object::cast_to<PMScreen>(current);
		if (sc) {
			sc->on_back();
		} else if (current && current->has_method("on_back")) {
			current->call("on_back");
		} else if (current_name != "menu") {
			go("menu");
		} else {
			get_tree()->quit();
		}
	}
}

} // namespace pm
