// SuperGUI desktop demo: every control on four tab pages, running in an SDL window on macOS / Linux / Windows.
// The demo UI itself is the same DemoUI.h as the Freenove ESP32-S3 demo (480x320), so what you see here is what runs on the board.
//
//   Mouse wheel       = encoder (move the highlight / change a value)
//   Wheel click       = ButtonA  (short = edit / select, long = accept)
//   Right click       = ButtonB  (short = cancel, long = revert)
//   Left button       = touch (LovyanGFX's SDL touch emulation, read with lcd.getTouch(), exactly as on real hardware)
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include "desktop_input.h"
#include "DemoUI.h"

using namespace DesktopInput;

// true: draw every control in the Sleek style (label on the frame line, left / centre / right). false: the standard look.
constexpr bool USE_SLEEK = false;

static LGFX* lcd = nullptr;
static DemoUI ui;

// Touch source for SuperTouch: the LovyanGFX SDL panel turns the left mouse button into a touch panel.
static bool ReadTouch(int16_t& x, int16_t& y) {
  int32_t tx, ty;
  if (!lcd || !lcd->getTouch(&tx, &ty)) return false;
  x = (int16_t)tx; y = (int16_t)ty;
  return true;
}
static SuperTouch Touch(ReadTouch);

void setup() {
  lcd = new LGFX(480, 320, 2);          // 480x320 window, scaled 2x
  lcd->init();

  Begin();                              // DesktopInput: wheel = encoder, wheel click = ButtonA, right click = ButtonB
  Touch.ClickD_MAX = 0;                 // taps arrive on release, no double-click delay
  // The shared-buffer / DMA options work here too (DMA is a plain copy on the desktop):
  //   SuperControl::SharedBuffer = true; SuperControl::UseDMA = true; SuperControl::LabelOpaque = true;
  ui.Begin(lcd, &Enc, &ButtonA, &ButtonB, &Touch);
  if (USE_SLEEK) ui.ApplyStyle(true);
  ui.SetAbort(Quitting);                // closing the window ends a blocking edit (e.g. the message box)
}

void loop() {
  ui.Service();
  delay(2);
}
