#pragma once
// SuperInputBox - template-driven input built on SuperTextBox, edited with a SuperEncoder
// and two SuperButtons (see SuperEditIO.h for blocking / non-blocking edits).
//
// Template characters (everything else is a literal: drawn, but skipped when reading/writing the value):
//   #  digit 0-9                     (empty = '0')
//   A  space + upper-case A-Z        (empty = ' ')
//   a  space + lower-case a-z        (empty = ' ')
//   B  space + A-Z + a-z             (empty = ' ')
//   *  any typeable US keyboard char, space..~   (empty = ' ')
//
// "###.###.###" displays 123.456.789; GetValue() / SetValue() use "123456789".
// Short values passed to SetValue() are padded with empty characters: on the left when every slot is
// a digit (so "42" in ### gives 042), otherwise on the right.
//
// Editing (BeginEdit() blocks and returns true if accepted; StartEdit()/Service() is the non-blocking form):
//   Selecting a position:  encoder moves between editable slots (stops at the ends, text scrolls)
//                          ButtonA short = edit the selected slot
//   Editing a position:    encoder scrolls the characters (wraps around)
//                          ButtonA short = finish the slot, back to selecting
//   ButtonA long   = accept the whole edit (unless the validator rejects it)
//   ButtonB short  = cancel the edit
//   ButtonB long   = revert to the starting value and keep editing at the cursor
//   Touch:  touch a character = highlight it, drag up/down = change it, lift = accept (see OnPointer below)
#include "SuperTextBox.h"
#include "SuperEditIO.h"

class SuperInputBox : public SuperTextBox {
public:
    uint16_t ColLiteral  = 0xBDF7;      // template literals
    bool     TrimRight   = true;        // GetValue() drops trailing spaces (the box itself still shows them)

    SuperInputBox(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font,
                  const char* tmpl, SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB)
        : SuperTextBox(dst, x, y, width, font), IO(enc, btnA, btnB)
    {
        for (const char* p = tmpl; *p; ++p) {
            if (IsType(*p)) { SlotCell.push_back((int)Tmpl.size()); Committed.push_back(EmptyChar(*p)); }
            Tmpl.push_back(*p);
        }
        Sync();
    }

    // Polled while editing; if it returns true the edit is cancelled (e.g. window closed).
    void SetAbort(bool (*fn)()) { IO.Abort = fn; }
    // Called whenever the value being edited changes (live, before it is accepted).
    void SetOnChange(void (*fn)(void*), void* user = nullptr) { IO.OnChange = fn; IO.OnChangeUser = user; }
    // Called once when an edit is accepted (GetValue() already returns the new value).
    void SetOnAccept(void (*fn)(void*), void* user = nullptr) { IO.OnAccept = fn; IO.OnAcceptUser = user; }
    // Called when the user tries to accept; return false to refuse (the edit continues).
    // 'value' is the working value (literals removed, trimmed if TrimRight).
    void SetValidator(bool (*fn)(const std::string& value, void* user), void* user = nullptr) { Validator = fn; ValidatorUser = user; }

    int  SlotCount() const { return (int)SlotCell.size(); }

    // Value without literals. Returns false (and changes nothing) if it is too long or has a
    // character the slot can't hold.
    bool SetValue(const std::string& v) {
        if (v.size() > SlotCell.size()) return false;
        bool numeric = true;
        for (int c : SlotCell) if (Tmpl[c] != '#') numeric = false;
        size_t pad = numeric ? SlotCell.size() - v.size() : 0;     // empty slots in front
        std::string nv;
        for (size_t i = 0; i < SlotCell.size(); ++i) {
            char t = Tmpl[SlotCell[i]];
            if (i >= pad && i - pad < v.size()) { char c = v[i - pad]; if (IndexOf(t, c) < 0) return false; nv.push_back(c); }
            else nv.push_back(EmptyChar(t));
        }
        Committed = nv;
        Sync();
        Refresh();
        return true;
    }
    std::string GetValue() const { return ValueOf(Committed); }
    std::string GetDisplay() const { return Build(Committed); }
    std::string GetLiveValue() const { return Editing ? ValueOf(Work) : GetValue(); }      // the value being edited (use inside OnChange)

