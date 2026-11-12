// SuperGUI hardware test: Raspberry Pi Pico 2 W + 320x240 SPI LCD + rotary encoder + two buttons.
//
// 1. Put your LovyanGFX display class in include/MyLGFX.h (class name LGFX).
// 2. Set the four pin numbers below (GP numbers) to match your wiring.
// 3. Upload, open the serial monitor (115200) if you want the log lines.
//
// Encoder = move the highlight / change a value.  ButtonA short = edit the highlighted control.
// Inside an edit: ButtonA long = accept, ButtonB short = cancel, ButtonB long = revert.
#include <Arduino.h>
#include "MyLGFX.h"
#include <SuperButton.h>
#include <SuperEncoder.h>
#include "DemoUI.h"

// ---------------- WIRING: edit these ----------------
constexpr uint8_t PIN_ENC_A = 2;          // encoder channel A
constexpr uint8_t PIN_ENC_B = 3;          // encoder channel B
constexpr uint8_t PIN_BTN_A = 4;          // ButtonA: select / edit / accept (long)
constexpr uint8_t PIN_BTN_B = 5;          // ButtonB: cancel / revert (long)
constexpr bool    BTN_ACTIVE_LOW = true;  // buttons wired to GND with the internal pull-up
constexpr bool    USE_SHARED_BUFFER = false;  // true = ONE shared render buffer (about the size of the biggest control) instead of a buffer per control: much less RAM, nearly the same speed
constexpr bool    USE_DMA = false;           // needs USE_SHARED_BUFFER: two shared buffers, the CPU renders the next control while the previous one is sent to the display
constexpr bool    LABEL_OPAQUE = false;      // labels get a solid background (your screen colour) instead of being transparent; required for DMA with labelled controls
// Encoder detent / direction: swap PIN_ENC_A and PIN_ENC_B if it turns the wrong way.
// ----------------------------------------------------

LGFX         lcd;
SuperEncoder Enc(PIN_ENC_A, PIN_ENC_B);
SuperButton  BtnA(PIN_BTN_A, BTN_ACTIVE_LOW), BtnB(PIN_BTN_B, BTN_ACTIVE_LOW);
DemoUI       ui;


// Memory / speed report (serial monitor): how much RAM the UI took, and how long a full redraw of the page takes.
static inline size_t ESP_FREE() { return rp2040.getFreeHeap(); }
void report(size_t heap0) {
  size_t used = heap0 > ESP_FREE() ? heap0 - ESP_FREE() : 0;
  uint32_t t0 = millis();
  ui.Form->Draw();
  uint32_t ms = millis() - t0;
  Serial.printf("shared buffer: %s (%u bytes)   UI took %u bytes of heap   full page redraw: %u ms\n",
                SuperControl::SharedBuffer ? "ON" : "off", (unsigned)SuperControl::SharedBytes(), (unsigned)used, (unsigned)ms);
}

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.fillScreen(TFT_BLACK);
  Serial.printf("display %d x %d\n", (int)lcd.width(), (int)lcd.height());

  Enc.Begin();
  BtnA.Begin(BTN_ACTIVE_LOW ? INPUT_PULLUP : INPUT);
  BtnB.Begin(BTN_ACTIVE_LOW ? INPUT_PULLUP : INPUT);

  SuperControl::SharedBuffer = USE_SHARED_BUFFER;
  SuperControl::UseDMA = USE_DMA;
  SuperControl::LabelOpaque = LABEL_OPAQUE;   // must be set before the UI is created / first drawn
  size_t heap0 = ESP_FREE();
  ui.Begin(&lcd, &Enc, &BtnA, &BtnB);
  report(heap0);
}

void loop() {
  ui.Service();     // never blocks for long: focus, edits, graph, status line
  delay(2);
}
