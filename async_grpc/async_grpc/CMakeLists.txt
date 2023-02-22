add_library(async_grpc
  async_grpc.cpp
  async_grpc.hpp
  server.cpp
  server.hpp
  service_impl.hpp
)

target_link_libraries(async_grpc
  PUBLIC libprotobuf grpc grpc++
)