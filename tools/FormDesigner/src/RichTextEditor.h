#pragma once
// A modal editor for the text of a Rich text control (the Form Designer), drawn inside the designer window.
//
//   [ Source ] [ Rendered ]                                  line 3, col 12
//   +-------------------------------------------------------+
//   | <center><color=yellow>Title</color></center>          |   Source: the markup as text, edited with the multi-line field
//   | Some <u>underlined</u> text.                          |   Rendered: the real control with this text (read-only; wheel scrolls)
//   +-------------------------------------------------------+
//   tags: <br> <tab> <hr> <color=red></color> <bg=#003> <u> <s> <big> <left> <center> <right> &lt; &gt; &amp;     [Cancel] [OK]
//
// Source mode: arrows, Up / Down keep the column, Home / End, Shift selects, Enter = new line, Ctrl/Cmd + A C X V Z, mouse click / drag, double click =
// word, triple click = line, wheel scrolls. Esc = Cancel, Ctrl/Cmd + Enter = OK, Ctrl/Cmd + R = Source <-> Rendered. Rendered mode: Enter = OK, Esc = Cancel.
// US-ASCII only; at most 4000 characters. There is no syntax checking: what you type is what the control gets.
#include <algorithm>
#include <functional>
#include <memory>
#include <LovyanGFX.hpp>
#include <SuperPushButton.h>
#include <SuperRichText.h>
#include "DesignerInput.h"
#include "DesignerRichTextField.h"
#include "DesignerWidgets.h"
#include "DesignerControls.h"

namespace FD {
class RichTextEditor {
public:
    enum State { OPEN, OK, CANCEL } state = OPEN;

    RichTextEditor(lgfx::LGFX_Sprite* g, const std::string& title, const DControl& proto, uint16_t formBg) : G(g), Title(title), Proto(proto), FormBg(formBg) {
        Field.SetLines(proto.list);
        SuperPushButton probe(G, 0, 0, 90, &lgfx::fonts::Font2, "OK"); BtnH = probe.GetHeight();
        G->setFont(&lgfx::fonts::Font2); LineH = (int)G->fontHeight() + 2;
        Rows = AH() / LineH;
    }

    static constexpr int W = 980, H = 640, X = (FD::WIN_W - W) / 2, Y = (FD::WIN_H - H) / 2;
    int AX() const { return X + 16; }                       // the edit / preview area
    int AY() const { return Y + 74; }
    static constexpr int AW() { return W - 32; }
    static constexpr int AH() { return H - 74 - 96; }
    int SourceX() const { return X + 16; }
    int RenderedX() const { return X + 16 + 110; }
    int ModeY() const { return Y + 40; }
    int BtnY() const { return Y + H - 16 - BtnH; }
    int OkX() const { return X + W - 16 - 90; }
    int CancelX() const { return OkX() - 10 - 90; }
    std::vector<std::string> Lines() const { return Field.Lines(); }
    RichTextField& F() { return Field; }
    bool Rendered() const { return Mode == M_RENDERED; }
    int Rows_() const { return Rows; }
    int RenderedFirstLine() const { return RenderFirst; }

    void Event(const InputEvent& e) {
        switch (e.type) {
        case InputEvent::DOWN: if (e.button == SDL_BUTTON_LEFT) Down(e); break;
        case InputEvent::MOVE: if (Drag && Mode == M_SOURCE) { Field.SetCaretFromXY(e.x - AX() - 6, e.y - AY() - 3, true, Measure(), LineH); Reveal(); } break;
        case InputEvent::UP: Drag = false; break;
        case InputEvent::WHEEL:
            if (e.x >= X && e.x < X + W && e.y >= Y && e.y < Y + H) {
                if (Mode == M_RENDERED) RenderFirst = std::max(0, RenderFirst - e.wheel * 2);
                else Field.firstLine = std::max(0, std::min(Field.firstLine - e.wheel * 2, std::max(0, Field.LineCount() - 1)));
            }
            break;
        case InputEvent::KEY_DOWN: Key(e); break;
        case InputEvent::TEXT: if (Mode == M_SOURCE) { Field.Insert(e.text); Reveal(); } break;
        default: break;
        }
    }

