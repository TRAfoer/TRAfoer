# Default targets are a pure-C, no-window voxel analysis program.
.DEFAULT_GOAL := all
PYTHON ?= python
ifeq ($(origin CC),default)
CC := gcc
endif
ifeq ($(origin CXX),default)
CXX := g++
endif
AR ?= ar

HOST_PLATFORM := $(shell $(PYTHON) -c "import sys; print(sys.platform)")
ifeq ($(HOST_PLATFORM),win32)
ifeq ($(findstring msys,$(MAKE_HOST)),)
SHELL := cmd.exe
.SHELLFLAGS := /C
endif
EXE := .exe
C_RUNTIME := -static-libgcc
CXX_RUNTIME := -static-libgcc -static-libstdc++
else
EXE :=
C_RUNTIME :=
CXX_RUNTIME := -pthread
endif

CPPFLAGS += -Isrc
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++17 -Wall -Wextra
DEPFLAGS := -MMD -MP
HEADLESS_OBJECTS := build/headless.o build/voxel.o build/voxel_export.o

.PHONY: all run test test-core test-export test-args test-cli smoke clean help deps test-rigid test-physics view viewer test-viewer
all: build/pixphysiccc$(EXE)

build/headless.o: src/main.c src/cli.h src/voxel.h src/voxel_export.h
	@$(PYTHON) tools/build_helpers.py mkdir build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/voxel.o: src/voxel.c src/voxel.h
	@$(PYTHON) tools/build_helpers.py mkdir build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/voxel_export.o: src/voxel_export.c src/voxel_export.h src/voxel.h
	@$(PYTHON) tools/build_helpers.py mkdir build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/pixphysiccc$(EXE): $(HEADLESS_OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ -lm $(C_RUNTIME)

build/test_voxel$(EXE): tests/test_voxel.c build/voxel.o src/voxel.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_voxel.c build/voxel.o -lm $(C_RUNTIME)

build/test_voxel_export$(EXE): tests/test_voxel_export.c build/voxel.o build/voxel_export.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lm $(C_RUNTIME)

build/test_cli_args$(EXE): tests/test_cli_args.c src/main.c src/cli.h
	@$(PYTHON) tools/build_helpers.py mkdir build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DVX_CLI_ARGS_ONLY $(LDFLAGS) -o $@ tests/test_cli_args.c src/main.c -lm $(C_RUNTIME)

test-core: build/test_voxel$(EXE)
	$(PYTHON) tools/build_helpers.py run build/test_voxel$(EXE)

test-export: build/test_voxel_export$(EXE)
	$(PYTHON) tools/build_helpers.py run build/test_voxel_export$(EXE)

test-args: build/test_cli_args$(EXE)
	$(PYTHON) tools/build_helpers.py run build/test_cli_args$(EXE)

test-cli: all
	$(PYTHON) tools/headless_smoke.py

test: test-core test-export test-args test-cli
smoke: test-cli

run: all
	$(PYTHON) tools/build_helpers.py run build/pixphysiccc$(EXE)

clean:
	$(PYTHON) tools/build_helpers.py clean

help:
	@$(PYTHON) -c "print('make [-j4] [all | run | test | smoke | clean]', 'Default: pure C CLI, no windows or graphics libraries.', 'Explicit display: make view; build only: make viewer; graphics check: make test-viewer', 'Optional backend: make test-rigid; dependencies: make deps', sep=chr(10))"

# The opt-in display draws voxel surfaces transformed by the real physics world.
ifeq ($(HOST_PLATFORM),win32)
VIEW_LIBS := -lopengl32 -lgdi32 -lwinmm
else
VIEW_LIBS := -lGL -lX11 -lm -lpthread -ldl -lrt
endif
RAYLIB_FLAGS := -Ivendor/raylib/src -Ivendor/raylib/src/external/glfw/include \
  -DPLATFORM_DESKTOP -DPLATFORM_DESKTOP_GLFW -DGRAPHICS_API_OPENGL_33
RAYLIB_MODULES := rcore rshapes rtextures rtext rmodels raudio utils rglfw
RAYLIB_SOURCE_FILES := $(addprefix vendor/raylib/src/,$(addsuffix .c,$(RAYLIB_MODULES)))
RAYLIB_OBJECTS := $(addprefix build/raylib/,$(addsuffix .o,$(RAYLIB_MODULES)))

$(RAYLIB_SOURCE_FILES) vendor/raylib/src/raylib.h &:
	$(PYTHON) tools/bootstrap.py raylib

build/raylib/%.o: vendor/raylib/src/%.c vendor/raylib/src/raylib.h
	@$(PYTHON) tools/build_helpers.py mkdir "$(@D)"
	$(CC) -O2 -std=c99 $(RAYLIB_FLAGS) $(DEPFLAGS) -c $< -o $@

build/libraylib.a: $(RAYLIB_OBJECTS)
	$(AR) rcs $@ $(RAYLIB_OBJECTS)

build/viewer.o: src/viewer.c src/simulation.h src/rigid.h src/voxel.h src/voxel_export.h vendor/raylib/src/raylib.h
	@$(PYTHON) tools/build_helpers.py mkdir build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_FLAGS) $(DEPFLAGS) -c $< -o $@

