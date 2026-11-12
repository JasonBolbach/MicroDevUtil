#pragma once
// SuperUserDraw - a control whose picture is drawn by YOUR code. It has the usual frame (border, label, Sleek style, focus highlight,
// dimming, DMA / shared buffer support); everything inside the frame is yours.
//
//   SuperUserDraw gauge(&lcd, 10, 10, 160, /*body height*/ 100);
//   gauge.SetBorder(1, TFT_WHITE);  gauge.SetLabel("Gauge");
//   gauge.SetOnDraw([](SuperUserDraw& c, lgfx::LGFX_Sprite* s, const SuperRect& r, void* user) {
//       s->drawCircle(r.x + r.w / 2, r.y + r.h / 2, 30, TFT_GREEN);       // draw inside 'r'; everything is clipped to it
//   });
//   gauge.SetAutoRedraw(50);                                              // repaint every 50 ms (animation), or call Repaint()
//
// THE DRAW EVENT   void fn(SuperUserDraw& ctl, lgfx::LGFX_Sprite* spr, const SuperRect& body, void* user)
//   'spr' is a sprite you draw on with the normal LovyanGFX calls; 'body' is the rectangle to draw in (the area inside the border, padding
//   and label), and the clip rectangle is already set to it. When the callback returns the library pushes the result to the display,
//   dimmed for an unfocused control, through DMA if enabled.
//   * Normal mode (default): 'spr' is the control's own render sprite, valid ONLY during the callback (with SharedBuffer it is a buffer
//     lent to the control). It arrives holding the control's background colour and the frame, NOT your previous picture: the callback is
//     called on every redraw (including focus changes) and must paint everything it wants to see. Do not keep the pointer, block, or draw
//     other controls from inside it. The sprite is 16-bit, byte-swapped RGB565: use LovyanGFX's drawing calls, not the raw buffer. The
//     colour 0x0001 is the transparent key when a label is not opaque (SuperControl::LabelOpaque): avoid it.
//   * Persistent canvas (SetPersistent(true)): the control owns a canvas sprite of the body size (0,0 = top-left, 'body' = 0,0,w,h) that KEEPS
//     what you drew. The callback runs only when you ask (Repaint(), or auto-redraw); focus changes and dimming just re-blit the canvas.
//     Use it for traces, sketch pads, games and anything drawn incrementally. Costs width x height x 2 bytes (follows UsePsram).
//
// REDRAW   Repaint()       call your draw event at the next redraw (needed in persistent mode; harmless in normal mode)
//          Invalidate()    redraw the control; normal mode calls your draw event, persistent mode just re-blits the canvas
//          SetAutoRedraw(ms)  Repaint() every ms milliseconds (0 = off). Driven by SuperForm::Service(); without a form call Update() from loop().
//          DeltaMs()       milliseconds since the previous draw event (for animation)
//
// TOUCH (optional)   SetOnPointer(fn)   void fn(SuperUserPointer e) with positions relative to the body ((0,0) = its top-left; may be
//   outside it while dragging). Events: PRESS (finger down), MOVE (finger moved, only once it left the click radius), then TAP or LONG_PRESS
//   if it was a click or a long hold, and always RELEASE last. Call ctl.Invalidate() / Repaint() inside to redraw. Touching the control
//   focuses it and runs the gesture as an "edit"; when the finger lifts the edit ends and SetOnAccept fires (like the slider).
//   Without a pointer callback the control is display only (add it to a SuperForm with focusable = false).
#include "SuperControl.h"

class SuperUserDraw;

struct SuperRect { int x, y, w, h; };

struct SuperUserPointer {
    enum Kind : uint8_t { PRESS, MOVE, RELEASE, TAP, LONG_PRESS } Type = PRESS;
    int X = 0, Y = 0;                      // current position, relative to the body's top-left
    int StartX = 0, StartY = 0;            // where the finger went down, relative to the body
};

typedef void (*SuperUserDrawFn)(SuperUserDraw& ctl, lgfx::LGFX_Sprite* spr, const SuperRect& body, void* user);
typedef void (*SuperUserPointerFn)(SuperUserDraw& ctl, const SuperUserPointer& e, void* user);

