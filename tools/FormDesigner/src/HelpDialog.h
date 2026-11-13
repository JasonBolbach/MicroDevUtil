#pragma once
// The "?" help pop-up of the Form Designer: a centred header, then the keyboard commands as rich text (the library's SuperRichText), an OK button.
// It is also the demo and the test of SuperRichText. While HELP_SOURCE_TOGGLE is true (main.cpp) a "Show source" check box switches the text to
// its raw markup for debugging; remove the flag when the control is trusted.
//
// Keys: Esc / Enter / ? / F1 = close; Up / Down / PageUp / PageDown / Home / End and the wheel scroll.
#include <memory>
#include <LovyanGFX.hpp>
#include <SuperPushButton.h>
#include <SuperRichText.h>
#include "DesignerInput.h"
#include "DesignerWidgets.h"

namespace FD {
class HelpDialog {
public:
    bool open = true;

    HelpDialog(lgfx::LGFX_Sprite* g, bool sourceToggle) : G(g), Toggle(sourceToggle) {
        Head.reset(new SuperRichText(G, X + 16, Y + 12, W - 32, HEAD_H, &lgfx::fonts::Font2, nullptr, nullptr, nullptr));
        Head->SetBorder(0, 0); Head->SetPadding(0); Head->ColBg = COL_PANEL; Head->SetTextStatic(HeaderText());
        Body.reset(new SuperRichText(G, X + 16, Y + 16 + HEAD_H, W - 32, BODY_H, &lgfx::fonts::Font2, nullptr, nullptr, nullptr));
        Body->SetBorder(1, COL_LINE); Body->SetPadding(6); Body->ColBg = 0x0000;
        lgfx::LGFX_Sprite probe(G); probe.setFont(&lgfx::fonts::Font2); Body->SetTabChars(std::max(1, 230 / std::max(1, (int)probe.textWidth(" "))));
        Body->SetTextStatic(BodyText());
        SuperPushButton ok(G, 0, 0, 90, &lgfx::fonts::Font2, "OK"); BtnH = ok.GetHeight();
    }

    static constexpr int W = 760, H = 640, X = (FD::WIN_W - W) / 2, Y = (FD::WIN_H - H) / 2, HEAD_H = 92, BODY_H = 440;
    int OkX() const { return X + W - 16 - 90; }
    int BtnY() const { return Y + H - 16 - BtnH; }
    int ToggleX() const { return X + 16; }
    SuperRichText* BodyCtl() { return Body.get(); }
    SuperRichText* HeadCtl() { return Head.get(); }
    bool Source() const { return Src; }

    void Event(const InputEvent& e) {
        switch (e.type) {
        case InputEvent::DOWN:
            if (e.button != SDL_BUTTON_LEFT) break;
            if (e.x >= OkX() && e.x < OkX() + 90 && e.y >= BtnY() && e.y < BtnY() + BtnH) open = false;
            else if (Toggle && e.x >= ToggleX() && e.x < ToggleX() + 150 && e.y >= BtnY() && e.y < BtnY() + BtnH) { Src = !Src; Body->SetShowSource(Src); }
            break;
        case InputEvent::WHEEL: if (e.x >= X && e.x < X + W && e.y >= Y && e.y < Y + H) Body->ScrollBy(-e.wheel * 2); break;
        case InputEvent::KEY_DOWN:
            switch (e.key) {
            case SDLK_ESCAPE: case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_QUESTION: case SDLK_F1: open = false; break;
            case SDLK_SLASH: if (e.mod & KMOD_SHIFT) open = false; break;
            case SDLK_UP: Body->ScrollBy(-1); break;
            case SDLK_DOWN: Body->ScrollBy(1); break;
            case SDLK_PAGEUP: Body->ScrollBy(-12); break;
            case SDLK_PAGEDOWN: Body->ScrollBy(12); break;
            case SDLK_HOME: Body->ScrollTo(0); break;
            case SDLK_END: Body->ScrollTo(100000); break;
            default: break;
            }
            break;
        default: break;
        }
    }

