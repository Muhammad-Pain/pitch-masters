#pragma once
// Small helpers shared by the whole game.

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/callable_custom.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <cmath>
#include <cstdint>
#include <functional>

namespace pm {

using godot::Callable;
using godot::Color;
using godot::String;
using godot::Variant;
using godot::Vector2;
using godot::Vector3;

// UTF-8 literal -> godot::String (String(const char*) would treat bytes as Latin-1).
inline String S(const char *utf8) { return String::utf8(utf8); }
inline String I(int64_t v) { return String::num_int64(v); }

// Callable that wraps a std::function. Lets UI code connect lambdas to signals.
class LambdaCallable : public godot::CallableCustom {
	std::function<void(const Variant **, int)> fn;
	uint64_t owner_id = 0;
	uint32_t h;

	static bool cmp_eq(const godot::CallableCustom *a, const godot::CallableCustom *b) { return a == b; }
	static bool cmp_less(const godot::CallableCustom *a, const godot::CallableCustom *b) { return a < b; }

public:
	LambdaCallable(uint64_t owner, std::function<void(const Variant **, int)> f) :
			fn(std::move(f)), owner_id(owner) { h = (uint32_t)(uintptr_t)this; }
	uint32_t hash() const override { return h; }
	String get_as_text() const override { return "LambdaCallable"; }
	CompareEqualFunc get_compare_equal_func() const override { return cmp_eq; }
	CompareLessFunc get_compare_less_func() const override { return cmp_less; }
	godot::ObjectID get_object() const override { return godot::ObjectID(owner_id); }
	int get_argument_count(bool &r_is_valid) const override {
		r_is_valid = false;
		return 0;
	}
	void call(const Variant **args, int argc, Variant &ret, GDExtensionCallError &err) const override {
		err.error = GDEXTENSION_CALL_OK;
		fn(args, argc);
		ret = Variant();
	}
};

// Lambda ignoring signal arguments. `owner` = object whose lifetime bounds the callable.
inline Callable Fn(godot::Object *owner, std::function<void()> f) {
	std::function<void(const Variant **, int)> w = [f](const Variant **, int) { f(); };
	uint64_t id = owner ? (uint64_t)owner->get_instance_id() : 0;
	LambdaCallable *lc = memnew(LambdaCallable(id, w));
	return Callable(lc);
}
// Lambda receiving the first signal argument (e.g. item index).
inline Callable FnV(godot::Object *owner, std::function<void(const Variant &)> f) {
	std::function<void(const Variant **, int)> w = [f](const Variant **a, int n) { f(n > 0 ? *a[0] : Variant()); };
	uint64_t id = owner ? (uint64_t)owner->get_instance_id() : 0;
	LambdaCallable *lc = memnew(LambdaCallable(id, w));
	return Callable(lc);
}

// Fast deterministic RNG (xorshift64*).
struct Rng {
	uint64_t s = 0x9E3779B97F4A7C15ull;
	explicit Rng(uint64_t seed = 1) { set_seed(seed); }
	void set_seed(uint64_t seed) { s = seed * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull; if (s == 0) s = 1; next(); }
	uint64_t next() {
		s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
		return s * 2685821657736338717ull;
	}
	double randf() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
	double range(double a, double b) { return a + (b - a) * randf(); }
	int irange(int a, int b) { return a + (int)(next() % (uint64_t)(b - a + 1)); }
	double gauss(double mean, double dev) {
		double u1 = randf(); double u2 = randf();
		if (u1 < 1e-12) u1 = 1e-12;
		return mean + dev * std::sqrt(-2.0 * std::log(u1)) * std::cos(Math_TAU * u2);
	}
};

Rng &rng(); // global game RNG

inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline int clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
inline float signf(float v) { return v > 0.f ? 1.f : (v < 0.f ? -1.f : 0.f); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float fdist(const Vector3 &a, const Vector3 &b) { return Vector2(a.x - b.x, a.z - b.z).length(); }
inline Color html(const String &s, const Color &fallback = Color(1, 1, 1)) {
	return Color::html_is_valid(s) ? Color::html(s) : fallback;
}

} // namespace pm
