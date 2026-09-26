include(FetchContent)
set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "" FORCE)
FetchContent_Declare(spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.15.2
  SYSTEM
  FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(spdlog)
