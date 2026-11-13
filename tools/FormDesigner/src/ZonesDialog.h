#pragma once
// A modal editor for the colour zones of a gauge (the bands painted along the dial), drawn inside the designer window.
//
//   zones (the real SuperListBox)     |  From [ 0 ]   To [ 60 ]   Color [ 07E0 ]  (RGB565 hex, 4 digits; click a swatch below)
//     0 to 60    [0x07E0]             |  [ palette: two rows of common colours ]
//     60 to 85   [0xFFE0]             |  [Add] [Delete] [Move up] [Move down] [Fill 3 bands] [Clear all]
//     85 to 100  [0xF800]             |  preview: the real gauge with these zones
//
// A zone is from..to in the gauge's own units and must end above where it starts (strict: otherwise the field turns red and the old value stays).
// Zones may overlap (the later one paints over the earlier); at most 20. "Fill 3 bands" makes green / yellow / red zones over 0-60 %, 60-85 %, 85-100 %
// of the gauge's Minimum..Maximum. Keys: Up / Down in the zone list, Tab = next field, Insert = Add, Delete, Enter = OK, Esc = Cancel (Esc in a field = back to the list).
#include <algorithm>
#include <memory>
#include <LovyanGFX.hpp>
#include <SuperListBox.h>
#include <SuperPushButton.h>
#include "DesignerInput.h"
#include "DesignerTextField.h"
#include "DesignerWidgets.h"
#include "DesignerControls.h"
#include "ColorDialog.h"

namespace FD {
class ZonesDialog {
public:
    enum State { OPEN, OK, CANCEL } state = OPEN;
    std::vector<GaugeZone> zones;                             // the bands being edited (the result when state == OK)

    ZonesDialog(lgfx::LGFX_Sprite* g, const std::string& title, const DControl& proto, uint16_t formBg)
        : zones(proto.zones), G(g), Title(title), Proto(proto), FormBg(formBg), Min(XFloat(proto, "min")), Max(XFloat(proto, "max")) {
        List.reset(new SuperListBox(G, X + 16, Y + 44, LIST_W, &lgfx::fonts::Font2, ROWS, nullptr, nullptr, nullptr));
        FFrom.maxLen = FTo.maxLen = 12; FFrom.filter = FTo.filter = TextField::DECIMAL; FCol.maxLen = 4; FCol.filter = TextField::HEX;
        SuperPushButton probe(G, 0, 0, 90, &lgfx::fonts::Font2, "OK"); BtnH = probe.GetHeight();
        Sel = zones.empty() ? -1 : 0; Rebuild();
    }

    static constexpr int W = 900, H = 520, X = (FD::WIN_W - W) / 2, Y = (FD::WIN_H - H) / 2, LIST_W = 340, ROWS = 12, CELL = 24;
    // geometry (public: the self-test aims at it)
    int RX() const { return X + 16 + LIST_W + 16; }
    int RW() const { return W - 16 - (16 + LIST_W + 16); }
    int FieldY() const { return Y + 64; }
    int FieldX(int k) const { return RX() + k * 150; }                  // From, To, Color
    int FieldW(int k) const { return k == 2 ? 90 : 130; }
    int PalX() const { return RX(); }
    int PalY() const { return Y + 120; }
    int Row1Y() const { return Y + 220; }
    int BtnX(int k) const { static const int off[6] = { 0, 80, 164, 262, 372, 0 }; return RX() + off[k]; }       // row 1: Add .. Fill 3 bands; row 2: Clear all
    int BtnW(int k) const { static const int w[6] = { 74, 78, 94, 104, 130, 100 }; return w[k]; }
    int BtnRow(int k) const { return k >= 5 ? 1 : 0; }
    int BtnY(int k) const { return Row1Y() + BtnRow(k) * 40; }
    int OkY() const { return Y + H - 16 - BtnH; }
    int OkX() const { return X + W - 16 - 90; }
    int CancelX() const { return OkX() - 10 - 90; }
    SuperListBox* Lb() { return List.get(); }
    int Selected() const { return Sel; }
    const std::string& FieldText(int k) const { return (k == 0 ? FFrom : k == 1 ? FTo : FCol).text; }
    int FocusedField() const { return Focus; }
    const std::string& Message() const { return Msg; }
    static uint16_t PaletteColor(int i) { return ColorDialog::Palette(i % 16, i < 16 ? 1 : 3); }

