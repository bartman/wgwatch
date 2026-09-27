include(${CMAKE_SOURCE_DIR}/cmake/gtest.cmake)

add_executable(wgwatch_tests
  test_parser.cpp
  test_rates.cpp
  test_format.cpp
  test_cli.cpp
  test_collector.cpp
  test_sort.cpp
  test_theme.cpp
  test_ui.cpp
  ../src/parser.cpp
  ../src/rates.cpp
  ../src/cli.cpp
  ../src/collector.cpp
  ../src/ui.cpp
)
target_link_libraries(wgwatch_tests PRIVATE
  cpptui::cpptui
  GTest::gtest_main
  fmt::fmt
  spdlog::spdlog
)
# Tests read the checked-in example dump straight from _attic (no copy).
target_compile_definitions(wgwatch_tests PRIVATE
  WG_ATTIC_DIR="${CMAKE_SOURCE_DIR}/_attic"
)

include(GoogleTest)
gtest_discover_tests(wgwatch_tests)
