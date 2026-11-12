#pragma once
// SuperColorPicker - HSV color picker built on SuperControl, edited with a SuperEncoder and two SuperButtons.
//  * A swatch with the current color, and three gradient bars: H (hue 0-359), S (saturation 0-100),
//    V (value 0-100). Each bar shows what the color would be along that axis.
//  * Editing: encoder changes the active component (ColCursor frame + label), ButtonA short = next component
//    (H -> S -> V -> H), ButtonA long = accept, ButtonB short = cancel, ButtonB long = revert and keep editing.
//    OnChange = the color being edited changed (live), OnAccept = accepted.
//  * Get / set as RGB565 (for LovyanGFX drawing) or 8-bit RGB. The stored color is kept as HSV.
#include "SuperControl.h"
#include "SuperEditIO.h"

class SuperColorPicker : public SuperControl {
public:
    SuperColorPicker(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font,
                     SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB)
        : SuperControl(dst, x, y, width, font), IO(enc, btnA, btnB) {}

    void SetAbort(bool (*fn)()) { IO.Abort = fn; }
    void SetOnChange(void (*fn)(void*), void* user = nullptr) { IO.OnChange = fn; IO.OnChangeUser = user; }
    void SetOnAccept(void (*fn)(void*), void* user = nullptr) { IO.OnAccept = fn; IO.OnAcceptUser = user; }

    void SetHSV(int h, int s, int v) { Cur = { Wrap360(h), std::clamp(s, 0, 100), std::clamp(v, 0, 100) }; Dirty = true; Refresh(); }
    void SetRGB(uint8_t r, uint8_t g, uint8_t b) { Cur = FromRGB(r, g, b); Dirty = true; Refresh(); }
    void SetColor565(uint16_t c) { SetRGB((c >> 8) & 0xF8, (c >> 3) & 0xFC, (c << 3) & 0xF8); }
    void GetHSV(int& h, int& s, int& v) const { h = Cur.h; s = Cur.s; v = Cur.v; }
    void GetRGB(uint8_t& r, uint8_t& g, uint8_t& b) const { ToRGB(Cur, r, g, b); }
    uint16_t GetColor565() const { uint8_t r, g, b; ToRGB(Cur, r, g, b); return To565(r, g, b); }
    uint32_t GetColor888() const { uint8_t r, g, b; ToRGB(Cur, r, g, b); return ((uint32_t)r << 16) | (g << 8) | b; }

