#!/usr/bin/env bash
# Pitch Masters: build the C++ library for Android (Linux/macOS).
# Requires: python3, git, Android SDK with NDK installed. Set ANDROID_HOME (or ANDROID_NDK_ROOT).
set -e
cd "$(dirname "$0")"
: "${ANDROID_HOME:?Set ANDROID_HOME to your Android SDK folder}"
NDK_VERSION="${NDK_VERSION:-28.1.13356709}"
python3 -m pip install --user scons
[ -d godot-cpp ] || git clone --depth 1 -b 4.5 https://github.com/godotengine/godot-cpp.git godot-cpp
scons platform=android arch=arm64 target=template_release ndk_version="$NDK_VERSION" -j"$(nproc 2>/dev/null || echo 4)"
scons platform=android arch=arm32 target=template_release ndk_version="$NDK_VERSION" -j"$(nproc 2>/dev/null || echo 4)"
scons platform=linux target=template_debug -j"$(nproc 2>/dev/null || echo 4)"
echo "Done. Libraries are in game/bin. Open the 'game' folder in Godot 4.6 and export the Android preset."
