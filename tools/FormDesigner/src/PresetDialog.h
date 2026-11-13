#pragma once
// A modal list of common input templates for the Form Designer (SuperInputBox), drawn inside the designer window.
//   list (the real SuperListBox)  |  preview: the control itself with that template and a sample value, in its own font and colours
// Click a row (double-click = OK), Up / Down / PageUp / PageDown / Home / End, wheel, Enter = OK, Esc = Cancel.
#include <memory>
#include <LovyanGFX.hpp>
#include <SuperListBox.h>
#include <SuperPushButton.h>
#include "DesignerInput.h"
#include "DesignerWidgets.h"
#include "DesignerControls.h"

namespace FD {
struct TemplatePreset { const char* name; const char* tmpl; };
inline const std::vector<TemplatePreset>& TemplatePresets() {
    static const std::vector<TemplatePreset> v = {
        { "Phone (US)", "###-###-####" }, { "Phone with area code", "(###) ###-####" }, { "ZIP code", "#####" }, { "ZIP+4", "#####-####" },
        { "Date (US)", "##/##/####" }, { "Date (ISO)", "####-##-##" }, { "Time", "##:##" }, { "Time with seconds", "##:##:##" },
        { "IPv4 address", "###.###.###.###" }, { "MAC address", "**:**:**:**:**:**" }, { "Social security number", "###-##-####" },
        { "Credit card", "#### #### #### ####" }, { "Licence plate", "AAA-####" }, { "Initials", "AAA" }, { "PIN", "####" }, { "Product code", "AA-####-#" },
        { "Name (letters)", "BBBBBBBBBBBBBBBB" }, { "Name (capitals)", "AAAAAAAAAAAAAAAA" }, { "Any text, 16 characters", "****************" },
    };
    return v;
}
// A sample value that fits a template (digits, capitals, ...), for previews.
inline std::string SampleValueFor(const std::string& t) {
    std::string v; int d = 1;
    for (char c : t) switch (c) {
        case '#': v += (char)('0' + (d++ % 10)); break;
        case 'A': v += (char)('A' + (v.size() % 26)); break;
        case 'a': v += (char)('a' + (v.size() % 26)); break;
        case 'B': v += (v.size() % 2 ? 'x' : 'X'); break;
        case '*': v += (char)('a' + (v.size() % 26)); break;
        default: break;
    }
    return v;
}

class PresetDialog {
public:
    enum State { OPEN, OK, CANCEL } state = OPEN;
    std::string tmpl;                                         // the highlighted template (the result when state == OK)

    PresetDialog(lgfx::LGFX_Sprite* g, const std::string& title, const DControl& proto, uint16_t formBg) : G(g), Title(title), Proto(proto), FormBg(formBg) {
        std::vector<std::string> items; int cur = 0, i = 0;
        for (const TemplatePreset& p : TemplatePresets()) { items.push_back(std::string(p.name) + "   " + p.tmpl); if (XStr(proto, "template") == p.tmpl) cur = i; ++i; }
        List.reset(new SuperListBox(G, X + 16, Y + 44, LIST_W, &lgfx::fonts::Font2, ROWS, nullptr, nullptr, nullptr));
        List->SetItems(items); List->SetSelected(cur); List->StartEdit();
        tmpl = TemplatePresets()[List->GetCursor()].tmpl;
        SuperPushButton ok(G, 0, 0, 90, &lgfx::fonts::Font2, "OK"); BtnH = ok.GetHeight();
    }

