// SuperRadioButton + SuperRadioGroup: pick one
// Minimal example. Edit the pins / panel in the HARDWARE block for your board.
// Wiring: encoder A/B -> pins 2/3, ButtonA -> pin 4, ButtonB -> pin 5 (buttons to GND, internal pull-ups).
//   Encoder = move / change    ButtonA short = edit / select    ButtonA long = accept
//   ButtonB short = cancel     ButtonB long = revert
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <SuperRadioButton.h>

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

SuperRadioButton radio[3] = {
  { &lcd, 10, 10, 160, &lgfx::fonts::Font2, "Slow" },
  { &lcd, 10, 36, 160, &lgfx::fonts::Font2, "Normal" },
  { &lcd, 10, 62, 160, &lgfx::fonts::Font2, "Fast" },
};
SuperRadioGroup group;
int focus = 0;

void onAccept(int index, void*) { Serial.printf("mode = %d\n", index); }

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.fillScreen(TFT_BLACK);
  Enc.Begin(); BtnA.Begin(INPUT_PULLUP); BtnB.Begin(INPUT_PULLUP);
  for (auto& r : radio) group.Add(&r);
  group.SetOnAccept(onAccept);
  group.SetSelected(1);
  for (int i = 0; i < 3; ++i) radio[i].DrawPreview(i == focus);
}

void loop() {
  // The encoder moves a highlight box between the buttons, ButtonA short click selects the focused one.
  int32_t d = Enc.GetDelta();
  if (d) {
    focus = ((focus + (d > 0 ? 1 : -1)) % 3 + 3) % 3;
    for (int i = 0; i < 3; ++i) radio[i].DrawPreview(i == focus);
  }
  uint8_t ev;
  if (BtnA.GetEvent(ev) && (ev & 0x0F) == 1) radio[focus].BeginEdit();
}
