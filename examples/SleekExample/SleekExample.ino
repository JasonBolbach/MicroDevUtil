// BORDER_SLEEK: a plain one-colour frame with the label sitting ON the frame line (like a fieldset legend).
// Minimal example. Edit the pins / panel in the HARDWARE block for your board.
// Wiring: encoder A/B -> pins 2/3, ButtonA -> pin 4, ButtonB -> pin 5 (buttons to GND, internal pull-ups).
//   Encoder = move / change    ButtonA short = edit / select    ButtonA long = accept
//   ButtonB short = cancel     ButtonB long = revert
//
// Label alignment: SetLabelAlign(ALIGN_LEFT / ALIGN_CENTER / ALIGN_RIGHT). With LEFT / RIGHT a short stub of the frame line is drawn beside
// the label (SleekMargin and SleekGap tune it); with CENTER the line runs to both sides. A label too wide for the frame is cut and "..." is
// appended: "Really Long Label" in a narrow box shows as "Really Lo...". A label below the box (LABEL_BELOW) sits on the bottom line.
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <SuperInputBox.h>
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

SuperInputBox phone(&lcd, 10, 10, 220, &lgfx::fonts::Font4, "(###) ###-####", &Enc, &BtnA, &BtnB);
SuperInputBox name (&lcd, 10, 70, 220, &lgfx::fonts::Font2, "BBBBBBBBBBBB",    &Enc, &BtnA, &BtnB);
SuperSpinBox  level(&lcd, 10, 120, 100, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
SuperSpinBox  tiny (&lcd, 130, 120, 70, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
SuperForm form(&Enc, &BtnA);

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.fillScreen(TFT_BLACK);
  Enc.Begin(); BtnA.Begin(INPUT_PULLUP); BtnB.Begin(INPUT_PULLUP);

  // The same three calls for every control: style, border colour / thickness, label.
  phone.SetBorderStyle(SuperControl::BORDER_SLEEK); phone.SetBorder(1, TFT_WHITE);
  phone.SetLabel("Phone");  phone.SetLabelAlign(SuperControl::ALIGN_LEFT);   phone.SetLabelColor(TFT_GREEN);
  phone.SetValue("5551234567");

  name.SetBorderStyle(SuperControl::BORDER_SLEEK);  name.SetBorder(1, TFT_WHITE);
  name.SetLabel("Name");    name.SetLabelAlign(SuperControl::ALIGN_CENTER);   name.SetLabelColor(TFT_CYAN);
  name.SetValue("SuperGUI");

  level.SetBorderStyle(SuperControl::BORDER_SLEEK); level.SetBorder(1, TFT_WHITE);
  level.SetLabel("Level");  level.SetLabelAlign(SuperControl::ALIGN_RIGHT);  level.SetLabelColor(TFT_YELLOW);
  level.SetRange(0, 100); level.SetSuffix(" %"); level.SetValue(40);

  tiny.SetBorderStyle(SuperControl::BORDER_SLEEK);  tiny.SetBorder(1, TFT_WHITE);
  tiny.SetLabel("A label that is far too long");  // cut to fit: "A l..."
  tiny.SetRange(0, 9); tiny.SetValue(3);

  form.Add(&phone); form.Add(&name); form.Add(&level); form.Add(&tiny);
  form.Draw();
}

void loop() {
  form.Service();
}
