#pragma once
// SuperRadioButton / SuperRadioGroup - round "pick one" buttons.
//
// A SuperRadioButton is a SuperCheckBox with a round glyph. Put it on screen wherever you like. To make
// several of them exclusive, add them to a SuperRadioGroup:
//
//     SuperRadioGroup group;
//     group.Add(&red); group.Add(&green); group.Add(&blue);
//     group.SetSelected(1);                       // silent
//     group.SetOnAccept([](int index, void*) { ... });
//
// Selecting a button (a click through BeginEdit(), or Select()) clears the others, then fires the
// button's OnChange / OnAccept and the group's OnChange / OnAccept (index of the new selection).
// Like the check box it is instant: BeginEdit() selects it and returns true; a button that is already
// selected stays selected. A button without a group simply turns on.
// The group is not a screen control: each button is its own stop for a GUI that moves focus.
#include "SuperCheckBox.h"

class SuperRadioGroup;

class SuperRadioButton : public SuperCheckBox {
public:
    SuperRadioButton(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font, const std::string& caption)
        : SuperCheckBox(dst, x, y, width, font, caption) {}

    void Select() { Activate(); }
    void SetChecked(bool v) override;                       // silent; checking one unchecks the rest of its group
    SuperRadioGroup* GetGroup() const { return Group; }

protected:
    void Activate() override;
    void DrawGlyph(int x, int y, int s) override {          // circle, with a dot when selected
        int r = s / 2, cx = x + r, cy = y + r;
        Spr.drawCircle(cx, cy, r, ColFg);
        if (Checked) Spr.fillCircle(cx, cy, std::max(1, r - 3), ColCheck);
    }

private:
    friend class SuperRadioGroup;
    SuperRadioGroup* Group = nullptr;
    void Clear() { if (Checked) { Checked = false; Invalidate(); Refresh(); } }
    void Set()   { if (!Checked) { Checked = true; Invalidate(); Refresh(); } }
    void Notify() { FireChange(); FireAccept(); }
};

class SuperRadioGroup {
public:
    // Group callbacks get the index (order of Add) of the newly selected button.
    void SetOnChange(void (*fn)(int, void*), void* user = nullptr) { ChangeFn = fn; ChangeUser = user; }
    void SetOnAccept(void (*fn)(int, void*), void* user = nullptr) { AcceptFn = fn; AcceptUser = user; }

    void Add(SuperRadioButton* b) {
        if (!b || b->Group == this) return;
        b->Group = this;
        Buttons.push_back(b);
        if (b->GetChecked()) Choose(b, false);               // first checked button wins
    }
    int Count() const { return (int)Buttons.size(); }
    SuperRadioButton* Get(int i) const { return (i >= 0 && i < Count()) ? Buttons[i] : nullptr; }
    int GetSelected() const {                                // -1 when none
        for (int i = 0; i < Count(); ++i) if (Buttons[i]->GetChecked()) return i;
        return -1;
    }
    void SetSelected(int i) {                                // silent; -1 clears
        SuperRadioButton* b = Get(i);
        if (b) Choose(b, false);
        else for (auto* x : Buttons) x->Clear();
    }
    void Select(int i) { if (SuperRadioButton* b = Get(i)) b->Select(); }   // with callbacks

private:
    friend class SuperRadioButton;
    std::vector<SuperRadioButton*> Buttons;
    void (*ChangeFn)(int, void*) = nullptr;  void* ChangeUser = nullptr;
    void (*AcceptFn)(int, void*) = nullptr;  void* AcceptUser = nullptr;

    void Choose(SuperRadioButton* sel, bool notify) {
        for (auto* b : Buttons) if (b != sel) b->Clear();
        sel->Set();
        if (!notify) return;
        sel->Notify();
        int idx = GetSelected();
        if (ChangeFn) ChangeFn(idx, ChangeUser);
        if (AcceptFn) AcceptFn(idx, AcceptUser);
    }
};

inline void SuperRadioButton::Activate() {
    if (Checked) return;
    if (Group) Group->Choose(this, true);
    else { Set(); Notify(); }
}
inline void SuperRadioButton::SetChecked(bool v) {
    if (v) { if (Group) Group->Choose(this, false); else Set(); }
    else Clear();
}
