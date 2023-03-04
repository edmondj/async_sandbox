add_library(async_grpc
  async_grpc.cpp
  async_grpc.hpp
  client.cpp
  client.hpp
  server.cpp
  server.hpp
  client.cpp
  iostream.cpp
  iostream.hpp
)

target_link_libraries(async_grpc
  PUBLIC libprotobuf grpc grpc++ async_lib
)
target_include_directories(async_grpc
  PRIVATE "$<TARGET_PROPERTY:async_lib,SOURCE_DIR>/.."
)
setup_target_compile_options(async_grpc)
target_compile_definitions(async_grpc
  PUBLIC _SILENCE_ALL_CXX20_DEPRECATION_WARNINGS
)