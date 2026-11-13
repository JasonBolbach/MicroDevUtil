#pragma once
// SuperSpinBox - a number spinner built on SuperTextBox, edited with a SuperEncoder and two SuperButtons
// (see SuperEditIO.h).
//
//  * Integer value with Min / Max / Step, optional wrap-around, an optional fixed decimal point
//    (SetDecimals(1): the value 125 shows as 12.5 -- the value itself stays an integer) and optional
//    Prefix / Suffix text ("$", " C", " %").
//  * Digits get a fixed cell width so the text doesn't shift while the value changes.
//
// Editing (BeginEdit() blocks and returns true if accepted; StartEdit()/Service() is the non-blocking form):
//   Encoder            = change the value by detents * Step (the encoder's speed scaling still applies)
//   ButtonA short/long = accept
//   ButtonB short      = cancel
//   ButtonB long       = revert to the starting value and keep editing
//   Touch              = touch activates, drag up/down changes, lift accepts (tap left/right third = step down/up + accept)
#include "SuperTextBox.h"
#include "SuperEditIO.h"

class SuperSpinBox : public SuperTextBox {
public:
    bool ShowTouchHints = true;         // draw "-" and "+" at the ends while editing (touch zones)

    SuperSpinBox(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font,
                 SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB)
        : SuperTextBox(dst, x, y, width, font), IO(enc, btnA, btnB)
    {
        Alignment = ALIGN_CENTER;
        Sync();
    }

    void SetAbort(bool (*fn)()) { IO.Abort = fn; }
    // OnChange: the value being edited changed (live). OnAccept: an edit was accepted (GetValue() is new).
    void SetOnChange(void (*fn)(void*), void* user = nullptr) { IO.OnChange = fn; IO.OnChangeUser = user; }
    void SetOnAccept(void (*fn)(void*), void* user = nullptr) { IO.OnAccept = fn; IO.OnAcceptUser = user; }
    // Return false to refuse an accept (the edit continues).
    void SetValidator(bool (*fn)(int32_t value, void* user), void* user = nullptr) { Validator = fn; ValidatorUser = user; }

    void SetRange(int32_t mn, int32_t mx) { Min = mn; Max = mx < mn ? mn : mx; Value = std::clamp(Value, Min, Max); Sync(); }
    void SetStep(int32_t s)               { Step = s < 1 ? 1 : s; }
    void SetWrap(bool w)                  { Wrap = w; }
    void SetDecimals(int d)               { Decimals = std::clamp(d, 0, 6); Sync(); }
    void SetPrefix(const std::string& s)  { Prefix = s; Sync(); }
    void SetSuffix(const std::string& s)  { Suffix = s; Sync(); }

    void SetValue(int32_t v) { Value = std::clamp(v, Min, Max); Sync(); Refresh(); }
    int32_t GetValue() const { return Value; }
    int32_t GetLiveValue() const { return State == EDIT_ACTIVE ? Work : Value; }   // the value being edited (use inside OnChange)

    // ---- editing ----
    SuperEditIO* EditIO() { return &IO; }                          // the input plumbing: InjectDelta / Inject for keyboards, mice and tests
    bool IsEditing() const override { return State == EDIT_ACTIVE; }
    // Touch, like the text boxes: touching the box activates it at once (even when idle), dragging up / down changes the
    // value live, lifting the finger accepts (OnAccept). A plain tap: left third = one step down, right third = one step up,
    // middle = no change, and it accepts. Long press = accept, tap outside = cancel.
    // TouchAcceptOnLift = false: lifting does not accept (tap / long press as before).
    bool TouchAcceptOnLift = true;
    bool StartOnPress(int, int) override { return true; }
    bool OnPointer(const SuperPointerEvent& e) override {
        if (State != EDIT_ACTIVE) return false;
        bool in = HitTest(e.X, e.Y, TouchPad);
        switch (e.Type) {
        case SuperPointerEvent::PRESS:
            PressSeen = in; PressDragged = false;
            return in;
        case SuperPointerEvent::TAP: {
            bool seen = PressSeen; PressSeen = false;
            if (!in) return SuperPointerDefault(IO, *this, e);                       // outside: cancel
            int third = std::max(1, GetWidth() / 3), rx = e.X - GetX();
            if (rx < third)                       IO.InjectDelta(-1);
            else if (rx >= GetWidth() - third)    IO.InjectDelta(1);
            bool zone = rx < third || rx >= GetWidth() - third;
            if (TouchAcceptOnLift || !zone) IO.Inject(SuperEditIO::A_LONG);          // accept (a step zone only steps when lift-accept is off)
            (void)seen;
            return true; }
        case SuperPointerEvent::DRAG_START:
            PressDragged = true;
            if (!PressSeen && !HitTest(e.StartX, e.StartY, TouchPad)) return false;
            PressSeen = true;
            break;
        case SuperPointerEvent::DRAG_END: {
            bool grabbed = PressSeen; PressSeen = false;
            SuperPointerDefault(IO, *this, e);
            if (grabbed && TouchAcceptOnLift) IO.Inject(SuperEditIO::A_LONG);       // lift = accept
            return true; }
        case SuperPointerEvent::LONG_PRESS:
            PressSeen = false;
            break;
        default: break;
        }
        return SuperPointerDefault(IO, *this, e);
    }

