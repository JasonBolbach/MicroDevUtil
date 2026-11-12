// SuperTimeBox and SuperDateBox: time (24 hour) and date made of numeric fields
// Scrolling changes the whole field like a spin box, A short = next field, each field stays in its range. Changing the month pulls the day
// back to the month's last day (Dec 31 -> Feb = 28, or 29 in a leap year). The date's value is always YYYY-MM-DD, whatever the display style.
// Minimal example. Edit the pins / panel in the HARDWARE block for your board.
// Wiring: encoder A/B -> pins 2/3, ButtonA -> pin 4, ButtonB -> pin 5 (buttons to GND, internal pull-ups).
//   Encoder = move / change    ButtonA short = edit / select    ButtonA long = accept
//   ButtonB short = cancel     ButtonB long = revert
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <SuperTimeBox.h>

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

SuperTimeBox timeBox(&lcd, 10, 10, 140, &lgfx::fonts::Font4, &Enc, &BtnA, &BtnB);
SuperDateBox dateBox(&lcd, 10, 70, 190, &lgfx::fonts::Font4, &Enc, &BtnA, &BtnB);   // , SuperDateBox::DATE_MDY for 12-31-2026
int focus = 0;

void onTime(void*) { Serial.printf("time %s\n", timeBox.GetValue().c_str()); }                 // "HH:MM:SS"
void onDate(void*) { Serial.printf("date %s\n", dateBox.GetValue().c_str()); }                 // always "YYYY-MM-DD"

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.fillScreen(TFT_BLACK);
  Enc.Begin(); BtnA.Begin(INPUT_PULLUP); BtnB.Begin(INPUT_PULLUP);
  timeBox.SetLabel("Time"); timeBox.SetBorder(1, TFT_WHITE); timeBox.SetOnAccept(onTime); timeBox.SetTime(12, 30, 0);
  dateBox.SetLabel("Date"); dateBox.SetBorder(1, TFT_WHITE); dateBox.SetOnAccept(onDate); dateBox.SetDate(2025, 2, 28);
  timeBox.DrawPreview(true); dateBox.DrawPreview(false);
}

// Fields cannot leave their range (HH 0-23, MM / SS 0-59, month 1-12, day 1..days in the month), so every accepted value is valid.
// timeBox.SetWrap(true) makes a field go round (59 -> 00) instead of stopping.
void loop() {
  int32_t d = Enc.GetDelta();
  if (d) { focus = 1 - focus; timeBox.DrawPreview(focus == 0); dateBox.DrawPreview(focus == 1); }
  uint8_t ev;
  if (BtnA.GetEvent(ev) && (ev & 0x0F) == 1) { if (focus == 0) timeBox.BeginEdit(); else dateBox.BeginEdit(); }
}
