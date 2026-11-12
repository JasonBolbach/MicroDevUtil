#pragma once
// SuperGauge - a dial with a needle, display only (like SuperProgressBar: form.Add(&gauge, false)).
//
//   SuperGauge g(&lcd, 10, 10, 200, /*body height*/ 120, &lgfx::fonts::Font2);
//   g.SetRange(0, 100);
//   g.AddZone(0, 60, TFT_GREEN);  g.AddZone(60, 85, TFT_YELLOW);  g.AddZone(85, 100, TFT_RED);
//   g.SetFormat(0, "%");                // the value shown as text: no decimals, a unit
//   g.SetLabel("Load");
//   g.SetValue(42);                     // the needle glides there (smoothing is on by default)
//
//  * Sweep: SetSweep(startDeg, endDeg), measured clockwise from 12 o'clock. The default -90 .. +90 is a half dial (9 o'clock to 3 o'clock);
//    -135 .. +135 is a 270 degree dial. A sweep that fits in the upper half uses the control as a half dial (hub at the bottom), anything
//    wider is centred as a full circle. Everything scales with the control size.
//  * Ticks: SetTicks(major, minor) = number of major intervals and minor ticks between majors; the major ticks carry the value as a small
//    number when there is room (ShowTickLabels).
//  * Zones: AddZone(from, to, colour) paints coloured bands along the dial (the plain track shows where there is no zone). With
//    NeedleByZone (default) the needle and hub take the colour of the zone the value is in.
//  * Needle: NEEDLE_TAPER (default) or NEEDLE_LINE. ShowValue (default) prints the value under the hub, in the control's font.
//  * Smoothing (default ON, SetSmoothing(false) to switch it off): SetValue() starts a glide of about SmoothMs milliseconds. It is driven by
//    SuperControl::Tick(), which SuperForm::Service() calls, and the gauge only redraws while the needle is moving. Without a form call
//    Update() from loop(). With smoothing off SetValue() jumps and redraws at once.
//  * MinMax mode (optional): SetMinMax(windowMs, minDegrees) draws two lines on the dial, the lowest and the highest value SetValue() has
//    received during the last windowMs milliseconds (a value counts for as long as it was held). A line is only drawn when it is at least
//    minDegrees away from the needle, so nothing clutters the dial while the reading is steady. The lines expire by themselves as old
//    extremes age out of the window (SuperForm / Update() drive this like the glide). Each line crosses the dial ring, centred on its value,
//    MinMaxThickness pixels thick, in the INVERTED colour of the dial segment under it (the zone's colour, or the track's) so it always contrasts.
//  * It is built from LovyanGFX primitives, so it needs no bitmaps and works with the shared buffer / DMA like every control.
#include "SuperControl.h"
#include <cmath>
#include <cstdio>
#include <deque>
#include <vector>

class SuperGauge : public SuperControl {
public:
    enum NeedleStyle : uint8_t { NEEDLE_TAPER = 0, NEEDLE_LINE };

    uint16_t ColTrack  = 0x2104;           // the dial track where no zone covers it
    uint16_t ColTick   = 0x7BEF;           // tick marks and tick numbers
    uint16_t ColNeedle = 0xFFFF;           // needle and hub when there is no zone (or NeedleByZone is off)
    bool     NeedleByZone    = true;       // the needle takes the colour of the zone the value is in
    bool     ShowValue       = true;       // the value as text under the hub
    bool     ShowTickLabels  = true;       // numbers at the major ticks (hidden automatically when the dial is small)
    NeedleStyle Needle       = NEEDLE_TAPER;
    int      SmoothMs        = 250;        // about how long the needle takes to reach a new value
    const lgfx::IFont* TickFont = &lgfx::fonts::Font0;    // font of the tick numbers
    int      MinMaxThickness = 2;          // MinMax mode: line thickness in pixels, centred on the value (odd numbers are exactly symmetric)

    SuperGauge(lgfx::LovyanGFX* dst, int x, int y, int width, int bodyHeight, const lgfx::IFont* font = &lgfx::fonts::Font2)
        : SuperControl(dst, x, y, width, font), BodyH(bodyHeight < 8 ? 8 : bodyHeight) {}

