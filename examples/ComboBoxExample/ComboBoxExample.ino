// SuperComboBox: drop-down list
// Minimal example. Edit the pins / panel in the HARDWARE block for your board.
// Wiring: encoder A/B -> pins 2/3, ButtonA -> pin 4, ButtonB -> pin 5 (buttons to GND, internal pull-ups).
//   Encoder = move / change    ButtonA short = edit / select    ButtonA long = accept
//   ButtonB short = cancel     ButtonB long = revert
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <SuperComboBox.h>

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

// The popup covers part of the screen. To put that back when it closes, the combo needs to know what was
// there: most SPI panels can't be read, so keep the background in a sprite and give it to SetBackdrop().
lgfx::LGFX_Sprite Background(&lcd);
SuperComboBox combo(&lcd, 10, 30, 160, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);

void onAccept(void*) { Serial.printf("color = %s\n", combo.GetValue().c_str()); }

void setup() {
  Serial.begin(115200);
  lcd.init();
  Background.setColorDepth(16);
  Background.createSprite(240, 160);                 // the part of the screen the popup can cover
  Background.fillSprite(TFT_NAVY);
  Background.drawString("background", 10, 120);
  Background.pushSprite(&lcd, 0, 0);

  Enc.Begin(); BtnA.Begin(INPUT_PULLUP); BtnB.Begin(INPUT_PULLUP);
  combo.SetBackdrop(&Background);
  combo.SetLabel("Color");
  combo.SetBorder(1, TFT_WHITE);
  for (const char* c : {"Red", "Green", "Blue", "Yellow", "Cyan", "Magenta"}) combo.AddItem(c);
  combo.SetOnAccept(onAccept);
  combo.SetSelected(2);
}

void loop() {
  uint8_t ev;
  if (BtnA.GetEvent(ev) && (ev & 0x0F) == 1) combo.BeginEdit();
}
