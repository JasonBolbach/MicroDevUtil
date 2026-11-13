# SuperGUI Form Designer (work in progress)

A desktop-only designer for SuperGUI forms: macOS, Linux and Windows, LovyanGFX + SDL2, one CMake project. Forms are saved as JSON (designer
only); the output is C++ (`X.designer.h`, regenerated every time, plus a user file written once).

**Status: step 1, the input spike.** It opens the designer window (mock panel layout, a 320x240 form outline), and shows every mouse, keyboard,
text and resize event in the designer's logical pixels. Arrow keys change the form size (limits: default 320x240, maximum 1024x768,
`src/DesignerConfig.h`).

## Build and run
```
brew install sdl2 cmake            # macOS (Linux: your package manager's SDL2 + cmake)
cmake -S . -B build
cmake --build build -j
build/FormDesigner
```
LovyanGFX is looked up next to the SuperGUI repository (`../../../LovyanGFX`); otherwise `-DLGFX_ROOT=/path/to/LovyanGFX`.
On Windows point `CMakeLists.txt` at your SDL2 development files (`C:/SDL2`).

## Checks
`FD_SELFTEST=1 build/FormDesigner` runs an automatic check of the input layer with synthetic events (opens the window briefly, exits 0 / 1).
`FD_SNAPSHOT=frame.raw build/FormDesigner` writes the first frame (raw big-endian RGB565, 1400x820) and exits.

## Notes
- LovyanGFX's SDL panel treats plain `1`-`6` as "rescale the window" and `r` / `l` as "rotate the screen". The designer has text fields, so
  `main.cpp` sets an impossible modifier combination (`Panel_sdl::setShortcutKeymod`) to switch those shortcuts off.
- Mouse positions are scaled from window points to logical pixels with the window's current size, so resizing the window works.
