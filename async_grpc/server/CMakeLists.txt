add_executable(server
  main.cpp
  echo_service_impl.cpp
  echo_service_impl.hpp
  variable_service_impl.cpp
  variable_service_impl.hpp
)
target_link_libraries(server
  PRIVATE async_grpc protos utils
)
target_include_directories(server
  PRIVATE "$<TARGET_PROPERTY:async_grpc,SOURCE_DIR>/.." "$<TARGET_PROPERTY:utils,SOURCE_DIR>/.."
)
target_include_directories(server
  SYSTEM PRIVATE "$<TARGET_PROPERTY:protos,BINARY_DIR>/.."
)

setup_target_compile_options(server)