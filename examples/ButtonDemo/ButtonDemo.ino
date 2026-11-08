// SuperButton demo: short / medium / long clicks, with double-click flag.
// Buttons are serviced by a background timer on ESP32 and RP2040 — no Poll() needed.
#include <Arduino.h>
#include <iterator>
#include <SuperButton.h>

// { GPIO, activeLow }  — add as many as you like
SuperButton Buttons[] = {
  { 0, true },     // ESP32-S3 BOOT button
};

void setup() {
  Serial.begin(115200);
  for (auto &b : Buttons) b.Begin(INPUT_PULLUP);
}

void loop() {
#if SB_BACKEND_MANUAL
  SuperButton::Service();          // only needed on boards without a timer back-end
#endif
  static const char* names[] = { "", "Short", "Medium", "Long" };
  for (size_t i = 0; i < std::size(Buttons); i++) {
    uint8_t ev;
    while (Buttons[i].GetEvent(ev)) {
      Serial.printf("Button %u: %s click%s\n", (unsigned)i, names[ev & 0x0F],
                    (ev & SuperButton::SB_DOUBLE) ? " + double" : "");
    }
  }
}
