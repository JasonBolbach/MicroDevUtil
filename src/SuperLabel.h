#pragma once
// SuperLabel - plain text and nothing else: no border, no title, no padding (all of those are still available if you want them,
// it is a SuperControl). Display only, like SuperProgressBar: form.Add(&label, false).
//
//   SuperLabel title(&lcd, 0, 4, 320, &lgfx::fonts::Font4, "Settings", SuperControl::ALIGN_CENTER);
//   title.SetTextColor(TFT_CYAN);
//   SuperLabel info(&lcd, 10, 40, 300, &lgfx::fonts::Font2, "A longer text that wraps onto several lines.");
//   info.SetWrap(true);                                   // word wrap (the height follows the number of lines)
//   info.SetBodyHeight(60); info.SetVAlign(SuperLabel::VALIGN_MIDDLE);   // or a fixed height with vertical alignment
//   value.SetTextf("%d.%d C", t / 10, t % 10);            // printf style, handy for live values
//
//  * Alignment: SetAlign(ALIGN_LEFT / ALIGN_CENTER / ALIGN_RIGHT) for every line; SetVAlign(VALIGN_TOP / MIDDLE / BOTTOM) when the
//    height is fixed with SetBodyHeight(h) (0 = automatic: as tall as its lines).
//  * Lines: '\n' starts a new line. SetWrap(true) also breaks long lines at spaces (a word wider than the control is broken inside).
//    Without wrapping, a line that is too wide is cut and ends in "..." (SetEllipsis(false) simply clips it).
//    SetMaxLines(n) limits the number of lines; the last one ends in "..." when text was dropped.
//  * SetLineGap(px) adds space between lines; SetUnderline(true) underlines every line.
//  * Colours: SetTextColor(c) is the text, ColBg the background. The control fills its whole area with ColBg, so set it to the colour
//    of what is behind (or use ApplyTheme). There is no transparent mode.
//  * Memory: like every control, one sprite of width x height x 2 bytes (follows SharedBuffer / UsePsram).
#include "SuperControl.h"
#include "SuperRichCore.h"
#include <cstdarg>
#include <cstdio>
#include <vector>

class SuperLabel : public SuperControl {
public:
    enum VAlign : uint8_t { VALIGN_TOP = 0, VALIGN_MIDDLE, VALIGN_BOTTOM };

    SuperLabel(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font, const std::string& text = "", Align align = ALIGN_LEFT)
        : SuperControl(dst, x, y, width, font), Text(text) {
        SetPadding(0);
        Alignment = align;
    }

    void SetText(const std::string& t) { if (t == Text) return; Text = t; LinesOk = false; Dirty = true; Refresh(); }
    void SetTextf(const char* fmt, ...) {
        char b[256];
        va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
        SetText(b);
    }
    const std::string& GetText() const { return Text; }

    void SetTextColor(uint16_t c) { ColFg = c; Dirty = true; }
    void SetVAlign(VAlign v)      { VA = v; Dirty = true; }
    void SetWrap(bool on)         { Wrap = on; LinesOk = false; Dirty = true; }
    void SetEllipsis(bool on)     { Ellipsis = on; LinesOk = false; Dirty = true; }
    void SetMaxLines(int n)       { MaxLines = n < 0 ? 0 : n; LinesOk = false; Dirty = true; }
    void SetLineGap(int px)       { Gap = px < 0 ? 0 : px; Dirty = true; }
    void SetUnderline(bool on)    { Underline = on; Dirty = true; }
    void SetBodyHeight(int h)     { FixedH = h < 0 ? 0 : h; Dirty = true; }         // 0 = as tall as the lines
    // SetRich(true): the text may contain the tags of SuperRichCore.h (<color=red>, <bg=...>, <u>, <s>, <big>, <center>, <br>, <tab>, entities ...).
    // Default off: a plain label shows every character as it is. In rich mode SetWrap / SetAlign / SetVAlign / SetBodyHeight / SetLineGap still work;
    // SetMaxLines, SetEllipsis and SetUnderline do not (use the tags).
    void SetRich(bool on)         { if (on != Rich) { Rich = on; LinesOk = false; Dirty = true; } }
    bool GetRich() const          { return Rich; }
    int  GetLineCount()           { Layout(); return Rich ? (int)RichLines.size() : (int)Lines.size(); }

protected:
    void OnFontChanged() override { LinesOk = false; }

    int BodyHeight() override {
        if (FixedH > 0) return FixedH;
        return BlockHeight();
    }

