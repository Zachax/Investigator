Investigator — Wireframe FPS Demo (C, Win32)
Sakari Eskelinen from June 2026
(with AI support)

Overview
- Small demo game showing first-person wireframe movement similar to 1980s vector graphics.
- Movement constrained to horizontal plane (no vertical motion).
- Uses Win32 GDI for windowing and line rendering; no external dependencies.

Files
- main.c : Source code for the demo.
- Makefile: Build recipe for MinGW-w64 (cross-compiler) on Windows.

Building
- Using MinGW-w64 (recommended):

  Open a MinGW-w64 shell (or use MSYS2) and run:

  gcc -O2 -Wall -mwindows main.c -o Investigator.exe -lgdi32

  Or with the included Makefile (if using a mingw cross-compiler):

  make

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
- AI text messages seem so darn verbose sometimes, and even it doesn't always keep up with the real code status.

License
- Try and share if you like, but don't claim as your own