#pragma once
// SuperSlider - horizontal slider built on SuperControl, edited with a SuperEncoder and two SuperButtons.
//  * Integer Min..Max with Step. The knob shows the value; ShowValue adds the number at the right.
//  * Editing: encoder = move the knob (detents * Step, speed scaling applies), ButtonA short/long = accept,
//    ButtonB short = cancel, ButtonB long = revert and keep editing. The knob turns ColCursor while editing.
//  * OnChange = value changed while editing (live), OnAccept = edit accepted.
#include "SuperControl.h"
#include "SuperEditIO.h"

class SuperSlider : public SuperControl {
public:
    bool ShowValue = true;

    SuperSlider(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font,
                SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB)
        : SuperControl(dst, x, y, width, font), IO(enc, btnA, btnB) {}

    void SetAbort(bool (*fn)()) { IO.Abort = fn; }
    void SetOnChange(void (*fn)(void*), void* user = nullptr) { IO.OnChange = fn; IO.OnChangeUser = user; }
    void SetOnAccept(void (*fn)(void*), void* user = nullptr) { IO.OnAccept = fn; IO.OnAcceptUser = user; }
    void SetRange(int32_t mn, int32_t mx) { Min = mn; Max = mx <= mn ? mn + 1 : mx; Value = std::clamp(Value, Min, Max); Dirty = true; }
    void SetStep(int32_t s) { Step = s < 1 ? 1 : s; }
    void SetValue(int32_t v) { Value = std::clamp(v, Min, Max); Dirty = true; Refresh(); }
    int32_t GetValue() const { return Value; }
    int32_t GetLiveValue() const { return State == EDIT_ACTIVE ? Work : Value; }   // the value being edited (use inside OnChange)

    SuperEditIO* EditIO() { return &IO; }                          // the input plumbing: InjectDelta / Inject for keyboards, mice and tests
    bool IsEditing() const override { return State == EDIT_ACTIVE; }
    // Touch, no activation needed: touching the track moves the value there at once (even on an idle slider), dragging keeps
    // moving it live, and lifting the finger accepts (OnAccept). Long press = accept, tap outside = cancel.
    bool StartOnPress(int, int) override { return true; }
    bool OnPointer(const SuperPointerEvent& e) override {
        if (State != EDIT_ACTIVE) return false;
        bool in = HitTest(e.X, e.Y, TouchPad);
        switch (e.Type) {
        case SuperPointerEvent::PRESS:
            if (!in) return false;
            MoveTo(e.X); return true;                                   // jump to the finger
        case SuperPointerEvent::TAP:
            if (!in) { IO.Inject(SuperEditIO::B_SHORT); return true; }
            MoveTo(e.X); IO.Inject(SuperEditIO::A_SHORT); return true;  // touched and lifted: accept
        case SuperPointerEvent::LONG_PRESS:
            if (!in) return false;
            MoveTo(e.X); IO.Inject(SuperEditIO::A_LONG); return true;
        case SuperPointerEvent::DRAG_START:
            IO.DragOk = HitTest(e.StartX, e.StartY, TouchPad);
            if (IO.DragOk) MoveTo(e.X);
            return IO.DragOk;
        case SuperPointerEvent::DRAG_MOVE:
            if (IO.DragOk) MoveTo(e.X);
            return IO.DragOk;
        case SuperPointerEvent::DRAG_END:
            if (IO.DragOk) { IO.DragOk = false; IO.Inject(SuperEditIO::A_SHORT); }   // lift = accept
            return true;
        }
        return false;
    }
    bool StartEdit() override {
        Work = Value; IO.Flush(); State = EDIT_ACTIVE; PushUnlessPress(); return true;
    }
    SuperEditState Service() override {
        if (State != EDIT_ACTIVE) return EDIT_IDLE;
        if (IO.Aborted()) return Finish(EDIT_CANCELLED);
        bool redraw = false;
        int32_t d = IO.Delta();
        if (d) {
            int64_t nv = std::clamp<int64_t>((int64_t)Work + (int64_t)d * Step, Min, Max);
            if (nv != Work) { Work = (int32_t)nv; redraw = true; IO.Changed(); }
        }
        switch (IO.NextKey()) {
        case SuperEditIO::A_SHORT:
        case SuperEditIO::A_LONG:  Value = Work; return Finish(EDIT_ACCEPTED);
        case SuperEditIO::B_SHORT: return Finish(EDIT_CANCELLED);
        case SuperEditIO::B_LONG:  if (Work != Value) { Work = Value; redraw = true; IO.Changed(); } break;
        default: break;
        }
        if (redraw) Push();
        return EDIT_ACTIVE;
    }

protected:
    int BodyHeight() override { return FontH() + 2; }
    void RenderBody(int x, int y, int w, int h) override {
        int32_t v = State == EDIT_ACTIVE ? Work : Value;
        int tw, kw, trackW;
        Metrics(w, h, tw, kw, trackW);
        int cy = y + h / 2;
        int kx = x + (int)((int64_t)(trackW - kw) * (v - Min) / (Max - Min));
        Spr.fillRect(x, cy - 1, trackW, 2, ColArrow);                    // track
        Spr.fillRect(x, cy - 1, kx - x + kw / 2, 2, ColFg);              // filled part
        Spr.fillRect(kx, y, kw, h, State == EDIT_ACTIVE ? ColCursor : ColFg);   // knob
        if (ShowValue) {
            char s[16]; snprintf(s, sizeof s, "%ld", (long)v);
            Spr.setTextColor(ColFg);
            Spr.setTextDatum(lgfx::textdatum_t::middle_right);
            Spr.drawString(s, x + w - 1, cy);
        }
    }

private:
    void Metrics(int w, int h, int& tw, int& kw, int& trackW) {
        tw = 0;
        if (ShowValue) {
            Spr.setFont(Font);
            char a[16], b[16];
            snprintf(a, sizeof a, "%ld", (long)Min); snprintf(b, sizeof b, "%ld", (long)Max);
            tw = std::max((int)Spr.textWidth(a), (int)Spr.textWidth(b)) + 4;
        }
        kw = std::max(5, h / 2);
        trackW = std::max(kw + 2, w - tw);
    }
    int32_t ValueAtX(int sx) {
        int tw, kw, trackW;
        Metrics(InnerW(), BodyHeight(), tw, kw, trackW);
        float f = (float)(sx - (PosX + Inset()) - kw / 2) / (float)(trackW - kw);
        int64_t v = Min + (int64_t)(f * (Max - Min) + (f < 0 ? -0.5f : 0.5f));
        v = Min + ((v - Min + Step / 2) / Step) * Step;                 // snap to Step
        return (int32_t)std::clamp<int64_t>(v, Min, Max);
    }
    void MoveTo(int sx) { int32_t v = ValueAtX(sx); if (v != Work) { Work = v; IO.Changed(); Push(); } }
    SuperEditIO IO;
    int32_t Value = 0, Work = 0, Min = 0, Max = 100, Step = 1;
    SuperEditState State = EDIT_IDLE;
    SuperEditState Finish(SuperEditState r) {
        State = EDIT_IDLE; Push();
        if (r == EDIT_ACCEPTED) IO.Accepted();
        return r;
    }
};
