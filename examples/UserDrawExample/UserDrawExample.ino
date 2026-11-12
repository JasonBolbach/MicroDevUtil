// SuperUserDraw: a control that YOUR code draws. Two examples:
//   * a gauge   - normal mode, painted from scratch on every redraw, repainted by itself every 40 ms (auto-redraw)
//   * a sketch  - persistent canvas that keeps what is drawn, plus touch events (needs a touch panel: see SetOnPointer)
// Minimal example. Edit the pins / panel in the HARDWARE block for your board.
// Wiring: encoder A/B -> pins 2/3, ButtonA -> pin 4, ButtonB -> pin 5 (buttons to GND, internal pull-ups).
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <SuperUserDraw.h>
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

SuperUserDraw gauge(&lcd, 10, 10, 150, /*body height*/ 90);
SuperUserDraw pad  (&lcd, 170, 10, 140, 90);
SuperForm form(&Enc, &BtnA);
int lastX = 0, lastY = 0;

// The draw event: draw inside 'r' (everything is clipped to it). In normal mode 's' arrives holding only the background and the frame, so paint it all.
void drawGauge(SuperUserDraw& c, lgfx::LGFX_Sprite* s, const SuperRect& r, void*) {
  int cx = r.x + r.w / 2, cy = r.y + r.h - 8, rad = r.h - 16;
  float v = 0.5f + 0.5f * sinf(millis() * 0.002f);                         // 0..1, any value you like
  float a = (-90.f + 180.f * v) * 0.0174533f;
  s->drawLine(cx, cy, cx + (int)(sinf(a) * rad), cy - (int)(cosf(a) * rad), TFT_YELLOW);
  s->fillCircle(cx, cy, 4, TFT_YELLOW);
}

// Persistent canvas: this runs once (and on Repaint()), so it only paints the hint; the strokes are drawn on the canvas from the pointer events.
void drawPad(SuperUserDraw& c, lgfx::LGFX_Sprite* s, const SuperRect& r, void*) {
  s->setTextColor(TFT_DARKGREY); s->setTextDatum(lgfx::textdatum_t::middle_center);
  s->drawString("draw here", r.x + r.w / 2, r.y + r.h / 2);
}
// Pointer events arrive in body coordinates ((0,0) = the top-left of the drawing area): PRESS, MOVE, [TAP / LONG_PRESS], RELEASE.
void padPointer(SuperUserDraw& c, const SuperUserPointer& e, void*) {
  lgfx::LGFX_Sprite* canvas = c.GetCanvas();
  if (!canvas) return;
  switch (e.Type) {
    case SuperUserPointer::PRESS:      lastX = e.X; lastY = e.Y; canvas->fillCircle(e.X, e.Y, 2, TFT_CYAN); c.Invalidate(); break;
    case SuperUserPointer::MOVE:       canvas->drawLine(lastX, lastY, e.X, e.Y, TFT_CYAN); lastX = e.X; lastY = e.Y; c.Invalidate(); break;
    case SuperUserPointer::LONG_PRESS: c.ClearCanvas(TFT_BLACK); c.Repaint(); break;               // hold = clear
    default: break;
  }
}

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.fillScreen(TFT_BLACK);
  Enc.Begin(); BtnA.Begin(INPUT_PULLUP); BtnB.Begin(INPUT_PULLUP);

  gauge.SetBorder(1, TFT_WHITE);  gauge.SetLabel("Gauge");
  gauge.SetOnDraw(drawGauge);
  gauge.SetAutoRedraw(40);                      // Repaint() every 40 ms, driven by form.Service()

  pad.SetBorder(1, TFT_WHITE);  pad.SetLabel("Sketch");
  pad.SetPersistent(true);                      // the canvas keeps the strokes between redraws (costs width x height x 2 bytes)
  pad.SetOnDraw(drawPad);
  pad.SetOnPointer(padPointer);                 // makes the control touch-interactive

  form.Add(&gauge, false);                      // display only: never focused
  form.Add(&pad);
  form.Draw();
}

void loop() {
  form.Service();
}
