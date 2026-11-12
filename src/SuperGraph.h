#pragma once
// SuperGraph - scrolling line graph built on SuperControl (display only).
//  * AddSample(v) appends a value; the oldest scroll off the left. By default one sample per pixel of
//    inner width (SetCapacity(n) changes that; the line is scaled to fit).
//  * Vertical range is automatic (min..max of what is shown, plus a margin) unless SetRange(lo, hi) is used.
//  * ShowRange prints the top / bottom values; ShowGrid draws three faint horizontal lines.
//  * Memory: capacity * 4 bytes for the samples, plus the sprite.
//  * Not editable (StartEdit() returns false).
#include "SuperControl.h"

class SuperGraph : public SuperControl {
public:
    uint16_t ColLine = 0x07FF;
    uint16_t ColGrid = 0x2104;
    bool ShowGrid  = true;
    bool ShowRange = false;

    SuperGraph(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font, int bodyHeight)
        : SuperControl(dst, x, y, width, font), BodyH(bodyHeight < 8 ? 8 : bodyHeight) {}

    void SetBodyHeight(int h) { BodyH = h < 8 ? 8 : h; Dirty = true; }
    void SetCapacity(int n)   { Cap = n < 2 ? 2 : n; Buf.assign(Cap, 0.f); Head = Count = 0; Dirty = true; }
    void SetRange(float lo, float hi) { Lo = lo; Hi = hi > lo ? hi : lo + 1; Auto = false; Dirty = true; }
    void SetAutoRange() { Auto = true; Dirty = true; }
    void Clear() { Head = Count = 0; Dirty = true; Refresh(); }
    int  GetCount() const { return Count; }

    // redraw=false lets you add several samples and call Refresh() once.
    void AddSample(float v, bool redraw = true) {
        if (Buf.empty()) SetCapacity(InnerW());
        Buf[Head] = v; Head = (Head + 1) % Cap; if (Count < Cap) ++Count;
        Dirty = true;
        if (redraw) Refresh();
    }

protected:
    int BodyHeight() override { return BodyH; }

    void RenderBody(int x, int y, int w, int h) override {
        if (ShowGrid) for (int i = 1; i < 4; ++i) Spr.drawFastHLine(x, y + h * i / 4, w, ColGrid);
        if (Count < 1) return;
        float lo = Lo, hi = Hi;
        if (Auto) {
            lo = hi = At(0);
            for (int i = 1; i < Count; ++i) { float v = At(i); lo = std::min(lo, v); hi = std::max(hi, v); }
            float m = (hi - lo) * 0.05f; if (m <= 0) m = 1;
            lo -= m; hi += m;
        }
        auto px = [&](int i) { return x + w - 1 - (int)((int64_t)(Count - 1 - i) * (w - 1) / std::max(1, Cap - 1)); };
        auto py = [&](float v) { float f = (v - lo) / (hi - lo); f = std::clamp(f, 0.f, 1.f); return y + h - 1 - (int)(f * (h - 1) + .5f); };
        int ox = px(0), oy = py(At(0));
        for (int i = 1; i < Count; ++i) {
            int nx = px(i), ny = py(At(i));
            Spr.drawLine(ox, oy, nx, ny, ColLine);
            ox = nx; oy = ny;
        }
        if (ShowRange) {
            char s[24];
            Spr.setFont(Font); Spr.setTextColor(ColFg);
            Spr.setTextDatum(lgfx::textdatum_t::top_left);     snprintf(s, sizeof s, "%g", hi); Spr.drawString(s, x + 1, y);
            Spr.setTextDatum(lgfx::textdatum_t::bottom_left);  snprintf(s, sizeof s, "%g", lo); Spr.drawString(s, x + 1, y + h);
        }
    }

private:
    int BodyH, Cap = 0, Head = 0, Count = 0;
    bool Auto = true;
    float Lo = 0, Hi = 1;
    std::vector<float> Buf;
    float At(int i) const { return Buf[((Head - Count + i) % Cap + Cap) % Cap]; }   // i = 0 oldest
};
