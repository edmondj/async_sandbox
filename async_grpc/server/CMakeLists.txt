add_executable(server
  main.cpp
)
target_link_libraries(server
  PRIVATE protos
)
target_include_directories(server
  PRIVATE "$<TARGET_PROPERTY:protos,INTERFACE_INCLUDE_DIRECTORIES>"
)