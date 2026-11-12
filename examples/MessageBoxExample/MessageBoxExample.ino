// SuperMessageBox: modal confirm
// Minimal example. Edit the pins / panel in the HARDWARE block for your board.
// Wiring: encoder A/B -> pins 2/3, ButtonA -> pin 4, ButtonB -> pin 5 (buttons to GND, internal pull-ups).
//   Encoder = move / change    ButtonA short = edit / select    ButtonA long = accept
//   ButtonB short = cancel     ButtonB long = revert
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <SuperMessageBox.h>

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

SuperMessageBox box(&lcd, 200, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.fillScreen(TFT_BLACK);
  Enc.Begin(); BtnA.Begin(INPUT_PULLUP); BtnB.Begin(INPUT_PULLUP);
  lcd.fillScreen(TFT_NAVY);
  lcd.setTextColor(TFT_WHITE);
  lcd.drawString("Click ButtonA", 10, 10);
  box.SetLabel("Confirm");
  box.SetMessage("Erase all settings?\nThis cannot be undone.");
  box.AddButton("Yes"); box.AddButton("No");
  box.SetDefault(1);                   // "No" is highlighted first
}

void loop() {
  uint8_t ev;
  if (BtnA.GetEvent(ev) && (ev & 0x0F) == 1) {
    // The area under the box is put back when it closes. If your panel can't be read back,
    // call box.SetBackdrop(&sprite) or box.SetRestore(fn) (see ComboBoxExample).
    if (box.BeginEdit()) Serial.printf("button %d\n", box.GetResult()); else Serial.println("cancelled");
  }
}
