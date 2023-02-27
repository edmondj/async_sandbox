add_executable(client
  main.cpp
)
target_link_libraries(client
  PRIVATE async_grpc protos utils
)
target_include_directories(client
  PRIVATE "$<TARGET_PROPERTY:async_grpc,SOURCE_DIR>/.." "$<TARGET_PROPERTY:utils,SOURCE_DIR>/.."
)
target_include_directories(client
  SYSTEM PRIVATE "$<TARGET_PROPERTY:protos,BINARY_DIR>/.."
)
setup_target_compile_options(client)