    // ---- value ----
    void SetRange(float mn, float mx) { Min = mn; Max = mx > mn ? mx : mn + 1; Value = std::clamp(Value, Min, Max); Disp = std::clamp(Disp, Min, Max); Hist.clear(); LastRecorded = Value; HaveSample = false; Dirty = true; }
    void SetValue(float v) {
        v = std::clamp(v, Min, Max);
        if (MinMaxMs > 0) Record(v);
        if (v == Value && !Moving) return;
        Value = v;
        if (!Smooth) { Disp = v; Moving = false; Dirty = true; Refresh(); return; }
        Moving = (Disp != Value); LastMs = lgfx::millis();
    }
    float GetValue() const { return Value; }              // the value you set
    float GetShownValue() const { return Disp; }          // where the needle is right now (differs while it glides)
    void  SetSmoothing(bool on, int ms = 250) { Smooth = on; SmoothMs = ms < 1 ? 1 : ms; if (!on) { Disp = Value; Moving = false; Dirty = true; } }

    // ---- look ----
    void SetSweep(float startDeg, float endDeg) { Start = startDeg; End = endDeg > startDeg ? endDeg : startDeg + 1; Dirty = true; }
    void SetTicks(int major, int minor) { MajorN = major < 1 ? 1 : major; MinorN = minor < 0 ? 0 : minor; Dirty = true; }
    void SetFormat(int decimals, const std::string& unit = "") { Decimals = std::clamp(decimals, 0, 4); Unit = unit; Dirty = true; }
    void AddZone(float from, float to, uint16_t colour) { Zones.push_back({ from, to, colour }); Dirty = true; }
    void ClearZones() { Zones.clear(); Dirty = true; }
    void SetBodyHeight(int h) { BodyH = h < 8 ? 8 : h; Dirty = true; }

    // MinMax mode: lines for the lowest / highest value of the last 'windowMs' milliseconds, drawn only when at least 'minDegrees' away from the
    // needle. windowMs = 0 switches it off.
    void SetMinMax(int windowMs, float minDegrees = 3.f, int thickness = 2) {
        MinMaxMs = windowMs < 0 ? 0 : windowMs; MinMaxDeg = minDegrees < 0 ? 0 : minDegrees; MinMaxThickness = thickness < 1 ? 1 : thickness;
        Hist.clear(); LastRecorded = Value; HaveSample = false; Dirty = true;
    }
    bool  IsMinMax() const { return MinMaxMs > 0; }
    float GetWindowMin() { float lo, hi; Window(lo, hi); return lo; }       // lowest value of the window (the current value when MinMax is off)
    float GetWindowMax() { float lo, hi; Window(lo, hi); return hi; }
    void  ResetMinMax() { Hist.clear(); LastRecorded = Value; HaveSample = false; Dirty = true; }

    // ---- animation clock (SuperForm calls it; without a form call Update() from loop()) ----
    bool Tick() override {
        if (!IsVisible()) return false;
        uint32_t now = lgfx::millis();
        bool redraw = false;
        if (Moving && now - LastDraw >= 20) {                                  // at most about 50 redraws a second
            float dt = (float)std::min<uint32_t>(now - LastMs, 100);
            LastMs = now; LastDraw = now;
            float k = 1.f - std::exp(-dt / (SmoothMs * 0.35f));                // about 95% of the way after SmoothMs
            Disp += (Value - Disp) * k;
            if (std::fabs(Value - Disp) < (Max - Min) * 0.003f) { Disp = Value; Moving = false; }
            redraw = true;
        }
        if (MinMaxMs > 0 && now - LastMM >= 50) {                              // MinMax lines expire with time: redraw when one moved
            LastMM = now;
            float lo, hi; Window(lo, hi);
            float eps = (Max - Min) * 0.002f;
            if (std::fabs(lo - DrawnLo) > eps || std::fabs(hi - DrawnHi) > eps) redraw = true;
        }
        if (redraw) Dirty = true;
        return redraw;
    }
    void Update() { if (Tick()) Refresh(); }

protected:
    int BodyHeight() override { return BodyH; }

