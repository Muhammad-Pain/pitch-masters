#pragma once
#include "game_data.h"

#include <godot_cpp/classes/e_net_multiplayer_peer.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>

namespace pm {

// PvP over Wi-Fi / hotspot (ENet). Host simulates the match; client sends input and renders snapshots.
class PMNet : public godot::Node {
	GDCLASS(PMNet, godot::Node)

	godot::Ref<godot::ENetMultiplayerPeer> peer;
	void set_status(const String &t);

protected:
	static void _bind_methods();

public:
	static PMNet *singleton;
	static const int PORT = 7777;
	bool is_host = false, active = false, connected = false, in_match = false;
	bool has_remote = false;
	MatchTeam remote_team;
	String last_status;
	std::function<void(const Dictionary &)> on_start_match; // set by the app

	void _ready() override;
	void _exit_tree() override;
	int host();
	int join(const String &ip);
	void close();
	godot::PackedStringArray local_ips() const;
	void start_match(int minutes);
	void send_my_team();

	void send_input(Vector2 move, bool sprint, bool press);
	void send_action(const String &a, float v);
	void send_snapshot(const godot::PackedFloat32Array &d);
	void send_event(const String &kind, const Dictionary &info);
	void send_over(const Dictionary &result);

	// RPC targets
	void rpc_team(const Dictionary &team);
	void rpc_start(const Dictionary &params);
	void rpc_input(Vector2 move, bool sprint, bool press);
	void rpc_action(const String &a, float v);
	void rpc_snapshot(const godot::PackedFloat32Array &d);
	void rpc_event(const String &kind, const Dictionary &info);
	void rpc_over(const Dictionary &result);

	void _on_peer_connected(int64_t id);
	void _on_peer_disconnected(int64_t id);
	void _on_connected();
	void _on_failed();
	void _on_server_gone();
};

} // namespace pm