    void Draw(uint32_t ticks) {
        lgfx::LGFX_Sprite& g = *G;
        g.fillRectAlpha(0, 0, FD::WIN_W, FD::WIN_H, 150, 0x0000);
        g.fillRect(X, Y, W, H, COL_PANEL); g.drawRect(X, Y, W, H, COL_ACCENT); g.drawRect(X + 1, Y + 1, W - 2, H - 2, COL_ACCENT);
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left);
        g.setTextColor(COL_ACCENT); g.drawString(Title.c_str(), X + 16, Y + 12);
        { SuperPushButton a(G, SourceX(), ModeY(), 100, &lgfx::fonts::Font2, "Source"), b(G, RenderedX(), ModeY(), 100, &lgfx::fonts::Font2, "Rendered");
          SuperPushButton& on = Mode == M_SOURCE ? a : b; on.ColBorder = COL_ACCENT; on.ColBg = COL_SEL; a.Push(); b.Push(); }
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left);
        char st[64]; int cl = Field.LineOf(Field.caret);
        snprintf(st, sizeof st, "line %d, col %d     %d lines, %d characters", cl + 1, Field.ColOf(Field.caret) + 1, Field.LineCount(), (int)Field.text.size());
        g.setTextColor(COL_DIM); g.drawString(st, X + W - 16 - 320, ModeY() + 4);
        g.fillRect(AX(), AY(), AW(), AH(), Mode == M_SOURCE ? 0x0000 : FormBg); g.drawRect(AX(), AY(), AW(), AH(), Mode == M_SOURCE ? (Focused ? COL_ACCENT : 0x7BEF) : COL_LINE);
        g.setClipRect(AX() + 1, AY() + 1, AW() - 2, AH() - 2);
        if (Mode == M_SOURCE) DrawSource(ticks); else DrawRendered();
        g.clearClipRect();
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left); g.setTextColor(COL_DIM);
        g.drawString("tags: <br> <tab> <hr> <color=red></color> <bg=#003> <u> <s> <big> <left> <center> <right> <b> <i> <h1>   &lt; &gt; &amp;   (<b> <i> <h1> need style fonts; other tags are ignored)", X + 16, AY() + AH() + 8);
        g.drawString("Ctrl/Cmd + R: Source / Rendered     Ctrl/Cmd + Enter: OK     Esc: Cancel", X + 16, AY() + AH() + 28);
        SuperPushButton cancel(G, CancelX(), BtnY(), 90, &lgfx::fonts::Font2, "Cancel"), ok(G, OkX(), BtnY(), 90, &lgfx::fonts::Font2, "OK");
        ok.ColBorder = COL_ACCENT; cancel.Push(); ok.Push();
    }

