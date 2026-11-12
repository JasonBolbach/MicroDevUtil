# SuperGUI Pico W test

Demo of every SuperGUI control on a Raspberry Pi Pico W with a 320x240 SPI LCD, a rotary encoder and two buttons.

## Before you build
1. **Display:** replace `include/LGFX_User_TE004.h` (class `LGFX_TE004`, ST7789 on the TE004 board) with your own LovyanGFX class, and set the pins in `include/Pins.h` (landscape 320x240).
2. **Wiring:** edit `PIN_Encoder_A`, `PIN_Encoder_B`, `PIN_ButtonA`, `PIN_ButtonB` in `include/Pins.h` (GP numbers), and `BTN_ACTIVE_LOW` at the top of `src/main.cpp`.
   Encoder turns the wrong way? Swap `PIN_ENC_A` and `PIN_ENC_B`.
3. **Library:** `platformio.ini` uses the local clone `~/Documents/lib/SuperGUI`; switch to the GitHub URL line when you prefer.

`pio run -e picow -t upload`

## Using it
Encoder = move the highlight / change a value. ButtonA short = edit the highlighted control. In an edit: ButtonA long = accept,
ButtonB short = cancel, ButtonB long = revert. Tabs at the top switch between four pages (Input, Choose, Values, Menu).
The bottom strip shows the last event and some live values.

## Things to check on real hardware
- Does `Dim()` (the out-of-focus dimming) feel fast enough? If not, set `PreviewBrightness = 100` on the controls.
- Combo / message box popups redraw the page when they close (SPI panels can't be read back).
- Touch is not wired in this demo. If your LGFX class has a touch driver, see `examples/TouchFormExample` in SuperGUI.

`src/DemoUI.h` is the demo; `src/main.cpp` is only hardware.