    // ---- editing ----
    SuperEditIO* EditIO() { return &IO; }                          // the input plumbing: InjectDelta / Inject for keyboards, mice and tests
    bool IsEditing() const override { return State == EDIT_ACTIVE; }
    // Touch (input / time / date boxes): touch a character = it is highlighted at once (even on an idle box) and the box
    // waits for a drag; dragging up / down changes the character; lifting the finger accepts (OnAccept).
    // Tap a "<" / ">" arrow = scroll, a literal = nothing, tap outside = cancel, long press = accept.
    // Set TouchAcceptOnLift = false for the older style: lifting only selects, tap the highlighted slot again to finish it,
    // long press to accept.
    bool TouchAcceptOnLift = true;
    bool StartOnPress(int, int) override { return true; }
    bool OnPointer(const SuperPointerEvent& e) override {
        if (State != EDIT_ACTIVE) return false;
        bool in = HitTest(e.X, e.Y, TouchPad);
        switch (e.Type) {
        case SuperPointerEvent::PRESS: {
            PressSeen = OnSlot = false;
            if (!in) return false;
            int slot = SlotOfCell(CharAt(e.X));
            if (slot < 0) return true;                              // literal / arrow / padding: nothing to grab
            PressSeen = OnSlot = true;
            PressWasOn = (slot == Cursor && EditingChar);
            if (slot != Cursor || !EditingChar) { Cursor = slot; EditingChar = true; FocusCursor(); Invalidate(); Refresh(); }
            return true; }
        case SuperPointerEvent::TAP: {
            bool seen = PressSeen, grabbed = OnSlot;
            PressSeen = OnSlot = false;
            if (!in) return SuperPointerDefault(IO, *this, e);      // outside: cancel
            int c = CharAt(e.X);
            if (c == CHAR_ARROW_LEFT)  { ScrollBy(-3); Refresh(); return true; }
            if (c == CHAR_ARROW_RIGHT) { ScrollBy(3);  Refresh(); return true; }
            int slot = SlotOfCell(c);
            if (slot < 0) return true;
            if (seen && grabbed && TouchAcceptOnLift) { IO.Inject(SuperEditIO::A_LONG); return true; }   // touched, lifted: accept
            if (seen) { if (PressWasOn && slot == Cursor) EditingChar = false; }     // older style: selection was done on touch
            else if (slot == Cursor) EditingChar = !EditingChar;                     // no touch-down events available
            else { Cursor = slot; EditingChar = true; }
            FocusCursor(); Invalidate(); Refresh();
            return true; }
        case SuperPointerEvent::DRAG_START:
            if (!PressSeen && HitTest(e.StartX, e.StartY, TouchPad)) {   // no PRESS seen (no ST_DOWN source): grab the slot now
                int slot = SlotOfCell(CharAt(e.StartX));
                if (slot >= 0) {
                    OnSlot = true;
                    if (slot != Cursor || !EditingChar) { Cursor = slot; EditingChar = true; FocusCursor(); Invalidate(); Refresh(); }
                }
            }
            PressSeen = false;
            break;
        case SuperPointerEvent::DRAG_END: {
            bool grabbed = OnSlot;
            PressSeen = OnSlot = false;
            SuperPointerDefault(IO, *this, e);
            if (grabbed && TouchAcceptOnLift) IO.Inject(SuperEditIO::A_LONG);          // lift = accept
            return true; }
        case SuperPointerEvent::LONG_PRESS:
            PressSeen = OnSlot = false;
            break;
        default: break;
        }
        return SuperPointerDefault(IO, *this, e);
    }

    bool StartEdit() {                                             // non-blocking: then call Service()
        if (SlotCell.empty()) return false;
        Work = Committed; Cursor = 0; EditingChar = false; Editing = true;
        IO.Flush();
        ScrollTo(0);
        Sync(); FocusCursor(); PushUnlessPress();
        State = EDIT_ACTIVE;
        return true;
    }

