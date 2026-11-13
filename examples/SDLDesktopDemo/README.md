# SuperGUI desktop demo (SDL)

Every SuperGUI control in a desktop window, no hardware needed. It has its own copy of `DemoUI.h` (the same screen as the Freenove ESP32-S3 demo, 480x320, five tab pages),
so it is a quick way to try the controls, work on them, or reproduce a problem before going to the board.

| Input | Desktop emulation |
|---|---|
| Encoder | mouse wheel |
| ButtonA | wheel click (short = edit / select, long = accept) |
| ButtonB | right click (short = cancel, long = revert) |
| Touch | left mouse button (LovyanGFX's SDL touch emulation, read with `lcd.getTouch()` like on real hardware) |

## Build and run
```
brew install sdl2 cmake            # macOS (Linux: your package manager's SDL2 + cmake)
cmake -S . -B build
cmake --build build -j
build/SuperGUI_SDL_Demo
```
LovyanGFX is looked up next to this repository (`../../../LovyanGFX`). Elsewhere: `cmake -S . -B build -DLGFX_ROOT=/path/to/LovyanGFX`.

## How it is put together
- `main.cpp`: creates the SDL display (`LGFX(480, 320, 2)`), starts the desktop input, hands LovyanGFX's touch to `SuperTouch`, and runs `DemoUI`.
- `sdl_main.cpp`: LovyanGFX's SDL entry point (calls `setup()` / `loop()` until the window is closed).
- `../../extras/desktop/`: `desktop_input.h` (wheel and mouse buttons as encoder / buttons) and a minimal `Arduino.h`, shared by any desktop build.
- `DemoUI.h`: the demo UI. It started as a copy of the Freenove S3 demo's and is now independent (change it freely here). Only LovyanGFX and SuperGUI are used in it, which is why it runs on the desktop and the board alike.

The memory options (`SuperControl::SharedBuffer`, `UseDMA`, `LabelOpaque`) can be tried here as well; see the commented line in `main.cpp`.
