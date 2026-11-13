// SuperRichText: scrollable text with a few HTML-like tags (colour, background, underline, strike, big, alignment, <br>, <tab>, <hr>, entities),
// drawn with ONE font. Also SuperLabel::SetRich(true) and SuperMessageBox::SetRich(true) use the same tags (off by default).
// Unknown tags are ignored and nothing is checked: bad text shows as bad text. The text below is a string literal, so it lives in flash (SetTextStatic: no copy).
// Turn the encoder to scroll, ButtonA closes, ButtonB long = back to the top.
// Wiring: encoder A/B -> pins 2/3, ButtonA -> pin 4, ButtonB -> pin 5 (buttons to GND, internal pull-ups).
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <SuperRichText.h>
#include <SuperLabel.h>
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


SuperRichText help(&lcd, 10, 10, 220, 200, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
SuperLabel    status(&lcd, 10, 225, 220, &lgfx::fonts::Font2);

static const char HELP[] =
  "<center><color=yellow><big>Rich text</big></color></center>\n"
  "<hr>\n"
  "Colours: <color=red>red</color>, <color=#0F0>green</color>, <color=cyan>cyan</color>.\n"
  "Background: <bg=#0000A0><color=white> inverse </color></bg>\n"
  "<u>underline</u> and <s>strike</s>\n"
  "Name<tab>Value\n"
  "Width<tab>240\n"
  "<right>right aligned</right>\n"
  "&lt;b&gt; is not supported and is ignored: <b>bold?</b>\n"
  "A long line wraps at the width of the control, a word wider than the control is broken inside.\n"
  "<hr>\n"
  "Scroll down for more.\n\n\n\n\n\n\n\n\n"
  "<center><color=gray>the end</color></center>";

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.fillScreen(TFT_BLACK);
  Enc.Begin(); BtnA.Begin(INPUT_PULLUP); BtnB.Begin(INPUT_PULLUP);

  help.SetBorder(1, TFT_DARKGREY);
  // help.SetStyleFont(SuperRichText::FONT_BOLD, &lgfx::fonts::FreeSansBold9pt7b);   // register a font and <b> becomes real bold (also FONT_ITALIC, FONT_BOLDITALIC, FONT_HEADING)
  help.SetTabChars(10);                         // tab stops every 10 spaces
  help.SetTextStatic(HELP);                     // no copy: the literal stays in flash
  // help.SetShowSource(true);                  // debugging: shows the markup itself instead of interpreting it
  status.SetRich(true);                         // a label that understands the tags too
  status.SetText("<color=gray>turn: scroll   A: close</color>");
  status.Push();
  if (help.BeginEdit()) status.SetText("<color=green>closed</color> with A");
  else                  status.SetText("<color=orange>cancelled</color> with B");
}

void loop() {}
