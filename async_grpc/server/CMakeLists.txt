add_executable(server
  main.cpp
  echo_service_impl.cpp
  echo_service_impl.hpp
  variable_service_impl.cpp
  variable_service_impl.hpp
)
target_link_libraries(server
  PRIVATE async_grpc protos
)
target_include_directories(server
  PRIVATE "$<TARGET_PROPERTY:async_grpc,SOURCE_DIR>/.." "$<TARGET_PROPERTY:protos,BINARY_DIR>/.."
)