CC_WIN := x86_64-w64-mingw32-gcc
CFLAGS_WIN := -O2 -Wall -mwindows
LDFLAGS_WIN := -lgdi32 -luser32 -lkernel32

CC_NATIVE := gcc

WIN_SRCS := main.c map.c render.c input.c game.c
SDL_SRCS := main_sdl.c map.c render.c render_sdl.c input_sdl.c game.c

SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)

all: Investigator.exe Investigator

Investigator.exe: $(WIN_SRCS)
	$(CC_WIN) $(CFLAGS_WIN) $(WIN_SRCS) -o Investigator.exe $(LDFLAGS_WIN)

Investigator: $(SDL_SRCS)
	$(CC_NATIVE) $(SDL_CFLAGS) $(SDL_SRCS) -o Investigator $(SDL_LIBS) -lm

clean:
	rm -f Investigator.exe Investigator
