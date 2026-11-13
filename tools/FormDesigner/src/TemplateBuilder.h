#pragma once
// A modal template builder for SuperInputBox, drawn inside the designer window. The template is a row of cells; each cell is a slot
//   #  digit    A  upper-case letter (or space)    a  lower-case (or space)    B  any letter (or space)    *  any typeable character
// or a literal (any other character: - / : . , ( ) + @ _ space ...). The letters A, a, B and the symbols # and * are always slots (library rule).
//
// Mouse: click a cell to select it (the "+" cell appends), click a type button to make the selected cell that slot, click a literal button for a literal,
//        Insert / Delete / move buttons.  Keys: type a character = it becomes the selected cell and the selection moves on (# A a B * are slots,
//        anything else a literal), Left / Right / Home / End select, Insert adds a digit cell, Delete / Backspace removes, Enter = OK, Esc = Cancel.
// The preview is the real SuperInputBox in the control's own font and colours.
#include <memory>
#include <LovyanGFX.hpp>
#include <SuperPushButton.h>
#include "DesignerInput.h"
#include "DesignerWidgets.h"
#include "DesignerControls.h"
#include "PresetDialog.h"

namespace FD {
class TemplateBuilder {
public:
    enum State { OPEN, OK, CANCEL } state = OPEN;
    std::string tmpl;                                         // the template being built (the result when state == OK)
    static constexpr int MAX_CELLS = 40;

    TemplateBuilder(lgfx::LGFX_Sprite* g, const std::string& title, const DControl& proto, uint16_t formBg, const std::string& start)
        : tmpl(start), G(g), Title(title), Proto(proto), FormBg(formBg) {
        if (tmpl.size() > MAX_CELLS) tmpl.resize(MAX_CELLS);
        Sel = 0;
        SuperPushButton probe(G, 0, 0, 90, &lgfx::fonts::Font2, "OK"); BtnH = probe.GetHeight();
    }

    static constexpr int W = 760, H = 500, X = (FD::WIN_W - W) / 2, Y = (FD::WIN_H - H) / 2, CHIP_W = 34, CHIP_H = 40, PER_ROW = 20;
    static bool IsSlot(char c) { return c == '#' || c == 'A' || c == 'a' || c == 'B' || c == '*'; }
    int Slots() const { int n = 0; for (char c : tmpl) if (IsSlot(c)) ++n; return n; }
    int Selected() const { return Sel; }
    // geometry (public: the self-test aims at it)
    int ChipX(int i) const { return X + 16 + (i % PER_ROW) * (CHIP_W + 2); }
    int ChipY(int i) const { return Y + 44 + (i / PER_ROW) * (CHIP_H + 4); }
    int TypeBtnX(int k) const { return X + 16 + 80 + k * 112; }          // # A a B *
    int TypeBtnY() const { return Y + 262; }
    int LitX(int k) const { return X + 16 + 80 + k * 44; }
    int LitY() const { return Y + 306; }
    int OpX(int k) const { return X + 16 + 80 + k * 136; }                // Insert, Delete, Move left, Move right
    int OpY() const { return Y + 350; }
    int BtnY() const { return Y + H - 16 - BtnH; }
    int OkX() const { return X + W - 16 - 90; }
    int CancelX() const { return OkX() - 10 - 90; }
    static const char* LiteralChars() { return "-/:.,()+@_ "; }

    void Event(const InputEvent& e) {
        if (e.type == InputEvent::DOWN && e.button == SDL_BUTTON_LEFT) Down(e);
        else if (e.type == InputEvent::KEY_DOWN) Key(e);
        else if (e.type == InputEvent::TEXT) for (const char* p = e.text; *p; ++p) Typed(*p);
    }

