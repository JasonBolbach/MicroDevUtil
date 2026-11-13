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
//  * Drawing order: SuperControl::SetZOrder(n). The lowest number is in front (drawn last); equal numbers keep the order they were added.
//    Touch hits the front-most control first. Focus order: SuperControl::SetTabOrder(n). The encoder visits the lowest number first;
//    equal numbers keep the order they were added. Until you call SetFocus() (or touch a control) the first control in focus order has the focus.
//  * Modal controls (SuperMessageBox) can be run with BeginEdit() from your own code; call form.Draw() afterwards.
#include "SuperControl.h"
#include "SuperEditIO.h"
#include <SuperTouch.h>
#include <algorithm>
#include <vector>

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
        FocusAuto = false;
        if (i < 0 || i >= Count() || !Can(i) || i == Focus) return;
        int old = Focus; Focus = i;
        if (old >= 0) Items[old].C->DrawPreview(false);
        Items[Focus].C->DrawPreview(true);
    }

    // Redraw every visible control (focused one highlighted).
    void Draw() { DrawItems(false, 0, 0, 0, 0); }

    // ---- control groups (SuperControl::SetGroup; group 0 = no group, never touched) ----
    //  * SetGroupVisible(g, v)  show or hide every control of group g. A hidden control is erased and needs no memory.
    //  * SetGroupEnabled(g, e)  enable or disable (dim) every control of group g.
    //  * ShowPage(pages, n, show)  tab pages: shows the controls of group `show` and hides the controls of the other groups listed in pages[0..n)
    //    in one pass (hidden first, then shown, so overlapping pages do not flicker). show = 0 hides all of them. Groups that are not listed stay as they are.
    //  Afterwards the focus moves to a control that can take it. draw = false only changes the state (use it before the first Draw()).
    //  Group operations override a control's own Visible / Enabled setting: showing a group shows all of its controls.
    void SetGroupVisible(uint16_t g, bool v, bool draw = true) {
        if (!g) return;
        for (auto& it : Items) if (it.C->GetGroup() == g) it.C->SetVisible(v, draw);
        AfterGroupChange(draw);
    }
    void SetGroupEnabled(uint16_t g, bool e) {
        if (!g) return;
        for (auto& it : Items) if (it.C->GetGroup() == g) it.C->SetEnabled(e);
        AfterGroupChange(true);
    }
    void ShowPage(const uint16_t* pages, int n, uint16_t show, bool draw = true) {
        for (int k = 0; k < n; ++k) if (pages[k] && pages[k] != show)
            for (auto& it : Items) if (it.C->GetGroup() == pages[k]) it.C->SetVisible(false, draw);
        if (show) for (auto& it : Items) if (it.C->GetGroup() == show) it.C->SetVisible(true, draw);
        AfterGroupChange(draw);
    }
    int GroupCount(uint16_t g) const { int n = 0; if (g) for (auto& it : Items) if (it.C->GetGroup() == g) ++n; return n; }

    // A popup (message box, combo list) went away and left its pixels behind: clear that rectangle to 'bg' (your screen colour) and
    // redraw every visible control that touches it, in its normal / focused look. Use it from the popup's SetRestore() callback.
    void Redraw(int x, int y, int w, int h, uint16_t bg) {
        if (Items.empty()) return;
        if (lgfx::LovyanGFX* d = Items[0].C->GetDestination()) d->fillRect(x, y, w, h, bg);
        DrawItems(true, x, y, w, h);
    }

    SuperEditState Service() {
        if (Touch) PumpTouch();
        TickControls();
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
            for (int k = Count() - 1; k >= 0; --k) {
                int i = Order(true)[k];                                // front-most first
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
    }
    // Finger just landed on an idle control that wants to react at once (input boxes): focus it, start the edit, tell it.
    void PressIdle(const SuperPointerEvent& pe) {
        for (int pass = 0; pass < 2; ++pass)
            for (int k = Count() - 1; k >= 0; --k) {
                int i = Order(true)[k];
                if (Can(i) && Items[i].C->HitTest(pe.X, pe.Y, pass ? SuperControl::TouchPad : 0) && Items[i].C->StartOnPress(pe.X, pe.Y)) {
                    SetFocus(i);
                    SuperControl::PressStartedEdit = true;                        // StartEdit() need not draw: the PRESS redraws at once
                    if (Items[i].C->StartEdit()) {
                        Active = Items[i].C;
                        Active->OnPointer(pe);
                        Active->Refresh();                                       // draw now if the PRESS did not
                    }
                    SuperControl::PressStartedEdit = false;
                    return;
                }
            }
    }

    struct Entry { SuperControl* C; bool Focusable; };
    SuperEditIO IO;
    std::vector<Entry> Items;
    int Focus = -1;
    bool FocusAuto = true;                                            // no explicit SetFocus yet: the first control in focus order is focused
    std::vector<int> Ord;
    // Item indices sorted by ZOrder (z = true: draw order, highest number first / front last) or by TabOrder (z = false). Stable, so equal
    // numbers keep list order.
    const std::vector<int>& Order(bool z) {
        Ord.resize(Items.size());
        for (size_t i = 0; i < Ord.size(); ++i) Ord[i] = (int)i;
        if (z) std::stable_sort(Ord.begin(), Ord.end(), [&](int a, int b) { return Items[a].C->GetZOrder() > Items[b].C->GetZOrder(); });
        else   std::stable_sort(Ord.begin(), Ord.end(), [&](int a, int b) { return Items[a].C->GetTabOrder() < Items[b].C->GetTabOrder(); });
        return Ord;
    }
    void ResolveAutoFocus() {
        if (!FocusAuto) return;
        int want = -1;
        for (int i : Order(false)) if (Can(i)) { want = i; break; }
        if (want == Focus) return;
        Focus = want;
    }
    SuperControl* Active = nullptr;
    void (*OnEdit)(SuperControl*, SuperEditState, void*) = nullptr;
    void* OnEditUser = nullptr;

    // Controls that animate (SuperUserDraw with auto-redraw) say when they are due; they are redrawn in their normal / focused look.
    void TickControls() {
        for (int i = 0; i < Count(); ++i) {
            SuperControl* c = Items[i].C;
            if (c == Active || !c->IsVisible() || SuperControl::EditingNow) continue;
            if (c->Tick()) { if (Items[i].Focusable) c->DrawPreview(i == Focus); else c->Push(); }
        }
    }
    void DrawItems(bool clip, int rx, int ry, int rw, int rh) {
        if (SuperControl::SharedBuffer) {                             // size the shared render buffer for the biggest control once
            size_t m = 0;
            for (auto& it : Items) m = std::max(m, (size_t)it.C->GetWidth() * it.C->GetHeight() * 2);   // all of them, visible or not: no growing later
            SuperControl::ReserveShared(m);
        }
        // With DMA, LovyanGFX keeps a transfer running in the background only inside an open transaction (startWrite / endWrite);
        // otherwise every push waits for its transfer to finish before returning. Hold one open for the whole batch.
        lgfx::LovyanGFX* batch = nullptr;
        if (SuperControl::DmaActive)
            for (auto& it : Items) if (it.C->IsVisible()) { batch = it.C->GetDestination(); break; }
        if (batch) batch->startWrite();
        SuperControl::DeferBoxes = (batch != nullptr);              // focus boxes after the last transfer is queued
        ResolveAutoFocus();
        std::vector<int> drawOrder = Order(true);                    // back to front (lowest ZOrder last)
        for (int i : drawOrder) {
            SuperControl* c = Items[i].C;
            if (!c->IsVisible()) continue;
            if (clip) {                                               // only controls (with their focus box) that touch the rectangle
                int m = c->HighlightGap + c->HighlightSize + 1;
                if (c->GetX() - m >= rx + rw || c->GetX() + c->GetWidth() + m <= rx ||
                    c->GetY() - m >= ry + rh || c->GetY() + c->GetHeight() + m <= ry) continue;
            }
            if (Items[i].Focusable) c->DrawPreview(i == Focus);
            else                    c->Push();
        }
        SuperControl::FlushBoxes();                                   // waits for the display, then draws the focus boxes
        if (batch) batch->endWrite();
    }
    void AfterGroupChange(bool draw) {                           // the focused control may have gone: focus the first one that can take it
        if (Focus >= 0 && Focus < Count() && !Can(Focus)) { FocusAuto = true; Focus = -1; }
        ResolveAutoFocus();
        if (draw && Focus >= 0 && Focus < Count() && Items[Focus].C->IsVisible()) Items[Focus].C->DrawPreview(true);
    }
    bool Can(int i) const { return Items[i].Focusable && Items[i].C->IsEnabled() && Items[i].C->IsVisible(); }
    void Move(int dir) {
        ResolveAutoFocus();
        std::vector<int> tab = Order(false);                          // focus order
        int n = (int)tab.size(), pos = 0;
        for (int k = 0; k < n; ++k) if (tab[k] == Focus) { pos = k; break; }
        for (int k = 1; k <= n; ++k) {
            int i = tab[((pos + dir * k) % n + n) % n];
            if (Can(i)) { SetFocus(i); return; }
        }
    }
};