class SuperUserDraw : public SuperControl {
public:
    int PointerGraceMs = 150;              // a key-started edit (no touch) with no finger down ends after this long

    // width = control width, bodyHeight = height of the drawing area. 'font' only sets the default label font.
    SuperUserDraw(lgfx::LovyanGFX* dst, int x, int y, int width, int bodyHeight, const lgfx::IFont* font = &lgfx::fonts::Font2)
        : SuperControl(dst, x, y, width, font), BodyH(bodyHeight < 1 ? 1 : bodyHeight), Canvas(nullptr) {}

    void SetOnDraw(SuperUserDrawFn fn, void* user = nullptr) { DrawFn = fn; DrawUser = user; NeedPaint = true; Dirty = true; }
    void SetOnPointer(SuperUserPointerFn fn, void* user = nullptr) { PointerFn = fn; PointerUser = user; }
    void SetOnAccept(void (*fn)(void*), void* user = nullptr) { AcceptFn = fn; AcceptUser = user; }
    void SetBodyHeight(int h) { BodyH = h < 1 ? 1 : h; Dirty = true; }

    // Persistent canvas on / off (see the header comment). Turning it on clears the canvas and calls the draw event once.
    void SetPersistent(bool on) {
        if (on == Persistent) return;
        Persistent = on;
        if (!on) FreeCanvas();
        NeedPaint = true; Dirty = true;
    }
    bool IsPersistent() const { return Persistent; }
    // The canvas (persistent mode) for drawing outside the draw event, e.g. from your own code; nullptr in normal mode or before the first draw.
    // Coordinates start at (0,0) = the body's top-left. Call Invalidate() afterwards to show the change.
    lgfx::LGFX_Sprite* GetCanvas() { return Persistent && Canvas.getBuffer() ? &Canvas : nullptr; }
    void ClearCanvas(uint16_t colour) { if (GetCanvas()) Canvas.fillSprite(colour); Dirty = true; }
    void FreeCanvas() { Canvas.deleteSprite(); }

    void Repaint() { NeedPaint = true; Dirty = true; }
    void SetAutoRedraw(int ms) { AutoMs = ms < 0 ? 0 : ms; LastTick = lgfx::millis(); }
    uint32_t DeltaMs() const { return Delta; }
    SuperRect GetBodyRect() { return { Inset(), BoxOffsetPublic() + Inset(), InnerW(), BodyH }; }

    // Auto-redraw clock: true when a redraw is due (SuperForm then redraws the control in its proper look). Without a form call Update().
    bool Tick() override {
        if (AutoMs <= 0 || !IsVisible()) return false;
        uint32_t now = lgfx::millis();
        if (now - LastTick < (uint32_t)AutoMs) return false;
        LastTick = now;
        Repaint();
        return true;
    }
    void Update() { if (Tick()) Refresh(); }

