# No fetch: cpptui is vendored (see vendor/cpptui.hpp shim over _attic/cpptui.hpp).
add_library(cpptui_iface INTERFACE)
target_include_directories(cpptui_iface SYSTEM INTERFACE ${CMAKE_SOURCE_DIR}/vendor)
add_library(cpptui::cpptui ALIAS cpptui_iface)