    void Event(const InputEvent& e) {
        switch (e.type) {
        case InputEvent::DOWN: if (e.button == SDL_BUTTON_LEFT) Down(e); break;
        case InputEvent::MOVE:
            if (Drag == D_LIST) { Pointer(Started ? SuperPointerEvent::DRAG_MOVE : SuperPointerEvent::DRAG_START, e.x, e.y); Started = true; }
            else if (Drag == D_TEXT && Cur()) Cur()->SetCaretFromX(e.x - FieldX(Focus) - 6, true, Measure());
            break;
        case InputEvent::UP: if (Drag == D_LIST) Pointer(SuperPointerEvent::DRAG_END, e.x, e.y); Drag = D_NONE; break;
        case InputEvent::WHEEL: if (e.x >= X && e.x < X + W && e.y >= Y && e.y < Y + H) { List->EditIO()->InjectDelta(-e.wheel * 3); Pump(); } break;
        case InputEvent::KEY_DOWN: Key(e); break;
        case InputEvent::TEXT: if (Cur()) { Cur()->Insert(e.text); Edited(); } break;
        default: break;
        }
    }

    void Draw(uint32_t ticks) {
        lgfx::LGFX_Sprite& g = *G;
        g.fillRectAlpha(0, 0, FD::WIN_W, FD::WIN_H, 150, 0x0000);
        g.fillRect(X, Y, W, H, COL_PANEL); g.drawRect(X, Y, W, H, COL_ACCENT); g.drawRect(X + 1, Y + 1, W - 2, H - 2, COL_ACCENT);
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left);
        g.setTextColor(COL_ACCENT); g.drawString(Title.c_str(), X + 16, Y + 12);
        if (zones.empty()) { g.setTextColor(COL_DIM); g.drawString("(no zones: Add, or Fill 3 bands)", X + 24, Y + 52); }
        List->Push();
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left);
        static const char* lab[3] = { "From", "To", "Color (RGB565 hex)" };
        for (int k = 0; k < 3; ++k) {
            g.setTextColor(0xFFFF); g.drawString(lab[k], FieldX(k), FieldY() - 22);
            TextField& f = k == 0 ? FFrom : k == 1 ? FTo : FCol; bool bad = false;
            if (Sel >= 0) { std::string model = k == 0 ? FloatText(zones[(size_t)Sel].from) : k == 1 ? FloatText(zones[(size_t)Sel].to) : HexDigits(zones[(size_t)Sel].color); bad = f.text != model; }
            DrawField(g, f, FieldX(k), FieldY(), FieldW(k), 26, Focus == k, bad, ticks);
        }
        if (Sel >= 0) { g.fillRect(FieldX(2) + 100, FieldY(), 36, 26, zones[(size_t)Sel].color); g.drawRect(FieldX(2) + 100, FieldY(), 36, 26, 0xFFFF); }
        g.setTextColor(COL_DIM); g.drawString("Common colours (click one):", PalX(), PalY() - 20);
        for (int i = 0; i < 32; ++i) { int x = PalX() + (i % 16) * (CELL + 2), y = PalY() + (i / 16) * (CELL + 2); g.fillRect(x, y, CELL, CELL, PaletteColor(i)); g.drawRect(x, y, CELL, CELL, Sel >= 0 && PaletteColor(i) == zones[(size_t)Sel].color ? 0xFFFF : COL_LINE); }
        static const char* bl[6] = { "Add", "Delete", "Move up", "Move down", "Fill 3 bands", "Clear all" };
        for (int k = 0; k < 6; ++k) { SuperPushButton b(G, BtnX(k), BtnY(k), BtnW(k), &lgfx::fonts::Font2, bl[k]); b.Push(); }
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left);
        char b[100]; snprintf(b, sizeof b, "%d zones (at most %d). The dial runs from %s to %s.", (int)zones.size(), MAX_GAUGE_ZONES, FloatText(Min).c_str(), FloatText(Max).c_str());
        g.setTextColor(COL_DIM); g.drawString(b, RX(), Row1Y() + 84);
        if (!Msg.empty()) { g.setTextColor(COL_ERR); g.drawString(Msg.c_str(), RX(), Row1Y() + 104); }
        int py = Y + 340, ph = OkY() - 14 - py;
        g.fillRect(RX(), py, RW(), ph, FormBg); g.drawRect(RX(), py, RW(), ph, COL_LINE);
        DControl t = Proto; t.zones = zones; t.x = 0; t.y = 0; t.w = std::min(RW() - 24, std::max(Proto.w, 120));
        g.setClipRect(RX() + 1, py + 1, RW() - 2, ph - 2); MakeControl(&g, t, RX() + 12, py + 8)->Push(); g.clearClipRect();
        SuperPushButton cancel(G, CancelX(), OkY(), 90, &lgfx::fonts::Font2, "Cancel"), ok(G, OkX(), OkY(), 90, &lgfx::fonts::Font2, "OK");
        ok.ColBorder = COL_ACCENT; cancel.Push(); ok.Push();
    }