    void RenderBody(int x, int y, int w, int h) override {
        Spr.setFont(Font);
        int fh = ShowValue ? FontH() + 2 : 0;                                 // the readout strip at the bottom
        int ah = h - fh, m = 2;
        bool half = Start >= -90.f && End <= 90.f;                            // fits in the upper half plane: hub at the bottom
        int cx = x + w / 2, cy, rad, hubR;
        if (half) {
            rad = std::min(w / 2 - m, ah - m - 6);
            hubR = std::max(3, rad / 12);
            cy = y + ah - hubR - 2;
            rad = std::min(w / 2 - m, cy - y - m);
        } else {
            rad = std::min(w / 2 - m, ah / 2 - m);
            hubR = std::max(3, rad / 12);
            cy = y + ah / 2;
        }
        if (rad < 8) return;                                                  // too small to draw anything sensible
        int ring = std::max(3, rad / 8);
        Spr.setClipRect(x, y, w, h);

        // the track, then the zones on top of it
        Arc(cx, cy, rad - ring, rad, Start, End, ColTrack);
        for (auto& z : Zones) {
            float a0 = Angle(std::clamp(z.From, Min, Max)), a1 = Angle(std::clamp(z.To, Min, Max));
            if (a1 > a0) Arc(cx, cy, rad - ring, rad, a0, a1, z.Colour);
        }

        // ticks and their numbers
        int majorLen = std::max(4, rad / 9), minorLen = std::max(2, majorLen / 2);
        bool labels = ShowTickLabels && rad >= 50;
        int steps = MajorN * (MinorN + 1);
        for (int i = 0; i <= steps; ++i) {
            bool major = (i % (MinorN + 1)) == 0;
            float a = Start + (End - Start) * i / steps, t = a * 0.0174533f;
            int r1 = rad - ring - 2, r0 = r1 - (major ? majorLen : minorLen);
            Spr.drawLine(cx + (int)(std::sin(t) * r0), cy - (int)(std::cos(t) * r0), cx + (int)(std::sin(t) * r1), cy - (int)(std::cos(t) * r1), ColTick);
            if (major && labels) {
                float v = Min + (Max - Min) * i / steps;
                char b[16]; FormatNumber(v, b, sizeof b, std::fabs(Max - Min) < 10 ? 1 : 0);
                Spr.setFont(TickFont); Spr.setTextColor(ColTick); Spr.setTextDatum(lgfx::textdatum_t::middle_center);
                int tw = (int)Spr.textWidth(b), th = Spr.fontHeight();
                int rl = r0 - 3 - (int)(std::fabs(std::sin(t)) * tw / 2 + std::fabs(std::cos(t)) * th / 2);   // keep the number clear of its tick
                Spr.drawString(b, cx + (int)(std::sin(t) * rl), cy - (int)(std::cos(t) * rl));
                Spr.setFont(Font);
            }
        }

        // MinMax lines: the window's lowest and highest value, only when far enough from the needle. Each one crosses the ring, centred on its
        // value, in the inverted colour of the segment it lies on.
        if (MinMaxMs > 0) {
            float lo, hi; Window(lo, hi);
            DrawnLo = lo; DrawnHi = hi;
            float na = Angle(Disp);
            auto line = [&](float v) {
                float a = Angle(v);
                if (std::fabs(a - na) < MinMaxDeg) return;
                uint16_t c = (uint16_t)~SegmentColour(v);
                float t = a * 0.0174533f, s = std::sin(t), co = std::cos(t);
                float r0 = (float)(rad - ring), r1 = (float)rad;
                float x0 = cx + s * r0, y0 = cy - co * r0, x1 = cx + s * r1, y1 = cy - co * r1;
                if (MinMaxThickness <= 1) { Spr.drawLine((int)std::lround(x0), (int)std::lround(y0), (int)std::lround(x1), (int)std::lround(y1), c); return; }
                float half = (MinMaxThickness - 1) * 0.5f, px = co * half, py = s * half;   // sideways (perpendicular to the radius), so the line is centred on the value
                int ax = (int)std::lround(x0 - px), ay = (int)std::lround(y0 - py), bx = (int)std::lround(x0 + px), by = (int)std::lround(y0 + py);
                int cx2 = (int)std::lround(x1 + px), cy2 = (int)std::lround(y1 + py), dx = (int)std::lround(x1 - px), dy = (int)std::lround(y1 - py);
                Spr.fillTriangle(ax, ay, bx, by, cx2, cy2, c);
                Spr.fillTriangle(ax, ay, cx2, cy2, dx, dy, c);
            };
            line(lo); line(hi);
        }

        // the needle and hub
        uint16_t nc = ColNeedle;
        if (NeedleByZone) for (auto& z : Zones) if (Disp >= z.From && Disp <= z.To) nc = z.Colour;
        float t = Angle(Disp) * 0.0174533f, sx = std::sin(t), sy = -std::cos(t);
        int tipR = rad - ring - 1;
        int tx = cx + (int)(sx * tipR), ty = cy + (int)(sy * tipR);
        if (Needle == NEEDLE_LINE) {
            Spr.drawLine(cx, cy, tx, ty, nc);
            Spr.drawLine(cx + 1, cy, tx + 1, ty, nc);
        } else {
            int bx = (int)(-sy * hubR * 0.8f), by = (int)(sx * hubR * 0.8f);   // the base is perpendicular to the needle
            Spr.fillTriangle(tx, ty, cx + bx, cy + by, cx - bx, cy - by, nc);
        }
        Spr.fillCircle(cx, cy, hubR, nc);
        Spr.drawCircle(cx, cy, hubR + 1, ColFg);

        if (ShowValue) {                                                      // the value as text, in the control's font
            char b[24], u[40];
            FormatNumber(Value, b, sizeof b, Decimals);
            snprintf(u, sizeof u, "%s%s", b, Unit.c_str());
            Spr.setFont(Font); Spr.setTextColor(ColFg); Spr.setTextDatum(lgfx::textdatum_t::top_center);
            Spr.drawString(u, cx, y + ah + 1);
        }
        Spr.clearClipRect();
    }

private:
    struct Zone { float From, To; uint16_t Colour; };
    int   BodyH;
    float Min = 0, Max = 100, Value = 0, Disp = 0;
    float Start = -90.f, End = 90.f;
    int   MajorN = 4, MinorN = 1, Decimals = 0;
    std::string Unit;
    std::vector<Zone> Zones;
    bool  Smooth = true, Moving = false;
    uint32_t LastMs = 0, LastDraw = 0;

