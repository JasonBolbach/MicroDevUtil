#pragma once
#include <Arduino.h>

// SuperEncoder
// Quadrature rotary encoder with bounce rejection and 3-speed acceleration.
// Keeps a cumulative Position that moves by StepSlow / StepMedium / StepFast per
// detent depending on how quickly detents arrive.
//
// Decoding: a full Gray-code state machine on BOTH channels. A count is only
// committed when the encoder arrives back at a detent (rest) position, so:
//   - contact bounce just toggles between two adjacent states and nets to zero
//   - rocking a knob part-way toward the next detent and back produces nothing
//   - a direction reversal always counts at the slow rate (no fast jump backwards)
//
// Input modes (Begin):
//   useInterrupts = true  : CHANGE interrupts on both pins (ESP32, RP2040/RP2350 arduino-pico)
//   useInterrupts = false : call Poll() from loop() as fast as possible (any board)
//
// Rules:
//   - Instances must outlive the program (global/static) when using interrupts.
//   - The knob should be resting on a detent when Begin() runs (rest state is learned).

#if defined(ARDUINO_ARCH_ESP32)
    #define SE_ISR_ATTR        ARDUINO_ISR_ATTR
    #define SE_HAS_INTERRUPTS  1
#elif defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
    #define SE_ISR_ATTR
    #define SE_HAS_INTERRUPTS  1
#else
    #define SE_ISR_ATTR
    #define SE_HAS_INTERRUPTS  0
#endif

enum SE_Speed : uint8_t { SE_SLOW = 0, SE_MEDIUM, SE_FAST };

class SuperEncoder {
public:
    // Speed classification: time between consecutive detents in the same direction.
    uint16_t Fast_MS    = 25;    // dt <  Fast_MS            -> fast
    uint16_t Medium_MS  = 85;    // Fast_MS <= dt < Medium_MS -> medium, else slow
    int16_t  StepSlow   = 1;     // Position change per detent at each speed
    int16_t  StepMedium = 5;
    int16_t  StepFast   = 20;
    bool     Reverse    = false; // flip direction without rewiring

    // quartersPerDetent: 4 = one full quadrature cycle per click (most EC11 / KY-040)
    //                    2 = half cycle per click, 1 = every edge is a click
    SuperEncoder(uint8_t pinA, uint8_t pinB, uint8_t quartersPerDetent = 4)
        : PinA(pinA), PinB(pinB),
          QPD(quartersPerDetent == 1 || quartersPerDetent == 2 ? quartersPerDetent : 4) {}
    SuperEncoder(const SuperEncoder&) = delete;
    SuperEncoder& operator=(const SuperEncoder&) = delete;

    void Begin(bool useInterrupts = true, uint8_t mode = INPUT_PULLUP) {
        pinMode(PinA, mode);
        pinMode(PinB, mode);
        delay(2);                                  // let pull-ups settle
        Prev = Rest = ReadState();
        Q = 0;
#if SE_HAS_INTERRUPTS
        if (useInterrupts) {
    #if defined(ARDUINO_ARCH_ESP32)
            attachInterruptArg(digitalPinToInterrupt(PinA), Isr, this, CHANGE);
            attachInterruptArg(digitalPinToInterrupt(PinB), Isr, this, CHANGE);
    #else
            attachInterruptParam(digitalPinToInterrupt(PinA), Isr, CHANGE, this);
            attachInterruptParam(digitalPinToInterrupt(PinB), Isr, CHANGE, this);
    #endif
        }
#else
        (void)useInterrupts;
#endif
    }

    // Polling mode only. Call as often as possible (every loop pass, no delays).
    void Poll() { Tick(ReadState()); }

    // Cumulative value (with limits applied).
    int32_t GetPosition() {
        Lock(); int32_t p = Position; Unlock();
        return p;
    }

    void SetPosition(int32_t p) {
        Lock(); Position = Apply(p); Unlock();
    }

    // Accumulated steps (speed-scaled) since the last call; ignores limits.
    // Handy for "nudge this value" UIs.
    int32_t GetDelta() {
        Lock(); int32_t d = Delta; Delta = 0; Unlock();
        return d;
    }

    // Constrain Position to [min, max]. wrap = true rolls over instead of clamping.
    void SetLimits(int32_t min, int32_t max, bool wrap = false) {
        Lock();
        Min = min; Max = max; Wrap = wrap; Limited = (max > min);
        Position = Apply(Position);
        Unlock();
    }
    void ClearLimits() { Lock(); Limited = false; Unlock(); }

