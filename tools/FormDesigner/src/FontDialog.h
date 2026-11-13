#pragma once
// A modal font dialog for the Form Designer, drawn inside the designer window (no OS dialogs).
//
//   +------------------------+--------------------------------------+
//   | the real SuperListBox  |  preview: the control itself with    |
//   | of fonts               |  this font, then lorem ipsum, in     |
//   |                        |  the control's colours               |
//   +------------------------+--------------------------------------+
//   | Font2   code name   height 16 px                [Cancel] [ OK ]|
//
// A font's size is part of the font (Font4 = 26 px, FreeSans 18pt ...), so picking a font picks the size.
// Mouse: click a row (double-click = OK), drag, wheel.  Keys: Up / Down / PageUp / PageDown / Home / End, Enter = OK, Esc = Cancel.
#include <memory>
#include <LovyanGFX.hpp>
#include <SuperListBox.h>
#include <SuperPushButton.h>
#include "DesignerInput.h"
#include "DesignerWidgets.h"
#include "DesignerControls.h"
#include "FontTable.h"

namespace FD {
class FontDialog {
public:
    enum State { OPEN, OK, CANCEL } state = OPEN;
    std::string font;                                         // the chosen font id (valid when state == OK; the highlighted one while open)

    // title: what is being edited; proto: the control the font is for (its type, text, colours are used in the preview); formBg: its backdrop
    FontDialog(lgfx::LGFX_Sprite* g, const std::string& title, const DControl& proto, uint16_t formBg)
        : G(g), Title(title), Proto(proto), FormBg(formBg) {
        std::vector<std::string> items;
        for (const FontInfo& f : Fonts()) items.push_back(f.label);
        List.reset(new SuperListBox(G, X + 16, Y + 44, LIST_W, &lgfx::fonts::Font2, ROWS, nullptr, nullptr, nullptr));
        List->SetItems(items);
        List->SetSelected(std::max(0, FontIndex(proto.font)));
        List->StartEdit();
        font = Fonts()[List->GetCursor()].id;
        SuperPushButton ok(G, 0, 0, 90, &lgfx::fonts::Font2, "OK"); BtnH = ok.GetHeight();
    }

    static constexpr int W = 780, H = 440, X = (FD::WIN_W - W) / 2, Y = (FD::WIN_H - H) / 2;
    static constexpr int LIST_W = 280, ROWS = 18;
    int PreviewX() const { return X + 16 + LIST_W + 16; }
    int PreviewW() const { return W - 16 - (16 + LIST_W + 16); }
    int PreviewY() const { return Y + 44; }
    int PreviewH() const { return List->GetHeight(); }
    int BtnY() const { return Y + H - 16 - BtnH; }
    int OkX() const { return X + W - 16 - 90; }
    int CancelX() const { return OkX() - 10 - 90; }
    SuperListBox* Lb() { return List.get(); }                 // (for the self-test)

    void Event(const InputEvent& e) {
        switch (e.type) {
        case InputEvent::DOWN: if (e.button == SDL_BUTTON_LEFT) Down(e); break;
        case InputEvent::MOVE: if (Drag) { Pointer(Started ? SuperPointerEvent::DRAG_MOVE : SuperPointerEvent::DRAG_START, e.x, e.y); Started = true; } break;
        case InputEvent::UP:   if (Drag) { Pointer(SuperPointerEvent::DRAG_END, e.x, e.y); Drag = false; } break;
        case InputEvent::WHEEL: if (e.x >= X && e.x < X + W && e.y >= Y && e.y < Y + H) { List->EditIO()->InjectDelta(-e.wheel * 3); Pump(); } break;
        case InputEvent::KEY_DOWN: Key(e); break;
        default: break;
        }
    }