private:
    enum Mode { M_SOURCE, M_RENDERED } Mode = M_SOURCE;
    lgfx::LGFX_Sprite* G; std::string Title; DControl Proto; uint16_t FormBg;
    RichTextField Field; bool Drag = false, Focused = true; int BtnH = 20, LineH = 18, Rows = 20, RenderFirst = 0;

    std::function<int(const std::string&)> Measure() { return [this](const std::string& s) { return MeasureText(*G, s); }; }
    void Reveal() { Field.EnsureVisibleXY(Measure(), AW() - 12, Rows); }
    void Toggle() { Mode = Mode == M_SOURCE ? M_RENDERED : M_SOURCE; Drag = false; RenderFirst = 0; }

    void DrawSource(uint32_t ticks) {
        lgfx::LGFX_Sprite& g = *G;
        int tx = AX() + 6 - Field.scrollX;
        for (int r = 0; r < Rows + 1; ++r) {
            int line = Field.firstLine + r; if (line >= Field.LineCount()) break;
            int y = AY() + 3 + r * LineH, a = Field.LineStart(line), b = Field.LineEnd(line);
            if (Field.HasSel()) {
                int s0 = std::max(Field.SelBegin(), a), s1 = std::min(Field.SelEnd(), b + 1);
                if (s0 < s1 || (Field.SelBegin() <= b && Field.SelEnd() > b)) {
                    int x0 = tx + MeasureText(g, Field.text.substr(a, std::max(0, s0 - a))), x1 = tx + MeasureText(g, Field.text.substr(a, std::max(0, std::min(s1, b) - a)));
                    if (Field.SelEnd() > b && Field.SelBegin() <= b) x1 += 6;                    // the selected line break
                    g.fillRect(x0, y, std::max(1, x1 - x0), LineH, COL_SEL);
                }
            }
            g.setTextColor(0xFFFF); g.drawString(Field.text.substr(a, b - a).c_str(), tx, y + 1);
        }
        if (Focused && (ticks / 500) % 2 == 0) {
            int line = Field.LineOf(Field.caret);
            if (line >= Field.firstLine && line < Field.firstLine + Rows + 1) {
                int cx = tx + MeasureText(g, Field.text.substr(Field.LineStart(line), Field.caret - Field.LineStart(line)));
                g.drawFastVLine(cx, AY() + 3 + (line - Field.firstLine) * LineH, LineH, 0xFFFF);
            }
        }
    }
    void DrawRendered() {
        DControl t = Proto; t.list = Field.Lines(); t.x = 0; t.y = 0; t.w = std::min(AW() - 24, std::max(Proto.w, 100));
        auto c = MakeControl(G, t, AX() + 12, AY() + 12);
        if (auto* rt = dynamic_cast<SuperRichText*>(c.get())) { rt->ScrollTo(RenderFirst); RenderFirst = rt->GetFirstLine(); }
        c->Push();
    }
    void Down(const InputEvent& e) {
        auto in = [&](int x, int y, int w, int h) { return e.x >= x && e.x < x + w && e.y >= y && e.y < y + h; };
        if (in(SourceX(), ModeY(), 100, BtnH)) { Mode = M_SOURCE; return; }
        if (in(RenderedX(), ModeY(), 100, BtnH)) { Mode = M_RENDERED; Drag = false; RenderFirst = 0; return; }
        if (in(OkX(), BtnY(), 90, BtnH)) { state = OK; return; }
        if (in(CancelX(), BtnY(), 90, BtnH)) { state = CANCEL; return; }
        if (Mode == M_SOURCE && in(AX(), AY(), AW(), AH())) {
            Focused = true; Drag = true;
            Field.SetCaretFromXY(e.x - AX() - 6, e.y - AY() - 3, (SDL_GetModState() & KMOD_SHIFT) != 0, Measure(), LineH);
            if (e.clicks == 2) Field.SelectWord(Field.caret); else if (e.clicks >= 3) Field.SelectLine(Field.LineOf(Field.caret));
            Reveal();
        }
    }
    void Key(const InputEvent& e) {
        bool cmd = e.mod & (KMOD_CTRL | KMOD_GUI);
        if (cmd && e.key == SDLK_r) { Toggle(); return; }
        if (e.key == SDLK_ESCAPE) { state = CANCEL; return; }
        if (cmd && (e.key == SDLK_RETURN || e.key == SDLK_KP_ENTER)) { state = OK; return; }
        if (Mode == M_RENDERED) {
            if (e.key == SDLK_RETURN || e.key == SDLK_KP_ENTER) state = OK;
            else if (e.key == SDLK_DOWN) RenderFirst += 1; else if (e.key == SDLK_UP) RenderFirst = std::max(0, RenderFirst - 1);
            else if (e.key == SDLK_PAGEDOWN) RenderFirst += Rows; else if (e.key == SDLK_PAGEUP) RenderFirst = std::max(0, RenderFirst - Rows);
            return;
        }
        if (e.key == SDLK_PAGEUP || e.key == SDLK_PAGEDOWN) { Field.MoveLine(e.key == SDLK_PAGEUP ? -(Rows - 1) : Rows - 1, (e.mod & KMOD_SHIFT) != 0); Reveal(); return; }
        if (Field.Key(e)) Reveal();
    }
};
}