    SE_Speed GetSpeed() const { return LastSpeed; }   // speed of the most recent detent

    void Reset() {
        Lock(); Position = Apply(0); Delta = 0; Q = 0; Unlock();
    }

private:
    const uint8_t PinA, PinB, QPD;

    // Decoder state (touched only by the ISR / Poll)
    uint8_t  Prev = 0, Rest = 0;
    int8_t   Q = 0;                 // quarter-steps since last rest
    int8_t   LastDir = 0;
    uint32_t LastDetentMs = 0;

    // Shared with loop() — guarded by Lock()
    volatile int32_t  Position = 0;
    volatile int32_t  Delta    = 0;
    volatile SE_Speed LastSpeed = SE_SLOW;
    int32_t Min = 0, Max = 0;
    bool    Limited = false, Wrap = false;

#if defined(ARDUINO_ARCH_ESP32)
    portMUX_TYPE Mux = portMUX_INITIALIZER_UNLOCKED;
    SE_ISR_ATTR void Lock()   { portENTER_CRITICAL_SAFE(&Mux); }
    SE_ISR_ATTR void Unlock() { portEXIT_CRITICAL_SAFE(&Mux); }
#else
    // ISR can't be pre-empted by loop(); loop() masks interrupts briefly.
    bool InIsr = false;
    void Lock()   { if (!InIsr) noInterrupts(); }
    void Unlock() { if (!InIsr) interrupts(); }
#endif

    SE_ISR_ATTR uint8_t ReadState() {
        return (digitalRead(PinA) ? 2 : 0) | (digitalRead(PinB) ? 1 : 0);
    }

#if SE_HAS_INTERRUPTS
    static SE_ISR_ATTR void Isr(void* arg) {
        SuperEncoder* e = static_cast<SuperEncoder*>(arg);
    #if !defined(ARDUINO_ARCH_ESP32)
        e->InIsr = true;
    #endif
        e->Tick(e->ReadState());
    #if !defined(ARDUINO_ARCH_ESP32)
        e->InIsr = false;
    #endif
    }
#endif

    SE_ISR_ATTR bool IsRest(uint8_t s) const {
        if (QPD == 1) return true;
        if (QPD == 2) return s == Rest || s == (Rest ^ 3);
        return s == Rest;
    }

    SE_ISR_ATTR void Tick(uint8_t s) {
        if (s == Prev) return;
        // Gray-code transition table, index = prev<<2 | cur.
        // +1 / -1 = valid quarter step, 0 = invalid (both bits changed / missed edge).
        static constexpr int8_t T[16] = { 0,+1,-1, 0,  -1, 0, 0,+1,
                                          +1, 0, 0,-1,   0,-1,+1, 0 };
        int8_t d = T[(Prev << 2) | s];
        Prev = s;
        Q += d;

        if (IsRest(s)) {
            // Back on a detent. Commit only if we really travelled there.
            // Rocking/bounce returns Q to 0. Threshold QPD/2 tolerates one missed edge.
            int8_t need = (QPD > 1) ? QPD / 2 : 1;
            if      (Q >=  need) Detent(+1);
            else if (Q <= -need) Detent(-1);
            Q = 0;
        }
    }

    SE_ISR_ATTR void Detent(int8_t dir) {
        if (Reverse) dir = -dir;
        uint32_t now = millis();
        uint32_t dt  = now - LastDetentMs;
        LastDetentMs = now;

        SE_Speed sp = (dir != LastDir) ? SE_SLOW
                    : (dt < Fast_MS)   ? SE_FAST
                    : (dt < Medium_MS) ? SE_MEDIUM
                    :                    SE_SLOW;
        LastDir = dir;

        int32_t step = dir * (sp == SE_FAST ? StepFast : sp == SE_MEDIUM ? StepMedium : StepSlow);

        Lock();
        Position  = Apply(Position + step);
        Delta    += step;
        LastSpeed = sp;
        Unlock();
    }

    SE_ISR_ATTR int32_t Apply(int32_t p) const {
        if (!Limited) return p;
        if (Wrap) {
            int32_t range = Max - Min + 1;
            int32_t r = (p - Min) % range;
            if (r < 0) r += range;
            return Min + r;
        }
        return p < Min ? Min : p > Max ? Max : p;
    }
};
