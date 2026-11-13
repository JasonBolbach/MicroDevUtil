#pragma once
// A modal colour dialog for the Form Designer, drawn inside the designer window (no OS dialogs).
//
//   +--------------------------------------------------------------+
//   |  the real SuperColorPicker (H / S / V bars + swatch)         |
//   |  palette of common colours        |  Old / New swatches       |
//   |  R [ ]  G [ ]  B [ ]  # [      ]    RGB565 0x....            |
//   |  sample text in the colour (on the partner colour)           |
//   |                                         [Cancel]  [  OK  ]    |
//   +--------------------------------------------------------------+
//
// Mouse: drag the picker bars, click a palette cell, click "Old" to go back, type R / G / B / hex.
// Keys:  Left / Right = change the picker's active bar (Shift = x10), Tab / Down = next bar, Up = previous bar, Enter = OK, Esc = Cancel.
// The colour is RGB565 (what the SuperGUI controls draw); R, G, B and the hex show it expanded to 8 bits per channel.
#include <memory>
#include <LovyanGFX.hpp>
#include <SuperColorPicker.h>
#include <SuperPushButton.h>
#include "DesignerInput.h"
#include "DesignerTextField.h"
#include "DesignerWidgets.h"

namespace FD {
class ColorDialog {
public:
    enum State { OPEN, OK, CANCEL } state = OPEN;
    uint16_t color = 0;                                       // the colour chosen so far (RGB565)

    // title: what is being coloured; initial: its colour now; partner: the colour it is shown against; isText: initial is a text colour
    // (the sample then draws text in `color` on `partner`), otherwise a background (sample text in `partner` on `color`).
    ColorDialog(lgfx::LGFX_Sprite* g, const std::string& title, uint16_t initial, uint16_t partner, bool isText)
        : G(g), Title(title), Old(initial), Partner(partner), IsText(isText) {
        Picker.reset(new SuperColorPicker(G, X + 16, Y + 40, W - 32, &lgfx::fonts::Font4, nullptr, nullptr, nullptr));
        Picker->SetColor565(initial);
        Picker->StartEdit();                                  // stays in edit mode for the dialog's whole life: pointer / keys edit the live value
        for (int i = 0; i < 4; ++i) { Fld[i].filter = i < 3 ? TextField::DIGITS : TextField::HEX; Fld[i].maxLen = i < 3 ? 3 : 6; }
        SetColor(initial);
        SuperPushButton ok(G, 0, 0, 90, &lgfx::fonts::Font2, "OK");
        BtnH = ok.GetHeight();
    }

    // ---- geometry (public: the self-test aims at it) ----
    static constexpr int W = 620, H = 470, X = (FD::WIN_W - W) / 2, Y = (FD::WIN_H - H) / 2;
    static constexpr int CELL = 26, GAP = 2, COLS = 16, ROWS = 4;
    int PalX() const { return X + 16; }
    int PalY() const { return Y + 164; }
    int OldX() const { return X + 490; }
    int FieldX(int i) const { static const int fx[4] = { 34, 118, 202, 284 }; return X + fx[i]; }
    int FieldW(int i) const { return i < 3 ? 52 : 90; }
    int FieldY() const { return Y + 296; }
    const std::string& FieldText(int i) const { return Fld[i].text; }     // (for the self-test)
    SuperColorPicker* Pk() { return Picker.get(); }
    int BtnY() const { return Y + H - 16 - BtnH; }
    int OkX() const { return X + W - 16 - 90; }
    int CancelX() const { return OkX() - 10 - 90; }
    static uint16_t Palette(int col, int row) {               // row 0 greys, rows 1..3 hues: vivid, pastel, dark
        if (row == 0) { int v = col * 255 / (COLS - 1); return To565(v, v, v); }
        int s = row == 2 ? 55 : 100, v = row == 3 ? 55 : 100;
        uint8_t r, g, b; HsvToRgb(col * 360 / COLS, s, v, r, g, b); return To565(r, g, b);
    }

