cmake_policy(SET CMP0024 OLD)

add_subdirectory("third_party")
add_subdirectory("async_grpc")
if (ASYNC_LIB_EXAMPLES)
  add_subdirectory("protos")
  add_subdirectory("server")
  add_subdirectory("client")
endif ()

organize_targets_in("async_grpc")