#pragma once
#include <Arduino.h>
#include <atomic>

// SuperButton
// Debounced button with click-length classification and double-click detection.
// All instances are serviced by ONE shared periodic hardware timer — no Poll() needed.
//
// Supported back-ends (selected automatically):
//   ESP32 (all variants)          : esp_timer        (callback runs in esp_timer task)
//   RP2040 / RP2350 (arduino-pico): pico-sdk repeating_timer (callback runs in IRQ)
//   Anything else, or if SUPERBUTTON_NO_TIMER is defined before including:
//                                   call SuperButton::Service() from loop()
//
// Event byte returned by GetEvent():
//   low nibble : 1 = short, 2 = medium, 3 = long (press duration)
//   SB_DOUBLE  : set if a second press followed within ClickD_MAX ms of release
//
// Rules:
//   - Instances must outlive the program (global/static) — they are never unlinked.
//   - Call Begin() from setup() / a single thread.
//   - Call GetEvent()/Reset() for a given button from a single thread.

#if !defined(SUPERBUTTON_NO_TIMER) && defined(ARDUINO_ARCH_ESP32)
    #define SB_BACKEND_ESP32 1
    #include <esp_timer.h>
#elif !defined(SUPERBUTTON_NO_TIMER) && defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
    #define SB_BACKEND_PICO 1
    #include <pico/time.h>
#else
    #define SB_BACKEND_MANUAL 1
#endif

class SuperButton {
public:
    static constexpr uint8_t  SB_DOUBLE  = 0x10;
    static constexpr uint32_t TICK_MS    = 5;
    static constexpr uint8_t  DEBOUNCE_N = 4;      // 4 x 5 ms = 20 ms stable level

    uint16_t Click1_MAX = 175;   // held <= this  -> short
    uint16_t Click2_MAX = 800;   // held <= this  -> medium, else long
    uint16_t ClickD_MAX = 150;   // window after release for a double-click

    SuperButton(uint8_t pin, bool activeLow = false) : Pin(pin), Invert(activeLow) {}
    SuperButton(const SuperButton&) = delete;
    SuperButton& operator=(const SuperButton&) = delete;

    // Call from setup(). Safe for any number of buttons.
    void Begin(uint8_t mode = INPUT) {
        if (Started) return;
        Started = true;
        pinMode(Pin, mode);
        Next = Head.load(std::memory_order_relaxed);
        Head.store(this, std::memory_order_release);   // publish fully-built node
        StartTimer();
    }

    // Non-blocking. Returns true and fills 'ev' if an event is waiting.
    bool GetEvent(uint8_t &ev) {
        uint8_t t = QTail.load(std::memory_order_relaxed);
        if (t == QHead.load(std::memory_order_acquire)) return false;
        ev = Queue[t];
        QTail.store((t + 1) & Q_MASK, std::memory_order_release);
        return true;
    }

    // Discard pending events and return to idle.
    void Reset() {
        QTail.store(QHead.load(std::memory_order_acquire), std::memory_order_release);
        St = IDLE;
        Result = 0;
    }

    uint8_t GetPin() const { return Pin; }
    bool IsPressed() const { return Pressed; }     // current debounced level

#if SB_BACKEND_MANUAL
    // No hardware timer on this platform: call this from loop() as often as possible.
    static void Service() {
        static uint32_t last = 0;
        uint32_t now = millis();
        if (now - last < TICK_MS) return;
        last = now;
        TickAll(now);
    }
#endif

private:
    enum State : uint8_t { IDLE, WAIT1, WAIT2, WAIT3, D_IDLE, D_WAIT };

    // Single-producer (timer) / single-consumer (GetEvent) lock-free ring buffer.
    static constexpr uint8_t Q_SIZE = 4;           // power of two
    static constexpr uint8_t Q_MASK = Q_SIZE - 1;

    const uint8_t Pin;
    const bool    Invert;
    bool          Started = false;
    SuperButton*  Next    = nullptr;
    volatile State St     = IDLE;
    uint8_t  Result  = 0;
    uint8_t  History = 0;
    volatile bool Pressed = false;
    uint32_t T0 = 0;
    uint8_t  Queue[Q_SIZE] = {};
    std::atomic<uint8_t> QHead{0}, QTail{0};

    static inline std::atomic<SuperButton*> Head{nullptr};

    // ---------- platform timer back-ends ----------
#if SB_BACKEND_ESP32
    static inline esp_timer_handle_t Timer = nullptr;
    static void StartTimer() {
        if (Timer) return;
        esp_timer_create_args_t a = {};
        a.callback = [](void*) { TickAll(millis()); };
        a.name     = "SuperButton";
        ESP_ERROR_CHECK(esp_timer_create(&a, &Timer));
        ESP_ERROR_CHECK(esp_timer_start_periodic(Timer, TICK_MS * 1000));
    }
#elif SB_BACKEND_PICO
    static inline repeating_timer_t Timer;
    static inline bool TimerRunning = false;
    static void StartTimer() {
        if (TimerRunning) return;
        TimerRunning = true;
        // Negative delay = fixed period between callback starts.
        add_repeating_timer_ms(-(int32_t)TICK_MS,
                               [](repeating_timer_t*) -> bool { TickAll(millis()); return true; },
                               nullptr, &Timer);
    }
#else
    static void StartTimer() {}
#endif

    static void TickAll(uint32_t now) {
        for (SuperButton* b = Head.load(std::memory_order_acquire); b; b = b->Next)
            b->Tick(now);
    }

    void Emit(uint8_t ev) {                        // dropped if queue full
        uint8_t h = QHead.load(std::memory_order_relaxed);
        uint8_t n = (h + 1) & Q_MASK;
        if (n == QTail.load(std::memory_order_acquire)) return;
        Queue[h] = ev;
        QHead.store(n, std::memory_order_release);
    }

    void Tick(uint32_t now) {
        // Debounce: level must be identical for DEBOUNCE_N consecutive samples
        History = (History << 1) | ((digitalRead(Pin) == HIGH) != Invert);
        const uint8_t m = (1 << DEBOUNCE_N) - 1;
        if ((History & m) == m)      Pressed = true;
        else if ((History & m) == 0) Pressed = false;
        // otherwise still bouncing: keep previous level

        switch (St) {
        case IDLE:
            if (Pressed) { St = WAIT1; T0 = now; }
            break;

        case WAIT1:
        case WAIT2:
        case WAIT3:
            if (!Pressed) {
                Result = (St == WAIT1) ? 1 : (St == WAIT2) ? 2 : 3;
                St = D_IDLE;
                T0 = now;
            } else if (St == WAIT1 && now - T0 > Click1_MAX) {
                St = WAIT2;
            } else if (St == WAIT2 && now - T0 > Click2_MAX) {
                St = WAIT3;
            }
            break;

        case D_IDLE:
            if (now - T0 > ClickD_MAX) { Emit(Result); St = IDLE; }
            else if (Pressed)          { Result |= SB_DOUBLE; St = D_WAIT; }
            break;

        case D_WAIT:
            if (!Pressed) { Emit(Result); St = IDLE; }
            break;
        }
    }
};
