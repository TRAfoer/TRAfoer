# Build the C renderer and TIGR together with MinGW GCC.
# olive.c and openPPMp6.c are included by main.c; do not compile them twice.
CC = gcc
CFLAGS ?=  -O2
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Linux)
	LDLIBS ?= -lGLU -lGL -lX11 -lm
else
	LDLIBS ?= -lopengl32 -lgdi32 -lm
TARGET ?= main
SOURCES := main.c tigr.c
DEPENDENCIES := tigr.h olive.c openPPMp6.c
CPPFLAGS += -DTIGR_DO_NOT_PRESERVE_WINDOW_POSITION
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(DEPENDENCIES) Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o "$@" $(LDLIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	$(RM) "$(TARGET)"
