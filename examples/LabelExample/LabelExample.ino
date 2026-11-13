// SuperLabel: plain text (no frame, no title) with left / centre / right alignment, word wrap, vertical alignment, live values.
// Also shows SuperControl::SetTabOrder (focus order) and SetZOrder (drawing order).
// Minimal example. Edit the pins / panel in the HARDWARE block for your board.
// Wiring: encoder A/B -> pins 2/3, ButtonA -> pin 4, ButtonB -> pin 5 (buttons to GND, internal pull-ups).
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <SuperLabel.h>
#include <SuperSpinBox.h>
#include <SuperForm.h>

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
    setPanel(&panel);
  }
};
LGFX lcd;
SuperEncoder Enc(2, 3);
SuperButton  BtnA(4, true), BtnB(5, true);   // active low
// ----------------------------------------------------

SuperLabel title(&lcd, 0, 6, 240, &lgfx::fonts::Font4, "Settings", SuperControl::ALIGN_CENTER);
SuperLabel help (&lcd, 10, 40, 220, &lgfx::fonts::Font2, "Turn the wheel to move between the boxes. A short click edits, A long accepts.");
SuperLabel value(&lcd, 10, 120, 220, &lgfx::fonts::Font2, "", SuperControl::ALIGN_RIGHT);        // a live value, right aligned
SuperSpinBox a(&lcd, 10, 150, 100, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
SuperSpinBox b(&lcd, 130, 150, 100, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
SuperSpinBox c(&lcd, 10, 200, 100, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
SuperForm form(&Enc, &BtnA);

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.fillScreen(TFT_BLACK);
  Enc.Begin(); BtnA.Begin(INPUT_PULLUP); BtnB.Begin(INPUT_PULLUP);

  title.SetTextColor(TFT_CYAN);
  title.SetUnderline(true);
  help.SetWrap(true);                           // breaks at spaces; the label's height follows the number of lines
  help.SetTextColor(TFT_LIGHTGREY);
  value.SetTextColor(TFT_YELLOW);

  a.SetLabel("A"); b.SetLabel("B"); c.SetLabel("C");
  // Focus order: the wheel visits the lowest TabOrder first. Equal numbers keep the order they were added. Here: C, then A, then B.
  c.SetTabOrder(0);  a.SetTabOrder(1);  b.SetTabOrder(2);
  // Drawing order: the LOWEST ZOrder is in front. Only matters where controls overlap; the default 0 means "in the order added".
  // a.SetZOrder(1); b.SetZOrder(0);          // b would be drawn over a

  form.Add(&title, false);                      // labels are display only: never focused
  form.Add(&help, false);
  form.Add(&value, false);
  form.Add(&a); form.Add(&b); form.Add(&c);
  form.Draw();
}

void loop() {
  value.SetTextf("A + B + C = %d", (int)(a.GetValue() + b.GetValue() + c.GetValue()));   // redraws only when the text changed
  form.Service();
}