    bool IsEditing() const override { return State == EDIT_ACTIVE; }
    // Touch works directly on the bands, no activation needed: touching a bar sets that component (H / S / V) at once,
    // dragging along it adjusts live, and lifting the finger commits (OnAccept). Elsewhere: tap a letter = select that row,
    // tap the swatch or long press = accept, tap outside = cancel; other drags act like the encoder.
    bool StartOnPress(int x, int y) override { return OnBar(x, y) >= 0; }
    bool OnPointer(const SuperPointerEvent& e) override {
        if (State != EDIT_ACTIVE) return false;
        int row = OnBar(e.X, e.Y), sRow = OnBar(e.StartX, e.StartY);
        switch (e.Type) {
        case SuperPointerEvent::PRESS:
            if (row >= 0) { Comp = row; SetFromX(e.X); Changed(); return true; }
            return false;
        case SuperPointerEvent::TAP:
            if (row >= 0) { Comp = row; SetFromX(e.X); IO.Inject(SuperEditIO::A_LONG); return true; }       // set + commit
            if (RowAtY(e.Y) >= 0 && e.X >= BodyX() + SwatchW() && e.X < BarX0() - 3) { Comp = RowAtY(e.Y); Changed(); return true; }   // the letter
            if (HitTest(e.X, e.Y, TouchPad)) { IO.Inject(SuperEditIO::A_LONG); return true; }             // swatch / frame = accept
            IO.Inject(SuperEditIO::B_SHORT); return true;
        case SuperPointerEvent::DRAG_START:
            DragRow = sRow;
            if (DragRow >= 0) { IO.DragOk = true; Comp = DragRow; SetFromX(e.X); return true; }
            return SuperPointerDefault(IO, *this, e);
        case SuperPointerEvent::DRAG_MOVE:
            if (DragRow >= 0) { SetFromX(e.X); return true; }
            return SuperPointerDefault(IO, *this, e);
        case SuperPointerEvent::DRAG_END:
            if (DragRow >= 0) { DragRow = -1; IO.DragOk = false; IO.Inject(SuperEditIO::A_LONG); return true; }   // lift = commit
            return SuperPointerDefault(IO, *this, e);
        default:
            return SuperPointerDefault(IO, *this, e);
        }
    }
    bool StartEdit() override { Work = Cur; Comp = 0; IO.Flush(); State = EDIT_ACTIVE; Push(); return true; }
    SuperEditState Service() override {
        if (State != EDIT_ACTIVE) return EDIT_IDLE;
        if (IO.Aborted()) return Finish(EDIT_CANCELLED);
        bool redraw = false;
        int32_t d = IO.Delta();
        if (d) {
            HSV n = Work;
            if (Comp == 0)      n.h = Wrap360(n.h + (int)d);
            else if (Comp == 1) n.s = std::clamp(n.s + (int)d, 0, 100);
            else                n.v = std::clamp(n.v + (int)d, 0, 100);
            if (n.h != Work.h || n.s != Work.s || n.v != Work.v) { Work = n; redraw = true; IO.Changed(); }
        }
        switch (IO.NextKey()) {
        case SuperEditIO::A_SHORT: Comp = (Comp + 1) % 3; redraw = true; break;
        case SuperEditIO::A_LONG:  Cur = Work; return Finish(EDIT_ACCEPTED);
        case SuperEditIO::B_SHORT: return Finish(EDIT_CANCELLED);
        case SuperEditIO::B_LONG:  Work = Cur; redraw = true; IO.Changed(); break;
        default: break;
        }
        if (redraw) Push();
        return EDIT_ACTIVE;
    }

protected:
    struct HSV { int h, s, v; };
    int RowH() { return std::max(FontH(), 8) + 2; }
    int BodyHeight() override { return 3 * RowH(); }

