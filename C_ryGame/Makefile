# Build the C renderer and TIGR together with GCC.
# olive.c / openPPMp6.c / ChatReader.c / SigAnima.c are included by main.c;
# do not compile them twice. JsonLib is a separate translation unit.
CC = gcc
CFLAGS ?= -O1
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Linux)
	PLATFORM := linux
	EXEEXT :=
	LDLIBS ?= -lGLU -lGL -lX11 -lm -ldl -lpthread
else ifeq ($(UNAME_S), Darwin)
	PLATFORM := macos
	EXEEXT :=
	LDLIBS ?= -framework OpenGL -framework Cocoa -lm -ldl -lpthread
else
	PLATFORM := windows
	EXEEXT := .exe
	LDLIBS ?= -lopengl32 -lgdi32 -lm
endif
BUILD_DIR := tmp/$(PLATFORM)
TARGET ?= main
ifeq ($(EXEEXT), .exe)
PROGRAM := $(BUILD_DIR)/$(patsubst %.exe,%,$(TARGET)).exe
else
PROGRAM := $(BUILD_DIR)/$(TARGET)
endif
OBJECTS := $(addprefix $(BUILD_DIR)/,main.o tigr.o miniaudio.o jsonLib.o md5.o timeTicker.o)
TEST_PROGRAM := $(BUILD_DIR)/timeTicker_test$(EXEEXT)
CPPFLAGS += -DTIGR_DO_NOT_PRESERVE_WINDOW_POSITION
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
.PHONY: all run test clean

all: $(PROGRAM)

$(PROGRAM): $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o "$@" $(LDLIBS)

$(BUILD_DIR):
	mkdir -p "$@"

# 每个翻译单元精确声明自己的依赖，避免 main.c/tigr.c 改动时连累 miniaudio.o
$(BUILD_DIR)/main.o: main.c tigr.h olive.c ChatReader.c openPPMp6.c NoteDef.h SigAnima.c miniaudio.h timeTicker.h Makefile | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/jsonLib.o: JsonLib/jsonLib.c JsonLib/jsonLib.h JsonLib/jsonDataStruct.h JsonLib/md5.h Makefile | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/md5.o: JsonLib/md5.c JsonLib/md5.h Makefile | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
	
$(BUILD_DIR)/tigr.o: tigr.c tigr.h Makefile | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/miniaudio.o: miniaudio.c miniaudio.h Makefile | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/timeTicker.o: timeTicker.c timeTicker.h Makefile | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TEST_PROGRAM): tests/timeTicker_test.c timeTicker.c timeTicker.h Makefile | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c99 -Wall -Wextra -Werror -pedantic tests/timeTicker_test.c timeTicker.c $(LDFLAGS) -o "$@"

test: $(TEST_PROGRAM)
	./$(TEST_PROGRAM)

run: $(PROGRAM)
	./$(PROGRAM)

clean:
	$(RM) "$(PROGRAM)" $(OBJECTS) "$(TEST_PROGRAM)"
