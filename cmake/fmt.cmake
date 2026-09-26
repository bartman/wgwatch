include(FetchContent)
FetchContent_Declare(fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG 11.1.4
  SYSTEM
  FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(fmt)
