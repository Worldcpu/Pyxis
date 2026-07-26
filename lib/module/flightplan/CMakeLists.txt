# lib/module/flightplan/ — flight planning engine (LGPL v3.0-or-later)
# Phase 8: altitude optimization, fuel planning, alternate selection,
#           wind/weather model, airframe performance profiles.

add_library(px_flightplan STATIC
  # Sources to be added in Phase 8
)

target_include_directories(px_flightplan PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(px_flightplan PUBLIC px_core px_navdata px_router)
target_compile_features(px_flightplan PUBLIC cxx_std_17)
