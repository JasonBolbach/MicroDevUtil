#pragma once
// SuperRichText - a scrollable, display-only text area that understands a few HTML-like tags (colour, background, underline, strike, big text,
// alignment, <br>, <tab>, <hr>, entities). See SuperRichCore.h for the whole tag list. One font, no syntax checking: bad text shows as bad text.
//
//   SuperRichText help(&lcd, 10, 10, 300, 160, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
//   help.SetTextStatic("<center><color=yellow>Help</color></center>\n"
//                      "Turn the <u>encoder</u> to scroll.<br>Press A to close.");
//   help.BeginEdit();                       // the user scrolls; A closes (accept), B closes (cancel)
//
//  * Text: SetText(std::string) copies it; SetTextStatic(const char*) does NOT copy (the text must stay valid: a string literal in flash is perfect,
//    and costs no RAM). Any new line character is a line break, as in an editor.
//  * Memory: the layout is a table of 16 bytes per visual line (a 200 line text = 3 KB) plus the usual sprite (width x height x 2). The text is
//    never parsed into a tree; lines are read again when drawn.
//  * SetShowSource(true) draws the text with all tags visible (for debugging your markup).
//  * Scrolling: encoder / buttons while editing (StartEdit / Service / BeginEdit, like the list box), touch drag, or in code with ScrollTo / ScrollBy.
//    A thin bar at the right edge shows the position when the text is longer than the area.
//      Encoder = scroll a line, ButtonA short / long = close (accepted), ButtonB short = close (cancelled), ButtonB long = back to the top.
#include "SuperControl.h"
#include "SuperEditIO.h"
#include "SuperRichCore.h"

class SuperRichText : public SuperControl {
public:
    SuperRichText(lgfx::LovyanGFX* dst, int x, int y, int width, int bodyHeight, const lgfx::IFont* font,
                  SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB)
        : SuperControl(dst, x, y, width, font), BodyH(bodyHeight < 8 ? 8 : bodyHeight), IO(enc, btnA, btnB) {}

    void SetAbort(bool (*fn)()) { IO.Abort = fn; }
    void SetOnAccept(void (*fn)(void*), void* user = nullptr) { IO.OnAccept = fn; IO.OnAcceptUser = user; }
    SuperEditIO* EditIO() { return &IO; }

    // ---- text ----
    void SetText(const std::string& s) { Own = s; Txt = Own.c_str(); Len = Own.size(); Changed(); }
    void SetTextStatic(const char* s) { Own.clear(); Txt = s ? s : ""; Len = strlen(Txt); Changed(); }       // no copy: s must stay valid
    std::string GetText() const { return std::string(Txt, Len); }
    // Style fonts (optional): <b> <i> <b><i> and <h1> use the fonts you register here, e.g. SetStyleFont(FONT_BOLD, &lgfx::fonts::FreeSansBold9pt7b).
    // A slot with no font leaves its tag ignored. The line height follows the tallest font on the line.
    static constexpr int FONT_BOLD = SuperRich::FS_BOLD, FONT_ITALIC = SuperRich::FS_ITALIC, FONT_BOLDITALIC = SuperRich::FS_BOLDITALIC, FONT_HEADING = SuperRich::FS_HEADING;
    void SetStyleFont(int slot, const lgfx::IFont* f) { if (slot >= 0 && slot < SuperRich::FS_COUNT) { StyleFonts.Slot[slot] = f; LayoutOk = false; Dirty = true; } }

    void SetShowSource(bool on) { if (on != Source) { Source = on; Changed(); } }
    bool GetShowSource() const { return Source; }
    void SetTabChars(int n) { TabChars = n < 1 ? 1 : n; LayoutOk = false; Dirty = true; }
    void SetTextColor(uint16_t c) { ColFg = c; LayoutOk = false; Dirty = true; }
    void SetAlign(Align a) { Alignment = a; LayoutOk = false; Dirty = true; }
    void SetBodyHeight(int h) { BodyH = h < 8 ? 8 : h; LayoutOk = false; Dirty = true; }
    void SetLineGap(int px) { Gap = px < 0 ? 0 : px; LayoutOk = false; Dirty = true; }

    // ---- scrolling ----
    int  GetLineCount() { Layout(); return (int)Lines.size(); }
    int  GetFirstLine() const { return First; }
    int  GetContentHeight() { Layout(); return SuperRich::ContentHeight(Lines, Gap); }
    bool NeedsScroll() { Layout(); return Scrolls; }
    void ScrollTo(int line) { Layout(); line = std::max(0, std::min(line, MaxFirst)); if (line != First) { First = line; Dirty = true; } }
    void ScrollBy(int d) { ScrollTo(First + d); }

