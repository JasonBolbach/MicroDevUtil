#pragma once
// SuperProgressBar - display-only progress bar built on SuperControl.
//  * Value 0..Max (default 100). The fill grows left to right in ColBar.
//  * ShowPercent draws "42%" centered; the text changes colour where the fill passes under it.
//  * Not editable: it never takes focus input (StartEdit() returns false).
#include "SuperControl.h"

class SuperProgressBar : public SuperControl {
public:
    uint16_t ColBar   = 0x07E0;         // fill
    uint16_t ColBarFg = 0x0000;         // percent text where it sits on the fill
    bool     ShowPercent = true;

    SuperProgressBar(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font)
        : SuperControl(dst, x, y, width, font) {}

    void SetRange(int32_t max)   { Max = max < 1 ? 1 : max; Value = std::clamp(Value, (int32_t)0, Max); Dirty = true; }
    void SetValue(int32_t v)     { v = std::clamp(v, (int32_t)0, Max); if (v == Value) return; Value = v; Dirty = true; Refresh(); }
    int32_t GetValue() const     { return Value; }
    int  GetPercent() const      { return (int)((int64_t)Value * 100 / Max); }

protected:
    void RenderBody(int x, int y, int w, int h) override {
        int fw = (int)((int64_t)w * Value / Max);
        if (fw > 0) Spr.fillRect(x, y, fw, h, ColBar);
        if (!ShowPercent) return;
        char s[8];
        snprintf(s, sizeof s, "%d%%", GetPercent());
        Spr.setFont(Font);
        Spr.setTextDatum(lgfx::textdatum_t::middle_center);
        int cx = x + w / 2, cy = y + h / 2;
        Spr.setClipRect(x + fw, y, w - fw, h);  Spr.setTextColor(ColFg);   Spr.drawString(s, cx, cy);
        Spr.setClipRect(x, y, fw, h);           Spr.setTextColor(ColBarFg); Spr.drawString(s, cx, cy);
        Spr.clearClipRect();
    }

private:
    int32_t Value = 0, Max = 100;
};
