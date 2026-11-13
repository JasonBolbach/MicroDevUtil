// SuperGUI Form Designer - step 1: the input spike.
// One SDL window (logical 1400 x 820, resizable: the content scales), a mock of the panel layout, a 320x240 form outline on the design
// surface, and a live event log. It proves that the designer receives, in logical pixels: mouse move / buttons / wheel, keys, typed text
// and window resizes. Run with the environment variable FD_SELFTEST=1 for an automatic check using synthetic SDL events.
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>
#include "DesignerConfig.h"
#include "DesignerInput.h"

extern bool* g_running;                              // set in sdl_main.cpp: false once the window is closed
using FD::Input; using FD::InputEvent;

static LGFX* lcd = nullptr;
static lgfx::LGFX_Sprite scr;                        // everything is drawn here, then pushed to the window in one go
static std::deque<std::string> logLines;
static std::string typed;                            // the text-entry demo
static int formW = FD::DEFAULT_FORM_W, formH = FD::DEFAULT_FORM_H;
static bool dirty = true;
static int winW = 0, winH = 0;

static void Log(const char* fmt, ...) {
    char b[160]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    logLines.push_back(b); while (logLines.size() > 16) logLines.pop_front();
    dirty = true;
}

static void Frame(int x, int y, int w, int h, const char* title) {
    scr.fillRect(x, y, w, h, 0x18C3); scr.drawRect(x, y, w, h, 0x4A69);
    scr.setFont(&lgfx::fonts::Font2); scr.setTextColor(0x07FF); scr.setTextDatum(lgfx::textdatum_t::top_left);
    scr.drawString(title, x + 8, y + 6);
}

static void Render() {
    scr.fillSprite(0x1082);
    scr.fillRect(0, 0, FD::WIN_W, 44, 0x18C3); scr.drawFastHLine(0, 44, FD::WIN_W, 0x4A69);
    scr.setFont(&lgfx::fonts::Font2); scr.setTextColor(0xFFFF);
    scr.drawString("FormDesigner - input spike    (move, click, wheel, type; all coordinates are logical pixels)", 10, 14);
    Frame(10, 56, 290, 754, "Controls on form");
    Frame(1000, 56, 390, 754, "Properties");
    Frame(310, 56, 680, 754, "Design surface");
    scr.fillRect(320, 84, 660, 716, 0x2124);
    // the form outline: default 320x240, never larger than 1024x768
    int fx = 340, fy = 110;
    scr.fillRect(fx, fy, formW, formH, 0x0000);
    scr.drawRect(fx - 1, fy - 1, formW + 2, formH + 2, 0xFFE0);
    scr.setTextColor(0xFFE0);
    char b[96]; snprintf(b, sizeof b, "Form %d x %d   (max %d x %d)", formW, formH, FD::MAX_FORM_W, FD::MAX_FORM_H);
    scr.drawString(b, fx, fy - 18);
    // crosshair at the mouse
    int mx = Input::MouseX(), my = Input::MouseY();
    scr.drawFastHLine(0, my, FD::WIN_W, 0x7BEF); scr.drawFastVLine(mx, 0, FD::WIN_H, 0x7BEF);
    scr.drawCircle(mx, my, 6, Input::ButtonHeld(SDL_BUTTON_LEFT) ? 0xF800 : 0x07E0);
    snprintf(b, sizeof b, "mouse %d, %d   window %d x %d   L%d M%d R%d", mx, my, winW, winH,
             Input::ButtonHeld(SDL_BUTTON_LEFT), Input::ButtonHeld(SDL_BUTTON_MIDDLE), Input::ButtonHeld(SDL_BUTTON_RIGHT));
    scr.setTextColor(0xFFFF); scr.drawString(b, 1010, 84);
    // text-entry demo
    scr.setTextColor(0x07FF); scr.drawString("Type here (plain 1-6, r, l must NOT resize / rotate the window):", 1010, 120);
    scr.drawRect(1010, 144, 370, 26, 0x7BEF);
    scr.setTextColor(0xFFFF); scr.drawString((typed + ((SDL_GetTicks() / 500) % 2 ? "_" : " ")).c_str(), 1016, 150);
    // event log
    scr.setTextColor(0x07FF); scr.drawString("Events", 1010, 200);
    int y = 224; scr.setTextColor(0xFFFF);
    for (auto& l : logLines) { scr.drawString(l.c_str(), 1010, y); y += 18; }
    scr.pushSprite(lcd, 0, 0);
    dirty = false;
}

