#pragma once
// SuperForm - focus handling for a set of Super* controls, so a sketch doesn't have to.
//
//     SuperForm form(&Enc, &BtnA);
//     form.Add(&phone); form.Add(&level); form.Add(&bar, false);      // false = shown, never focused
//     form.Draw();
//     void loop() { form.Service(); ... }                              // non-blocking, call every pass
//
//  * Encoder (when no control is being edited) moves a highlight box between the controls
//    (DrawPreview: the focused one at full brightness with a box, the others dimmed). Focus wraps around and
//    skips controls that are disabled, hidden or added as non-focusable.
//  * ButtonA short click starts editing the focused control. While a control is being edited it owns the
//    encoder and buttons (see each control's header); the form just calls its Service().
//  * SetOnEdit(fn) is called when an edit ends: fn(control, EDIT_ACCEPTED or EDIT_CANCELLED, user).
//  * Service() returns EDIT_ACTIVE while a control is being edited, EDIT_ACCEPTED / EDIT_CANCELLED once when
//    an edit ends, otherwise EDIT_IDLE.
//  * Touch (optional): form.SetTouch(&touch) with a SuperTouch. A tap on a control focuses it and starts its edit
//    (one tap). While editing, touch goes to that control (tap = ButtonA, long press = ButtonA long, tap outside
//    = cancel, drag = encoder; lists / tabs / menus / sliders / message boxes add precise picking). Touch and the
//    encoder / buttons can be used together, even in the same edit. Blocking BeginEdit() calls (a message box
//    you open yourself) keep receiving touch too. Hit areas grow by SuperControl::TouchPad (default 4) pixels.
//    Set touch.ClickD_MAX = 0 so taps arrive without the double-click delay.
//  * Modal controls (SuperMessageBox) can be run with BeginEdit() from your own code; call form.Draw() afterwards.
#include "SuperControl.h"
#include "SuperEditIO.h"
#include <SuperTouch.h>

class SuperForm {
public:
    SuperForm(SuperEncoder* enc, SuperButton* btnA) : IO(enc, btnA, nullptr) {}

    void SetTouch(SuperTouch* t) {
        Touch = t; Instance = this;
        if (t) t->EmitDown = true;                                // controls get PRESS (touch-down) events
        SuperControl::PressEventsOn = (t != nullptr);
        SuperControl::BlockingHook = t ? &Hook : nullptr;
    }
    void SetOnEdit(void (*fn)(SuperControl*, SuperEditState, void*), void* user = nullptr) { OnEdit = fn; OnEditUser = user; }

    void Add(SuperControl* c, bool focusable = true) {
        if (!c) return;
        Items.push_back({ c, focusable });
        if (Focus < 0 && Can((int)Items.size() - 1)) Focus = (int)Items.size() - 1;
    }
    int  Count() const { return (int)Items.size(); }
    int  GetFocus() const { return Focus; }
    SuperControl* GetFocused() const { return Focus >= 0 ? Items[Focus].C : nullptr; }
    SuperControl* GetActive() const { return Active; }
    bool IsEditing() const { return Active != nullptr; }

    void SetFocus(int i) {
        if (i < 0 || i >= Count() || !Can(i) || i == Focus) return;
        int old = Focus; Focus = i;
        if (old >= 0) Items[old].C->DrawPreview(false);
        Items[Focus].C->DrawPreview(true);
    }

    // Redraw every visible control (focused one highlighted).
    void Draw() {
        if (SuperControl::SharedBuffer) {                             // size the shared render buffer for the biggest visible control
            size_t m = 0;
            for (auto& it : Items) if (it.C->IsVisible()) m = std::max(m, (size_t)it.C->GetWidth() * it.C->GetHeight() * 2);
            SuperControl::ReserveShared(m);
        }
        for (int i = 0; i < Count(); ++i) {
            SuperControl* c = Items[i].C;
            if (!c->IsVisible()) continue;
            if (Items[i].Focusable) c->DrawPreview(i == Focus);
            else                    c->Push();
        }
    }

