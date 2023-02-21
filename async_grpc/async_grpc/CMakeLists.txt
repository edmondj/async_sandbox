add_library(async_grpc
  server.cpp
  threads.cpp
  async_grpc.cpp
)

target_link_libraries(async_grpc
  PUBLIC
    libprotobuf
    grpc
    grpc++
)