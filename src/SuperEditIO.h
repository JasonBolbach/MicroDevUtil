#pragma once
// SuperEditIO - input plumbing shared by the editable Super* controls (SuperInputBox, SuperSpinBox,
// SuperListBox, SuperComboBox). Wraps one SuperEncoder and two SuperButtons.
//
// Every editable control offers two ways to run an edit:
//   blocking:      bool accepted = ctl.BeginEdit();            // returns when the edit ends
//   non-blocking:  ctl.StartEdit();  then call ctl.Service() often (e.g. every loop pass);
//                  Service() returns EDIT_ACTIVE until the edit ends with EDIT_ACCEPTED / EDIT_CANCELLED.
// The stored value only changes when an edit is accepted.
//
// Key decoding: medium clicks and the double-click flag are ignored.
#include "SuperControl.h"      // SuperEditState
#include <SuperButton.h>
#include <SuperEncoder.h>


struct SuperEditIO {
    SuperEncoder* Enc  = nullptr;
    SuperButton*  BtnA = nullptr;
    SuperButton*  BtnB = nullptr;
    bool (*Abort)()          = nullptr;   // polled by Service(); true cancels the edit (e.g. window closed)
    void (*OnChange)(void*)  = nullptr;   // called whenever the value being edited changes (live)
    void*  OnChangeUser      = nullptr;
    void (*OnAccept)(void*)  = nullptr;   // called once when an edit is accepted (the stored value has changed)
    void*  OnAcceptUser      = nullptr;

    enum Key : uint8_t { K_NONE = 0, A_SHORT, A_LONG, B_SHORT, B_LONG };

    // Input that doesn't come from the encoder / buttons (touch). Merged with the real input by Delta() / NextKey().
    int32_t InjDelta = 0;
    Key     InjKey   = K_NONE;
    void Inject(Key k)          { InjKey = k; }
    void InjectDelta(int32_t d) { InjDelta += d; }
    bool    DragOk = false;                             // a drag that started on the control (SuperPointerDefault)
    int16_t DragAnchor = 0;

    SuperEditIO() = default;
    SuperEditIO(SuperEncoder* e, SuperButton* a, SuperButton* b) : Enc(e), BtnA(a), BtnB(b) {}

    void Flush() {                                      // drop stale input
        uint8_t ev;
        InjDelta = 0; InjKey = K_NONE;
        if (Enc) Enc->GetDelta();
        while (BtnA && BtnA->GetEvent(ev)) {}
        while (BtnB && BtnB->GetEvent(ev)) {}
    }
    int32_t Delta() {                                   // detents, speed-scaled by the encoder (+ touch drag steps)
        int32_t d = (Enc ? Enc->GetDelta() : 0) + InjDelta;
        InjDelta = 0;
        return d;
    }

    Key NextKey() {                                     // at most one key per call
        if (InjKey != K_NONE) { Key k = InjKey; InjKey = K_NONE; return k; }
        uint8_t ev;
        while (BtnA && BtnA->GetEvent(ev)) {
            uint8_t k = ev & 0x0F;
            if (k == 1) return A_SHORT;
            if (k == 3) return A_LONG;
        }
        while (BtnB && BtnB->GetEvent(ev)) {
            uint8_t k = ev & 0x0F;
            if (k == 1) return B_SHORT;
            if (k == 3) return B_LONG;
        }
        return K_NONE;
    }
    bool Aborted() const { return Abort && Abort(); }
    void Changed() { if (OnChange) OnChange(OnChangeUser); }
    void Accepted() { if (OnAccept) OnAccept(OnAcceptUser); }
};

// Blocking driver used by every control's BeginEdit().
template <class T> bool SuperRunEdit(T& c) {
    if (!c.StartEdit()) return false;
    SuperEditState s;
    while ((s = c.Service()) == EDIT_ACTIVE) delay(1);
    return s == EDIT_ACCEPTED;
}

// Default pointer handling for an editable control (see SuperControl::OnPointer).
// stepPx = how far a finger drags for one encoder step (0 = SuperControl::TouchDragStepPx).
inline bool SuperPointerDefault(SuperEditIO& io, SuperControl& c, const SuperPointerEvent& e, int stepPx = 0) {
    if (stepPx <= 0) stepPx = SuperControl::TouchDragStepPx;
    bool in = c.HitTest(e.X, e.Y, SuperControl::TouchPad);
    switch (e.Type) {
    case SuperPointerEvent::TAP:
        io.Inject(in ? SuperEditIO::A_SHORT : SuperEditIO::B_SHORT);        // tap outside cancels
        return true;
    case SuperPointerEvent::LONG_PRESS:
        if (!in) return false;
        io.Inject(SuperEditIO::A_LONG);
        return true;
    case SuperPointerEvent::DRAG_START:                 // (the start event may already carry a moved position)
        io.DragOk = c.HitTest(e.StartX, e.StartY, SuperControl::TouchPad);
        io.DragAnchor = e.StartY;
        if (!io.DragOk) return false;
        // fall through: count the movement so far
    case SuperPointerEvent::DRAG_MOVE: {
        if (!io.DragOk) return false;
        int steps = (io.DragAnchor - e.Y) / (stepPx < 1 ? 1 : stepPx);      // dragging up = positive
        if (steps) { io.InjectDelta(steps); io.DragAnchor -= steps * stepPx; }
        return true; }
    case SuperPointerEvent::DRAG_END:
        io.DragOk = false;
        return true;
    case SuperPointerEvent::PRESS:
        return in;
    }
    return false;
}
