#pragma once
#include "util.h"

#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/node.hpp>

#include <map>
#include <vector>

namespace pm {

// Procedural sound effects: whistle, kicks, crowd, UI clicks.
class PMSfx : public godot::Node {
	GDCLASS(PMSfx, godot::Node)

	std::map<String, godot::Ref<godot::AudioStreamWAV>> streams;
	std::vector<godot::AudioStreamPlayer *> pool;
	godot::AudioStreamPlayer *crowd = nullptr;

protected:
	static void _bind_methods() {}

public:
	static PMSfx *singleton;
	void _ready() override;
	void _exit_tree() override;
	void play(const String &name, float volume_db = 0.0f);
	void crowd_on(bool on);
	static void p(const char *name, float volume_db = 0.0f) {
		if (singleton) singleton->play(name, volume_db);
	}
};

} // namespace pm
