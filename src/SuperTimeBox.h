#pragma once
// SuperTimeBox / SuperDateBox - a time and a date made of numeric fields (see SuperFieldBox.h for the editing and touch behaviour).
//
//   SuperTimeBox   "HH:MM:SS" (or "HH:MM" with seconds = false), always 24 hour. HH 0-23, MM 0-59, SS 0-59.
//   SuperDateBox   year, month, day with the day kept inside the month (leap years included): changing the month from December to
//                  February in 2026-12-31 changes the day to 28 (29 in a leap year). Two display styles, DATE_YMD "2026-12-31" and
//                  DATE_MDY "12-31-2026" (SetStyle). Whatever the style, SetValue() takes and GetValue() returns "YYYY-MM-DD".
//
// Scrolling changes the whole field (like a spin box), A short moves to the next field. Wrap (SetWrap) makes a field go round instead of
// stopping at its limit. Callbacks: SetOnChange (live) and SetOnAccept, as on every control.
#include "SuperFieldBox.h"

class SuperTimeBox : public SuperFieldBox {
public:
    SuperTimeBox(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font,
                 SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB, bool seconds = true)
        : SuperFieldBox(dst, x, y, width, font, enc, btnA, btnB), Secs(seconds)
    {
        int n = seconds ? 3 : 2;
        Vals.assign(n, 0); Digits.assign(n, 2); Seps.assign(n - 1, ':');
        for (int i = 0; i < n; ++i) Order.push_back(i);
        Build();
    }

    bool SetTime(int h, int m, int s = 0) {
        if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59) return false;
        Vals[0] = h; Vals[1] = m; if (Secs) Vals[2] = s;
        Commit();
        return true;
    }
    int GetHour()    const { return Vals[0]; }
    int GetMinute()  const { return Vals[1]; }
    int GetSecond()  const { return Secs ? Vals[2] : 0; }
    int GetSeconds() const { return GetHour() * 3600 + GetMinute() * 60 + GetSecond(); }   // since midnight

    // "HH:MM:SS" ("HH:MM" without seconds)
    std::string GetValue() const {
        char b[12];
        if (Secs) snprintf(b, sizeof b, "%02d:%02d:%02d", Vals[0], Vals[1], Vals[2]); else snprintf(b, sizeof b, "%02d:%02d", Vals[0], Vals[1]);
        return b;
    }
    // The time being edited (use inside OnChange); the stored time when no edit is active.
    std::string GetLiveValue() const {
        const std::vector<int>& v = LiveVals(); char b[12];
        if (Secs) snprintf(b, sizeof b, "%02d:%02d:%02d", v[0], v[1], v[2]); else snprintf(b, sizeof b, "%02d:%02d", v[0], v[1]);
        return b;
    }
    bool SetValue(const std::string& v) {
        int h, m, s = 0;
        int n = sscanf(v.c_str(), "%d:%d:%d", &h, &m, &s);
        if (n < 2) return false;
        return SetTime(h, m, s);
    }

protected:
    int FieldMin(int) const override { return 0; }
    int FieldMax(int f, const std::vector<int>&) const override { return f == 0 ? 23 : 59; }

private:
    bool Secs;
};

class SuperDateBox : public SuperFieldBox {
public:
    enum DateStyle { DATE_YMD = 0, DATE_MDY };        // shown as 2026-12-31 or 12-31-2026

    SuperDateBox(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font,
                 SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB, DateStyle style = DATE_YMD)
        : SuperFieldBox(dst, x, y, width, font, enc, btnA, btnB)
    {
        Vals = { 2000, 1, 1 };                         // field numbers: 0 = year, 1 = month, 2 = day
        Digits = { 4, 2, 2 };
        Seps = { '-', '-' };
        SetStyle(style);
    }

    void SetStyle(DateStyle s) {                       // display order only: the value is always YYYY-MM-DD
        Order = (s == DATE_MDY) ? std::vector<int>{ 1, 2, 0 } : std::vector<int>{ 0, 1, 2 };
        Build();
    }
    void SetSeparator(char c) { for (auto& x : Seps) x = c; Build(); }                 // default '-'
    void SetYearRange(int mn, int mx) {                                                 // default 1970..2099
        YMin = mn; YMax = mx < mn ? mn : mx;
        Vals[0] = std::clamp(Vals[0], YMin, YMax);
        Constrain(Vals);
        Commit();
    }

    bool SetDate(int y, int m, int d) {
        if (y < YMin || y > YMax || m < 1 || m > 12 || d < 1 || d > DaysInMonth(y, m)) return false;
        Vals[0] = y; Vals[1] = m; Vals[2] = d;
        Commit();
        return true;
    }
    int GetYear()  const { return Vals[0]; }
    int GetMonth() const { return Vals[1]; }
    int GetDay()   const { return Vals[2]; }

    // Always "YYYY-MM-DD", whatever the display style.
    std::string GetValue() const {
        char b[16]; snprintf(b, sizeof b, "%04d-%02d-%02d", Vals[0], Vals[1], Vals[2]);
        return b;
    }
    // The date being edited, "YYYY-MM-DD" (use inside OnChange); the stored date when no edit is active.
    std::string GetLiveValue() const {
        const std::vector<int>& v = LiveVals(); char b[16];
        snprintf(b, sizeof b, "%04d-%02d-%02d", v[0], v[1], v[2]);
        return b;
    }
    bool SetValue(const std::string& v) {              // "YYYY-MM-DD"
        int y, m, d;
        if (sscanf(v.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return false;
        return SetDate(y, m, d);
    }

    static int DaysInMonth(int y, int m) {
        static const uint8_t dm[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        if (m < 1 || m > 12) return 0;
        bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
        return dm[m - 1] + (m == 2 && leap ? 1 : 0);
    }

protected:
    int FieldMin(int f) const override { return f == 0 ? YMin : 1; }
    int FieldMax(int f, const std::vector<int>& v) const override {
        return f == 0 ? YMax : f == 1 ? 12 : DaysInMonth(v[0], v[1]);
    }
    void Constrain(std::vector<int>& v) override {     // a shorter month (or a non-leap year) pulls the day back: 12-31 -> Feb = 02-28 / 29
        int dim = DaysInMonth(v[0], v[1]);
        if (dim > 0 && v[2] > dim) v[2] = dim;
    }

private:
    int YMin = 1970, YMax = 2099;
};
