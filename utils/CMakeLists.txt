add_library(utils
  logs.hpp
  logs.cpp
)
setup_target_compile_options(utils)

if (MSVC)
  target_compile_definitions(utils PRIVATE "_CRT_SECURE_NO_WARNINGS")
endif ()
