add_executable(game
  async_game.cpp
  async_game.hpp
  grpc_game.hpp
  character_service.hpp
  character_service_grpc.hpp
  character_service_grpc.cpp
  main.cpp
)
target_link_libraries(game
  PRIVATE async_grpc protos utils
)
target_include_directories(game
  PRIVATE "$<TARGET_PROPERTY:async_grpc,SOURCE_DIR>/.." "$<TARGET_PROPERTY:utils,SOURCE_DIR>/.."
)
target_include_directories(game
  SYSTEM PRIVATE "$<TARGET_PROPERTY:protos,BINARY_DIR>/.."
)
setup_target_compile_options(game)
