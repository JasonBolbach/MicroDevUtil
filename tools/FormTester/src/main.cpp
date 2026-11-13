// SuperGUI Form Tester: runs the UserForm exported by the Form Designer, live, on the desktop (SDL window).
// Export the form into this folder (UserForm.h + UserForm.designer.h), rebuild, run.
//
//   Mouse wheel       = encoder (move the highlight / change a value)
//   Wheel click       = ButtonA  (short = edit / select, long = accept)
//   Right click       = ButtonB  (short = cancel, long = revert)
//   Left button       = touch (LovyanGFX's SDL touch emulation, read with lcd.getTouch(), exactly as on real hardware)
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include "desktop_input.h"
#include "UserForm.h"

using namespace DesktopInput;

// Window scale: the logical size is the form's own size. Small forms (the usual 480x320 devices) are shown at 2x.
constexpr int WIN_SCALE = (UserForm::FormWidth <= 640 && UserForm::FormHeight <= 400) ? 2 : 1;

static LGFX* lcd = nullptr;
static UserForm* ui = nullptr;

// Touch source for SuperTouch: the LovyanGFX SDL panel turns the left mouse button into a touch panel.
static bool ReadTouch(int16_t& x, int16_t& y) {
  int32_t tx, ty;
  if (!lcd || !lcd->getTouch(&tx, &ty)) return false;
  x = (int16_t)tx; y = (int16_t)ty;
  return true;
}
static SuperTouch Touch(ReadTouch);

void setup() {
  lcd = new LGFX(UserForm::FormWidth, UserForm::FormHeight, WIN_SCALE);
  lcd->init();

  Begin();                              // DesktopInput: wheel = encoder, wheel click = ButtonA, right click = ButtonB
  Touch.ClickD_MAX = 0;                 // taps arrive on release, no double-click delay

  ui = new UserForm(lcd, &Enc, &ButtonA, &ButtonB);
  ui->Form.SetTouch(&Touch);            // taps and drags work together with the encoder / buttons
  ui->Show();
}

void loop() {
  ui->Service();
  delay(2);
}
