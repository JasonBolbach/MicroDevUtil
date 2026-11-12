#pragma once
// Minimal desktop stand-in for Arduino.h so SuperGUI compiles in the SDL build.
// Pins are virtual: desktop_input.h drives them from mouse events.
#include <cstdint>
#include <chrono>
#include <thread>
#include <mutex>

#define HIGH 1
#define LOW  0
#define INPUT 0
#define INPUT_PULLUP 2
#define CHANGE 3

namespace desktop_pins {
    constexpr int COUNT = 16;
    inline volatile uint8_t level[COUNT] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}; // pulled up
}

inline uint32_t millis() {
    using namespace std::chrono;
    static const auto t0 = steady_clock::now();
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now() - t0).count();
}
inline void delay(uint32_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
inline void pinMode(uint8_t, uint8_t) {}
inline int  digitalRead(uint8_t p) { return p < desktop_pins::COUNT ? desktop_pins::level[p] : HIGH; }
// SuperEncoder guards its shared position with noInterrupts()/interrupts().
// On the desktop the "interrupt" is a worker thread, so these are a real lock.
inline std::recursive_mutex& desktop_isr_mutex() { static std::recursive_mutex m; return m; }
inline void noInterrupts() { desktop_isr_mutex().lock(); }
inline void interrupts()   { desktop_isr_mutex().unlock(); }
