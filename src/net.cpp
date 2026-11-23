#include "net.h"

#include <godot_cpp/classes/ip.hpp>
#include <godot_cpp/classes/multiplayer_api.hpp>
#include <godot_cpp/classes/multiplayer_peer.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace pm {

PMNet *PMNet::singleton = nullptr;

void PMNet::_bind_methods() {
	ClassDB::bind_method(D_METHOD("rpc_team", "team"), &PMNet::rpc_team);
	ClassDB::bind_method(D_METHOD("rpc_start", "params"), &PMNet::rpc_start);
	ClassDB::bind_method(D_METHOD("rpc_input", "move", "sprint", "press"), &PMNet::rpc_input);
	ClassDB::bind_method(D_METHOD("rpc_action", "a", "v"), &PMNet::rpc_action);
	ClassDB::bind_method(D_METHOD("rpc_snapshot", "d"), &PMNet::rpc_snapshot);
	ClassDB::bind_method(D_METHOD("rpc_event", "kind", "info"), &PMNet::rpc_event);
	ClassDB::bind_method(D_METHOD("rpc_over", "result"), &PMNet::rpc_over);
	ADD_SIGNAL(MethodInfo("status_changed", PropertyInfo(Variant::STRING, "text")));
	ADD_SIGNAL(MethodInfo("lobby_changed"));
	ADD_SIGNAL(MethodInfo("net_disconnected"));
	ADD_SIGNAL(MethodInfo("snapshot_received", PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "data")));
	ADD_SIGNAL(MethodInfo("remote_input", PropertyInfo(Variant::VECTOR2, "move"), PropertyInfo(Variant::BOOL, "sprint"), PropertyInfo(Variant::BOOL, "press")));
	ADD_SIGNAL(MethodInfo("remote_action", PropertyInfo(Variant::STRING, "action"), PropertyInfo(Variant::FLOAT, "value")));
	ADD_SIGNAL(MethodInfo("match_event", PropertyInfo(Variant::STRING, "kind"), PropertyInfo(Variant::DICTIONARY, "info")));
	ADD_SIGNAL(MethodInfo("match_over", PropertyInfo(Variant::DICTIONARY, "result")));
}

static Dictionary rpc_cfg(MultiplayerAPI::RPCMode mode, MultiplayerPeer::TransferMode tm, bool local) {
	Dictionary d;
	d["rpc_mode"] = mode;
	d["transfer_mode"] = tm;
	d["call_local"] = local;
	d["channel"] = 0;
	return d;
}

void PMNet::_ready() {
	singleton = this;
	auto any = MultiplayerAPI::RPC_MODE_ANY_PEER;
	auto auth = MultiplayerAPI::RPC_MODE_AUTHORITY;
	auto rel = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	auto unr = MultiplayerPeer::TRANSFER_MODE_UNRELIABLE_ORDERED;
	rpc_config("rpc_team", rpc_cfg(any, rel, false));
	rpc_config("rpc_start", rpc_cfg(auth, rel, true));
	rpc_config("rpc_input", rpc_cfg(any, unr, false));
	rpc_config("rpc_action", rpc_cfg(any, rel, false));
	rpc_config("rpc_snapshot", rpc_cfg(auth, unr, false));
	rpc_config("rpc_event", rpc_cfg(auth, rel, false));
	rpc_config("rpc_over", rpc_cfg(auth, rel, false));
	Ref<MultiplayerAPI> mp = get_multiplayer();
	mp->connect("peer_connected", callable_mp(this, &PMNet::_on_peer_connected));
	mp->connect("peer_disconnected", callable_mp(this, &PMNet::_on_peer_disconnected));
	mp->connect("connected_to_server", callable_mp(this, &PMNet::_on_connected));
	mp->connect("connection_failed", callable_mp(this, &PMNet::_on_failed));
	mp->connect("server_disconnected", callable_mp(this, &PMNet::_on_server_gone));
}

void PMNet::_exit_tree() {
	if (singleton == this) singleton = nullptr;
}

void PMNet::set_status(const String &t) {
	last_status = t;
	emit_signal("status_changed", t);
}

int PMNet::host() {
	close();
	peer.instantiate();
	// LAN play over Wi-Fi / hotspot is IPv4; binding the IPv4 wildcard also works on devices without IPv6
	peer->set_bind_ip("0.0.0.0");
	Error err = peer->create_server(PORT, 1);
	if (err != OK) {
		peer.unref();
		set_status(S("Не удалось создать сервер (порт ") + I(PORT) + S(" занят?)"));
		return err;
	}
	get_multiplayer()->set_multiplayer_peer(peer);
	is_host = true;
	active = true;
	set_status(S("Ожидание соперника…"));
	return OK;
}