static void Handle(const InputEvent& e) {
    switch (e.type) {
    case InputEvent::MOVE:     dirty = true; break;
    case InputEvent::DOWN:     Log("DOWN  btn %d x%d at %d,%d", e.button, e.clicks, e.x, e.y); break;
    case InputEvent::UP:       Log("UP    btn %d at %d,%d", e.button, e.x, e.y); break;
    case InputEvent::WHEEL:    Log("WHEEL %+d at %d,%d", e.wheel, e.x, e.y); break;
    case InputEvent::KEY_DOWN:
        Log("KEY   %s mod %04x%s", SDL_GetKeyName(e.key), e.mod, e.repeat ? " (repeat)" : "");
        if (e.key == SDLK_BACKSPACE && !typed.empty()) {
            do typed.pop_back(); while (!typed.empty() && (typed.back() & 0xC0) == 0x80);   // whole UTF-8 character
        }
        if (e.key == SDLK_UP   && !(e.mod & KMOD_SHIFT) && formH < FD::MAX_FORM_H) formH += 8;   // the size limits, try them
        if (e.key == SDLK_DOWN && !(e.mod & KMOD_SHIFT) && formH > 8) formH -= 8;
        if (e.key == SDLK_RIGHT && formW < FD::MAX_FORM_W) formW += 8;
        if (e.key == SDLK_LEFT  && formW > 8) formW -= 8;
        break;
    case InputEvent::TEXT:     Log("TEXT  \"%s\"", e.text); typed += e.text; break;
    case InputEvent::RESIZE:   winW = e.w; winH = e.h; Log("RESIZE %d x %d", e.w, e.h); break;
    default: break;
    }
}

