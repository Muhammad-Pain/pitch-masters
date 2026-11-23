#!/usr/bin/env python
# Build: scons platform=<linux|windows|android|macos> target=<template_debug|template_release> [arch=arm64]
import os

env = SConscript("godot-cpp/SConstruct")
env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp") + Glob("src/match/*.cpp")

lib_name = "libpitchmasters{}{}".format(env["suffix"], env["SHLIBSUFFIX"])
library = env.SharedLibrary("game/bin/" + lib_name, source=sources)
Default(library)
