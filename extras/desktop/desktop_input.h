#pragma once
// Desktop input for the SDL build, using SuperGUI. Emulates the interrupt/timer
// driven input of the ESP32 / RP2040 with a 1 ms worker thread:
//   mouse wheel        -> SuperEncoder  Enc
//   wheel click        -> SuperButton   ButtonA
//   right mouse button -> SuperButton   ButtonB
//   left mouse button  -> touch: LovyanGFX's SDL panel emulates it, use lcd.getTouch() (correct even if the window is resized)
// Call DesktopInput::Begin() once in setup(). No per-loop update is needed.
#define SUPERBUTTON_NO_TIMER
#include <SDL.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <SuperButton.h>
#include <SuperEncoder.h>

extern bool* g_running;   // defined in sdl_main.cpp

namespace DesktopInput {
    // Virtual pin numbers (see compat/Arduino.h)
    constexpr uint8_t PIN_ENC_A = 1, PIN_ENC_B = 2, PIN_BTN_A = 3, PIN_BTN_B = 4;

    inline SuperEncoder Enc(PIN_ENC_A, PIN_ENC_B);
    inline SuperButton  ButtonA(PIN_BTN_A, true);   // wheel click
    inline SuperButton  ButtonB(PIN_BTN_B, true);   // right click

    inline std::atomic<int>  PendingTicks{0};       // wheel notches not yet fed to the encoder
    inline std::atomic<bool> WorkerRun{false};
    inline std::thread       Worker;

    // True once the window has been closed; blocking code should bail out.
    inline bool Quitting() { return g_running && !*g_running; }

    // Runs on whichever thread SDL pushes the event from, before LovyanGFX can discard it.
    inline int EventWatch(void*, SDL_Event* e) {
        switch (e->type) {
        case SDL_MOUSEWHEEL:
            PendingTicks += e->wheel.y;
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            uint8_t lvl = (e->type == SDL_MOUSEBUTTONDOWN) ? LOW : HIGH;
            if (e->button.button == SDL_BUTTON_MIDDLE) desktop_pins::level[PIN_BTN_A] = lvl;
            if (e->button.button == SDL_BUTTON_RIGHT)  desktop_pins::level[PIN_BTN_B] = lvl;
            break;
        }
        }
        return 0;
    }

    inline void SetEnc(uint8_t state) {             // state: A in bit1, B in bit0
        desktop_pins::level[PIN_ENC_A] = (state >> 1) & 1;
        desktop_pins::level[PIN_ENC_B] = state & 1;
        Enc.Poll();
    }

    // One wheel notch = one full quadrature cycle (one detent). Rest state is 3 (both high).
    inline void FeedTick(int dir) {
        static const uint8_t fwd[] = {2, 0, 1, 3};
        static const uint8_t rev[] = {1, 0, 2, 3};
        for (uint8_t s : (dir > 0 ? fwd : rev)) SetEnc(s);
    }

    inline void WorkerMain() {
        while (WorkerRun) {
            int n = PendingTicks.load();
            if (n > 0)      { --PendingTicks; FeedTick(+1); }
            else if (n < 0) { ++PendingTicks; FeedTick(-1); }
            SuperButton::Service();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // Declared after Worker so it is destroyed first and joins the thread at exit.
    struct Joiner { ~Joiner() { WorkerRun = false; if (Worker.joinable()) Worker.join(); } };
    inline Joiner TheJoiner;

    inline void Begin() {
        if (WorkerRun) return;
        Enc.Begin(false);                           // polling mode, driven by the worker
        ButtonA.Begin(INPUT_PULLUP);
        ButtonB.Begin(INPUT_PULLUP);
        SDL_AddEventWatch(EventWatch, nullptr);
        WorkerRun = true;
        Worker = std::thread(WorkerMain);
    }
}