    void Event(const InputEvent& e) {
        switch (e.type) {
        case InputEvent::DOWN: if (e.button == SDL_BUTTON_LEFT) Down(e); break;
        case InputEvent::MOVE: Move(e); break;
        case InputEvent::UP:   Drag = D_NONE; break;
        case InputEvent::KEY_DOWN: Key(e); break;
        case InputEvent::TEXT: if (Focus >= 0) { Fld[Focus].Insert(e.text); Edited(Focus); Reveal(Focus); } break;
        default: break;
        }
    }

    void Draw(uint32_t ticks) {
        lgfx::LGFX_Sprite& g = *G;
        g.fillRectAlpha(0, 0, FD::WIN_W, FD::WIN_H, 150, 0x0000);                          // dim the designer behind the dialog
        g.fillRect(X, Y, W, H, COL_PANEL); g.drawRect(X, Y, W, H, COL_ACCENT); g.drawRect(X + 1, Y + 1, W - 2, H - 2, COL_ACCENT);
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left);
        g.setTextColor(COL_ACCENT); g.drawString(("Color: " + Title).c_str(), X + 16, Y + 12);
        Picker->Push();
        g.setTextColor(COL_ACCENT); g.drawString("Common colors", PalX(), PalY() - 20);
        for (int r = 0; r < ROWS; ++r) for (int c = 0; c < COLS; ++c) {
            uint16_t col = Palette(c, r); int x = PalX() + c * (CELL + GAP), y = PalY() + r * (CELL + GAP);
            g.fillRect(x, y, CELL, CELL, col);
            g.drawRect(x, y, CELL, CELL, col == color ? 0xFFFF : COL_LINE);
        }
        g.setTextColor(COL_DIM); g.drawString("Old (click: restore)", OldX(), PalY() - 20);
        g.fillRect(OldX(), PalY(), 114, 50, Old); g.drawRect(OldX(), PalY(), 114, 50, COL_LINE);
        g.setTextColor(COL_DIM); g.drawString("New", OldX(), PalY() + 58);
        g.fillRect(OldX(), PalY() + 76, 114, 50, color); g.drawRect(OldX(), PalY() + 76, 114, 50, 0xFFFF);
        static const char* lab[4] = { "R", "G", "B", "#" };
        for (int i = 0; i < 4; ++i) {
            g.setTextColor(0xFFFF); g.drawString(lab[i], FieldX(i) - (i < 3 ? 18 : 14), FieldY() + 6);
            DrawField(g, Fld[i], FieldX(i), FieldY(), FieldW(i), 26, Focus == i, false, ticks);
        }
        char b[48]; snprintf(b, sizeof b, "RGB565  0x%04X", color);
        g.setTextColor(COL_DIM); g.drawString(b, X + 400, FieldY() + 6);
        // sample: how the colour looks as text / as a background
        int sy = Y + 340, sh = 66;
        uint16_t bg = IsText ? Partner : color, fg = IsText ? color : Partner;
        g.fillRect(X + 16, sy, W - 32, sh, bg); g.drawRect(X + 16, sy, W - 32, sh, COL_LINE);
        g.setTextDatum(lgfx::textdatum_t::top_left);
        g.setFont(&lgfx::fonts::Font4); g.setTextColor(fg, bg); g.drawString("Lorem ipsum dolor sit amet", X + 28, sy + 6);
        g.setFont(&lgfx::fonts::Font2); g.drawString("Consectetur adipiscing elit, sed do eiusmod tempor 0123456789", X + 28, sy + 40);
        SuperPushButton cancel(G, CancelX(), BtnY(), 90, &lgfx::fonts::Font2, "Cancel");
        SuperPushButton ok(G, OkX(), BtnY(), 90, &lgfx::fonts::Font2, "OK");
        ok.ColBorder = COL_ACCENT;
        cancel.Push(); ok.Push();
    }

private:
    lgfx::LGFX_Sprite* G; std::string Title; uint16_t Old, Partner; bool IsText;
    std::unique_ptr<SuperColorPicker> Picker;
    TextField Fld[4]; int Focus = -1;
    enum Drag { D_NONE, D_PICKER, D_TEXT } Drag = D_NONE; bool DragStarted = false; int PressX = 0, PressY = 0;
    int BtnH = 20;

