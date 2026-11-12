// Touch + encoder together: SuperForm with SuperTouch (LovyanGFX getTouch as the touch source)
// Minimal example. Edit the pins / panel in the HARDWARE block for your board.
// Wiring: encoder A/B -> pins 2/3, ButtonA -> pin 4, ButtonB -> pin 5 (buttons to GND, internal pull-ups).
//   Encoder = move / change    ButtonA short = edit / select    ButtonA long = accept
//   ButtonB short = cancel     ButtonB long = revert
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <SuperInputBox.h>
#include <SuperSlider.h>
#include <SuperCheckBox.h>
#include <SuperProgressBar.h>
#include <SuperForm.h>
#include <SuperTouch.h>

// ---------------- HARDWARE (edit me) ----------------
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 panel;
  lgfx::Bus_SPI      bus;
public:
  LGFX() {
    auto b = bus.config();
#if defined(ARDUINO_ARCH_RP2040)
    b.spi_host = 0;                         // Pico: SPI0
#else
    b.spi_host = SPI2_HOST;                 // ESP32
#endif
    b.freq_write = 40000000;
    b.pin_sclk = 18; b.pin_mosi = 19; b.pin_miso = -1; b.pin_dc = 20;
    bus.config(b);
    panel.setBus(&bus);
    auto p = panel.config();
    p.pin_cs = 17; p.pin_rst = 21; p.pin_busy = -1;
    p.panel_width = 240; p.panel_height = 320;
    panel.config(p);
    // Touch controller: configure one here and lcd.getTouch() will work, e.g. XPT2046 on SPI:
    //   static lgfx::Touch_XPT2046 touch; auto t = touch.config(); t.pin_cs = 22; t.x_max = 239; t.y_max = 319; ...
    //   touch.config(t); panel.setTouch(&touch);
    // Without one, getTouch() just returns false and the form works with the encoder / buttons only.
    setPanel(&panel);
  }
};
LGFX lcd;
SuperEncoder Enc(2, 3);
SuperButton  BtnA(4, true), BtnB(5, true);   // active low
// ----------------------------------------------------

SuperInputBox phone(&lcd, 10, 10, 220, &lgfx::fonts::Font2, "(###) ###-####", &Enc, &BtnA, &BtnB);
SuperSlider   slider(&lcd, 10, 60, 220, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
SuperCheckBox check(&lcd, 10, 110, 150, &lgfx::fonts::Font2, "Enabled");
SuperProgressBar bar(&lcd, 10, 150, 220, &lgfx::fonts::Font2);
SuperForm form(&Enc, &BtnA);

bool readTouch(int16_t& x, int16_t& y) {                         // placeholder source: LovyanGFX
  int32_t tx, ty;
  if (!lcd.getTouch(&tx, &ty)) return false;
  x = tx; y = ty;
  return true;
}
SuperTouch touch(readTouch);

void onSlider(void*) { bar.SetValue(slider.GetLiveValue()); }       // live: follows the slider while you turn it

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.fillScreen(TFT_BLACK);
  Enc.Begin(); BtnA.Begin(INPUT_PULLUP); BtnB.Begin(INPUT_PULLUP);
  phone.SetLabel("Phone"); phone.SetBorder(1, TFT_WHITE); phone.SetValue("5551234567");
  slider.SetLabel("Slider"); slider.SetBorder(1, TFT_WHITE); slider.SetOnChange(onSlider);
  bar.SetBorder(1, TFT_WHITE);
  touch.ClickD_MAX = 0;                                           // taps arrive on release, no double-click delay
  form.SetTouch(&touch);                                          // tap = focus + edit; encoder / buttons still work
  form.Add(&phone);
  form.Add(&slider);
  form.Add(&check);
  form.Add(&bar, false);                                          // shown, never focused
  form.Draw();
}

// Touch: tap a control to edit it, tap a list row / slider track to pick, drag to change, long press = accept,
// tap outside = cancel. Encoder and buttons work in the same edit.
// loop() is never blocked: the form moves the focus box with the wheel and runs an edit with ButtonA.
// Anything else (a radio link, a PID loop...) keeps running here.
void loop() {
  form.Service();
}