    void Draw() {
        lgfx::LGFX_Sprite& g = *G;
        g.fillRectAlpha(0, 0, FD::WIN_W, FD::WIN_H, 150, 0x0000);
        g.fillRect(X, Y, W, H, COL_PANEL); g.drawRect(X, Y, W, H, COL_ACCENT); g.drawRect(X + 1, Y + 1, W - 2, H - 2, COL_ACCENT);
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left);
        g.setTextColor(COL_ACCENT); g.drawString(Title.c_str(), X + 16, Y + 12);
        int n = (int)tmpl.size();
        for (int i = 0; i <= n && i < MAX_CELLS; ++i) DrawChip(i);
        int chipsBottom = ChipY(std::min(n, MAX_CELLS - 1)) + CHIP_H;
        // template text and the real control
        g.setTextDatum(lgfx::textdatum_t::top_left);
        g.setTextColor(COL_DIM); g.drawString("Template:", X + 16, chipsBottom + 14);
        g.setFont(&lgfx::fonts::Font4); g.setTextColor(0xFFFF); g.drawString(tmpl.empty() ? "(empty)" : tmpl.c_str(), X + 16 + 90, chipsBottom + 8);
        g.setFont(&lgfx::fonts::Font2);
        int py = chipsBottom + 44;
        g.fillRect(X + 16, py, W - 32, 62, FormBg); g.drawRect(X + 16, py, W - 32, 62, COL_LINE);
        if (Slots() > 0) {
            DControl t = Proto; FD::ExtraSet(t, *FindExtra(C_INPUTBOX, "template"), tmpl); FD::ExtraSet(t, *FindExtra(C_INPUTBOX, "value"), SampleValueFor(tmpl));
            t.x = 0; t.y = 0; t.w = std::min(W - 56, FitWidth(&g, t));
            g.setClipRect(X + 17, py + 1, W - 34, 60); auto ctl = MakeControl(&g, t, X + 28, py + 10); ctl->Push(); g.clearClipRect();
        } else { g.setTextColor(COL_ERR); g.drawString("a template needs at least one slot (# A a B *)", X + 28, py + 22); }
        // buttons
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left); g.setTextColor(0xFFFF);
        g.drawString("Slot type:", X + 16, TypeBtnY() + 4); g.drawString("Literal:", X + 16, LitY() + 4); g.drawString("Cells:", X + 16, OpY() + 4);
        static const char* tl[5] = { "#  digit", "A  upper", "a  lower", "B  letter", "*  any" };
        for (int k = 0; k < 5; ++k) { SuperPushButton b(G, TypeBtnX(k), TypeBtnY(), 106, &lgfx::fonts::Font2, tl[k]); b.Push(); }
        for (int k = 0; LiteralChars()[k]; ++k) { char s[2] = { LiteralChars()[k], 0 }; SuperPushButton b(G, LitX(k), LitY(), 38, &lgfx::fonts::Font2, LiteralChars()[k] == ' ' ? "sp" : s); b.Push(); }
        static const char* ol[4] = { "Insert", "Delete", "Move left", "Move right" };
        for (int k = 0; k < 4; ++k) { SuperPushButton b(G, OpX(k), OpY(), 128, &lgfx::fonts::Font2, ol[k]); b.Push(); }
        g.setTextColor(COL_DIM);
        g.drawString("Click a cell, then pick a type or a literal. Or just type: # A a B * make slots, any other character a literal.", X + 16, OpY() + 40);
        g.drawString("Left / Right select, Insert adds a digit cell, Delete removes, Enter = OK, Esc = Cancel.", X + 16, OpY() + 58);
        if (!Err.empty()) { g.setTextColor(COL_ERR); g.drawString(Err.c_str(), X + 16, BtnY() + 4); }
        SuperPushButton cancel(G, CancelX(), BtnY(), 90, &lgfx::fonts::Font2, "Cancel"), ok(G, OkX(), BtnY(), 90, &lgfx::fonts::Font2, "OK");
        ok.ColBorder = COL_ACCENT; cancel.Push(); ok.Push();
    }

