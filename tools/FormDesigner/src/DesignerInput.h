#pragma once
// Desktop input for the Form Designer: mouse (move / buttons / wheel), keyboard (keys, typed text) and window events, delivered as a queue
// of events in the designer's LOGICAL pixel coordinates. LovyanGFX's SDL panel only reports a left-button touch, so the designer listens with
// its own SDL event watch (which sees every event before the panel does).
//
// Coordinates: SDL reports the mouse in window points; the panel draws its logical frame centred in the window, so
//   see Scale(): the panel scales the frame in 0.5 steps and centres it (it does not stretch), so maximizing shows bars; positions in the
//   bars are outside 0..logical size.
// Wheel events carry no position in older SDL: the last known mouse position is used.
#include <SDL.h>
#include <deque>
#include <mutex>
#include <string>
#include <cstring>
#include <cmath>

namespace FD {
struct InputEvent {
    enum Type { MOVE, DOWN, UP, WHEEL, KEY_DOWN, KEY_UP, TEXT, RESIZE, QUIT } type = MOVE;
    int x = 0, y = 0;            // logical mouse position (MOVE / DOWN / UP / WHEEL)
    int button = 0;              // SDL_BUTTON_LEFT / MIDDLE / RIGHT
    int clicks = 0;              // 1 = single, 2 = double click ...
    int wheel = 0;               // +1 = away from the user (up) per notch
    SDL_Keycode key = 0;         // KEY_*: SDLK_*
    Uint16 mod = 0;              // KEY_*: KMOD_* bits
    bool repeat = false;         // KEY_DOWN: auto-repeat
    char text[32] = {0};         // TEXT: UTF-8 typed text
    int w = 0, h = 0;            // RESIZE: new window size in points
};

class Input {
public:
    static void Begin(int logicalW, int logicalH) {
        LW = logicalW; LH = logicalH;
        SDL_AddEventWatch(Watch, nullptr);
    }
    // Next queued event; false when there is none.
    static bool Poll(InputEvent& e) {
        std::lock_guard<std::mutex> g(M);
        if (Q.empty()) return false;
        e = Q.front(); Q.pop_front(); return true;
    }
    static int MouseX() { return LastX; }
    static int MouseY() { return LastY; }
    static bool ButtonHeld(int b) { return b >= 0 && b < 8 && Held[b]; }
    // The panel's window (LovyanGFX creates it on the main thread, possibly just after init() returns): first window id that exists.
    static SDL_Window* Window() {
        for (Uint32 id = 1; id <= 16; ++id) if (SDL_Window* w = SDL_GetWindowFromID(id)) return w;
        return SDL_GetMouseFocus();
    }
    static Uint32 WindowId() { SDL_Window* w = Window(); return w ? SDL_GetWindowID(w) : 1; }
    // window point -> logical pixel (pure, testable). ww x wh = window size in points, rw x rh = renderer output size in pixels (hi-dpi:
    // rw > ww), lw x lh = logical frame. LovyanGFX's panel does NOT stretch the frame: it scales it by floor(2*out/frame)/2 (steps of 0.5)
    // and centres it, so a maximized window shows bars around the image. The same maths is used here, otherwise the mouse drifts.
    static void Scale(int wx, int wy, int ww, int wh, int rw, int rh, int lw, int lh, int& lx, int& ly) {
        if (ww < 1) ww = 1; if (wh < 1) wh = 1;
        float sx = (rw * 2 / lw) / 2.0f, sy = (rh * 2 / lh) / 2.0f;
        if (sx <= 0) sx = 0.5f; if (sy <= 0) sy = 0.5f;
        float px = (float)wx * rw / ww, py = (float)wy * rh / wh;               // point -> output pixel
        float ox = (rw - lw * sx) / 2.0f, oy = (rh - lh * sy) / 2.0f;          // where the image starts (centred)
        lx = (int)std::floor((px - ox) / sx);
        ly = (int)std::floor((py - oy) / sy);                                  // outside the image: negative / >= lw (in the bars)
    }
    static void ToLogical(int wx, int wy, int& lx, int& ly) {
        int ww = 1, wh = 1, rw = 1, rh = 1;
        if (SDL_Window* w = Window()) {
            SDL_GetWindowSize(w, &ww, &wh); rw = ww; rh = wh;
            if (SDL_Renderer* r = SDL_GetRenderer(w)) SDL_GetRendererOutputSize(r, &rw, &rh);
        }
        Scale(wx, wy, ww, wh, rw, rh, LW, LH, lx, ly);
    }

private:
    inline static int LW = 1, LH = 1, LastX = 0, LastY = 0;
    inline static bool Held[8] = {};
    inline static std::deque<InputEvent> Q;
    inline static std::mutex M;

    static void Push(const InputEvent& e) {
        std::lock_guard<std::mutex> g(M);
        if (Q.size() < 4096) Q.push_back(e);
    }
    // Translate one SDL event (called by the watch; public so tests can feed events directly).
public:
    static void Feed(const SDL_Event* s) {
        InputEvent e;
        switch (s->type) {
        case SDL_MOUSEMOTION:
            e.type = InputEvent::MOVE; ToLogical(s->motion.x, s->motion.y, e.x, e.y);
            LastX = e.x; LastY = e.y; Push(e); break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            e.type = s->type == SDL_MOUSEBUTTONDOWN ? InputEvent::DOWN : InputEvent::UP;
            ToLogical(s->button.x, s->button.y, e.x, e.y);
            e.button = s->button.button; e.clicks = s->button.clicks;
            if (e.button < 8) Held[e.button] = (e.type == InputEvent::DOWN);
            LastX = e.x; LastY = e.y; Push(e); break;
        case SDL_MOUSEWHEEL:
            e.type = InputEvent::WHEEL; e.x = LastX; e.y = LastY;
            e.wheel = s->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -s->wheel.y : s->wheel.y;
            if (e.wheel) Push(e); break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            e.type = s->type == SDL_KEYDOWN ? InputEvent::KEY_DOWN : InputEvent::KEY_UP;
            e.key = s->key.keysym.sym; e.mod = s->key.keysym.mod; e.repeat = s->key.repeat != 0;
            Push(e); break;
        case SDL_TEXTINPUT:
            e.type = InputEvent::TEXT; strncpy(e.text, s->text.text, sizeof e.text - 1);
            Push(e); break;
        case SDL_WINDOWEVENT:
            if (s->window.event == SDL_WINDOWEVENT_RESIZED || s->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                e.type = InputEvent::RESIZE; e.w = s->window.data1; e.h = s->window.data2; Push(e);
            }
            break;
        case SDL_QUIT:
            e.type = InputEvent::QUIT; Push(e); break;
        }
    }
private:
    static int Watch(void*, SDL_Event* s) { Feed(s); return 0; }
};
}
