// SuperTouch demo: click / drag gestures from any touch controller.
// Replace MyGetTouch() with a read from your touch library.
#include <Arduino.h>
#include <SuperTouch.h>

bool MyGetTouch(int16_t &X, int16_t &Y) {
  // Example (TFT_eSPI):  uint16_t x, y; if (!tft.getTouch(&x, &y)) return false; X = x; Y = y; return true;
  return false;
}

SuperTouch Touch(MyGetTouch);

void setup() {
  Serial.begin(115200);
  Touch.Radius = 10;           // pixels of wobble still counted as a click
  Touch.ClickD_MAX = 150;      // ms window for a double-click (0 = off, clicks arrive immediately)
}

void loop() {
  Touch.Poll();
  SuperTouchEvent e;
  while (Touch.GetEvent(e)) {
    switch (e.Type) {
      case ST_SHORT_CLICK:  Serial.printf("Short click%s at %d,%d\n",  e.Double ? " (double)" : "", e.X, e.Y); break;
      case ST_MEDIUM_CLICK: Serial.printf("Medium click%s at %d,%d\n", e.Double ? " (double)" : "", e.X, e.Y); break;
      case ST_LONG_CLICK:   Serial.printf("Long click%s at %d,%d\n",   e.Double ? " (double)" : "", e.X, e.Y); break;
      case ST_DRAG:
        if      (e.Phase == ST_DRAG_START) Serial.printf("Drag start at %d,%d\n", e.StartX, e.StartY);
        else if (e.Phase == ST_DRAG_MOVE)  Serial.printf("Drag to %d,%d\n", e.X, e.Y);
        else                               Serial.printf("Drag end at %d,%d (%lu ms)\n", e.X, e.Y, (unsigned long)e.Duration);
        break;
      default: break;
    }
  }
}
