# Makefile for Yukon Solitaire

# The text version needs only a C compiler (gcc).
# The GUI version also needs SDL3 installed.
CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -g

# Target names
TARGET  = yukon       # text version
GUI     = yukon_gui   # GUI version

# Source files shared by both versions
CORE_SRCS = card.c game.c

# All source files for each version
CLI_SRCS = $(CORE_SRCS) display.c main.c
GUI_SRCS = $(CORE_SRCS) gui.c

# SDL3 flags
SDL_CFLAGS = -IC:/Users/mhhk0/Desktop/SDL3-3.4.8/x86_64-w64-mingw32/include
SDL_LIBS   = -LC:/Users/mhhk0/Desktop/SDL3-3.4.8/x86_64-w64-mingw32/lib -lSDL3

# Default target: build the text version
all: $(TARGET)

# Build the GUI version
gui: $(GUI)

$(TARGET): $(CLI_SRCS) yukon.h
	$(CC) $(CFLAGS) -o $@ $(CLI_SRCS)

$(GUI): $(GUI_SRCS) yukon.h
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $@ $(GUI_SRCS) $(SDL_LIBS)

clean:
	rm -f $(TARGET) $(GUI) *.o

.PHONY: all gui clean
