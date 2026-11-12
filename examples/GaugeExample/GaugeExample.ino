// SuperGauge: a dial with a needle (display only). Colour zones, needle coloured by zone, smooth needle glide.
// Minimal example. Edit the pins / panel in the HARDWARE block for your board.
// Wiring: encoder A/B -> pins 2/3, ButtonA -> pin 4, ButtonB -> pin 5 (buttons to GND, internal pull-ups).
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <SuperGauge.h>
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

SuperGauge load(&lcd, 10, 10, 220, /*body height*/ 130, &lgfx::fonts::Font2);          // half dial (-90..+90 degrees), the default
SuperGauge temp(&lcd, 10, 160, 150, 120, &lgfx::fonts::Font2);
SuperForm form(&Enc, &BtnA);
uint32_t last = 0;

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.fillScreen(TFT_BLACK);
  Enc.Begin(); BtnA.Begin(INPUT_PULLUP); BtnB.Begin(INPUT_PULLUP);

  load.SetBorder(1, TFT_WHITE);  load.SetLabel("Load");
  load.SetRange(0, 100);
  load.AddZone(0, 60, TFT_GREEN);  load.AddZone(60, 85, TFT_YELLOW);  load.AddZone(85, 100, TFT_RED);   // the needle takes the zone's colour
  load.SetFormat(0, "%");                       // the value under the hub: no decimals, a unit
  load.SetMinMax(5000, 4, 3);                   // lines for the lowest / highest value of the last 5 s: 4+ degrees from the needle to show, 3 px thick, inverted colour of the dial

  temp.SetBorder(1, TFT_WHITE);  temp.SetLabel("Temperature");
  temp.SetSweep(-135, 135);                     // a 270 degree dial; SetSweep(start, end) is degrees clockwise from 12 o'clock
  temp.SetRange(-20, 120);  temp.SetTicks(7, 1);
  temp.AddZone(-20, 0, TFT_CYAN);  temp.AddZone(0, 90, TFT_GREEN);  temp.AddZone(90, 120, TFT_RED);
  temp.SetFormat(1, " C");

  form.Add(&load, false);                       // display only: never focused
  form.Add(&temp, false);
  form.Draw();
}

void loop() {
  if (millis() - last > 700) {                  // a new reading: the needle glides to it (smoothing is on by default)
    last = millis();
    load.SetValue(50 + 45 * sinf(millis() * 0.0007f));
    temp.SetValue(50 + 60 * sinf(millis() * 0.0004f));
  }
  form.Service();                               // drives the needle glide; without a form call load.Update() / temp.Update() here
}
