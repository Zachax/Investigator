CC := x86_64-w64-mingw32-gcc
CFLAGS := -O2 -Wall -mwindows
LDFLAGS := -lgdi32 -luser32 -lkernel32

all: Investigator.exe

Investigator.exe: main.c
	$(CC) $(CFLAGS) main.c -o Investigator.exe $(LDFLAGS)

clean:
	rm -f Investigator.exe
