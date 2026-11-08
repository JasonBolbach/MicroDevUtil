// SuperEncoder demo: cumulative value with slow / medium / fast acceleration.
#include <Arduino.h>
#include <SuperEncoder.h>

#define PIN_ENC_A 4
#define PIN_ENC_B 5

SuperEncoder Enc(PIN_ENC_A, PIN_ENC_B);   // 3rd arg: 4 = full cycle per detent (EC11/KY-040)

void setup() {
  Serial.begin(115200);
  Enc.Begin();                 // INPUT_PULLUP, CHANGE interrupts on both pins
  Enc.SetLimits(0, 255);       // optional: clamp (add `true` to wrap)
}

void loop() {
  static int32_t last = -1;
  int32_t v = Enc.GetPosition();
  if (v != last) {
    static const char* speed[] = { "slow", "medium", "fast" };
    Serial.printf("Value %ld (%s)\n", (long)v, speed[Enc.GetSpeed()]);
    last = v;
  }
}
