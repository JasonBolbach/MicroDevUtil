#pragma once
// SuperCheckBox - a check box with a caption, built on SuperControl. Depends only on LovyanGFX.
//
//  * Square glyph (about one font height) with the caption to its right. The control's width is fixed;
//    a caption that doesn't fit is clipped. (SetLabel() still adds a separate label above / below.)
//  * Instant, not modal: BeginEdit() flips the value and returns true. There is no cursor, because
//    nothing is left "being edited". (The edit-state API is kept so a GUI can drive every control the
//    same way: StartEdit() flips it, then Service() reports EDIT_ACCEPTED once.)
//  * Toggle() flips it directly. SetChecked() changes it silently (no callbacks).
//  * OnChange and OnAccept both fire when the value changes through Toggle()/BeginEdit(); a check box
//    has no in-progress state, so they happen together (OnChange first).
#include "SuperControl.h"
#include "SuperEditIO.h"      // SuperEditState

class SuperCheckBox : public SuperControl {
public:
    uint16_t ColCheck = 0xFFFF;                 // the check mark / radio dot

    SuperCheckBox(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font, const std::string& caption)
        : SuperControl(dst, x, y, width, font), Caption(caption) {}

    void SetCaption(const std::string& s) { Caption = s; Dirty = true; }
    const std::string& GetCaption() const { return Caption; }
    void SetOnChange(void (*fn)(void*), void* user = nullptr) { ChangeFn = fn; ChangeUser = user; }
    void SetOnAccept(void (*fn)(void*), void* user = nullptr) { AcceptFn = fn; AcceptUser = user; }

    bool GetChecked() const { return Checked; }
    virtual void SetChecked(bool v) { if (v == Checked) return; Checked = v; Invalidate(); Refresh(); }
    bool Toggle() { Activate(); return true; }

    // ---- "editing" (instant) ----
    bool IsEditing() const { return false; }
    bool StartEdit() { Activate(); Pending = true; return true; }
    SuperEditState Service() { if (Pending) { Pending = false; return EDIT_ACCEPTED; } return EDIT_IDLE; }

protected:
    bool Checked = false;

    virtual void Activate() { SetCheckedNotify(!Checked); }        // what a click does
    void SetCheckedNotify(bool v) {
        if (v == Checked) return;
        Checked = v; Invalidate(); Refresh();
        FireChange(); FireAccept();
    }
    void FireChange() { if (ChangeFn) ChangeFn(ChangeUser); }
    void FireAccept() { if (AcceptFn) AcceptFn(AcceptUser); }

    int GlyphSize() { return std::max(5, FontH() - 2); }

    virtual void DrawGlyph(int x, int y, int s) {                  // square box + check mark
        Spr.drawRect(x, y, s, s, ColFg);
        if (!Checked) return;
        int t = std::max(1, s / 6);
        int ax = x + s / 5, ay = y + s / 2, bx = x + s * 2 / 5, by = y + s - s / 4, cx = x + s - s / 5, cy = y + s / 5;
        for (int o = 0; o < t; ++o) {
            Spr.drawLine(ax, ay + o, bx, by + o, ColCheck);
            Spr.drawLine(bx, by + o, cx, cy + o, ColCheck);
        }
    }

    int BodyHeight() override { return GlyphSize() + 2; }

    void RenderBody(int x, int y, int w, int h) override {
        int s = GlyphSize(), gy = y + (h - s) / 2;
        DrawGlyph(x, gy, s);
        int tx = x + s + 4;
        if (tx >= x + w) return;
        Spr.setFont(Font);
        Spr.setTextColor(ColFg);
        Spr.setTextDatum(lgfx::textdatum_t::middle_left);
        Spr.setClipRect(tx, y, x + w - tx, h);
        Spr.drawString(Caption.c_str(), tx, y + h / 2);
        Spr.clearClipRect();
    }

private:
    std::string Caption;
    void (*ChangeFn)(void*) = nullptr;  void* ChangeUser = nullptr;
    void (*AcceptFn)(void*) = nullptr;  void* AcceptUser = nullptr;
    bool Pending = false;
};