// ---- synthetic-event self test (FD_SELFTEST=1) ----
static int fails = 0;
static void Check(bool ok, const char* what) { printf("%s  %s\n", ok ? "ok  " : "FAIL", what); if (!ok) ++fails; }
static bool Next(InputEvent& e) { return Input::Poll(e); }
static void SelfTest() {
    SDL_Window* w = nullptr;
    for (int i = 0; i < 100 && !(w = Input::Window()); ++i) SDL_Delay(50);   // the window is created on the main thread
    Check(w != nullptr, "SDL window found");
    if (!w) exit(1);
    InputEvent e;
    while (Next(e)) {}                                        // drop events from start-up
    auto motion = [&](int x, int y) { SDL_Event s{}; s.type = SDL_MOUSEMOTION; s.motion.windowID = Input::WindowId(); s.motion.x = x; s.motion.y = y; SDL_PushEvent(&s); };
    int ww, wh; SDL_GetWindowSize(w, &ww, &wh);
    motion(ww / 2, wh / 2);
    Check(Next(e) && e.type == InputEvent::MOVE && std::abs(e.x - FD::WIN_W / 2) <= 1 && std::abs(e.y - FD::WIN_H / 2) <= 1, "mouse at window centre -> logical centre");
    motion(0, 0);
    Check(Next(e) && e.x == 0 && e.y == 0, "mouse at the top-left corner -> 0,0");
    // A resized window (SDL only lets the main thread resize it, so the scaling is checked as a pure function; try the real thing by hand).
    int lx, ly;
    Input::Scale(700, 410, 700, 410, FD::WIN_W, FD::WIN_H, lx, ly);
    Check(lx == 1400 && ly == 820, "window half the logical size: its bottom-right corner is the logical bottom-right");
    Input::Scale(350, 205, 700, 410, FD::WIN_W, FD::WIN_H, lx, ly);
    Check(lx == 700 && ly == 410, "window half the logical size: its centre is the logical centre");
    Input::Scale(1000, 500, 2800, 1640, FD::WIN_W, FD::WIN_H, lx, ly);
    Check(lx == 500 && ly == 250, "window twice the logical size (hi-dpi point scale)");
    Input::Scale(5, 5, 0, 0, FD::WIN_W, FD::WIN_H, lx, ly);
    Check(lx >= 0 && ly >= 0, "a zero-size window does not divide by zero");
    SDL_Event b{}; b.type = SDL_MOUSEBUTTONDOWN; b.button.windowID = Input::WindowId(); b.button.button = SDL_BUTTON_RIGHT; b.button.clicks = 2; b.button.x = ww / 2; b.button.y = wh / 2; SDL_PushEvent(&b);
    Check(Next(e) && e.type == InputEvent::DOWN && e.button == SDL_BUTTON_RIGHT && e.clicks == 2 && Input::ButtonHeld(SDL_BUTTON_RIGHT), "right button down (double click count, held state)");
    b.type = SDL_MOUSEBUTTONUP; SDL_PushEvent(&b);
    Check(Next(e) && e.type == InputEvent::UP && !Input::ButtonHeld(SDL_BUTTON_RIGHT), "right button up clears the held state");
    SDL_Event wh2{}; wh2.type = SDL_MOUSEWHEEL; wh2.wheel.windowID = Input::WindowId(); wh2.wheel.y = 3; wh2.wheel.preciseY = 3.0f; Input::Feed(&wh2);
    Check(Next(e) && e.type == InputEvent::WHEEL && e.wheel == 3, "wheel notches");
    SDL_Event k{}; k.type = SDL_KEYDOWN; k.key.windowID = Input::WindowId(); k.key.keysym.sym = SDLK_a; k.key.keysym.mod = KMOD_LSHIFT; SDL_PushEvent(&k);
    Check(Next(e) && e.type == InputEvent::KEY_DOWN && e.key == SDLK_a && (e.mod & KMOD_SHIFT), "key down with modifiers");
    SDL_Event t{}; t.type = SDL_TEXTINPUT; t.text.windowID = Input::WindowId(); strcpy(t.text.text, "A\xC3\xA9"); Input::Feed(&t);
    Check(Next(e) && e.type == InputEvent::TEXT && std::string(e.text) == "A\xC3\xA9", "typed text arrives as UTF-8");
    printf("%s (%d failed)\n", fails ? "SELFTEST FAILED" : "SELFTEST PASSED", fails);
    fflush(stdout);
    exit(fails ? 1 : 0);
}

void setup() {
    // LovyanGFX's SDL panel turns plain 1-6 into "rescale the window" and r / l into "rotate the screen". The designer has text fields, so
    // require a modifier combination nobody presses: the shortcuts are then effectively off.
    lgfx::Panel_sdl::setShortcutKeymod((SDL_Keymod)(KMOD_LSHIFT | KMOD_LCTRL | KMOD_LALT | KMOD_LGUI));
    lcd = new LGFX(FD::WIN_W, FD::WIN_H, 1);
    lcd->init();
    scr.setColorDepth(16); scr.createSprite(FD::WIN_W, FD::WIN_H);
    Input::Begin(FD::WIN_W, FD::WIN_H);
    if (SDL_Window* w = Input::Window()) SDL_GetWindowSize(w, &winW, &winH);
    if (getenv("FD_SELFTEST")) SelfTest();
}

void loop() {
    InputEvent e;
    while (Input::Poll(e)) Handle(e);
    static Uint32 blink = 0;
    if (SDL_GetTicks() / 500 != blink) { blink = SDL_GetTicks() / 500; dirty = true; }     // the caret blinks
    if (dirty) Render();
    static bool snapped = false;                          // FD_SNAPSHOT=file.raw: write the first frame (raw big-endian RGB565) and quit
    if (!snapped && getenv("FD_SNAPSHOT")) {
        snapped = true;
        if (FILE* f = fopen(getenv("FD_SNAPSHOT"), "wb")) { fwrite(scr.getBuffer(), 2, (size_t)FD::WIN_W * FD::WIN_H, f); fclose(f); }
        exit(0);
    }
    SDL_Delay(8);
}
