add_subdirectory("grpc")

set(protobuf_DIR ${protobuf_DIR} PARENT_SCOPE)
set(grpc_cpp_plugin_location ${grpc_cpp_plugin_location} PARENT_SCOPE)

message("third_party BUILDSYSTEM_TARGETS: ${BUILDSYSTEM_TARGETS}")

organize_targets_in("third_party")