    bool StartEdit() {                                             // non-blocking: then call Service()
        Work = Value;
        IO.Flush();
        State = EDIT_ACTIVE;
        Sync(); PushUnlessPress();
        return true;
    }

    // Returns EDIT_ACTIVE while editing, then EDIT_ACCEPTED or EDIT_CANCELLED once, then EDIT_IDLE.
    SuperEditState Service() {
        if (State != EDIT_ACTIVE) return EDIT_IDLE;
        if (IO.Aborted()) return Finish(EDIT_CANCELLED);
        bool redraw = false;

        int32_t d = IO.Delta();
        if (d) {
            int64_t nv = (int64_t)Work + (int64_t)d * Step;
            if (Wrap) {
                int64_t range = (int64_t)Max - Min + 1;
                nv = Min + (((nv - Min) % range) + range) % range;
            } else nv = std::clamp<int64_t>(nv, Min, Max);
            if (nv != Work) { Work = (int32_t)nv; Sync(); redraw = true; IO.Changed(); }
        }
        switch (IO.NextKey()) {
        case SuperEditIO::A_SHORT:
        case SuperEditIO::A_LONG:
            if (!Validator || Validator(Work, ValidatorUser)) { Value = Work; return Finish(EDIT_ACCEPTED); }
            break;
        case SuperEditIO::B_SHORT: return Finish(EDIT_CANCELLED);
        case SuperEditIO::B_LONG:  if (Work != Value) { Work = Value; Sync(); redraw = true; IO.Changed(); } break;
        default: break;
        }
        if (redraw) Refresh();
        return EDIT_ACTIVE;
    }

protected:
    int CellWidth(int i) override {
        char c = GetText()[i];
        if (c < '0' || c > '9') return SuperTextBox::CellWidth(i) + 1;
        int w = 1;
        for (char d = '0'; d <= '9'; ++d) { char s[2] = { d, 0 }; w = std::max(w, (int)Spr.textWidth(s)); }
        return w + 1;
    }
    void CellStyleAt(int, CellStyle& st) override {
        if (State == EDIT_ACTIVE) { st.fill = true; st.bg = ColCursor; st.fg = ColCursorFg; }
    }
    void RenderBody(int x, int y, int w, int h) override {
        SuperTextBox::RenderBody(x, y, w, h);
        if (State == EDIT_ACTIVE && ShowTouchHints) {              // "-" / "+" mark the touch zones
            Spr.setFont(Font); Spr.setTextColor(ColArrow); Spr.setTextDatum(lgfx::textdatum_t::middle_center);
            Spr.drawString("-", x + 6, y + h / 2);
            Spr.drawString("+", x + w - 6, y + h / 2);
        }
    }

private:
    SuperEditIO IO;
    bool (*Validator)(int32_t, void*) = nullptr;
    void* ValidatorUser = nullptr;

    bool PressSeen = false, PressDragged = false;
    int32_t Value = 0, Work = 0, Min = 0, Max = 100, Step = 1;
    bool    Wrap = false;
    int     Decimals = 0;
    std::string Prefix, Suffix;
    SuperEditState State = EDIT_IDLE;

    std::string Format(int32_t v) const {
        int64_t a = v < 0 ? -(int64_t)v : v, scale = 1;
        for (int i = 0; i < Decimals; ++i) scale *= 10;
        std::string s = std::to_string((long long)(a / scale));
        if (Decimals) {
            std::string f = std::to_string((long long)(a % scale));
            s += "." + std::string(Decimals - f.size(), '0') + f;
        }
        return Prefix + (v < 0 ? "-" : "") + s + Suffix;
    }
    void Sync() { SetText(Format(State == EDIT_ACTIVE ? Work : Value)); }

    SuperEditState Finish(SuperEditState r) {
        State = EDIT_IDLE;
        Sync(); Push();
        if (r == EDIT_ACCEPTED) IO.Accepted();
        return r;
    }
};
