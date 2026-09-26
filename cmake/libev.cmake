# System libev first; else build upstream ev.c directly (it ships no CMakeLists).
include(FetchContent)

find_path(LIBEV_INCLUDE_DIR NAMES ev.h)
find_library(LIBEV_LIBRARY NAMES ev)

if(LIBEV_INCLUDE_DIR AND LIBEV_LIBRARY)
  add_library(libev UNKNOWN IMPORTED GLOBAL)
  set_target_properties(libev PROPERTIES
    IMPORTED_LOCATION "${LIBEV_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBEV_INCLUDE_DIR}"
  )
else()
  FetchContent_Declare(libev
    URL https://github.com/enki/libev/archive/refs/heads/master.zip
  )
  FetchContent_GetProperties(libev)
  if(NOT libev_POPULATED)
    FetchContent_Populate(libev)
  endif()
  add_library(libev STATIC ${libev_SOURCE_DIR}/ev.c)
  target_include_directories(libev PUBLIC ${libev_SOURCE_DIR})
  target_compile_options(libev PRIVATE -w)
endif()

add_library(libev::libev ALIAS libev)