    void Draw() {
        lgfx::LGFX_Sprite& g = *G;
        g.fillRectAlpha(0, 0, FD::WIN_W, FD::WIN_H, 150, 0x0000);
        g.fillRect(X, Y, W, H, COL_PANEL); g.drawRect(X, Y, W, H, COL_ACCENT); g.drawRect(X + 1, Y + 1, W - 2, H - 2, COL_ACCENT);
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left);
        g.setTextColor(COL_ACCENT); g.drawString(("Font: " + Title).c_str(), X + 16, Y + 12);
        List->Push();
        // preview panel: the control itself in this font, then lorem ipsum, in the control's colours on the form's back colour
        const FontInfo& fi = FindFont(font);
        int px = PreviewX(), py = PreviewY(), pw = PreviewW(), ph = PreviewH();
        g.fillRect(px, py, pw, ph, FormBg); g.drawRect(px, py, pw, ph, COL_LINE);
        g.setClipRect(px + 1, py + 1, pw - 2, ph - 2);
        DControl t = Proto; t.font = font; t.x = 0; t.y = 0; t.w = std::min(Proto.w, pw - 24);
        auto ctl = MakeControl(&g, t, px + 12, py + 12);
        int ch = ctl->GetHeight();
        ctl->Push();
        g.setFont(fi.font); g.setTextDatum(lgfx::textdatum_t::top_left); g.setTextColor(Proto.fg);
        int y = py + 12 + ch + 14, fh = g.fontHeight();
        const char* lines[] = { "Lorem ipsum dolor sit amet,", "consectetur adipiscing elit,", "sed do eiusmod tempor", "incididunt ut labore 0123456789" };
        const char* digits[] = { "0123456789", "12:45.30", "-9.87", "" };
        for (int i = 0; i < 4 && y < py + ph; ++i) { g.drawString(fi.digitsOnly ? digits[i] : lines[i], px + 12, y); y += fh + 4; }
        g.clearClipRect();
        char b[120];
        snprintf(b, sizeof b, "%s     height %d px%s", fi.id, fh, fi.digitsOnly ? "     digits and : . - only" : "");
        g.setFont(&lgfx::fonts::Font2); g.setTextColor(0xC618); g.drawString(b, X + 16, BtnY() + 4);
        SuperPushButton cancel(G, CancelX(), BtnY(), 90, &lgfx::fonts::Font2, "Cancel");
        SuperPushButton ok(G, OkX(), BtnY(), 90, &lgfx::fonts::Font2, "OK");
        ok.ColBorder = COL_ACCENT; cancel.Push(); ok.Push();
    }

private:
    lgfx::LGFX_Sprite* G; std::string Title; DControl Proto; uint16_t FormBg;
    std::unique_ptr<SuperListBox> List;
    bool Drag = false, Started = false; int PressX = 0, PressY = 0, BtnH = 20;

    void Pump() {                                             // run the list box; a lift "accepts" a row, so keep editing afterwards
        SuperEditState s = List->Service();
        if (s == EDIT_ACCEPTED || s == EDIT_CANCELLED) List->StartEdit();
        font = Fonts()[List->GetCursor()].id;
    }
    void Pointer(SuperPointerEvent::Kind k, int x, int y) {
        SuperPointerEvent p; p.Type = k; p.X = (int16_t)x; p.Y = (int16_t)y; p.StartX = (int16_t)PressX; p.StartY = (int16_t)PressY;
        List->OnPointer(p); Pump();
    }
    void Down(const InputEvent& e) {
        if (List->RowAt(e.x, e.y) >= 0) {
            Drag = true; Started = false; PressX = e.x; PressY = e.y;
            Pointer(SuperPointerEvent::PRESS, e.x, e.y);
            if (e.clicks >= 2) state = OK;                    // double click
            return;
        }
        if (e.y >= BtnY() && e.y < BtnY() + BtnH) {
            if (e.x >= OkX() && e.x < OkX() + 90) state = OK;
            else if (e.x >= CancelX() && e.x < CancelX() + 90) state = CANCEL;
        }
    }
    void Key(const InputEvent& e) {
        SuperEditIO* io = List->EditIO();
        switch (e.key) {
        case SDLK_RETURN: case SDLK_KP_ENTER: state = OK; break;
        case SDLK_ESCAPE: state = CANCEL; break;
        case SDLK_UP:       io->InjectDelta(-1); Pump(); break;
        case SDLK_DOWN:     io->InjectDelta(1); Pump(); break;
        case SDLK_PAGEUP:   io->InjectDelta(-(ROWS - 1)); Pump(); break;
        case SDLK_PAGEDOWN: io->InjectDelta(ROWS - 1); Pump(); break;
        case SDLK_HOME:     io->InjectDelta(-100000); Pump(); break;
        case SDLK_END:      io->InjectDelta(100000); Pump(); break;
        default: break;
        }
    }
};
}
