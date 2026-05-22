#!/usr/bin/env python

import os

env = SConscript("godot-cpp/SConstruct")

sources = Glob("src/*.cpp")

library = env.SharedLibrary(
    target="extensions/game_manager/bin/game_manager",
    source=sources,
)

Default(library)