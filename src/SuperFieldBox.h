#pragma once
// SuperFieldBox - base for controls made of a few numeric fields with separators (time HH:MM:SS, date YYYY-MM-DD).
// SuperTimeBox and SuperDateBox (SuperTimeBox.h) are built on it; you normally use those.
//
// Editing works like a spin box that has several values:
//   Encoder            = change the current field by detents * 1 (the encoder's speed scaling still applies). Every field is kept in its
//                        own range (and wraps around when Wrap is true); a field that depends on another (the day on the month) is
//                        pulled back into range automatically.
//   ButtonA short      = next field (after the last one, back to the first)
//   ButtonA long       = accept
//   ButtonB short      = cancel
//   ButtonB long       = revert to the starting value and keep editing
//   Touch              = touch a field to select it (highlighted at once), drag up / down to change it, lift = accept
//                        (TouchAcceptOnLift = false: lift only selects; long press accepts). Tap outside = cancel.
// Stored values change only on accept. Callbacks: SetOnChange (value being edited changed, live), SetOnAccept (accepted).
#include "SuperTextBox.h"
#include "SuperEditIO.h"
#include <vector>
#include <cstdio>

class SuperFieldBox : public SuperTextBox {
public:
    uint16_t ColLiteral = 0xBDF7;          // separators
    bool     TouchAcceptOnLift = true;

    void SetAbort(bool (*fn)()) { IO.Abort = fn; }
    void SetOnChange(void (*fn)(void*), void* user = nullptr) { IO.OnChange = fn; IO.OnChangeUser = user; }
    void SetOnAccept(void (*fn)(void*), void* user = nullptr) { IO.OnAccept = fn; IO.OnAcceptUser = user; }
    void SetWrap(bool w) { Wrap = w; }                 // true: a field goes round (59 -> 00) instead of stopping at its limit

    // ---- editing ----
    bool IsEditing() const override { return State == EDIT_ACTIVE; }
    bool StartOnPress(int, int) override { return true; }

    bool StartEdit() override {
        Work = Vals; Cur = 0;
        IO.Flush();
        State = EDIT_ACTIVE;
        Sync(); PushUnlessPress();
        return true;
    }

    // Returns EDIT_ACTIVE while editing, then EDIT_ACCEPTED or EDIT_CANCELLED once, then EDIT_IDLE.
    SuperEditState Service() override {
        if (State != EDIT_ACTIVE) return EDIT_IDLE;
        if (IO.Aborted()) return Finish(EDIT_CANCELLED);
        bool redraw = false;
        int32_t d = IO.Delta();
        if (d && Change(Order[Cur], d)) redraw = true;
        switch (IO.NextKey()) {
        case SuperEditIO::A_SHORT: Cur = (Cur + 1) % (int)Order.size(); redraw = true; break;
        case SuperEditIO::A_LONG:  Vals = Work; return Finish(EDIT_ACCEPTED);
        case SuperEditIO::B_SHORT: return Finish(EDIT_CANCELLED);
        case SuperEditIO::B_LONG:  if (Work != Vals) { Work = Vals; Sync(); redraw = true; IO.Changed(); } break;
        default: break;
        }
        if (redraw) { Invalidate(); Refresh(); }
        return EDIT_ACTIVE;
    }

    // Touch: see the header comment.
    bool OnPointer(const SuperPointerEvent& e) override {
        if (State != EDIT_ACTIVE) return false;
        bool in = HitTest(e.X, e.Y, TouchPad);
        switch (e.Type) {
        case SuperPointerEvent::PRESS: {
            OnField = false;
            if (!in) return false;
            int k = FieldOfCell(CharAt(e.X));
            if (k < 0) return true;                                  // a separator / padding: nothing to grab
            OnField = true;
            Select(k);
            return true; }
        case SuperPointerEvent::TAP: {
            OnField = false;
            if (!in) return SuperPointerDefault(IO, *this, e);       // outside: cancel
            int k = FieldOfCell(CharAt(e.X));
            if (k < 0) return true;
            Select(k);
            if (TouchAcceptOnLift) IO.Inject(SuperEditIO::A_LONG);   // touched and lifted: accept
            return true; }
        case SuperPointerEvent::DRAG_START:
            if (!OnField && HitTest(e.StartX, e.StartY, TouchPad)) {   // no PRESS seen (no touch-down events): grab the field now
                int k = FieldOfCell(CharAt(e.StartX));
                if (k >= 0) { OnField = true; Select(k); }
            }
            break;
        case SuperPointerEvent::DRAG_END: {
            bool grabbed = OnField;
            OnField = false;
            SuperPointerDefault(IO, *this, e);
            if (grabbed && TouchAcceptOnLift) IO.Inject(SuperEditIO::A_LONG);       // lift = accept
            return true; }
        case SuperPointerEvent::LONG_PRESS:
            OnField = false;
            break;
        default: break;
        }
        return SuperPointerDefault(IO, *this, e);
    }

protected:
    SuperFieldBox(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font,
                  SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB)
        : SuperTextBox(dst, x, y, width, font), IO(enc, btnA, btnB) {}