int PMNet::join(const String &ip0) {
	close();
	String ip = ip0.strip_edges();
	if (!ip.is_valid_ip_address()) {
		set_status(S("Неверный IP-адрес"));
		return ERR_INVALID_PARAMETER;
	}
	peer.instantiate();
	Error err = peer->create_client(ip, PORT);
	if (err != OK) {
		peer.unref();
		set_status(S("Ошибка подключения"));
		return err;
	}
	get_multiplayer()->set_multiplayer_peer(peer);
	is_host = false;
	active = true;
	set_status(S("Подключение к ") + ip + S("…"));
	return OK;
}

void PMNet::close() {
	if (peer.is_valid()) peer->close();
	peer.unref();
	get_multiplayer()->set_multiplayer_peer(Ref<MultiplayerPeer>());
	active = connected = is_host = in_match = has_remote = false;
}

PackedStringArray PMNet::local_ips() const {
	PackedStringArray out;
	PackedStringArray all = IP::get_singleton()->get_local_addresses();
	for (int i = 0; i < all.size(); i++) {
		String a = all[i];
		if (a.begins_with("192.168.") || a.begins_with("10.") || a.begins_with("172.")) out.push_back(a);
	}
	return out;
}

void PMNet::_on_peer_connected(int64_t id) {
	connected = true;
	rpc_id(id, "rpc_team", GameData::get().build_my_team().to_dict());
	set_status(is_host ? S("Соперник подключён") : S("Подключено! Ждём старта от хоста"));
	emit_signal("lobby_changed");
}

void PMNet::_on_peer_disconnected(int64_t) {
	connected = false;
	has_remote = false;
	set_status(S("Соперник отключился"));
	emit_signal("lobby_changed");
	emit_signal("net_disconnected");
}

void PMNet::_on_connected() {
	connected = true;
	emit_signal("lobby_changed");
}

void PMNet::_on_failed() {
	close();
	set_status(S("Не удалось подключиться. Проверьте IP и Wi-Fi"));
	emit_signal("lobby_changed");
}

void PMNet::_on_server_gone() {
	close();
	set_status(S("Хост закрыл игру"));
	emit_signal("lobby_changed");
	emit_signal("net_disconnected");
}

void PMNet::rpc_team(const Dictionary &team) {
	remote_team = MatchTeam::from_dict(team);
	has_remote = true;
	emit_signal("lobby_changed");
}

void PMNet::send_my_team() {
	if (connected) rpc("rpc_team", GameData::get().build_my_team().to_dict());
}

void PMNet::start_match(int minutes) {
	if (!is_host || !connected || !has_remote) return;
	Dictionary p;
	p["mode"] = "pvp";
	p["home"] = GameData::get().build_my_team().to_dict();
	p["away"] = remote_team.to_dict();
	p["minutes"] = minutes;
	p["seed"] = (int64_t)(rng().next() & 0x7fffffff);
	rpc("rpc_start", p);
}

void PMNet::rpc_start(const Dictionary &params) {
	in_match = true;
	Dictionary p = params.duplicate(true);
	p["net_role"] = is_host ? "host" : "client";
	if (on_start_match) on_start_match(p);
}

void PMNet::send_input(Vector2 move, bool sprint, bool press) {
	if (connected && !is_host) rpc_id(1, "rpc_input", move, sprint, press);
}

void PMNet::rpc_input(Vector2 move, bool sprint, bool press) {
	if (move.length() > 1.f) move = move.normalized();
	emit_signal("remote_input", move, sprint, press);
}

void PMNet::send_action(const String &a, float v) {
	if (connected && !is_host) rpc_id(1, "rpc_action", a, v);
}

void PMNet::rpc_action(const String &a, float v) { emit_signal("remote_action", a.left(16), v); }

void PMNet::send_snapshot(const PackedFloat32Array &d) {
	if (connected && is_host) rpc("rpc_snapshot", d);
}

void PMNet::rpc_snapshot(const PackedFloat32Array &d) { emit_signal("snapshot_received", d); }

void PMNet::send_event(const String &kind, const Dictionary &info) {
	if (connected && is_host) rpc("rpc_event", kind, info);
}

void PMNet::rpc_event(const String &kind, const Dictionary &info) { emit_signal("match_event", kind, info); }

void PMNet::send_over(const Dictionary &result) {
	if (connected && is_host) rpc("rpc_over", result);
}

void PMNet::rpc_over(const Dictionary &result) {
	in_match = false;
	emit_signal("match_over", result);
}

} // namespace pm
