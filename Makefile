CC = gcc
SRCS = $(wildcard src/*.c)
OUT = build/cpu8085
INCLUDES = -Iinclude
OBJS = $(patsubst src/%.c, build/%.o, $(SRCS))

ifeq ($(OS), Windows_NT)
	SHELL = powershell.exe
	MKDIR = if (!(Test-Path build)) { New-Item -ItemType Directory -Path build | Out-Null }
	LIBS = -lws2_32
else
	MKDIR = mkdir -p build
	LIBS =
endif

windows: $(OUT)
linux: $(OUT)

$(OUT): $(OBJS)
	$(CC) $(OBJS) -o $(OUT) $(LIBS)

build/%.o: src/%.c
	$(MKDIR)
	$(CC) -c $< -o $@ $(INCLUDES)