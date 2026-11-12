#pragma once
// SuperPushButton - a captioned push button built on SuperControl.
//  * Caption centered in a bordered box. A click runs OnAccept (and OnChange just before it).
//  * Single click, like the check box: a click / tap / BeginEdit() flashes the face inverted for PressMs
//    (default 50 ms, blocking), fires OnAccept and returns true. Service() then reports EDIT_ACCEPTED once.
#include "SuperControl.h"

class SuperPushButton : public SuperControl {
public:
    int PressMs = 50;                               // how long the inverted face is shown (SuperTheme::PressMs)

    SuperPushButton(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font, const std::string& caption)
        : SuperControl(dst, x, y, width, font), Caption(caption) { Alignment = ALIGN_CENTER; SetBorder(1, ColBorder); }

    void SetCaption(const std::string& s) { Caption = s; Dirty = true; Refresh(); }
    const std::string& GetCaption() const { return Caption; }
    void SetOnAccept(void (*fn)(void*), void* user = nullptr) { AcceptFn = fn; AcceptUser = user; }

    // Instant, like the check box: StartEdit() flashes the face inverted for PressMs (blocking, brief),
    // runs OnAccept, and Service() then reports EDIT_ACCEPTED once.
    bool IsEditing() const override { return false; }
    bool StartEdit() override {
        Pressed = true; Push();
        lgfx::delay(PressMs);
        Pressed = false; Push();
        if (AcceptFn) AcceptFn(AcceptUser);
        Pending = true;
        return true;
    }
    SuperEditState Service() override {
        if (Pending) { Pending = false; return EDIT_ACCEPTED; }
        return EDIT_IDLE;
    }
    void ApplyTheme(const SuperTheme& t) override { PressMs = t.PressMs; SuperControl::ApplyTheme(t); }

protected:
    int BodyHeight() override { return FontH() + 2; }
    void RenderBody(int x, int y, int w, int h) override {
        if (Pressed) Spr.fillRect(x, y, w, h, ColCursor);
        Spr.setFont(Font);
        Spr.setTextColor(Pressed ? ColCursorFg : ColFg);
        Spr.setTextDatum(Alignment == ALIGN_LEFT ? lgfx::textdatum_t::middle_left
                       : Alignment == ALIGN_RIGHT ? lgfx::textdatum_t::middle_right : lgfx::textdatum_t::middle_center);
        int tx = Alignment == ALIGN_LEFT ? x + 2 : Alignment == ALIGN_RIGHT ? x + w - 2 : x + w / 2;
        Spr.setClipRect(x, y, w, h);
        Spr.drawString(Caption.c_str(), tx, y + h / 2);
        Spr.clearClipRect();
    }

private:
    std::string Caption;
    bool Pressed = false, Pending = false;
    void (*AcceptFn)(void*) = nullptr; void* AcceptUser = nullptr;
};