    // ---- editing (= scrolling): the same contract as the other editable controls ----
    bool IsEditing() const override { return State == EDIT_ACTIVE; }
    bool StartEdit() { IO.Flush(); State = EDIT_ACTIVE; PushUnlessPress(); return true; }
    bool BeginEdit() { return SuperRunEdit(*this); }
    SuperEditState Service() {
        if (State != EDIT_ACTIVE) return EDIT_IDLE;
        if (IO.Aborted()) return Finish(EDIT_CANCELLED);
        bool redraw = false; int d = (int)IO.Delta();
        if (d) { int f = First; ScrollBy(d); if (First != f) redraw = true; }
        switch (IO.NextKey()) {
        case SuperEditIO::A_SHORT: case SuperEditIO::A_LONG: return Finish(EDIT_ACCEPTED);
        case SuperEditIO::B_SHORT: return Finish(EDIT_CANCELLED);
        case SuperEditIO::B_LONG: if (First != 0) { First = 0; redraw = true; } break;
        default: break;
        }
        if (redraw) Push();
        return EDIT_ACTIVE;
    }
    bool OnPointer(const SuperPointerEvent& e) override {
        if (State != EDIT_ACTIVE) return false;
        return SuperPointerDefault(IO, *this, e, std::max(4, FontH()));
    }

protected:
    void OnFontChanged() override { LayoutOk = false; }
    int  BodyHeight() override { return BodyH; }

    void RenderBody(int x, int y, int w, int h) override {
        Layout(w, h);
        Spr.setFont(Font); Spr.setTextColor(ColFg);
        SuperRich::Options o = Opts();
        int tw = Scrolls ? std::max(1, w - Gutter) : w;
        Spr.setClipRect(x, y, w, h);
        int yy = 0;
        for (int i = First; i < (int)Lines.size() && yy < h; ++i) { SuperRich::DrawLine(Spr, Txt, Len, Lines[i], o, x, y + yy, tw); yy += Lines[i].H + Gap; }
        if (Scrolls) {                                                  // the position bar
            int total = SuperRich::ContentHeight(Lines, Gap), off = 0; for (int i = 0; i < First; ++i) off += Lines[i].H + Gap;
            int bx = x + w - 3, th = std::max(6, h * h / std::max(h, total)), range = std::max(1, total - h);
            int ty = y + (int)((long)off * (h - th) / range);
            Spr.drawFastVLine(bx + 1, y, h, ColSel); Spr.fillRect(bx, ty, 3, th, ColArrow);
        }
        Spr.clearClipRect();
    }

private:
    static constexpr int Gutter = 6;
    const char* Txt = ""; size_t Len = 0; std::string Own;
    int BodyH, Gap = 0, TabChars = 8, First = 0, MaxFirst = 0, LayoutW = -1;
    bool Source = false, LayoutOk = false, Scrolls = false;
    SuperEditIO IO; SuperEditState State = EDIT_IDLE; SuperRich::Fonts StyleFonts;
    std::vector<SuperRich::Line> Lines;

    void Changed() { LayoutOk = false; First = 0; Dirty = true; }
    SuperRich::Options Opts() {
        SuperRich::Options o; o.Base = Font; o.StyleFonts = &StyleFonts; o.Def.Fg = ColFg; o.Def.Bg = ColBg; o.Def.Align = (uint8_t)Alignment; o.Interpret = !Source;
        Spr.setFont(Font); o.TabPx = std::max(1, TabChars * (int)Spr.textWidth(" ")); return o;
    }
    void Layout() { Layout(InnerW(), BodyH); }
    void Layout(int w, int h) {
        if (LayoutOk && LayoutW == w) return;
        LayoutOk = true; LayoutW = w;
        Spr.setFont(Font);
        SuperRich::Options o = Opts();
        SuperRich::Layout(Spr, Txt, Len, w, o, Lines);
        Scrolls = SuperRich::ContentHeight(Lines, Gap) > h;
        if (Scrolls) { SuperRich::Layout(Spr, Txt, Len, std::max(1, w - Gutter), o, Lines); }      // the bar takes a few pixels
        int total = SuperRich::ContentHeight(Lines, Gap), acc = 0; MaxFirst = 0;                    // the last page is full: the first line that still fits with all below it
        for (int i = (int)Lines.size() - 1; i >= 0; --i) { acc += Lines[i].H + Gap; if (acc > h) { MaxFirst = i + 1; break; } }
        (void)total; First = std::max(0, std::min(First, MaxFirst));
    }
    SuperEditState Finish(SuperEditState r) { State = EDIT_IDLE; Push(); if (r == EDIT_ACCEPTED) IO.Accepted(); return r; }
};
