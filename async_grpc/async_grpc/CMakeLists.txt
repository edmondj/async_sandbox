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