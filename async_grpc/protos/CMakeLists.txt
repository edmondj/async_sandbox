find_package(protobuf CONFIG REQUIRED)

add_library(protos
  echo_service.proto
  variable_service.proto
)
target_link_libraries(protos
  PUBLIC libprotobuf grpc grpc++
)
target_include_directories(protos
  PUBLIC "$<TARGET_PROPERTY:async_grpc,INTERFACE_INCLUDE_DIRECTORIES>"
)

protobuf_generate(TARGET protos LANGUAGE cpp)
protobuf_generate(TARGET protos LANGUAGE grpc
  GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}"
)