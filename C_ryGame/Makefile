# Build the C renderer and TIGR together with GCC.
# olive.c / openPPMp6.c / ChatReader.c / SigAnima.c are included by main.c;
# do not compile them twice. JsonLib is a separate translation unit.
CC = gcc
CFLAGS ?= -O1
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Linux)
	LDLIBS ?= -lGLU -lGL -lX11 -lm -ldl -lpthread
else
	LDLIBS ?= -lopengl32 -lgdi32 -lm
endif
TARGET ?= main
OBJECTS := main.o tigr.o miniaudio.o jsonLib.o md5.o
CPPFLAGS += -DTIGR_DO_NOT_PRESERVE_WINDOW_POSITION
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o "$@" $(LDLIBS)

# 每个翻译单元精确声明自己的依赖，避免 main.c/tigr.c 改动时连累 miniaudio.o
main.o: main.c tigr.h olive.c ChatReader.c openPPMp6.c NoteDef.h SigAnima.c miniaudio.h Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

jsonLib.o: JsonLib/jsonLib.c JsonLib/jsonLib.h JsonLib/jsonDataStruct.h JsonLib/md5.h Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

md5.o: JsonLib/md5.c JsonLib/md5.h Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
	
tigr.o: tigr.c tigr.h Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

miniaudio.o: miniaudio.c miniaudio.h Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	$(RM) "$(TARGET)" $(OBJECTS)