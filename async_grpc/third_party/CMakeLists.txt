add_subdirectory("grpc")

set(protobuf_DIR ${protobuf_DIR} PARENT_SCOPE)
set(grpc_cpp_plugin_location ${grpc_cpp_plugin_location} PARENT_SCOPE)

organize_targets_in("third_party")