    // Returns EDIT_ACTIVE while editing, then EDIT_ACCEPTED or EDIT_CANCELLED once, then EDIT_IDLE.
    SuperEditState Service() {
        if (State != EDIT_ACTIVE) return EDIT_IDLE;
        if (IO.Aborted()) return Finish(EDIT_CANCELLED);
        bool redraw = false;

        int32_t d = IO.Delta();
        if (d) {
            if (!EditingChar) {
                int nc = std::clamp(Cursor + (d > 0 ? 1 : -1), 0, SlotCount() - 1);
                if (nc != Cursor) { Cursor = nc; FocusCursor(); redraw = true; }
            } else {
                char t = Tmpl[SlotCell[Cursor]];
                int sz = SetSize(t);
                int step = (sz <= 10) ? (d > 0 ? 1 : -1) : (int)d;   // fast spin skips on big sets
                int idx = IndexOf(t, Work[Cursor]);
                if (idx < 0) idx = 0;
                idx = ((idx + step) % sz + sz) % sz;
                Work[Cursor] = SetAt(t, idx);
                Sync(); redraw = true;
                IO.Changed();
            }
        }
        switch (IO.NextKey()) {
        case SuperEditIO::A_SHORT: EditingChar = !EditingChar; redraw = true; break;
        case SuperEditIO::A_LONG:
            if (!Validator || Validator(ValueOf(Work), ValidatorUser)) { Committed = Work; return Finish(EDIT_ACCEPTED); }
            break;                                                  // refused: keep editing
        case SuperEditIO::B_SHORT: return Finish(EDIT_CANCELLED);
        case SuperEditIO::B_LONG:  Work = Committed; Sync(); redraw = true; IO.Changed(); break;
        default: break;
        }
        if (redraw) { Invalidate(); Refresh(); }
        return EDIT_ACTIVE;
    }

protected:
    int CellWidth(int i) override {
        if (!IsType(Tmpl[i])) return SuperTextBox::CellWidth(i) + 2;
        int sz = SetSize(Tmpl[i]), w = 1;
        for (int k = 0; k < sz; ++k) { char s[2] = { SetAt(Tmpl[i], k), 0 }; w = std::max(w, (int)Spr.textWidth(s)); }
        return w + 2;                      // fixed per slot so the text doesn't shift while scrolling characters
    }

    void CellStyleAt(int i, CellStyle& st) override {
        if (!IsType(Tmpl[i])) { st.fg = ColLiteral; return; }
        if (Editing && SlotCell[Cursor] == i) {
            if (EditingChar) { st.fill = true; st.bg = ColCursor; st.fg = ColCursorFg; }
            else             { st.outline = true; st.outlineColor = ColCursor; }
        }
    }

private:
    SuperEditIO IO;
    bool (*Validator)(const std::string&, void*) = nullptr;
    void* ValidatorUser = nullptr;

    std::string      Tmpl;                 // template, one entry per displayed character
    std::vector<int> SlotCell;             // slot -> index in Tmpl / displayed text
    std::string      Committed, Work;      // one char per slot
    int  Cursor = 0;
    bool Editing = false, EditingChar = false;
    bool PressSeen = false, PressWasOn = false, OnSlot = false;     // touch: this gesture began with a PRESS that already selected a slot
    SuperEditState State = EDIT_IDLE;

    int SlotOfCell(int cell) const {                 // displayed character index -> slot, -1 if a literal
        for (size_t i = 0; i < SlotCell.size(); ++i) if (SlotCell[i] == cell) return (int)i;
        return -1;
    }

    static bool IsType(char c) { return c == '#' || c == 'A' || c == 'a' || c == 'B' || c == '*'; }
    static char EmptyChar(char t) { return t == '#' ? '0' : ' '; }
    static int  SetSize(char t) {
        switch (t) { case '#': return 10; case 'A': case 'a': return 27; case 'B': return 53; default: return 95; }
    }
    static char SetAt(char t, int i) {
        switch (t) {
        case '#': return '0' + i;
        case 'A': return i == 0 ? ' ' : 'A' + i - 1;
        case 'a': return i == 0 ? ' ' : 'a' + i - 1;
        case 'B': return i == 0 ? ' ' : (i <= 26 ? 'A' + i - 1 : 'a' + i - 27);
        default:  return ' ' + i;
        }
    }
    static int IndexOf(char t, char c) {
        for (int i = 0, n = SetSize(t); i < n; ++i) if (SetAt(t, i) == c) return i;
        return -1;
    }

    std::string ValueOf(const std::string& vals) const {
        if (!TrimRight) return vals;
        size_t n = vals.find_last_not_of(' ');
        return n == std::string::npos ? std::string() : vals.substr(0, n + 1);
    }
    std::string Build(const std::string& vals) const {
        std::string s = Tmpl;
        for (size_t i = 0; i < SlotCell.size(); ++i) s[SlotCell[i]] = vals[i];
        return s;
    }
    void Sync() { SetText(Build(Editing ? Work : Committed), true); }

    SuperEditState Finish(SuperEditState result) {
        Editing = false;
        ScrollTo(0);
        Sync(); Push();
        State = EDIT_IDLE;
        if (result == EDIT_ACCEPTED) IO.Accepted();
        return result;
    }

    // Keep the cursor slot plus its neighbouring slots (and what lies between) in view.
    void FocusCursor() {
        int lo = Cursor > 0 ? SlotCell[Cursor - 1] : 0;
        int hi = Cursor + 1 < SlotCount() ? SlotCell[Cursor + 1] : (int)Tmpl.size() - 1;
        EnsureVisible(lo, hi, SlotCell[Cursor]);
    }
};
