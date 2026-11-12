# SuperGUI Pico W demo

Every SuperGUI control on a Raspberry Pi **Pico W** (RP2040) or **Pico 2 W** (RP2350) with the TE004 board: a 320x240 ST7789 SPI display,
a rotary encoder with a push button, and one board button. The demo UI (`src/DemoUI.h`) has four tab pages: Input, Choose, Values, Menu.

```
pio run -e picow  -t upload      # Pico W
pio run -e pico2w -t upload      # Pico 2 W
```
Open the serial monitor (115200) on the Pico's **USB** port for the memory and draw-time log.

## Files
| File | What it holds |
|---|---|
| `include/Pins.h` | every GPIO: display (SPI0), encoder, buttons. Compile-time checks stop the build if two parts share a pin, a pin belongs to the wireless chip (GP23 / 24 / 25), or the SCLK / MOSI pins do not match the SPI port. |
| `include/LGFX_User_TE004.h` | the LovyanGFX display class `LGFX_TE004`: ST7789 240x320 on SPI0 rotated to landscape (320x240), PWM backlight. |
| `src/main.cpp` | hardware only: options, the serial report, `setup()` / `loop()`. |
| `src/DemoUI.h` | the demo UI (portable: LovyanGFX + SuperGUI only). |

## Using another board or display
Copy `LGFX_User_TE004.h` and `Pins.h`, change the panel class and the numbers, and keep `lcd` landscape (this layout is 320x240).
Encoder turns the wrong way? Swap `PIN_ENC_A` and `PIN_ENC_B` in `Pins.h`. Note that GP0 / GP1 (the encoder here) are also UART0, so `Serial1` is
not available for debug output with this wiring; the demo uses the USB serial port.

## Options (top of `src/main.cpp`)
- `USE_ENCODER`, `USE_BUTTONS`: set `false` for a part that is not fitted (its pins are then never read).
- `BTN_ACTIVE_LOW`: buttons wired to GND with the internal pull-up.
- `USE_SHARED_BUFFER`: one render buffer lent to each control instead of a buffer per control. **On this board it matters**: the Pico W has 264 KB of
  RAM (about 71 KB is used by the program before the UI starts), and per-control buffers need roughly 150 KB on the busiest page against
  roughly 90 KB with the shared buffer. The Pico 2 W has 520 KB.
- `USE_DMA` (needs `USE_SHARED_BUFFER`) and `LABEL_OPAQUE` (needed for DMA with labelled controls): two shared buffers, so the next control is
  rendered while the previous one is sent to the display. About 138 KB for the UI, which is a lot on a Pico W; try it on a Pico 2 W first.

## The serial log
At startup: the display size and heap, then the first full-page redraw with its render / dim / push times, the memory mode (shared buffer, DMA,
opaque labels), how much heap the UI took, and the full-page redraw time. Every 5 seconds: free heap, shared-buffer size and the time split of the
draws in that window.

## Things to check on real hardware
- Does the out-of-focus dimming (`PreviewBrightness`, 85%) feel fast enough? If not, set `PreviewBrightness = 100` on the controls.
- Combo and message box popups repaint what they covered when they close (SPI panels cannot be read back).
- Touch is not wired in this demo. With a touch driver in your LGFX class, see `examples/TouchFormExample` in SuperGUI.

## Styles
The Choose page has a **Sleek style** check box that switches every control between the standard frames and the Sleek look (label on the frame
line) live. Labels are small and left-aligned throughout: `DemoUI::LabelFont` (default `Font0`, set it to `&lgfx::fonts::Font2` for larger labels)
is applied to every control in `Begin()`.
