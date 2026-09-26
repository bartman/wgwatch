# wgwatch — thin wrapper over CMake/Ninja.
#
#   make
#   make test
#   make BUILD=build-debug TYPE=debug CXX=g++

BUILD ?= build
TYPE ?= release

# Default to clang++; honor command-line/env CXX; accept CC as shorthand.
# (Note: make predefines CXX=g++ with origin 'default', so plain ?= would
# never fire. := under origin-check does.)
ifeq ($(origin CXX),default)
ifneq ($(origin CC),default)
CXX := $(CC)
else
CXX := clang++
endif
endif

ifeq ($(strip $(BUILD)),)
$(error BUILD must not be empty)
endif

ifeq ($(TYPE),release)
CMAKE_BUILD_TYPE := Release
else ifeq ($(TYPE),debug)
CMAKE_BUILD_TYPE := Debug
else
$(error TYPE must be release or debug (got '$(TYPE)'))
endif

.PHONY: all clean distclean test install deb rpm help config

all: config
	cmake --build $(BUILD)

# Always re-run configure: cheap no-op when flags match, and it picks up
# TYPE/CXX switches under one BUILD dir without a distclean.
# Afterwards link the top-level compile_commands.json to the BUILD one so
# editors find it; the build itself populates BUILD/compile_commands.json.
config:
	cmake -S . -B $(BUILD) -G Ninja \
	  -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
	  -DCMAKE_CXX_COMPILER=$(CXX)
	ln -sfn $(BUILD)/compile_commands.json compile_commands.json

clean:
	-cmake --build $(BUILD) --target clean

distclean:
	cmake -E rm -rf $(BUILD)

test: all
	ctest --test-dir $(BUILD) --output-on-failure

install: all
	cmake --install $(BUILD) --prefix $(HOME)/.local

deb: all
	cmake -E chdir $(BUILD) cpack -G DEB

rpm: all
	cmake -E chdir $(BUILD) cpack -G RPM

help:
	@echo "targets:"
	@echo "  all            build the project (default)"
	@echo "  clean          clean the binaries"
	@echo "  distclean      remove BUILD directory"
	@echo "  test           run unit tests"
	@echo "  install        install binary in ~/.local/bin"
	@echo "  deb            build .deb package (Debian)"
	@echo "  rpm            build .rpm package (Fedora)"
	@echo ""
	@echo "variables:"
	@echo "  BUILD=build    use a different build directory"
	@echo "  TYPE=release   build type (release, debug)"
	@echo "  CXX=clang++    use a different compiler"