    // ---- MinMax history: fixed-width time buckets (Lo / Hi per bucket), so memory stays bounded however fast SetValue() is called ----
    struct Bucket { uint32_t T; float Lo, Hi; };
    std::deque<Bucket> Hist;
    int      MinMaxMs = 0;
    float    MinMaxDeg = 3.f, LastRecorded = 0, DrawnLo = 0, DrawnHi = 0;
    bool     HaveSample = false;                                               // the window only counts values SetValue() actually received
    uint32_t LastMM = 0;
    static constexpr int Buckets = 64;
    uint32_t BucketMs() const { return std::max<uint32_t>(1, MinMaxMs / Buckets); }

    void Advance(uint32_t now) {                                               // open buckets up to 'now'; a held value fills the gap
        uint32_t w = BucketMs();
        if (Hist.empty() || now - Hist.back().T > (uint32_t)MinMaxMs + w) { Hist.clear(); Hist.push_back({ now, LastRecorded, LastRecorded }); return; }
        while (now - Hist.back().T >= w) Hist.push_back({ Hist.back().T + w, LastRecorded, LastRecorded });
        while (Hist.size() > 1 && now - (Hist.front().T + w) >= (uint32_t)MinMaxMs) Hist.pop_front();     // fully outside the window
    }
    void Record(float v) {
        uint32_t now = lgfx::millis();
        if (!HaveSample) { HaveSample = true; LastRecorded = v; Hist.clear(); }
        Advance(now);
        Bucket& b = Hist.back();
        b.Lo = std::min(b.Lo, v); b.Hi = std::max(b.Hi, v);
        LastRecorded = v;
    }
    void Window(float& lo, float& hi) {
        lo = hi = Value;
        if (MinMaxMs <= 0 || !HaveSample) return;
        Advance(lgfx::millis());
        lo = hi = Hist.empty() ? Value : Hist.front().Lo;
        for (auto& b : Hist) { lo = std::min(lo, b.Lo); hi = std::max(hi, b.Hi); }
        lo = std::min(lo, Value); hi = std::max(hi, Value);
    }

    // colour of the dial where value v sits: its zone, or the track
    uint16_t SegmentColour(float v) const {
        uint16_t c = ColTrack;
        for (auto& z : Zones) if (v >= z.From && v <= z.To) c = z.Colour;
        return c;
    }

    float Angle(float v) const { return Start + (End - Start) * (v - Min) / (Max - Min); }       // degrees clockwise from 12 o'clock

    // Filled ring segment between two radii; angles are clockwise from 12 o'clock (LovyanGFX counts from 3 o'clock).
    void Arc(int cx, int cy, int r0, int r1, float a0, float a1, uint16_t colour) {
        auto norm = [](float a) { a = std::fmod(a - 90.f, 360.f); return a < 0 ? a + 360.f : a; };
        float s = norm(a0), e = norm(a1);
        if (a1 - a0 >= 359.9f) { s = 0; e = 360; }
        Spr.fillArc(cx, cy, r0, r1, s, e, colour);
    }
    static void FormatNumber(float v, char* b, size_t n, int decimals) {
        if (decimals <= 0) snprintf(b, n, "%d", (int)std::lround(v)); else snprintf(b, n, "%.*f", decimals, v);
    }
};