build/simulation.o: src/simulation.c src/simulation.h src/voxel.h src/rigid.h
	@$(PYTHON) tools/build_helpers.py mkdir build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/pixphysiccc-viewer$(EXE): build/viewer.o build/simulation.o build/voxel.o build/voxel_export.o build/rigid.o build/libjolt.a build/libraylib.a
	$(CXX) $(LDFLAGS) -o $@ $^ $(VIEW_LIBS) -lm $(CXX_RUNTIME)

viewer: build/pixphysiccc-viewer$(EXE)

view: viewer
	$(PYTHON) tools/build_helpers.py run build/pixphysiccc-viewer$(EXE)

test-viewer: viewer
	$(PYTHON) tools/viewer_check.py

# Jolt is used by the physical viewer/checks; default CLI goals remain independent.
JOLT_FLAGS := -DNDEBUG -Ivendor/JoltPhysics -fno-rtti -fno-exceptions -ffp-contract=off
PHYSICS_GOALS := view viewer test-viewer test-physics test-rigid build/test_rigid$(EXE) build/test_simulation$(EXE) build/libjolt.a build/pixphysiccc-viewer$(EXE)
ifneq ($(filter $(PHYSICS_GOALS),$(MAKECMDGOALS)),)
-include build/sources.mk
endif
JOLT_OBJECTS := $(patsubst vendor/JoltPhysics/Jolt/%.cpp,build/jolt/%.o,$(JOLT_SOURCES))

build/sources.mk: tools/build_helpers.py tools/bootstrap.py vendor/JoltPhysics/Jolt/Jolt.cmake
	$(PYTHON) tools/bootstrap.py JoltPhysics
	$(PYTHON) tools/build_helpers.py sources

vendor/JoltPhysics/Jolt/Jolt.cmake:
	$(PYTHON) tools/bootstrap.py JoltPhysics

build/jolt-objects.rsp: build/sources.mk
	$(PYTHON) tools/build_helpers.py sources

build/jolt/%.o: vendor/JoltPhysics/Jolt/%.cpp
	@$(PYTHON) tools/build_helpers.py mkdir "$(@D)"
	$(CXX) $(CXXFLAGS) $(JOLT_FLAGS) $(DEPFLAGS) -c $< -o $@

build/libjolt.a: $(JOLT_OBJECTS) build/jolt-objects.rsp
	$(AR) rcs $@ @build/jolt-objects.rsp

build/rigid.o: src/rigid.cpp src/rigid.h
	@$(PYTHON) tools/build_helpers.py mkdir build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(JOLT_FLAGS) $(DEPFLAGS) -c $< -o $@

build/test_rigid$(EXE): tests/test_rigid.c build/rigid.o build/libjolt.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -c tests/test_rigid.c -o build/test_rigid.o
	$(CXX) $(LDFLAGS) -o $@ build/test_rigid.o build/rigid.o build/libjolt.a $(CXX_RUNTIME)

test-rigid: build/test_rigid$(EXE)
	$(PYTHON) tools/build_helpers.py run build/test_rigid$(EXE)

build/test_simulation.o: tests/test_simulation.c src/simulation.h src/voxel.h src/rigid.h
	@$(PYTHON) tools/build_helpers.py mkdir build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/test_simulation$(EXE): build/test_simulation.o build/simulation.o build/voxel.o build/rigid.o build/libjolt.a
	$(CXX) $(LDFLAGS) -o $@ $^ -lm $(CXX_RUNTIME)

test-physics: test-rigid build/test_simulation$(EXE)
	$(PYTHON) tools/build_helpers.py run build/test_simulation$(EXE)

deps:
	$(PYTHON) tools/bootstrap.py
	$(PYTHON) tools/build_helpers.py sources

-include $(HEADLESS_OBJECTS:.o=.d)
ifneq ($(filter view viewer test-viewer build/pixphysiccc-viewer$(EXE),$(MAKECMDGOALS)),)
-include build/viewer.d $(RAYLIB_OBJECTS:.o=.d)
endif
ifneq ($(filter $(PHYSICS_GOALS),$(MAKECMDGOALS)),)
-include build/rigid.d build/simulation.d build/test_simulation.d $(JOLT_OBJECTS:.o=.d)
endif
