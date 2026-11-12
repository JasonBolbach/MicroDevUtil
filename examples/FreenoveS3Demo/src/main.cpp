// SuperGUI hardware test: Freenove ESP32-S3-WROOM (8 MB flash / 8 MB PSRAM) + 480x320 touch LCD + encoder + two buttons.
//
// 1. include/LGFX_User.h holds the LovyanGFX display class with the touch driver (class name LGFX) and include/Pins.h the pin map.
//    Replace both for a different board.
// 2. Set the four pin numbers below (GPIO numbers) to match your wiring.
// 3. Upload, open the serial monitor (115200) if you want the log lines.
//
// Encoder = move the highlight / change a value.  ButtonA short = edit the highlighted control.
// Inside an edit: ButtonA long = accept, ButtonB short = cancel, ButtonB long = revert.
// Touch (LovyanGFX getTouch): tap to edit, tap/drag to pick or change, long press = accept, tap outside = cancel.
#include <Arduino.h>
#include "LGFX_User.h"
#include <SuperButton.h>
#include <SuperEncoder.h>
#include <SuperTouch.h>
#include "DemoUI.h"

// Debug output goes to the HARDWARE UART0 (TX = GPIO43, RX = GPIO44 on the Freenove board, i.e. the USB-serial chip's port), never to
// the native USB CDC port: Serial0 is always UART0 whatever ARDUINO_USB_CDC_ON_BOOT says. Change DBG to Serial to follow that setting.
#define DBG Serial0

// ---------------- WIRING: edit these ----------------
// Pins already taken by your Pins.h / LGFX_User.h: TFT 4,5,10,11,12,13   touch 6,7,8,9   SD 39,40,41,42   flash/octal PSRAM 26-37.
// The default encoder / button pins below are FREE pins, but nothing is read from them until you set USE_ENCODER / USE_BUTTONS to true.
// (An un-wired but enabled input is harmless; an input on a pin that is already in use is not: TOUCH_INT on a button pin acts like a button press.)
constexpr uint8_t PIN_ENC_A = 1;          // encoder channel A (ignored when USE_ENCODER is false)
constexpr uint8_t PIN_ENC_B = 2;          // encoder channel B
constexpr uint8_t PIN_BTN_A = 3;          // ButtonA: select / edit / accept (long) (ignored when USE_BUTTONS is false)
constexpr uint8_t PIN_BTN_B = 14;         // ButtonB: cancel / revert (long)
constexpr bool    BTN_ACTIVE_LOW = true;  // buttons wired to GND with the internal pull-up
constexpr bool    USE_ENCODER = false;    // false = no encoder fitted: its pins are not touched
constexpr bool    USE_BUTTONS = false;    // false = no buttons fitted: touch only
constexpr bool    USE_SHARED_BUFFER = true;  // true = ONE shared render buffer (about the size of the biggest control) instead of a buffer per control: much less RAM, nearly the same speed
constexpr bool    USE_DMA = true;           // needs USE_SHARED_BUFFER: two shared buffers, the CPU renders the next control while the previous one is sent to the display
constexpr bool    LABEL_OPAQUE = true;      // labels get a solid background (your screen colour) instead of being transparent; required for DMA with labelled controls
constexpr bool    USE_TOUCH = true;       // false = encoder / buttons only
// Encoder turns the wrong way? Swap PIN_ENC_A and PIN_ENC_B.
// ----------------------------------------------------

LGFX         lcd;
SuperEncoder Enc(PIN_ENC_A, PIN_ENC_B);
SuperButton  BtnA(PIN_BTN_A, BTN_ACTIVE_LOW), BtnB(PIN_BTN_B, BTN_ACTIVE_LOW);
DemoUI       ui;

// Touch source for SuperTouch: LovyanGFX's own getTouch (works with whatever touch driver your LGFX class sets).
bool ReadTouch(int16_t& x, int16_t& y) {
  int32_t tx, ty;
  if (!lcd.getTouch(&tx, &ty)) return false;
  x = (int16_t)tx; y = (int16_t)ty;
  return true;
}
SuperTouch Touch(ReadTouch);


