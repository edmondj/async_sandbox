cmake_policy(SET CMP0024 OLD)

add_subdirectory("third_party")
add_subdirectory("async_grpc")
add_subdirectory("protos")
add_subdirectory("server")

organize_targets_in("async_grpc")