private:
    lgfx::LGFX_Sprite* G; std::string Title; DControl Proto; uint16_t FormBg;
    int Sel = 0, BtnH = 20; std::string Err;

    static uint16_t SlotColor(char c) { return c == '#' ? 0x2B5F : c == 'A' ? 0x0420 : c == 'a' ? 0x0410 : c == 'B' ? 0x6010 : 0x8200; }
    void DrawChip(int i) {
        lgfx::LGFX_Sprite& g = *G; int x = ChipX(i), y = ChipY(i), n = (int)tmpl.size();
        if (i == n) {                                         // the "+" cell: append here
            g.drawRect(x, y, CHIP_W, CHIP_H, i == Sel ? COL_ACCENT : COL_LINE); g.setTextDatum(lgfx::textdatum_t::middle_center);
            g.setTextColor(i == Sel ? COL_ACCENT : COL_DIM); g.drawString("+", x + CHIP_W / 2, y + CHIP_H / 2); g.setTextDatum(lgfx::textdatum_t::top_left); return;
        }
        char c = tmpl[i]; bool slot = IsSlot(c);
        g.fillRect(x, y, CHIP_W, CHIP_H, slot ? SlotColor(c) : 0x0000);
        g.drawRect(x, y, CHIP_W, CHIP_H, i == Sel ? COL_ACCENT : COL_LINE); if (i == Sel) g.drawRect(x + 1, y + 1, CHIP_W - 2, CHIP_H - 2, COL_ACCENT);
        g.setTextDatum(lgfx::textdatum_t::middle_center); g.setFont(&lgfx::fonts::Font4);
        g.setTextColor(slot ? 0xFFFF : 0xBDF7); char s[2] = { c == ' ' ? '_' : c, 0 }; g.drawString(s, x + CHIP_W / 2, y + CHIP_H / 2);
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left);
    }
    void Clamp() { Sel = std::max(0, std::min(Sel, (int)tmpl.size())); }
    void SetCell(char c) {                                    // the selected cell becomes c (or c is appended on the "+" cell)
        Err.clear();
        if (Sel >= (int)tmpl.size()) { if ((int)tmpl.size() >= MAX_CELLS) { Err = "a template has at most 40 cells"; return; } tmpl.push_back(c); }
        else tmpl[Sel] = c;
    }
    void Typed(char c) { if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7E) return; SetCell(c); ++Sel; Clamp(); }
    void Insert() { if ((int)tmpl.size() >= MAX_CELLS) { Err = "a template has at most 40 cells"; return; } Clamp(); tmpl.insert(tmpl.begin() + Sel, '#'); Err.clear(); }
    void Delete() { if (Sel < (int)tmpl.size()) { tmpl.erase(tmpl.begin() + Sel); Err.clear(); } Clamp(); }
    void Move(int d) { int j = Sel + d; if (Sel < (int)tmpl.size() && j >= 0 && j < (int)tmpl.size()) { std::swap(tmpl[Sel], tmpl[j]); Sel = j; } }
    void Accept() { if (Slots() == 0) { Err = "add at least one slot (# A a B *) first"; return; } state = OK; }
    void Down(const InputEvent& e) {
        auto in = [&](int x, int y, int w, int h) { return e.x >= x && e.x < x + w && e.y >= y && e.y < y + h; };
        for (int i = 0; i <= (int)tmpl.size() && i < MAX_CELLS; ++i) if (in(ChipX(i), ChipY(i), CHIP_W, CHIP_H)) { Sel = i; Err.clear(); return; }
        static const char slots[5] = { '#', 'A', 'a', 'B', '*' };
        for (int k = 0; k < 5; ++k) if (in(TypeBtnX(k), TypeBtnY(), 106, BtnH)) { SetCell(slots[k]); return; }
        for (int k = 0; LiteralChars()[k]; ++k) if (in(LitX(k), LitY(), 38, BtnH)) { SetCell(LiteralChars()[k]); return; }
        if (in(OpX(0), OpY(), 128, BtnH)) { Insert(); return; }
        if (in(OpX(1), OpY(), 128, BtnH)) { Delete(); return; }
        if (in(OpX(2), OpY(), 128, BtnH)) { Move(-1); return; }
        if (in(OpX(3), OpY(), 128, BtnH)) { Move(1); return; }
        if (in(OkX(), BtnY(), 90, BtnH)) { Accept(); return; }
        if (in(CancelX(), BtnY(), 90, BtnH)) { state = CANCEL; return; }
    }
    void Key(const InputEvent& e) {
        switch (e.key) {
        case SDLK_RETURN: case SDLK_KP_ENTER: Accept(); break;
        case SDLK_ESCAPE: state = CANCEL; break;
        case SDLK_LEFT: --Sel; Clamp(); break;
        case SDLK_RIGHT: ++Sel; Clamp(); break;
        case SDLK_HOME: Sel = 0; break;
        case SDLK_END: Sel = (int)tmpl.size(); break;
        case SDLK_INSERT: Insert(); break;
        case SDLK_DELETE: Delete(); break;
        case SDLK_BACKSPACE: if (Sel > 0) { --Sel; Delete(); } break;
        default: break;
        }
    }
};
}