    static constexpr int W = 760, H = 430, X = (FD::WIN_W - W) / 2, Y = (FD::WIN_H - H) / 2, LIST_W = 340, ROWS = 14;
    int PreviewX() const { return X + 16 + LIST_W + 16; }
    int PreviewW() const { return W - 16 - (16 + LIST_W + 16); }
    int BtnY() const { return Y + H - 16 - BtnH; }
    int OkX() const { return X + W - 16 - 90; }
    int CancelX() const { return OkX() - 10 - 90; }
    SuperListBox* Lb() { return List.get(); }

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
        g.setTextColor(COL_ACCENT); g.drawString(Title.c_str(), X + 16, Y + 12);
        List->Push();
        int px = PreviewX(), py = Y + 44, pw = PreviewW(), ph = List->GetHeight();
        g.fillRect(px, py, pw, ph, FormBg); g.drawRect(px, py, pw, ph, COL_LINE);
        g.setClipRect(px + 1, py + 1, pw - 2, ph - 2);
        DControl t = Proto;                                       // the control itself, with this template and a sample value that fits it
        FD::ExtraSet(t, *FindExtra(C_INPUTBOX, "template"), tmpl); FD::ExtraSet(t, *FindExtra(C_INPUTBOX, "value"), SampleValueFor(tmpl));
        t.x = 0; t.y = 0; t.w = std::min(pw - 24, FitWidth(&g, t));
        auto ctl = MakeControl(&g, t, px + 12, py + 14); ctl->Push();
        g.setFont(&lgfx::fonts::Font2); g.setTextColor(COL_DIM); g.setTextDatum(lgfx::textdatum_t::top_left);
        int y = py + 14 + ctl->GetHeight() + 20;
        g.drawString("Template", px + 12, y); g.setTextColor(0xFFFF); g.drawString(tmpl.c_str(), px + 12, y + 18);
        g.setTextColor(COL_DIM); g.drawString("# digit   A upper   a lower   B any letter", px + 12, y + 56);
        g.drawString("* any character   everything else is a literal", px + 12, y + 74);
        g.clearClipRect();
        SuperPushButton cancel(G, CancelX(), BtnY(), 90, &lgfx::fonts::Font2, "Cancel"), ok(G, OkX(), BtnY(), 90, &lgfx::fonts::Font2, "OK");
        ok.ColBorder = COL_ACCENT; cancel.Push(); ok.Push();
    }

private:
    lgfx::LGFX_Sprite* G; std::string Title; DControl Proto; uint16_t FormBg;
    std::unique_ptr<SuperListBox> List;
    bool Drag = false, Started = false; int PressX = 0, PressY = 0, BtnH = 20;
    void Pump() {
        SuperEditState s = List->Service();
        if (s == EDIT_ACCEPTED || s == EDIT_CANCELLED) List->StartEdit();
        tmpl = TemplatePresets()[List->GetCursor()].tmpl;
    }
    void Pointer(SuperPointerEvent::Kind k, int x, int y) {
        SuperPointerEvent p; p.Type = k; p.X = (int16_t)x; p.Y = (int16_t)y; p.StartX = (int16_t)PressX; p.StartY = (int16_t)PressY;
        List->OnPointer(p); Pump();
    }
    void Down(const InputEvent& e) {
        if (List->RowAt(e.x, e.y) >= 0) { Drag = true; Started = false; PressX = e.x; PressY = e.y; Pointer(SuperPointerEvent::PRESS, e.x, e.y); if (e.clicks >= 2) state = OK; return; }
        if (e.y >= BtnY() && e.y < BtnY() + BtnH) {
            if (e.x >= OkX() && e.x < OkX() + 90) state = OK; else if (e.x >= CancelX() && e.x < CancelX() + 90) state = CANCEL;
        }
    }
    void Key(const InputEvent& e) {
        SuperEditIO* io = List->EditIO();
        switch (e.key) {
        case SDLK_RETURN: case SDLK_KP_ENTER: state = OK; break;
        case SDLK_ESCAPE: state = CANCEL; break;
        case SDLK_UP: io->InjectDelta(-1); Pump(); break;
        case SDLK_DOWN: io->InjectDelta(1); Pump(); break;
        case SDLK_PAGEUP: io->InjectDelta(-(ROWS - 1)); Pump(); break;
        case SDLK_PAGEDOWN: io->InjectDelta(ROWS - 1); Pump(); break;
        case SDLK_HOME: io->InjectDelta(-100000); Pump(); break;
        case SDLK_END: io->InjectDelta(100000); Pump(); break;
        default: break;
        }
    }
};
}
