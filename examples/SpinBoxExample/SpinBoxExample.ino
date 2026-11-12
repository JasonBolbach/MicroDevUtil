// SuperSpinBox: number spinner
// Minimal example. Edit the pins / panel in the HARDWARE block for your board.
// Wiring: encoder A/B -> pins 2/3, ButtonA -> pin 4, ButtonB -> pin 5 (buttons to GND, internal pull-ups).
//   Encoder = move / change    ButtonA short = edit / select    ButtonA long = accept
//   ButtonB short = cancel     ButtonB long = revert
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <SuperSpinBox.h>

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

SuperSpinBox level(&lcd, 10, 10, 120, &lgfx::fonts::Font4, &Enc, &BtnA, &BtnB);

void onChange(void*) { Serial.println("changing..."); }                       // live, while editing
void onAccept(void*) { Serial.printf("level = %ld\n", (long)level.GetValue()); }

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.fillScreen(TFT_BLACK);
  Enc.Begin(); BtnA.Begin(INPUT_PULLUP); BtnB.Begin(INPUT_PULLUP);
  level.SetLabel("Level");
  level.SetBorder(2, TFT_WHITE);
  level.SetRange(0, 100);
  level.SetSuffix(" %");
  level.SetOnChange(onChange);
  level.SetOnAccept(onAccept);
  level.SetValue(40);
}

void loop() {
  uint8_t ev;
  if (BtnA.GetEvent(ev) && (ev & 0x0F) == 1) level.BeginEdit();
}