private:
    enum Drag { D_NONE, D_LIST, D_TEXT } Drag = D_NONE;
    lgfx::LGFX_Sprite* G; std::string Title; DControl Proto; uint16_t FormBg; double Min, Max;
    std::unique_ptr<SuperListBox> List; TextField FFrom, FTo, FCol; std::string Msg;
    int Sel = -1, Focus = -1, BtnH = 20, PressX = 0, PressY = 0; bool Started = false;

    static std::string HexDigits(uint16_t c) { char b[8]; snprintf(b, sizeof b, "%04X", c); return b; }
    TextField* Cur() { return Focus == 0 ? &FFrom : Focus == 1 ? &FTo : Focus == 2 ? &FCol : nullptr; }
    std::function<int(const std::string&)> Measure() { return [this](const std::string& s) { return MeasureText(*G, s); }; }
    std::string Label(int i) const { const GaugeZone& z = zones[(size_t)i]; return FloatText(z.from) + " to " + FloatText(z.to) + "    [" + HexColor(z.color) + "]"; }
    void SyncFields() {
        if (Sel < 0) { FFrom.SetText(""); FTo.SetText(""); FCol.SetText(""); return; }
        FFrom.SetText(FloatText(zones[(size_t)Sel].from)); FTo.SetText(FloatText(zones[(size_t)Sel].to)); FCol.SetText(HexDigits(zones[(size_t)Sel].color));
    }
    void Rebuild() {
        std::vector<std::string> items; for (int i = 0; i < (int)zones.size(); ++i) items.push_back(Label(i));
        if (zones.empty()) { Sel = -1; List->SetItems({}); } else { Sel = std::max(0, std::min(Sel, (int)zones.size() - 1)); List->SetItems(items); List->SetSelected(Sel); List->StartEdit(); }
        SyncFields();
    }
    void Edited() {                                           // strict: a complete, valid value is applied; anything else leaves the zone as it was (and shows red)
        if (Sel < 0 || Focus < 0) return; Msg.clear(); GaugeZone& z = zones[(size_t)Sel];
        if (Focus == 2) { if (FCol.text.size() == 4) z.color = (uint16_t)strtol(FCol.text.c_str(), nullptr, 16); else return; }
        else {
            const std::string& t = Focus == 0 ? FFrom.text : FTo.text; char* end = nullptr; double v = strtod(t.c_str(), &end);
            if (t.empty() || end == t.c_str() || *end) { Msg = "type a number"; return; }
            double from = Focus == 0 ? v : z.from, to = Focus == 1 ? v : z.to;
            if (!(from < to)) { Msg = "a zone must end above where it starts"; return; }
            z.from = from; z.to = to;
        }
        List->SetItem(Sel, Label(Sel));
    }
    void Pump() {
        if (zones.empty()) return;
        SuperEditState s = List->Service();
        if (s == EDIT_ACCEPTED || s == EDIT_CANCELLED) List->StartEdit();
        int c = List->GetCursor();
        if (c != Sel && c >= 0 && c < (int)zones.size()) { Sel = c; Msg.clear(); SyncFields(); }
    }
    void Pointer(SuperPointerEvent::Kind k, int x, int y) {
        SuperPointerEvent p; p.Type = k; p.X = (int16_t)x; p.Y = (int16_t)y; p.StartX = (int16_t)PressX; p.StartY = (int16_t)PressY;
        List->OnPointer(p); Pump();
    }
    void Add() {
        if ((int)zones.size() >= MAX_GAUGE_ZONES) { Msg = "a gauge has at most 20 zones"; return; }
        GaugeZone z; double span = Max - Min, start = Sel >= 0 ? zones[(size_t)Sel].to : Min;
        z.from = std::min(start, Max - span / 10); z.to = std::min(Max, z.from + std::max(span / 5, 0.0001)); if (!(z.from < z.to)) { z.from = Min; z.to = Max; }
        static const uint16_t cols[] = { 0x07E0, 0xFFE0, 0xF800, 0x07FF, 0x001F, 0xF81F };
        z.color = cols[zones.size() % 6];
        zones.insert(zones.begin() + (Sel >= 0 ? Sel + 1 : 0), z); Sel = Sel >= 0 ? Sel + 1 : 0; Msg.clear(); Rebuild(); Focus = 0; FFrom.SelectAll();
    }
    void Delete() { if (Sel < 0) return; zones.erase(zones.begin() + Sel); Msg.clear(); Rebuild(); }
    void Move(int d) { int j = Sel + d; if (Sel < 0 || j < 0 || j >= (int)zones.size()) return; std::swap(zones[(size_t)Sel], zones[(size_t)j]); Sel = j; Rebuild(); }
    void Fill3() {
        double span = Max - Min; GaugeZone a, b, c; a.from = Min; a.to = Min + span * 0.60; a.color = 0x07E0; b.from = a.to; b.to = Min + span * 0.85; b.color = 0xFFE0; c.from = b.to; c.to = Max; c.color = 0xF800;
        auto r = [](double v) { return std::round(v * 10000) / 10000; }; a.to = b.from = r(a.to); b.to = c.from = r(b.to);
        zones = { a, b, c }; Sel = 0; Msg.clear(); Rebuild(); Focus = -1;
    }
    void Down(const InputEvent& e) {
        auto in = [&](int x, int y, int w, int h) { return e.x >= x && e.x < x + w && e.y >= y && e.y < y + h; };
        for (int k = 0; k < 3; ++k) if (in(FieldX(k), FieldY(), FieldW(k), 26)) {
            if (Sel >= 0) { Focus = k; Drag = D_TEXT; TextField* f = Cur(); f->SetCaretFromX(e.x - FieldX(k) - 6, (SDL_GetModState() & KMOD_SHIFT) != 0, Measure()); if (e.clicks >= 3) f->SelectAll(); }
            return; }
        for (int i = 0; i < 32; ++i) if (in(PalX() + (i % 16) * (CELL + 2), PalY() + (i / 16) * (CELL + 2), CELL, CELL)) { if (Sel >= 0) { zones[(size_t)Sel].color = PaletteColor(i); Msg.clear(); FCol.SetText(HexDigits(zones[(size_t)Sel].color)); List->SetItem(Sel, Label(Sel)); } return; }
        for (int k = 0; k < 6; ++k) if (in(BtnX(k), BtnY(k), BtnW(k), BtnH)) { Focus = -1; if (k == 0) Add(); else if (k == 1) Delete(); else if (k == 2) Move(-1); else if (k == 3) Move(1); else if (k == 4) Fill3(); else { zones.clear(); Sel = -1; Msg.clear(); Rebuild(); } return; }
        if (!zones.empty() && List->RowAt(e.x, e.y) >= 0) { Focus = -1; Drag = D_LIST; Started = false; PressX = e.x; PressY = e.y; Pointer(SuperPointerEvent::PRESS, e.x, e.y); return; }
        if (in(OkX(), OkY(), 90, BtnH)) { state = OK; return; }
        if (in(CancelX(), OkY(), 90, BtnH)) { state = CANCEL; return; }
    }
    void Key(const InputEvent& e) {
        bool shift = e.mod & KMOD_SHIFT;
        if (Focus >= 0) {
            if (Cur()->Key(e)) { Edited(); return; }
            if (e.key == SDLK_ESCAPE || e.key == SDLK_RETURN || e.key == SDLK_KP_ENTER) { Focus = -1; Msg.clear(); SyncFields(); return; }
            if (e.key == SDLK_TAB) { Focus = (Focus + (shift ? 2 : 1)) % 3; SyncFields(); Msg.clear(); Cur()->SelectAll(); }
            return;
        }
        SuperEditIO* io = List->EditIO();
        switch (e.key) {
        case SDLK_RETURN: case SDLK_KP_ENTER: state = OK; break;
        case SDLK_ESCAPE: state = CANCEL; break;
        case SDLK_TAB: if (Sel >= 0) { Focus = shift ? 2 : 0; Cur()->SelectAll(); } break;
        case SDLK_INSERT: Add(); break;
        case SDLK_DELETE: Delete(); break;
        case SDLK_UP: if (!zones.empty()) { io->InjectDelta(-1); Pump(); } break;
        case SDLK_DOWN: if (!zones.empty()) { io->InjectDelta(1); Pump(); } break;
        default: break;
        }
    }
};
}
