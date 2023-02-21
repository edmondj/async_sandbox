include(FetchContent)

set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(
  gRPC
  GIT_REPOSITORY https://github.com/grpc/grpc
  GIT_TAG        v1.52.1
  CMAKE_CACHE_ARGS -DABSL_PROPAGATE_CXX_STD:BOOL=ON
  FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(gRPC)
set(FETCHCONTENT_QUIET ON)

cmake_policy(SET CMP0024 OLD)
if (MSVC)
  set(protobuf_DIR "${grpc_BINARY_DIR}/third_party/protobuf/cmake")
else ()
  set(protobuf_DIR "${grpc_BINARY_DIR}/third_party/protobuf/lib/cmake/protobuf")
endif()

set(grpc_cpp_plugin_location $<TARGET_FILE:grpc_cpp_plugin>)

add_subdirectory("async_grpc")
add_subdirectory("protos")
add_subdirectory("server")