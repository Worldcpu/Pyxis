# src/server/ — WebSocket server executable

add_executable(px_server
  main.cc
)

target_link_libraries(px_server PRIVATE
  px_service
  uv_a
  llhttp_shared
)

target_compile_features(px_server PRIVATE cxx_std_20)