    // ---- touch / edit ----
    bool IsEditing() const override { return State == EDIT_ACTIVE; }
    bool StartOnPress(int, int) override { return PointerFn != nullptr; }
    bool IdleTapActs() const override { return PointerFn != nullptr; }
    bool StartEdit() override {
        if (!PointerFn) return false;
        State = EDIT_ACTIVE; Done = Gesture = false; StartedAt = lgfx::millis();
        PushUnlessPress();
        return true;
    }
    SuperEditState Service() override {
        if (State != EDIT_ACTIVE) return EDIT_IDLE;
        if (Done) return Finish(EDIT_ACCEPTED);
        if (!Gesture && (int)(lgfx::millis() - StartedAt) > PointerGraceMs) return Finish(EDIT_CANCELLED);   // started by a key: no finger, nothing to do
        return EDIT_ACTIVE;
    }
    bool OnPointer(const SuperPointerEvent& e) override {
        if (State != EDIT_ACTIVE || !PointerFn) return false;
        int ox = PosX + Inset(), oy = GetBoxTop() + Inset();
        SuperUserPointer p;
        p.X = e.X - ox; p.Y = e.Y - oy; p.StartX = e.StartX - ox; p.StartY = e.StartY - oy;
        switch (e.Type) {
        case SuperPointerEvent::PRESS:
            Gesture = true; Send(SuperUserPointer::PRESS, p);
            break;
        case SuperPointerEvent::DRAG_START:
            if (!Gesture) { Gesture = true; SuperUserPointer q = p; q.X = p.StartX; q.Y = p.StartY; Send(SuperUserPointer::PRESS, q); }
            Send(SuperUserPointer::MOVE, p);
            break;
        case SuperPointerEvent::DRAG_MOVE:
            Send(SuperUserPointer::MOVE, p);
            break;
        case SuperPointerEvent::DRAG_END:
            Send(SuperUserPointer::RELEASE, p); End();
            break;
        case SuperPointerEvent::TAP:
            if (!Gesture) { Gesture = true; Send(SuperUserPointer::PRESS, p); }
            Send(SuperUserPointer::TAP, p); Send(SuperUserPointer::RELEASE, p); End();
            break;
        case SuperPointerEvent::LONG_PRESS:
            if (!Gesture) { Gesture = true; Send(SuperUserPointer::PRESS, p); }
            Send(SuperUserPointer::LONG_PRESS, p); Send(SuperUserPointer::RELEASE, p); End();
            break;
        }
        return true;
    }

protected:
    int BodyHeight() override { return BodyH; }
    void OnFontChanged() override {}

    void RenderBody(int x, int y, int w, int h) override {
        uint32_t now = lgfx::millis();
        if (Persistent && EnsureCanvas(w, h)) {
            if (NeedPaint) {
                NeedPaint = false;
                if (DrawFn) {
                    Delta = LastPaint ? now - LastPaint : 0; LastPaint = now;
                    Canvas.setClipRect(0, 0, w, h);
                    SuperRect r{ 0, 0, w, h };
                    DrawFn(*this, &Canvas, r, DrawUser);
                    Canvas.clearClipRect();
                }
            }
            Canvas.pushSprite(&Spr, x, y);                           // copy what the user has drawn into the control
            return;
        }
        NeedPaint = false;
        if (!DrawFn) return;
        Delta = LastPaint ? now - LastPaint : 0; LastPaint = now;
        Spr.setClipRect(x, y, w, h);
        SuperRect r{ x, y, w, h };
        DrawFn(*this, &Spr, r, DrawUser);
        Spr.clearClipRect();
    }

private:
    int  BodyH;
    SuperUserDrawFn    DrawFn = nullptr;     void* DrawUser = nullptr;
    SuperUserPointerFn PointerFn = nullptr;  void* PointerUser = nullptr;
    void (*AcceptFn)(void*) = nullptr;       void* AcceptUser = nullptr;
    bool Persistent = false, NeedPaint = true;
    lgfx::LGFX_Sprite Canvas;
    int  AutoMs = 0;
    uint32_t LastTick = 0, LastPaint = 0, Delta = 0, StartedAt = 0;
    SuperEditState State = EDIT_IDLE;
    bool Gesture = false, Done = false;

    int BoxOffsetPublic() { return GetBoxTop() - PosY; }

    // (Re)create the canvas when the body size changed (the picture is lost then, and the draw event runs again).
    bool EnsureCanvas(int w, int h) {
        if (Canvas.getBuffer() && Canvas.width() == w && Canvas.height() == h) return true;
        Canvas.deleteSprite();
        if (!AllocSprite(Canvas, w, h)) return false;
        Canvas.fillSprite(ColBg);
        NeedPaint = true;
        return true;
    }
    void Send(SuperUserPointer::Kind k, SuperUserPointer p) {
        p.Type = k;
        PointerFn(*this, p, PointerUser);
        Refresh();                                                   // show whatever the callback changed
    }
    void End() { Done = true; Gesture = false; }
    SuperEditState Finish(SuperEditState r) {
        State = EDIT_IDLE;
        Push();
        if (r == EDIT_ACCEPTED && AcceptFn) AcceptFn(AcceptUser);
        return r;
    }
};