    // ---- what a subclass describes ----
    std::vector<int>  Vals;                 // the stored value of every field (index = field number)
    std::vector<int>  Digits;               // digits shown for each field (zero padded)
    std::vector<int>  Order;                // field numbers in display order
    std::vector<char> Seps;                 // separator after each displayed field (size = Order.size() - 1)
    virtual int  FieldMin(int f) const = 0;
    virtual int  FieldMax(int f, const std::vector<int>& v) const = 0;   // may depend on the other fields
    virtual void Constrain(std::vector<int>& v) { (void)v; }             // pull dependent fields back into range (day <= days in month)

    void Build() { Sync(); Invalidate(); }                                // call after (re)describing the layout or changing Vals
    void Commit() { Sync(); Refresh(); }

    int CellWidth(int i) override {
        char c = GetText()[i];
        if (c < '0' || c > '9') return SuperTextBox::CellWidth(i) + 2;
        int w = 1;
        for (char d = '0'; d <= '9'; ++d) { char s[2] = { d, 0 }; w = std::max(w, (int)Spr.textWidth(s)); }
        return w + 1;                         // fixed width: the text does not shift while a value changes
    }
    void CellStyleAt(int i, CellStyle& st) override {
        int k = FieldOfCell(i);
        if (k < 0) { st.fg = ColLiteral; return; }
        if (State == EDIT_ACTIVE && k == Cur) { st.fill = true; st.bg = ColCursor; st.fg = ColCursorFg; }
    }

    // display position (index into Order) of the field that owns text cell 'cell', or -1 for a separator / outside
    int FieldOfCell(int cell) const {
        int pos = 0;
        for (size_t k = 0; k < Order.size(); ++k) {
            int len = Digits[Order[k]];
            if (cell >= pos && cell < pos + len) return (int)k;
            pos += len + 1;
        }
        return -1;
    }

    std::string Compose(const std::vector<int>& v) const {
        std::string s;
        for (size_t k = 0; k < Order.size(); ++k) {
            char b[16];
            snprintf(b, sizeof b, "%0*d", Digits[Order[k]], v[Order[k]]);
            s += b;
            if (k + 1 < Order.size()) s += Seps[k];
        }
        return s;
    }
    void Sync() { SetText(Compose(State == EDIT_ACTIVE ? Work : Vals), true); }

    SuperEditIO IO;
    bool Wrap = false;
    SuperEditState State = EDIT_IDLE;

private:
    std::vector<int> Work;                  // the value being edited
    int  Cur = 0;                           // display position of the current field
    bool OnField = false;                   // this touch gesture grabbed a field

    void Select(int k) { if (k != Cur) { Cur = k; Invalidate(); Refresh(); } }

    // Move field f by 'd' steps inside its range; true if the value changed.
    bool Change(int f, int32_t d) {
        int lo = FieldMin(f), hi = FieldMax(f, Work);
        if (hi < lo) hi = lo;
        int64_t nv = (int64_t)Work[f] + d;
        if (Wrap) { int64_t range = (int64_t)hi - lo + 1; nv = lo + (((nv - lo) % range) + range) % range; }
        else      nv = std::clamp<int64_t>(nv, lo, hi);
        if (nv == Work[f]) return false;
        Work[f] = (int)nv;
        Constrain(Work);
        Sync();
        IO.Changed();
        return true;
    }

    SuperEditState Finish(SuperEditState r) {
        State = EDIT_IDLE;
        Sync(); Push();
        if (r == EDIT_ACCEPTED) IO.Accepted();
        return r;
    }
};
