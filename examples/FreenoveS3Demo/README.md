# SuperGUI ESP32-S3 test (Freenove ESP32-S3-WROOM, 480x320 touch LCD)

Demo of every SuperGUI control on a Freenove ESP32-S3-WROOM (8 MB flash, 8 MB PSRAM, both QIO) with a 480x320 touch LCD,
a rotary encoder and two buttons.

## Before you build
1. **Display + touch:** `include/LGFX_User.h` (ST7796 480x320 on SPI2 at 80 MHz with DMA, FT6336 touch on I2C) and `include/Pins.h` describe the
   Freenove ESP32-S3 board this demo was tested on. For another board replace them with your own LovyanGFX `class LGFX` (landscape 480x320,
   touch driver set on the panel so `lcd.getTouch()` works) and pin map.
2. **Wiring:** (pins already used by `Pins.h`: TFT 4,5,10-13, touch 6-9, SD 39-42; flash / octal PSRAM 26-37). `USE_ENCODER` and `USE_BUTTONS` are `false` by default, so no encoder / button pin is read until you fit them and set them to `true`. Do NOT put a button on a pin another part uses: a button on `TOUCH_INT` acts like a button press whenever you touch the screen. edit `PIN_ENC_A`, `PIN_ENC_B`, `PIN_BTN_A`, `PIN_BTN_B` (GPIO numbers) and `BTN_ACTIVE_LOW` at the top of
   `src/main.cpp`. Encoder turns the wrong way? Swap `PIN_ENC_A` and `PIN_ENC_B`. `USE_TOUCH = false` turns touch off. `USE_ENCODER = false` (the default here) leaves the encoder unused, so touch and the buttons drive the UI.
   The placeholder pins avoid the placeholder LCD / touch pins only; check them against your board's schematic
   (flash and octal PSRAM use GPIO 26-37 on this module, so don't use those).
3. **Library:** `platformio.ini` uses the local clone `~/Documents/lib/SuperGUI`; switch to the GitHub URL line when you prefer.

`pio run -e esp32s3 -t upload`

`platformio.ini` sets `flash_mode = qio`, `psram_type = opi`, `memory_type = qio_opi` (QIO flash, OCTAL PSRAM: forcing quad PSRAM makes it fail with "PSRAM chip is not connected, or wrong PSRAM line mode"), the
8 MB partition table, and `-DBOARD_HAS_PSRAM`. The serial log prints the PSRAM size at start: it should say about 8 MB.
`ARDUINO_USB_CDC_ON_BOOT=0` sends Serial to the UART port; set it to 1 if you use the native USB port.

## Using it
Encoder = move the highlight / change a value. ButtonA short = edit the highlighted control. In an edit: ButtonA long = accept,
ButtonB short = cancel, ButtonB long = revert.

Touch (mixes freely with the encoder): tap a control to focus and edit it; tap a list row, tab, menu row, message-box button;
tap or drag a slider track or a color-picker bar to set it by position; drag up/down on a value to change it; long press = accept;
tap outside = cancel. Tuning knobs: `SuperControl::TouchPad` (hit area) and `SuperControl::TouchDragStepPx` (drag sensitivity).

## Notes
- Combo / message box popups redraw the page when they close (SPI panels can't be read back).
- `src/DemoUI.h` is the demo, `src/main.cpp` is only hardware.
