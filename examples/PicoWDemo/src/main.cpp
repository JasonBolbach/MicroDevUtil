// SuperGUI hardware demo: Raspberry Pi Pico W (RP2040) or Pico 2 W (RP2350) + 320x240 SPI LCD + rotary encoder + two buttons.
//
// 1. The display class is include/LGFX_User_TE004.h (class LGFX_TE004, the TE004 board). Replace it for another display.
// 2. All GPIO numbers are in include/Pins.h; the build stops if two parts share a pin or a pin is not usable.
// 3. Upload, open the serial monitor (115200, the Pico's USB port) for the memory / draw-time log lines.
//
// Encoder = move the highlight / change a value.  ButtonA short = edit the highlighted control.
// Inside an edit: ButtonA long = accept, ButtonB short = cancel, ButtonB long = revert.
#include <Arduino.h>
#include "Pins.h"
#include "LGFX_User_TE004.h"
#include <SuperButton.h>
#include <SuperEncoder.h>
#include "DemoUI.h"

// Debug output goes to the USB serial port (`Serial`, the USB CDC port on the Pico). The hardware UART (Serial1) is GP0 / GP1, which are the
// encoder pins here, so it is not an option with this wiring.
#define DBG Serial

// ---------------- OPTIONS: edit these (pins are in Pins.h) ----------------
constexpr bool    BTN_ACTIVE_LOW = true;  // buttons wired to GND with the internal pull-up
constexpr bool    USE_ENCODER = true;     // false = no encoder fitted: its pins are not touched
constexpr bool    USE_BUTTONS = true;     // false = no buttons fitted: nothing is read from their pins
constexpr bool    USE_SHARED_BUFFER = true;   // true = ONE shared render buffer (about the size of the biggest control) instead of a buffer per control: much less RAM, nearly the same speed
constexpr bool    USE_DMA = false;           // needs USE_SHARED_BUFFER: two shared buffers, the CPU renders the next control while the previous one is sent to the display
constexpr bool    LABEL_OPAQUE = false;      // labels get a solid background (your screen colour) instead of being transparent; required for DMA with labelled controls
// Encoder turns the wrong way? Swap PIN_ENC_A and PIN_ENC_B in Pins.h.
// ----------------------------------------------------

LGFX_TE004   lcd;
SuperEncoder Enc(PIN_ENC_A, PIN_ENC_B);
SuperButton  BtnA(PIN_BTN_A, BTN_ACTIVE_LOW), BtnB(PIN_BTN_B, BTN_ACTIVE_LOW);
DemoUI       ui;


// Where the draw time goes (microseconds, summed over the draws since the last call), then reset.
void printStats(const char* what) {
  uint32_t n = SuperControl::StatDraws;
  DBG.printf("[%s] draws=%u  render=%u us  dim=%u us  push=%u us  (per draw: render %u, dim %u, push %u us)\n", what, (unsigned)n,
             (unsigned)SuperControl::StatRenderUs, (unsigned)SuperControl::StatDimUs, (unsigned)SuperControl::StatPushUs,
             n ? (unsigned)(SuperControl::StatRenderUs / n) : 0, n ? (unsigned)(SuperControl::StatDimUs / n) : 0, n ? (unsigned)(SuperControl::StatPushUs / n) : 0);
  SuperControl::ResetStats();
}

// Memory / speed report (serial monitor): how much RAM the UI took, and how long a full redraw of the page takes.
static inline size_t FREE_HEAP() { return rp2040.getFreeHeap(); }
void report(size_t heap0) {
  size_t used = heap0 > FREE_HEAP() ? heap0 - FREE_HEAP() : 0;
  SuperControl::CollectStats = true;
  SuperControl::ResetStats();
  uint32_t t0 = millis();
  ui.Form->Draw();
  uint32_t ms = millis() - t0;
  printStats("first page redraw");
  DBG.printf("shared buffer: %s (%u bytes), DMA: %s, opaque labels: %s   UI took %u bytes of heap   full page redraw: %u ms\n",
             SuperControl::SharedBuffer ? "ON" : "off", (unsigned)SuperControl::SharedBytes(), SuperControl::DmaActive ? "ACTIVE" : "off",
             SuperControl::LabelOpaque ? "yes" : "no", (unsigned)used, (unsigned)ms);
}

void setup() {
  DBG.begin(115200);
  for (uint32_t t = millis(); !DBG && millis() - t < 3000; ) delay(10);   // USB serial: wait (up to 3 s) for the monitor so the startup lines are not lost
  lcd.init();
  lcd.fillScreen(TFT_BLACK);
  DBG.printf("display %d x %d, heap %u bytes (total %u)\n", (int)lcd.width(), (int)lcd.height(), (unsigned)rp2040.getFreeHeap(), (unsigned)rp2040.getTotalHeap());

  if (USE_ENCODER) Enc.Begin();
  if (USE_BUTTONS) {
    BtnA.Begin(BTN_ACTIVE_LOW ? INPUT_PULLUP : INPUT);
    BtnB.Begin(BTN_ACTIVE_LOW ? INPUT_PULLUP : INPUT);
  }

  SuperControl::SharedBuffer = USE_SHARED_BUFFER;
  SuperControl::UseDMA = USE_DMA;
  SuperControl::LabelOpaque = LABEL_OPAQUE;   // must be set before the UI is created / first drawn
  size_t heap0 = FREE_HEAP();
  ui.Begin(&lcd, &Enc, &BtnA, &BtnB);
  report(heap0);
}

void loop() {
  static uint32_t lastInfo = 0;
  if (millis() - lastInfo > 5000) {          // memory watch + where the draw time went in the last 5 s
    lastInfo = millis();
    DBG.printf("free heap %u of %u, shared buffer %u\n", (unsigned)rp2040.getFreeHeap(), (unsigned)rp2040.getTotalHeap(), (unsigned)SuperControl::SharedBytes());
    printStats("last 5 s");
  }
  ui.Service();     // never blocks for long: focus, edits, graph, status line
  delay(2);
}
