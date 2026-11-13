#pragma once
// SuperProgressBar - display-only progress bar built on SuperControl.
//  * Value 0..Max (default 100). The fill grows left to right in ColBar.
//  * ShowPercent draws "42%" (centred by default: SetPercentAlign(ALIGN_LEFT / ALIGN_CENTER / ALIGN_RIGHT)) in ColFg, the text colour, on the fill and off it.
//  * Not editable: it never takes focus input (StartEdit() returns false).
#include "SuperControl.h"

class SuperProgressBar : public SuperControl {
public:
    uint16_t ColBar   = 0x07E0;         // fill
    bool     ShowPercent = true;        // the percentage is shown at all

    SuperProgressBar(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font)
        : SuperControl(dst, x, y, width, font) {}

    void SetRange(int32_t max)   { Max = max < 1 ? 1 : max; Value = std::clamp(Value, (int32_t)0, Max); Dirty = true; }
    void SetValue(int32_t v)     { v = std::clamp(v, (int32_t)0, Max); if (v == Value) return; Value = v; Dirty = true; Refresh(); }
    int32_t GetValue() const     { return Value; }
    int  GetPercent() const      { return (int)((int64_t)Value * 100 / Max); }
    void SetPercentAlign(Align a) { PercentAlign = a; Dirty = true; }

protected:
    void RenderBody(int x, int y, int w, int h) override {
        int fw = (int)((int64_t)w * Value / Max);
        if (fw > 0) Spr.fillRect(x, y, fw, h, ColBar);
        if (!ShowPercent) return;
        char s[8];
        snprintf(s, sizeof s, "%d%%", GetPercent());
        Spr.setFont(Font);
        Spr.setTextDatum(PercentAlign == ALIGN_LEFT ? lgfx::textdatum_t::middle_left : PercentAlign == ALIGN_RIGHT ? lgfx::textdatum_t::middle_right : lgfx::textdatum_t::middle_center);
        int cx = PercentAlign == ALIGN_LEFT ? x + 4 : PercentAlign == ALIGN_RIGHT ? x + w - 4 : x + w / 2, cy = y + h / 2;
        Spr.setTextColor(ColFg); Spr.drawString(s, cx, cy);
    }

private:
    int32_t Value = 0, Max = 100;
    Align   PercentAlign = ALIGN_CENTER;
};
