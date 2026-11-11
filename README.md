# SuperInput

Header-only input helpers for Arduino-framework microcontrollers.

| Header | What it does |
|---|---|
| `SuperButton.h` | Debounced button: short / medium / long click plus a double-click flag. Any number of buttons, all serviced by one background timer. |
| `SuperEncoder.h` | Quadrature rotary encoder with bounce rejection and 3-speed acceleration (slow / medium / fast). Interrupt-driven. |
| `SuperTouch.h` | Single-touch gestures (short / medium / long click, drag) from a touch-read function you supply. |

**Platforms:** ESP32 (all variants) and RP2040 / RP2350 with the arduino-pico core are timer/interrupt-driven. Other boards fall back to polling (see below).

## Install (PlatformIO)

Add to `platformio.ini`:

```ini
lib_deps =
    https://github.com/JasonBolbach/SuperInput.git#v1.0.0
```

Drop `#v1.0.0` to always track the latest commit on `main`.

## SuperButton

```cpp
#include <SuperButton.h>

SuperButton Buttons[] = { {0, true}, {4, true} };   // { GPIO, activeLow }

void setup() { for (auto &b : Buttons) b.Begin(INPUT_PULLUP); }

void loop() {
  uint8_t ev;
  while (Buttons[0].GetEvent(ev)) {
    // ev & 0x0F : 1 = short, 2 = medium, 3 = long
    // ev & SuperButton::SB_DOUBLE : a second press followed quickly
  }
}
```

- Debounce: level must be stable for 20 ms. Thresholds `Click1_MAX` (175 ms), `Click2_MAX` (800 ms), `ClickD_MAX` (150 ms) are per-instance.
- Buttons must be global/static. On boards without a timer back-end (or with `#define SUPERBUTTON_NO_TIMER`), call `SuperButton::Service()` from `loop()`.

## SuperEncoder

```cpp
#include <SuperEncoder.h>

SuperEncoder Enc(PIN_A, PIN_B);          // 3rd arg: 4 = full cycle per detent, 2 = half

void setup() { Enc.Begin(); Enc.SetLimits(0, 255); }

void loop() {
  int32_t v = Enc.GetPosition();         // or Enc.GetDelta() for change since last call
}
```

- Full Gray-code state machine on both channels; a count is committed only on arriving at a detent, so bounce and part-way rocking produce nothing, and a direction reversal always counts at the slow rate.
- Speed: `Fast_MS` (25) and `Medium_MS` (85) between detents; step sizes `StepSlow` / `StepMedium` / `StepFast` (1 / 5 / 20).
- `Begin(false)` uses polling instead of interrupts: call `Poll()` every loop pass.
- The knob should rest on a detent when `Begin()` runs.

## SuperTouch

```cpp
#include <SuperTouch.h>

bool MyGetTouch(int16_t &X, int16_t &Y) { /* read controller; return true if touched */ }
SuperTouch Touch(MyGetTouch);

void loop() {
  Touch.Poll();
  SuperTouchEvent e;
  while (Touch.GetEvent(e)) {
    // e.Type: ST_SHORT_CLICK / ST_MEDIUM_CLICK / ST_LONG_CLICK / ST_DRAG
    // e.Double (clicks): a second click followed quickly, like SuperButton's SB_DOUBLE
    // e.Phase (drag): ST_DRAG_START / ST_DRAG_MOVE / ST_DRAG_END
  }
}
```

- `Radius` (10 px) is how far the finger may move and still count as a click.
- Double-click: a second click within `ClickD_MAX` (150 ms) and `DoubleRadius` (30 px) of the first sets `e.Double`; the event describes the first click. Clicks are therefore delivered `ClickD_MAX` ms after release; set `ClickD_MAX = 0` to disable double-click and get them immediately.
- Polled from `loop()` on purpose: touch controllers usually share the SPI/I2C bus with the display.

See `examples/` for complete sketches.

## License

MIT — see `LICENSE`.
