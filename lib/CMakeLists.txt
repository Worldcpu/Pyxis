# lib/ — core engine (no JSON/network/UI deps)
# Sets _sqlite3_dir for the bravofinder engine submodule before adding it.
set(_sqlite3_dir ${sqlite3_SOURCE_DIR})
add_subdirectory(bravofinder/libs/engine)
add_subdirectory(core)
add_subdirectory(module)