    SuperEditState Service() {
        if (Touch) PumpTouch();
        if (Active) {
            SuperEditState r = Active->Service();
            if (r == EDIT_ACTIVE) return EDIT_ACTIVE;
            SuperControl* c = Active;
            Active = nullptr;
            if (Focus >= 0 && Items[Focus].C == c) c->DrawPreview(true);
            if (OnEdit) OnEdit(c, r, OnEditUser);
            return r;
        }
        int32_t d = IO.Delta();
        if (d) Move(d > 0 ? 1 : -1);
        if (IO.NextKey() == SuperEditIO::A_SHORT && Focus >= 0 && Can(Focus)) {
            if (Items[Focus].C->StartEdit()) { Active = Items[Focus].C; return EDIT_ACTIVE; }
        }
        if (Active) return EDIT_ACTIVE;                           // a tap started an edit
        return EDIT_IDLE;
    }

private:
    SuperTouch* Touch = nullptr;
    inline static SuperForm* Instance = nullptr;
    static void Hook() { if (Instance) Instance->PumpTouch(); }

    // Read the touch panel and route the events: to the control being edited, or (idle) a tap picks a control.
    void PumpTouch() {
        Touch->Poll();
        SuperTouchEvent ev;
        while (Touch->GetEvent(ev)) {
            SuperPointerEvent pe;
            pe.X = ev.X; pe.Y = ev.Y; pe.StartX = ev.StartX; pe.StartY = ev.StartY;
            switch (ev.Type) {
            case ST_SHORT_CLICK:
            case ST_MEDIUM_CLICK: pe.Type = SuperPointerEvent::TAP; pe.StartX = ev.X; pe.StartY = ev.Y; break;
            case ST_LONG_CLICK:   pe.Type = SuperPointerEvent::LONG_PRESS; break;
            case ST_DOWN:         pe.Type = SuperPointerEvent::PRESS; pe.StartX = ev.X; pe.StartY = ev.Y; break;
            case ST_DRAG:
                pe.Type = ev.Phase == ST_DRAG_START ? SuperPointerEvent::DRAG_START
                        : ev.Phase == ST_DRAG_MOVE  ? SuperPointerEvent::DRAG_MOVE : SuperPointerEvent::DRAG_END;
                break;
            default: continue;
            }
            SuperControl* target = SuperControl::EditingNow ? SuperControl::EditingNow : Active;
            if (target) target->OnPointer(pe);
            else if (pe.Type == SuperPointerEvent::TAP || pe.Type == SuperPointerEvent::LONG_PRESS) TapIdle(pe.X, pe.Y);   // a long press clicks too
            else if (pe.Type == SuperPointerEvent::PRESS) PressIdle(pe);
        }
    }
    void TapIdle(int x, int y) {
        for (int pass = 0; pass < 2; ++pass)                      // exact hit first, then with touch padding
            for (int i = Count() - 1; i >= 0; --i)
                if (Can(i) && Items[i].C->HitTest(x, y, pass ? SuperControl::TouchPad : 0)) {
                    SetFocus(i);
                    if (Items[i].C->StartEdit()) {
                        Active = Items[i].C;
                        if (Active->IdleTapActs()) {                      // one tap does the whole job (tab bar, menu)
                            SuperPointerEvent t; t.Type = SuperPointerEvent::TAP; t.X = t.StartX = x; t.Y = t.StartY = y;
                            Active->OnPointer(t);
                        }
                    }
                    return;
                }
    }
    // Finger just landed on an idle control that wants to react at once (input boxes): focus it, start the edit, tell it.
    void PressIdle(const SuperPointerEvent& pe) {
        for (int pass = 0; pass < 2; ++pass)
            for (int i = Count() - 1; i >= 0; --i)
                if (Can(i) && Items[i].C->HitTest(pe.X, pe.Y, pass ? SuperControl::TouchPad : 0) && Items[i].C->StartOnPress(pe.X, pe.Y)) {
                    SetFocus(i);
                    if (Items[i].C->StartEdit()) {
                        Active = Items[i].C;
                        SuperControl::PressStartedEdit = true;
                        Active->OnPointer(pe);
                        SuperControl::PressStartedEdit = false;
                    }
                    return;
                }
    }

    struct Entry { SuperControl* C; bool Focusable; };
    SuperEditIO IO;
    std::vector<Entry> Items;
    int Focus = -1;
    SuperControl* Active = nullptr;
    void (*OnEdit)(SuperControl*, SuperEditState, void*) = nullptr;
    void* OnEditUser = nullptr;

    bool Can(int i) const { return Items[i].Focusable && Items[i].C->IsEnabled() && Items[i].C->IsVisible(); }
    void Move(int dir) {
        int n = Count();
        for (int k = 1; k <= n; ++k) {
            int i = ((Focus + dir * k) % n + n) % n;
            if (Can(i)) { SetFocus(i); return; }
        }
    }
};