    void RenderBody(int x, int y, int w, int h) override {
        Layout();
        Spr.setFont(Font);
        Spr.setTextColor(ColFg);
        if (Rich) {
            SuperRich::Options o = RichOpts(); int bh = BlockHeight();
            int ty = y + (VA == VALIGN_MIDDLE ? (h - bh) / 2 : VA == VALIGN_BOTTOM ? h - bh : 0);
            Spr.setClipRect(x, y, w, h);
            for (const SuperRich::Line& l : RichLines) { SuperRich::DrawLine(Spr, Text.c_str(), Text.size(), l, o, x, ty, w); ty += l.H + Gap; }
            Spr.clearClipRect();
            return;
        }
        int fh = Spr.fontHeight(), bh = BlockHeight();
        int ty = y + (VA == VALIGN_MIDDLE ? (h - bh) / 2 : VA == VALIGN_BOTTOM ? h - bh : 0);
        Spr.setClipRect(x, y, w, h);
        Spr.setTextDatum(Alignment == ALIGN_CENTER ? lgfx::textdatum_t::top_center
                       : Alignment == ALIGN_RIGHT  ? lgfx::textdatum_t::top_right : lgfx::textdatum_t::top_left);
        int tx = Alignment == ALIGN_CENTER ? x + w / 2 : Alignment == ALIGN_RIGHT ? x + w - 1 : x;
        for (const std::string& ln : Lines) {
            if (!ln.empty()) {
                Spr.drawString(ln.c_str(), tx, ty);
                if (Underline) {
                    int lw = Spr.textWidth(ln.c_str());
                    int ux = Alignment == ALIGN_CENTER ? x + (w - lw) / 2 : Alignment == ALIGN_RIGHT ? x + w - lw : x;
                    Spr.drawFastHLine(ux, ty + fh - 1, lw, ColFg);
                }
            }
            ty += fh + Gap;
        }
        Spr.clearClipRect();
    }

private:
    std::string Text;
    VAlign VA = VALIGN_TOP;
    bool Wrap = false, Ellipsis = true, Underline = false;
    int  MaxLines = 0, Gap = 0, FixedH = 0;
    std::vector<std::string> Lines;
    std::vector<SuperRich::Line> RichLines;
    bool Rich = false;
    bool LinesOk = false;
    int  LinesW = -1;

    int InnerWidth() { return InnerW(); }
    SuperRich::Options RichOpts() {
        SuperRich::Options o; o.Def.Fg = ColFg; o.Def.Bg = ColBg; o.Def.Align = (uint8_t)Alignment; o.Wrap = Wrap;
        Spr.setFont(Font); o.TabPx = std::max(1, 8 * (int)Spr.textWidth(" ")); return o;
    }
    int BlockHeight() {
        Layout();
        Spr.setFont(Font);
        if (Rich) return std::max(Spr.fontHeight(), SuperRich::ContentHeight(RichLines, Gap) - Gap);
        int n = std::max(1, (int)Lines.size());
        return n * Spr.fontHeight() + (n - 1) * Gap;
    }
    int TextW(const std::string& s) { return s.empty() ? 0 : (int)Spr.textWidth(s.c_str()); }

    // Shorten 's' until it and "..." fit in w pixels.
    std::string Cut(std::string s, int w) {
        if (TextW(s) <= w) return s;
        if (!Ellipsis) return s;                                    // clipped by the clip rectangle when drawn
        while (!s.empty() && TextW(s + "...") > w) s.pop_back();
        return s + "...";
    }

    void Layout() {
        int w = InnerWidth();
        if (LinesOk && w == LinesW) return;
        LinesOk = true; LinesW = w;
        Spr.setFont(Font);
        if (Rich) { SuperRich::Options o = RichOpts(); SuperRich::Layout(Spr, Text.c_str(), Text.size(), w, o, RichLines); return; }
        Lines.clear();
        size_t p = 0;
        while (true) {
            size_t e = Text.find('\n', p);
            std::string para = Text.substr(p, e == std::string::npos ? std::string::npos : e - p);
            if (!Wrap) Lines.push_back(Cut(para, w));
            else       WrapParagraph(para, w);
            if (e == std::string::npos) break;
            p = e + 1;
        }
        if (MaxLines > 0 && (int)Lines.size() > MaxLines) {
            Lines.resize(MaxLines);
            if (Ellipsis) {
                std::string& l = Lines.back();
                while (!l.empty() && TextW(l + "...") > w) l.pop_back();
                l += "...";
            }
        }
        if (Lines.empty()) Lines.push_back("");
    }

    void WrapParagraph(const std::string& s, int w) {
        std::string cur;
        size_t i = 0, n = s.size();
        while (i < n) {
            size_t j = s.find(' ', i);
            std::string word = s.substr(i, j == std::string::npos ? std::string::npos : j - i);
            i = (j == std::string::npos) ? n : j + 1;
            std::string trial = cur.empty() ? word : cur + " " + word;
            if (TextW(trial) <= w) { cur = trial; continue; }
            if (!cur.empty()) { Lines.push_back(cur); cur.clear(); }
            while (TextW(word) > w && word.size() > 1) {            // a word wider than the control: break inside it
                size_t k = word.size() - 1;
                while (k > 1 && TextW(word.substr(0, k)) > w) --k;
                Lines.push_back(word.substr(0, k)); word.erase(0, k);
            }
            cur = word;
        }
        Lines.push_back(cur);
    }
};
