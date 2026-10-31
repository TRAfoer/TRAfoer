# Build the C renderer and TIGR together with MinGW GCC.
# olive.c and openPPMp6.c are included by main.c; do not compile them twice.
CC = gcc
CFLAGS ?= -std=c17 -O2
LDLIBS ?= -lopengl32 -lgdi32 -lm

TARGET ?= main
SOURCES := main.c tigr.c
DEPENDENCIES := tigr.h olive.c openPPMp6.c

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
