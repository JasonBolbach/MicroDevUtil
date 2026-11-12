#pragma once
// SuperTimeBox / SuperDateBox - SuperInputBox with a fixed template and built-in validation.
//   SuperTimeBox: "##:##:##" (or "##:##" without seconds). An accept is refused unless 00:00:00..23:59:59.
//   SuperDateBox: "####-##-##" (ISO year-month-day). An accept is refused unless the date exists
//                 (month 1-12, day within the month, leap years handled).
// Editing, callbacks and look are exactly those of SuperInputBox. The SuperInputBox validator is used up by
// the built-in check; use SetOnAccept() to react to a valid value.
#include "SuperInputBox.h"
#include <cstdio>

class SuperTimeBox : public SuperInputBox {
public:
    SuperTimeBox(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font,
                 SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB, bool seconds = true)
        : SuperInputBox(dst, x, y, width, font, seconds ? "##:##:##" : "##:##", enc, btnA, btnB), Secs(seconds)
    { SetValidator(&Check, this); }

    bool SetTime(int h, int m, int s = 0) {
        if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59) return false;
        char b[8];
        if (Secs) snprintf(b, sizeof b, "%02d%02d%02d", h, m, s); else snprintf(b, sizeof b, "%02d%02d", h, m);
        return SetValue(b);
    }
    int GetHour()   const { return Field(0); }
    int GetMinute() const { return Field(2); }
    int GetSecond() const { return Secs ? Field(4) : 0; }
    int GetSeconds() const { return GetHour() * 3600 + GetMinute() * 60 + GetSecond(); }   // since midnight

private:
    bool Secs;
    int Field(int i) const { std::string v = GetValue(); return (v[i] - '0') * 10 + (v[i + 1] - '0'); }
    static bool Check(const std::string& v, void* self) {
        SuperTimeBox* t = (SuperTimeBox*)self;
        if (v.size() < (t->Secs ? 6u : 4u)) return false;
        int h = (v[0] - '0') * 10 + (v[1] - '0'), m = (v[2] - '0') * 10 + (v[3] - '0');
        int s = t->Secs ? (v[4] - '0') * 10 + (v[5] - '0') : 0;
        return h <= 23 && m <= 59 && s <= 59;
    }
};

class SuperDateBox : public SuperInputBox {
public:
    SuperDateBox(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font,
                 SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB)
        : SuperInputBox(dst, x, y, width, font, "####-##-##", enc, btnA, btnB)
    { SetValidator(&Check, nullptr); }

    bool SetDate(int y, int m, int d) {
        if (y < 0 || y > 9999 || !Valid(y, m, d)) return false;
        char b[12]; snprintf(b, sizeof b, "%04d%02d%02d", y, m, d);
        return SetValue(b);
    }
    int GetYear()  const { return Num(0, 4); }
    int GetMonth() const { return Num(4, 2); }
    int GetDay()   const { return Num(6, 2); }

    static int DaysInMonth(int y, int m) {
        static const uint8_t dm[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        if (m < 1 || m > 12) return 0;
        bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
        return dm[m - 1] + (m == 2 && leap ? 1 : 0);
    }

private:
    static bool Valid(int y, int m, int d) { return m >= 1 && m <= 12 && d >= 1 && d <= DaysInMonth(y, m); }
    int Num(int at, int n) const { std::string v = GetValue(); int r = 0; for (int i = 0; i < n; ++i) r = r * 10 + (v[at + i] - '0'); return r; }
    static bool Check(const std::string& v, void*) {
        if (v.size() < 8) return false;
        int y = 0; for (int i = 0; i < 4; ++i) y = y * 10 + (v[i] - '0');
        return Valid(y, (v[4] - '0') * 10 + (v[5] - '0'), (v[6] - '0') * 10 + (v[7] - '0'));
    }
};
