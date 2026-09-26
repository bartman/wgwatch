cmake_minimum_required(VERSION 3.24)

project(wgwatch VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# No hardcoded compiler: CXX=clang++ first, g++ fallback (see README).
add_compile_options(-Wall -Wextra -Wpedantic)

include(cmake/cpptui.cmake)
include(cmake/fmt.cmake)
include(cmake/spdlog.cmake)
include(cmake/libev.cmake)

add_executable(wgwatch
  src/main.cpp
  src/cli.cpp
  src/collector.cpp
  src/parser.cpp
  src/rates.cpp
  src/sampler.cpp
  src/ui.cpp
)
target_link_libraries(wgwatch PRIVATE
  cpptui::cpptui
  fmt::fmt
  spdlog::spdlog
  libev::libev
)

enable_testing()
add_subdirectory(tests)