    void RenderBody(int x, int y, int w, int h) override {
        HSV c = State == EDIT_ACTIVE ? Work : Cur;
        uint8_t r, g, b; ToRGB(c, r, g, b);
        int sz = h;
        Spr.fillRect(x, y, sz, sz, To565(r, g, b));
        Spr.drawRect(x, y, sz, sz, ColFg);
        int rh = RowH();
        Spr.setFont(Font);
        int lw = (int)Spr.textWidth("W") + 3;
        int bx = x + sz + 4 + lw, bw = std::max(8, w - sz - 4 - lw);
        static const char names[3] = { 'H', 'S', 'V' };
        int val[3] = { c.h, c.s, c.v }, mx[3] = { 359, 100, 100 };
        for (int row = 0; row < 3; ++row) {
            int ry = y + row * rh;
            bool act = State == EDIT_ACTIVE && row == Comp;
            if (act) Spr.fillRect(x + sz + 4, ry, lw - 1, rh - 1, ColCursor);
            char s[2] = { names[row], 0 };
            Spr.setTextColor(act ? ColCursorFg : ColFg);
            Spr.setTextDatum(lgfx::textdatum_t::middle_left);
            Spr.drawString(s, x + sz + 5, ry + rh / 2);
            for (int i = 0; i < bw; ++i) {                           // gradient
                HSV t = c;
                int v = (int)((int64_t)i * mx[row] / (bw - 1));
                if (row == 0) t.h = v; else if (row == 1) t.s = v; else t.v = v;
                uint8_t tr, tg, tb; ToRGB(t, tr, tg, tb);
                Spr.drawFastVLine(bx + i, ry + 1, rh - 3, To565(tr, tg, tb));
            }
            int mxp = bx + (int)((int64_t)val[row] * (bw - 1) / mx[row]);
            Spr.fillRect(mxp - 1, ry, 3, rh - 1, act ? ColCursor : ColFg);   // marker
            Spr.drawRect(mxp - 1, ry, 3, rh - 1, 0x0000);
        }
    }

private:
    int DragRow = -1;
    // Layout shared by the drawing and the touch mapping.
    int SwatchW()  { return BodyHeight(); }
    int BodyX()    { return PosX + Inset(); }
    int LetterW()  { Spr.setFont(Font); return (int)Spr.textWidth("W") + 3; }
    int BarX0()    { return BodyX() + SwatchW() + 4 + LetterW(); }
    int BarW()     { return std::max(8, InnerW() - SwatchW() - 4 - LetterW()); }
    int RowAtY(int sy) {
        int y0 = GetBoxTop() + Inset(), r = (sy - y0) / RowH();
        return (sy >= y0 && r >= 0 && r < 3) ? r : -1;
    }
    // Which bar (0..2) is under a screen point (a little slack left / right), or -1.
    int OnBar(int sx, int sy) { int r = RowAtY(sy); return (r >= 0 && sx >= BarX0() - 3 && sx <= BarX0() + BarW() + 3) ? r : -1; }
    void Changed() { IO.Changed(); Push(); }
    void SetFromX(int sx) {
        static const int mx[3] = { 359, 100, 100 };
        int v = (int)(((int64_t)(sx - BarX0()) * mx[Comp] + (BarW() - 1) / 2) / (BarW() - 1));
        v = std::clamp(v, 0, mx[Comp]);
        HSV n = Work;
        if (Comp == 0) n.h = v; else if (Comp == 1) n.s = v; else n.v = v;
        if (n.h != Work.h || n.s != Work.s || n.v != Work.v) { Work = n; Changed(); }
    }
    SuperEditIO IO;
    HSV Cur = { 0, 100, 100 }, Work = { 0, 100, 100 };
    int Comp = 0;
    SuperEditState State = EDIT_IDLE;

    static int Wrap360(int h) { return ((h % 360) + 360) % 360; }
    static uint16_t To565(uint8_t r, uint8_t g, uint8_t b) { return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)); }
    static void ToRGB(const HSV& c, uint8_t& r, uint8_t& g, uint8_t& b) {
        float s = c.s / 100.f, v = c.v / 100.f, h = c.h / 60.f;
        int i = (int)h % 6; float f = h - (int)h;
        float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f)), R, G, B;
        switch (i) { case 0: R = v; G = t; B = p; break; case 1: R = q; G = v; B = p; break; case 2: R = p; G = v; B = t; break;
                     case 3: R = p; G = q; B = v; break; case 4: R = t; G = p; B = v; break; default: R = v; G = p; B = q; }
        r = (uint8_t)(R * 255 + .5f); g = (uint8_t)(G * 255 + .5f); b = (uint8_t)(B * 255 + .5f);
    }
    static HSV FromRGB(uint8_t r8, uint8_t g8, uint8_t b8) {
        float r = r8 / 255.f, g = g8 / 255.f, b = b8 / 255.f;
        float mx = std::max({ r, g, b }), mn = std::min({ r, g, b }), d = mx - mn, h = 0;
        if (d > 0) {
            if (mx == r) h = 60 * (g - b) / d; else if (mx == g) h = 60 * (2 + (b - r) / d); else h = 60 * (4 + (r - g) / d);
        }
        return { Wrap360((int)(h + .5f)), mx > 0 ? (int)(d / mx * 100 + .5f) : 0, (int)(mx * 100 + .5f) };
    }
    SuperEditState Finish(SuperEditState r) {
        State = EDIT_IDLE; Push();
        if (r == EDIT_ACCEPTED) IO.Accepted();
        return r;
    }
};