    void Draw(uint32_t) {
        lgfx::LGFX_Sprite& g = *G;
        g.fillRectAlpha(0, 0, FD::WIN_W, FD::WIN_H, 150, 0x0000);
        g.fillRect(X, Y, W, H, COL_PANEL); g.drawRect(X, Y, W, H, COL_ACCENT); g.drawRect(X + 1, Y + 1, W - 2, H - 2, COL_ACCENT);
        Head->Push(); Body->Push();
        if (Toggle) {                                               // debugging aid: the markup itself
            int bx = ToggleX(), by = BtnY();
            g.drawRect(bx, by + 3, 14, 14, 0xFFFF); if (Src) { g.drawLine(bx + 3, by + 10, bx + 6, by + 14, 0xFFFF); g.drawLine(bx + 6, by + 14, bx + 11, by + 6, 0xFFFF); }
            g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left); g.setTextColor(0xFFFF); g.drawString("Show source", bx + 22, by + 1);
        }
        SuperPushButton ok(G, OkX(), BtnY(), 90, &lgfx::fonts::Font2, "OK"); ok.ColBorder = COL_ACCENT; ok.Push();
    }

    static const char* HeaderText() {
        return "<center><color=yellow><big>SuperGUI Form Designer</big></color>\n"
               "Part of the SuperGUI HMI Suite\n"
               "<color=gray>Jason Bolbach 2025-2026\n"
               "With Assistance from Claude</color></center>";
    }
    static const char* BodyText() {
        return
        "<color=cyan>Files</color>\n"
        "Ctrl/Cmd + O<tab>Open a form (file dialog)\n"
        "Ctrl/Cmd + S<tab>Save (file dialog; a new name becomes the form's file)\n"
        "Ctrl/Cmd + X<tab>Export the code: choose the output folder\n"
        "<hr>\n"
        "<color=cyan>Placing and selecting</color>\n"
        "Click a toolbox item,<br>then click the form<tab>Place a control\n"
        "Click a control<tab>Select it (click empty space: nothing)\n"
        "Drag a control<tab>Move it (snaps to the grid)\n"
        "Drag a side handle<tab>Change its width\n"
        "Drag a top / bottom handle<tab>Change the height of a list, menu, gauge, graph,<br>user draw or rich text (lists, menus and rich text<br>snap to whole rows / lines); or type the Height row\n"
        "Arrows<tab>Nudge 1 pixel (Shift: one grid step)\n"
        "Tab<tab>Select the next control\n"
        "Up / Down / Front / Back<tab>Buttons above the controls list: change the draw order<br>(the last control is in front; the tab order is not touched)\n"
        "Tab order row<tab>The number (1, 2, 3 ...) of a control in the encoder's focus order;<br>set when the control is added, then only you change it\n"
        "Delete / Backspace<tab>Delete the selected control\n"
        "Esc<tab>Cancel placing, or select nothing\n"
        "G<tab>Change the grid step\n"
        "<hr>\n"
        "<color=cyan>View</color>\n"
        "+  -  0  F<tab>Zoom in, zoom out, 1:1, fit\n"
        "Ctrl/Cmd + wheel<tab>Zoom around the mouse\n"
        "Wheel / Shift + wheel<tab>Scroll up and down / sideways\n"
        "<hr>\n"
        "<color=cyan>Properties and text fields</color>\n"
        "Click a row<tab>Edit it; Enter or Esc leaves the field\n"
        "Tab / Shift + Tab<tab>Next / previous field\n"
        "Ctrl/Cmd + A C X V<tab>Select all, copy, cut, paste\n"
        "Ctrl/Cmd + Z, Shift + Z<tab>Undo, redo\n"
        "Alt + arrows<tab>Move by word; Shift selects\n"
        "A refused value turns the field <color=red>red</color> and the reason is shown.\n"
        "<hr>\n"
        "<color=cyan>Dialogs</color>\n"
        "Enter<tab>OK\n"
        "Esc<tab>Cancel (Esc inside a field: back to the list)\n"
        "Tab<tab>Move between the list and the fields\n"
        "Message boxes: select nothing, then Edit... on the Message boxes row.\n"
        "<hr>\n"
        "<color=cyan>Help</color>\n"
        "?  or  F1<tab>This window (Esc, Enter or OK closes it)\n"
        "Up / Down, PageUp / PageDown,<br>Home / End, wheel<tab>Scroll this text\n";
    }

private:
    lgfx::LGFX_Sprite* G; bool Toggle, Src = false; int BtnH = 20;
    std::unique_ptr<SuperRichText> Head, Body;
};
}
