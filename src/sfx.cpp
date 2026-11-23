#include "sfx.h"
#include "game_data.h"

#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>

#include <cmath>

using namespace godot;

namespace pm {

PMSfx *PMSfx::singleton = nullptr;
static const int RATE = 22050;

static Ref<AudioStreamWAV> make(const std::vector<float> &s, bool loop = false) {
	PackedByteArray bytes;
	bytes.resize((int64_t)s.size() * 2);
	uint8_t *w = bytes.ptrw();
	for (size_t i = 0; i < s.size(); i++) {
		int v = (int)(clampf(s[i], -1.f, 1.f) * 32000.f);
		w[i * 2] = (uint8_t)(v & 0xff);
		w[i * 2 + 1] = (uint8_t)((v >> 8) & 0xff);
	}
	Ref<AudioStreamWAV> st;
	st.instantiate();
	st->set_format(AudioStreamWAV::FORMAT_16_BITS);
	st->set_mix_rate(RATE);
	st->set_stereo(false);
	st->set_data(bytes);
	if (loop) {
		st->set_loop_mode(AudioStreamWAV::LOOP_FORWARD);
		st->set_loop_begin(0);
		st->set_loop_end((int)s.size());
	}
	return st;
}

static Ref<AudioStreamWAV> whistle(float dur) {
	int n = (int)(dur * RATE);
	std::vector<float> o(n);
	double ph = 0;
	Rng r(5);
	for (int i = 0; i < n; i++) {
		float t = (float)i / RATE;
		float f = 2900.f + std::sin(t * Math_TAU * 32.f) * 160.f;
		ph += Math_TAU * f / RATE;
		float env = std::min(1.f, t / 0.02f) * std::min(1.f, (dur - t) / 0.05f);
		o[i] = (float)std::sin(ph) * 0.45f * env + (float)(r.randf() - 0.5) * 0.04f * env;
	}
	return make(o);
}

static Ref<AudioStreamWAV> kick() {
	int n = (int)(0.09f * RATE);
	std::vector<float> o(n);
	Rng r(7);
	for (int i = 0; i < n; i++) {
		float t = (float)i / RATE;
		float env = std::exp(-t * 45.f);
		o[i] = ((float)std::sin(Math_TAU * 110.f * t) * 0.8f + (float)(r.randf() - 0.5) * 0.6f) * env;
	}
	return make(o);
}

static Ref<AudioStreamWAV> tone(std::vector<float> freqs, float dur, float vol) {
	int n = (int)(dur * RATE);
	std::vector<float> o(n);
	float seg = (float)n / freqs.size();
	for (int i = 0; i < n; i++) {
		float t = (float)i / RATE;
		int k = std::min((int)(i / seg), (int)freqs.size() - 1);
		float lt = std::fmod((float)i, seg) / RATE;
		float env = std::exp(-lt * 9.f) * std::min(1.f, (dur - t) / 0.03f);
		o[i] = (float)std::sin(Math_TAU * freqs[k] * t) * vol * env;
	}
	return make(o);
}

static Ref<AudioStreamWAV> noise(float dur, float vol, bool swell, uint64_t seed) {
	int n = (int)(dur * RATE);
	std::vector<float> o(n);
	float lp = 0;
	Rng r(seed);
	for (int i = 0; i < n; i++) {
		float t = (float)i / RATE;
		lp = lp * 0.82f + (float)(r.randf() - 0.5) * 0.18f;
		float env = swell ? (float)std::sin(Math_PI * t / dur) : 1.f;
		o[i] = lp * vol * 5.f * env;
	}
	return make(o);
}

static Ref<AudioStreamWAV> crowd_loop() {
	float dur = 3.f;
	int n = (int)(dur * RATE);
	std::vector<float> o(n);
	float lp = 0, lp2 = 0;
	Rng r(11);
	for (int i = 0; i < n; i++) {
		float t = (float)i / RATE;
		lp = lp * 0.9f + (float)(r.randf() - 0.5) * 0.1f;
		lp2 = lp2 * 0.995f + (float)(r.randf() - 0.5) * 0.005f;
		o[i] = (lp * 3.f + lp2 * 6.f) * (0.8f + 0.2f * (float)std::sin(t * Math_TAU / dur));
	}
	int fade = (int)(0.2f * RATE);
	for (int i = 0; i < fade; i++) {
		float a = (float)i / fade;
		o[i] = o[i] * a + o[n - fade + i] * (1.f - a);
	}
	return make(o, true);
}

void PMSfx::_ready() {
	singleton = this;
	streams["whistle"] = whistle(0.32f);
	streams["whistle_long"] = whistle(0.95f);
	streams["kick"] = kick();
	streams["click"] = tone({ 1150.f }, 0.035f, 0.25f);
	streams["reveal"] = tone({ 523.f, 659.f, 784.f, 1046.f }, 0.45f, 0.3f);
	streams["legend"] = tone({ 392.f, 523.f, 659.f, 784.f, 1046.f, 1318.f }, 0.9f, 0.32f);
	streams["cheer"] = noise(2.2f, 0.55f, true, 3);
	streams["save"] = noise(0.5f, 0.35f, true, 9);
	for (int i = 0; i < 8; i++) {
		AudioStreamPlayer *p = memnew(AudioStreamPlayer);
		add_child(p);
		pool.push_back(p);
	}
	crowd = memnew(AudioStreamPlayer);
	crowd->set_stream(crowd_loop());
	crowd->set_volume_db(-16.f);
	add_child(crowd);
}

void PMSfx::_exit_tree() {
	if (singleton == this) singleton = nullptr;
}

void PMSfx::play(const String &name, float volume_db) {
	if (!GameData::get().settings.sound) return;
	auto it = streams.find(name);
	if (it == streams.end()) return;
	for (auto *p : pool) {
		if (!p->is_playing()) {
			p->set_stream(it->second);
			p->set_volume_db(volume_db);
			p->play();
			return;
		}
	}
}

void PMSfx::crowd_on(bool on) {
	if (!crowd) return;
	if (on && GameData::get().settings.sound) {
		if (!crowd->is_playing()) crowd->play();
	} else {
		crowd->stop();
	}
}

} // namespace pm
