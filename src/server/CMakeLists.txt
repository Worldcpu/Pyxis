# src/server/ — px_server 可执行文件（HTTP/1.1 + JSON-RPC over POST，决策 43）

add_executable(px_server
  main.cc
)

# bf_http_server PUBLIC 传递 uv_a + llhttp + include 路径（决策 44 线程池）；
# px_navdata 供机场索引（决策 48）。
target_link_libraries(px_server PRIVATE
  px_service
  px_navdata
  bf_http_server
)

target_compile_features(px_server PRIVATE cxx_std_20)
