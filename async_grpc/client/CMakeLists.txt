add_executable(client
  main.cpp
)
target_link_libraries(client
  PRIVATE async_grpc protos
)
target_include_directories(client
  PRIVATE "$<TARGET_PROPERTY:async_grpc,SOURCE_DIR>/.." "$<TARGET_PROPERTY:protos,BINARY_DIR>/.."
)