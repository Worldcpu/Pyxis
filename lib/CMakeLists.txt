# lib/ — core engine (no JSON/network/UI deps)
# Sets _sqlite3_dir for the bravofinder engine submodule before adding it.
set(_sqlite3_dir ${sqlite3_SOURCE_DIR})
add_subdirectory(bravofinder/libs/engine)
# bf::http_status (header-only) 是 bf_service_lib 的编译前置——顺序即依赖顺序。
# bf_service_lib (MIT) 是 app 层库，只能被 service/px_service 链接，不进 px_core。
add_subdirectory(bravofinder/libs/http_server)
add_subdirectory(bravofinder/libs/service)
add_subdirectory(core)
add_subdirectory(module)
