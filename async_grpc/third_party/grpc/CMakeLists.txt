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
if (MSVC)
  set(protobuf_DIR "${grpc_BINARY_DIR}/third_party/protobuf/cmake" PARENT_SCOPE)
else ()
  set(protobuf_DIR "${grpc_BINARY_DIR}/third_party/protobuf/lib/cmake/protobuf" PARENT_SCOPE)
endif()

set(grpc_cpp_plugin_location $<TARGET_FILE:grpc_cpp_plugin> PARENT_SCOPE)

organize_targets_in("grpc")