// Where the draw time goes (microseconds, summed over the draws since the last call), then reset.
void printStats(const char* what) {
  uint32_t n = SuperControl::StatDraws;
  DBG.printf("[%s] draws=%u  render=%u us  dim=%u us  push=%u us  (per draw: render %u, dim %u, push %u us)\n", what, (unsigned)n,
             (unsigned)SuperControl::StatRenderUs, (unsigned)SuperControl::StatDimUs, (unsigned)SuperControl::StatPushUs,
             n ? (unsigned)(SuperControl::StatRenderUs / n) : 0, n ? (unsigned)(SuperControl::StatDimUs / n) : 0, n ? (unsigned)(SuperControl::StatPushUs / n) : 0);
  SuperControl::ResetStats();
}

// Memory / speed report (serial monitor): how much RAM the UI took, and how long a full redraw of the page takes.
static inline size_t ESP_FREE() { return ESP.getFreeHeap(); }                     // internal RAM
void report(size_t heap0, size_t psram0) {
  size_t used = heap0 > ESP_FREE() ? heap0 - ESP_FREE() : 0;
  size_t usedPs = psram0 > ESP.getFreePsram() ? psram0 - ESP.getFreePsram() : 0;
  SuperControl::CollectStats = true;
  SuperControl::ResetStats();
  uint32_t t0 = millis();
  ui.Form->Draw();
  uint32_t ms = millis() - t0;
  printStats("first page redraw");
  DBG.printf("shared buffer: %s (%u bytes), DMA: %s, opaque labels: %s   UI took %u bytes of internal RAM + %u bytes of PSRAM   full page redraw: %u ms\n",
                SuperControl::SharedBuffer ? "ON" : "off", (unsigned)SuperControl::SharedBytes(), SuperControl::DmaActive ? "ACTIVE" : "off", SuperControl::LabelOpaque ? "yes" : "no", (unsigned)used, (unsigned)usedPs, (unsigned)ms);
}

void setup() {
  DBG.begin(115200);
  lcd.init();
  lcd.fillScreen(TFT_BLACK);
  if (!psramFound()) DBG.println("WARNING: no PSRAM. This module needs psram_type = opi / memory_type = qio_opi in platformio.ini; the UI will run from internal RAM only.");
  DBG.printf("display %d x %d, PSRAM %u bytes\n", (int)lcd.width(), (int)lcd.height(), (unsigned)ESP.getPsramSize());

  SuperControl::UsePsram = psramFound();      // control sprites go to PSRAM when the board has it (internal RAM stays free)
  if (USE_ENCODER) Enc.Begin();
  if (USE_BUTTONS) {
    BtnA.Begin(BTN_ACTIVE_LOW ? INPUT_PULLUP : INPUT);
    BtnB.Begin(BTN_ACTIVE_LOW ? INPUT_PULLUP : INPUT);
  }

  Touch.ClickD_MAX = 0;                       // taps arrive on release, no double-click delay
  SuperControl::SharedBuffer = USE_SHARED_BUFFER;
  SuperControl::UseDMA = USE_DMA;
  SuperControl::LabelOpaque = LABEL_OPAQUE;   // must be set before the UI is created / first drawn
  size_t heap0 = ESP_FREE(), psram0 = ESP.getFreePsram();
  ui.Begin(&lcd, &Enc, &BtnA, &BtnB, USE_TOUCH ? &Touch : nullptr);
  report(heap0, psram0);
}

void loop() {
  static uint32_t lastInfo = 0;
  if (millis() - lastInfo > 5000) {         // memory watch: a control that can't get its buffer draws a red outline
    lastInfo = millis();
    DBG.printf("free heap %u, free PSRAM %u, shared buffer %u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram(), (unsigned)SuperControl::SharedBytes());
    printStats("last 5 s");
  }
  ui.Service();     // never blocks for long: focus, edits, touch, graph, status line
  delay(2);
}
