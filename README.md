Investigator — Wireframe FPS Demo (C, Win32)
Sakari Eskelinen from June 2026
(with AI support)

Overview
- Small demo game showing first-person wireframe movement similar to 1980s vector graphics.
- Movement constrained to horizontal plane (no vertical motion).
- Uses Win32 GDI for windowing and line rendering; no external dependencies.

Files
- main.c : Entry point (refactored to use modules).
- map.c / map.h : Map parsing and storage (map.c implements parsing).
- render.c / render.h : Wireframe rendering utilities and primitive shapes.
- input.c / input.h : Keyboard input handling.
- game.c / game.h : Game logic (collision, teleport handling).
- common.h : Shared types and globals used across modules.
- Makefile: Build recipe for MinGW-w64 (cross-compiler) on Windows.


Building
- Using MinGW-w64 (recommended):

  The project is now split across multiple C files. Use the provided Makefile which builds all sources:

  make

  Or compile manually (example):

  x86_64-w64-mingw32-gcc -O2 -Wall -mwindows main.c map.c render.c input.c game.c -o Investigator.exe -lgdi32 -luser32 -lkernel32

- Using MSVC (Visual Studio Developer Command Prompt):

  cl /O2 /W3 /EHsc main.c user32.lib gdi32.lib

Running
- Run `Investigator.exe` on Windows.
- Controls:
  - W & Up Arrow: Move forward
  - S & Down Arrow: Move back
  - A/D & Left/Right Arrows: Turn
  - Z/C: Strafe left / right

Notes
- The project is intentionally minimal to be easy to inspect and port.
- If you want smoother rendering or texture support, consider using SDL2 or Direct2D.
- Editor diagnostics (e.g. VS Code) may show include errors for Windows headers (like `windows.h`) if the C/C++ extension doesn't know your compiler include paths. To fix this in VS Code, set your `compilerPath` or `includePath` in `.vscode/c_cpp_properties.json` to point to your MinGW-w64 installation so IntelliSense can resolve system headers.

License
- Try and share if you like, but don't claim as your own

Change notes (movement & teleports)
- Added per-object movement: cubes wander, pyramids chase the player.
- Map-file flags: `move=wandering|chase|none`, `mspeed=VALUE`, `turn=VALUE` (deg/s), `maxdist=VALUE`.
- Pyramids now turn toward the player at a limited turning speed (`turn`), so they don't instantaneously match the player's rotation.
- Teleporter map objects are rendered visibly now (they were previously skipped).
- Teleport target Y is clamped to avoid spawning the player under the ground (small values < 0.5 are bumped to 1.0).
- `map.c`'s `loadMap` was refactored into helper functions to improve readability.

Where to configure movement defaults
- The default movement/turn values are set when objects are created in `map.c` (near `loadMap`). Per-object values from the map file override these defaults.

Example map tokens:
- `cube 1 0 5 mspeed=0.04 turn=30 maxdist=5` — faster wandering cube
- `pyramid 3 0 10 move=chase mspeed=0.07 turn=20` — pyramid that chases the player with limited turn speed