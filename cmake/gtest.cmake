include(FetchContent)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/v1.15.2.zip
  SYSTEM
  FIND_PACKAGE_ARGS NAMES GTest
)
FetchContent_MakeAvailable(googletest)