    static uint16_t To565(int r, int g, int b) { return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xFF) >> 3)); }
    static void Expand(uint16_t c, int& r, int& g, int& b) {
        r = (c >> 8) & 0xF8; r |= r >> 5; g = (c >> 3) & 0xFC; g |= g >> 6; b = (c << 3) & 0xF8; b |= b >> 5;
    }
    static void HsvToRgb(int hdeg, int s, int v, uint8_t& r, uint8_t& g, uint8_t& b) {
        float S = s / 100.f, V = v / 100.f, h = hdeg / 60.f; int i = (int)h % 6; float f = h - (int)h;
        float p = V * (1 - S), q = V * (1 - S * f), t = V * (1 - S * (1 - f)), R, Gc, B;
        switch (i) { case 0: R = V; Gc = t; B = p; break; case 1: R = q; Gc = V; B = p; break; case 2: R = p; Gc = V; B = t; break;
                     case 3: R = p; Gc = q; B = V; break; case 4: R = t; Gc = p; B = V; break; default: R = V; Gc = p; B = q; }
        r = (uint8_t)(R * 255 + .5f); g = (uint8_t)(Gc * 255 + .5f); b = (uint8_t)(B * 255 + .5f);
    }
    int Meas(const std::string& s) { return MeasureText(*G, s); }
    void Reveal(int i) { Fld[i].EnsureVisible([this](const std::string& s) { return Meas(s); }, FieldW(i) - 12); }
    int FieldAt(int x, int y) { for (int i = 0; i < 4; ++i) if (x >= FieldX(i) && x < FieldX(i) + FieldW(i) && y >= FieldY() && y < FieldY() + 26) return i; return -1; }

    void SyncFields() {                                       // colour -> fields (the field being typed in keeps its text)
        int r, g, b; Expand(color, r, g, b);
        char hex[8]; snprintf(hex, sizeof hex, "%02X%02X%02X", r, g, b);
        std::string v[4] = { std::to_string(r), std::to_string(g), std::to_string(b), hex };
        for (int i = 0; i < 4; ++i) if (i != Focus) Fld[i].SetText(v[i]);
    }
    void SetColor(uint16_t c) {                               // a colour from outside the picker (palette, fields, Old): exact, and the picker follows
        color = c; int r, g, b; Expand(c, r, g, b);
        Picker->SetLiveRGB((uint8_t)r, (uint8_t)g, (uint8_t)b);
        SyncFields();
    }
    void FromPicker() { color = Picker->GetLive565(); SyncFields(); }       // after the picker itself changed
    void Edited(int i) {                                      // a field was typed in
        int r, g, b; Expand(color, r, g, b);
        if (i < 3) {
            if (Fld[i].text.empty()) return;
            int v = std::min(255, atoi(Fld[i].text.c_str()));
            (i == 0 ? r : i == 1 ? g : b) = v;
        } else {
            if (Fld[3].text.size() != 6) return;                // a full RRGGBB only
            long x = strtol(Fld[3].text.c_str(), nullptr, 16); r = (x >> 16) & 255; g = (x >> 8) & 255; b = x & 255;
        }
        SetColor(To565(r, g, b));
    }
    void SetFocus(int i) { int old = Focus; Focus = i; if (old >= 0 && old != i) SyncFields(); }
    bool InPicker(int x, int y) { return x >= Picker->GetX() && x < Picker->GetX() + Picker->GetWidth() && y >= Picker->GetY() && y < Picker->GetY() + Picker->GetHeight(); }
    void PickerEvent(SuperPointerEvent::Kind k, int x, int y) {
        SuperPointerEvent p; p.Type = k; p.X = (int16_t)x; p.Y = (int16_t)y; p.StartX = (int16_t)PressX; p.StartY = (int16_t)PressY;
        Picker->OnPointer(p); Picker->Service(); FromPicker();
    }
    void Down(const InputEvent& e) {
        int fi = FieldAt(e.x, e.y);
        if (fi >= 0) {
            SetFocus(fi); Drag = D_TEXT; TextField& f = Fld[fi];
            f.SetCaretFromX(e.x - FieldX(fi) - 6, (SDL_GetModState() & KMOD_SHIFT) != 0, [this](const std::string& s) { return Meas(s); });
            if (e.clicks == 2) f.SelectWord(f.caret); else if (e.clicks >= 3) f.SelectAll();
            Reveal(fi); return;
        }
        SetFocus(-1);
        if (InPicker(e.x, e.y)) { Drag = D_PICKER; DragStarted = false; PressX = e.x; PressY = e.y; PickerEvent(SuperPointerEvent::PRESS, e.x, e.y); return; }
        int c = (e.x - PalX()) / (CELL + GAP), r = (e.y - PalY()) / (CELL + GAP);
        if (e.x >= PalX() && e.y >= PalY() && c < COLS && r < ROWS && (e.x - PalX()) % (CELL + GAP) < CELL && (e.y - PalY()) % (CELL + GAP) < CELL) { SetColor(Palette(c, r)); return; }
        if (e.x >= OldX() && e.x < OldX() + 114 && e.y >= PalY() && e.y < PalY() + 50) { SetColor(Old); return; }
        if (e.y >= BtnY() && e.y < BtnY() + BtnH) {
            if (e.x >= OkX() && e.x < OkX() + 90) { state = OK; return; }
            if (e.x >= CancelX() && e.x < CancelX() + 90) { state = CANCEL; return; }
        }
    }
    void Move(const InputEvent& e) {
        if (Drag == D_TEXT && Focus >= 0) { Fld[Focus].SetCaretFromX(e.x - FieldX(Focus) - 6, true, [this](const std::string& s) { return Meas(s); }); Reveal(Focus); }
        else if (Drag == D_PICKER) {
            if (!DragStarted) { DragStarted = true; PickerEvent(SuperPointerEvent::DRAG_START, e.x, e.y); }
            else PickerEvent(SuperPointerEvent::DRAG_MOVE, e.x, e.y);
        }
    }
    void Key(const InputEvent& e) {
        bool shift = e.mod & KMOD_SHIFT;
        if (Focus >= 0) {
            if (Fld[Focus].Key(e)) { Edited(Focus); Reveal(Focus); return; }
            if (e.key == SDLK_TAB) { int n = (Focus + (shift ? 3 : 1)) % 4; SetFocus(n); Fld[n].SelectAll(); Reveal(n); }
            else if (e.key == SDLK_RETURN || e.key == SDLK_KP_ENTER || e.key == SDLK_ESCAPE) SetFocus(-1);
            return;
        }
        SuperEditIO& io = Picker->IOPort();
        switch (e.key) {
        case SDLK_RETURN: case SDLK_KP_ENTER: state = OK; break;
        case SDLK_ESCAPE: state = CANCEL; break;
        case SDLK_LEFT:  io.InjectDelta(shift ? -10 : -1); Picker->Service(); FromPicker(); break;
        case SDLK_RIGHT: io.InjectDelta(shift ? 10 : 1);   Picker->Service(); FromPicker(); break;
        case SDLK_TAB: case SDLK_DOWN: io.Inject(SuperEditIO::A_SHORT); Picker->Service(); break;
        case SDLK_UP:   io.Inject(SuperEditIO::A_SHORT); Picker->Service(); io.Inject(SuperEditIO::A_SHORT); Picker->Service(); break;
        default: break;
        }
    }
};
}
