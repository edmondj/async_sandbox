add_library(async_grpc
  async_grpc.cpp
  async_grpc.hpp
  client.cpp
  client.hpp
  server.cpp
  server.hpp
  
 "client.cpp")

target_link_libraries(async_grpc
  PUBLIC libprotobuf grpc grpc++
)
setup_target_compile_options(async_grpc)
target_compile_definitions(async_grpc
  PUBLIC _SILENCE_ALL_CXX20_DEPRECATION_WARNINGS
)