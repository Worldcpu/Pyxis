# lib/module/router/ — pathfinding: A* single-path search + Yen K-shortest

add_library(px_router STATIC
  astar/astar.cc
  yen/yen.cc
)

target_include_directories(px_router PUBLIC
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(px_router PUBLIC px_core)
target_compile_features(px_router PUBLIC cxx_std_17)
