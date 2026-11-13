#pragma once
// A modal editor for the form's message boxes (the Form Designer), drawn inside the designer window.
//
//   boxes (the real SuperListBox)   |  Name [ msgDelete ]     Title [ Careful ]
//     msgDelete                     |  Width [220] Default [1] X [0] Y [0]   [Centered: Yes]
//     msgSaved                      |  Message: [Edit message...]   2 lines: Delete all data? ...
//   [Add] [Delete] [Up] [Down]      |  Button text            Button name (optional constant, e.g. MSG_DELETE_YES)
//                                   |  [ Yes        ]         [ MSG_DELETE_YES ]      [ No ] [ ]   [ ] [ ]    [Auto names]
//                                   |  preview: the real message box
//
// A message box is not on the design surface: it is opened from your code (<box>_Ask() / _Open() / _Poll() in the generated class). 1 to 3 buttons: a button's text is what is shown,
// its optional name becomes `static constexpr int <name> = <index>;` so code reads  if (msgDelete_Ask() == MSG_DELETE_YES). Emptying a button's text removes the button.
// The message is a list of lines (the items dialog); fonts and colours are in the JSON file ("font", "textColor", "backColor", "borderColor").
// Strict: a name that is not a safe C++ name, or is already used by a control, group, menu item, box or button, is refused (red, with the reason, the old value stays).
// Keys: Up / Down in the list, Insert = Add, Delete = Delete, Tab = the fields, in a field Tab / Shift+Tab move, Enter or Esc = back to the list; in the list Enter = OK, Esc = Cancel.
#include <algorithm>
#include <functional>
#include <memory>
#include <LovyanGFX.hpp>
#include <SuperListBox.h>
#include <SuperPushButton.h>
#include "DesignerInput.h"
#include "DesignerTextField.h"
#include "DesignerWidgets.h"
#include "DesignerControls.h"
#include "ItemsDialog.h"
#include "FontDialog.h"
#include "ColorDialog.h"

namespace FD {
class MessageBoxEditor {
public:
    enum State { OPEN, OK, CANCEL } state = OPEN;
    enum Field { F_NAME, F_TITLE, F_W, F_DEF, F_X, F_Y, F_T0, F_N0, F_T1, F_N1, F_T2, F_N2, N_FIELDS };

    MessageBoxEditor(lgfx::LGFX_Sprite* g, const std::string& title, const DForm& form, uint16_t formBg) : F(form), G(g), Title(title), FormBg(formBg) {
        List.reset(new SuperListBox(G, X + 16, Y + 44, LIST_W, &lgfx::fonts::Font2, ROWS, nullptr, nullptr, nullptr));
        Fld[F_NAME].maxLen = 32; Fld[F_NAME].filter = TextField::IDENT; Fld[F_TITLE].maxLen = MAX_MSG_LINE;
        for (int k : { F_W, F_DEF, F_X, F_Y }) { Fld[k].filter = TextField::DIGITS; Fld[k].maxLen = 4; }
        for (int k = 0; k < 3; ++k) { Fld[F_T0 + 2 * k].maxLen = MAX_MSG_BTN_TEXT; Fld[F_N0 + 2 * k].maxLen = 32; Fld[F_N0 + 2 * k].filter = TextField::IDENT; }
        SuperPushButton probe(G, 0, 0, 90, &lgfx::fonts::Font2, "OK"); BtnH = probe.GetHeight();
        Sel = F.boxes.empty() ? -1 : 0; Rebuild();
    }

    static constexpr int W = 1040, H = 740, X = (FD::WIN_W - W) / 2, Y = (FD::WIN_H - H) / 2, LIST_W = 240, ROWS = 12;
    std::vector<DMessageBox>& Boxes() { return F.boxes; }
    // geometry (public: the self-test aims at it)
    int RX() const { return X + 16 + LIST_W + 16; }
    int RW() const { return W - 16 - (16 + LIST_W + 16); }
    int FX(int k) const { static const int dx[4] = { 0, 100, 200, 300 }; switch (k) { case F_NAME: case F_T0: case F_T1: case F_T2: return RX(); case F_TITLE: return RX() + 220;
                          case F_N0: case F_N1: case F_N2: return RX() + 240; default: return RX() + dx[k - F_W]; } }
    int FY(int k) const { if (k <= F_TITLE) return Y + 64; if (k <= F_Y) return Y + 120; return Y + 236 + ((k - F_T0) / 2) * 36; }
    int FW(int k) const { switch (k) { case F_NAME: return 200; case F_TITLE: return RW() - 220; case F_T0: case F_T1: case F_T2: return 220; case F_N0: case F_N1: case F_N2: return RW() - 240 - 120; default: return 80; } }
    // the look: font, three colours, rich text, style fonts
    int LookY() const { return Y + 404; }
    int FontBtnX() const { return RX(); }
    int ColBtnX(int k) const { return RX() + 230 + k * 74; }                      // 0 text, 1 back, 2 border
    int RichX(int seg) const { return RX() + 480 + seg * 56; }                    // 0 No, 1 Yes
    int StyleY() const { return Y + 462; }
    int StyleX(int k) const { return RX() + k * 186; }
    int CenterX() const { return RX() + 400; }
    int CenterY() const { return Y + 120; }
    int MsgBtnX() const { return RX(); }
    int MsgBtnY() const { return Y + 176; }
    int AutoX() const { return RX() + RW() - 110; }
    int AutoY() const { return Y + 236; }
    int LBY() const { return Y + 44 + List->GetHeight() + 10; }
    int YesNoX() const { return X + 16; }
    int YesNoY() const { return LBY() + 30; }
    int LBX(int k) const { return X + 16 + k * 60; }
    int BtnY() const { return Y + H - 16 - BtnH; }
    int OkX() const { return X + W - 16 - 90; }
    int CancelX() const { return OkX() - 10 - 90; }
    SuperListBox* Lb() { return List.get(); }
    ItemsDialog* Sub() { return Sub_.get(); }
    FontDialog* FontDlg() { return FDlg.get(); }
    ColorDialog* ColorDlg() { return CDlg.get(); }
    int Selected() const { return Sel; }
    int FocusedField() const { return Focus; }                  // -1 = the list
    const std::string& FieldText(int k) const { return Fld[k].text; }
    const std::string& Message() const { return Msg; }
    int BadField() const { return Bad; }

    void Event(const InputEvent& e) {
        if (FDlg) { FDlg->Event(e); if (FDlg->state != FontDialog::OPEN) EndFont(); return; }
        if (CDlg) { CDlg->Event(e); if (CDlg->state != ColorDialog::OPEN) EndColor(); return; }
        if (Sub_) { Sub_->Event(e); if (Sub_->state != ItemsDialog::OPEN) EndSub(); return; }
        switch (e.type) {
        case InputEvent::DOWN: if (e.button == SDL_BUTTON_LEFT) Down(e); break;
        case InputEvent::MOVE:
            if (Drag == D_LIST) { Pointer(Started ? SuperPointerEvent::DRAG_MOVE : SuperPointerEvent::DRAG_START, e.x, e.y); Started = true; }
            else if (Drag == D_TEXT && Focus >= 0) Fld[Focus].SetCaretFromX(e.x - FX(Focus) - 6, true, Measure());
            break;
        case InputEvent::UP: if (Drag == D_LIST) Pointer(SuperPointerEvent::DRAG_END, e.x, e.y); Drag = D_NONE; break;
        case InputEvent::WHEEL: if (e.x >= X && e.x < X + W && e.y >= Y && e.y < Y + H) { List->EditIO()->InjectDelta(-e.wheel * 3); Pump(); } break;
        case InputEvent::KEY_DOWN: Key(e); break;
        case InputEvent::TEXT: if (Focus >= 0) { Fld[Focus].Insert(e.text); Edited(Focus); } break;
        default: break;
        }
    }

    void Draw(uint32_t ticks) {
        lgfx::LGFX_Sprite& g = *G;
        g.fillRectAlpha(0, 0, FD::WIN_W, FD::WIN_H, 150, 0x0000);
        g.fillRect(X, Y, W, H, COL_PANEL); g.drawRect(X, Y, W, H, COL_ACCENT); g.drawRect(X + 1, Y + 1, W - 2, H - 2, COL_ACCENT);
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left);
        g.setTextColor(COL_ACCENT); g.drawString(Title.c_str(), X + 16, Y + 12);
        if (F.boxes.empty()) { g.setTextColor(COL_DIM); g.drawString("(no message boxes: click Add)", X + 24, Y + 52); }
        List->Push();
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left);
        static const char* lab[F_Y + 1] = { "Name", "Title (optional)", "Width", "Default button", "X", "Y" };
        const DMessageBox* b = Cur();
        for (int k = 0; k <= F_Y; ++k) { g.setTextColor(0xFFFF); g.drawString(lab[k], FX(k), FY(k) - 22); DrawField(g, Fld[k], FX(k), FY(k), FW(k), 26, Focus == k, Bad == k, ticks); }
        g.setTextColor(0xFFFF); g.drawString("Message", RX(), MsgBtnY() - 22);
        g.drawString("Button text", RX(), FY(F_T0) - 22); g.drawString("Button name (optional constant)", RX() + 240, FY(F_T0) - 22);
        for (int k = F_T0; k < N_FIELDS; ++k) DrawField(g, Fld[k], FX(k), FY(k), FW(k), 26, Focus == k, Bad == k, ticks);
        g.setTextColor(COL_DIM);
        if (b) { char s[40]; for (int k = 0; k < 3; ++k) { snprintf(s, sizeof s, "result %d", k); g.drawString(s, RX() + RW() - 110, FY(F_T0) + k * 36 + 6); }
                 std::string sm = std::to_string(b->lines.size()) + (b->lines.size() == 1 ? " line: " : " lines: "); for (size_t i = 0; i < b->lines.size() && sm.size() < 70; ++i) sm += (i ? " / " : "") + b->lines[i];
                 g.setClipRect(RX() + 176, MsgBtnY(), RW() - 180, 30); g.drawString(sm.c_str(), RX() + 176, MsgBtnY() + 6); g.clearClipRect(); }
        if (!Msg.empty()) { g.setTextColor(COL_ERR); g.drawString(Msg.c_str(), RX(), Y + 352); }
        { SuperPushButton c(G, CenterX(), CenterY(), 150, &lgfx::fonts::Font2, b && !b->centered ? "Centered: No" : "Centered: Yes"); c.Push();
          SuperPushButton m(G, MsgBtnX(), MsgBtnY(), 160, &lgfx::fonts::Font2, "Edit message..."); m.Push();
          SuperPushButton a(G, AutoX(), AutoY() + 36 * 3 - 4, 100, &lgfx::fonts::Font2, "Auto names"); a.Push(); }
        { SuperPushButton y(G, YesNoX(), YesNoY(), 120, &lgfx::fonts::Font2, "Add Yes/No"); y.Push(); }
        static const char* l1[4] = { "Add", "Delete", "Up", "Down" };
        for (int k = 0; k < 4; ++k) { SuperPushButton bb(G, LBX(k), LBY(), 56, &lgfx::fonts::Font2, l1[k]); bb.Push(); }
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left); g.setTextColor(COL_DIM);
        char s[60]; snprintf(s, sizeof s, "%d message box%s", (int)F.boxes.size(), F.boxes.size() == 1 ? "" : "es"); g.drawString(s, X + 16, LBY() + 66);
        // the look
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left); g.setTextColor(0xFFFF);
        g.drawString("Font", FontBtnX(), LookY() - 22); g.drawString("Text", ColBtnX(0), LookY() - 22); g.drawString("Back", ColBtnX(1), LookY() - 22); g.drawString("Border", ColBtnX(2), LookY() - 22); g.drawString("Rich text (tags)", RichX(0), LookY() - 22);
        g.drawString("Style fonts for <b> <i> <b><i> <h1> (Rich text only; empty = ignored)", RX(), StyleY() - 22);
        if (b) {
            g.fillRect(FontBtnX(), LookY(), 220, 26, 0x0000); g.drawRect(FontBtnX(), LookY(), 220, 26, 0x7BEF); g.setTextColor(0xFFFF); g.drawString(FindFont(b->font).label, FontBtnX() + 6, LookY() + 5);
            const uint16_t cols[3] = { b->fg, b->bg, b->border };
            for (int k = 0; k < 3; ++k) { g.fillRect(ColBtnX(k), LookY(), 64, 26, cols[k]); g.drawRect(ColBtnX(k), LookY(), 64, 26, 0xFFFF); }
            for (int k = 0; k < 2; ++k) { bool on = (k == 1) == b->rich; g.fillRect(RichX(k), LookY(), 52, 26, on ? COL_SEL : 0x0000); g.drawRect(RichX(k), LookY(), 52, 26, on ? COL_ACCENT : 0x7BEF); g.setTextDatum(lgfx::textdatum_t::middle_center); g.setTextColor(on ? 0xFFFF : COL_DIM); g.drawString(k ? "Yes" : "No", RichX(k) + 26, LookY() + 13); g.setTextDatum(lgfx::textdatum_t::top_left); }
            static const char* nm[4] = { "Bold", "Italic", "Bold italic", "Heading" };
            for (int k = 0; k < 4; ++k) {
                g.fillRect(StyleX(k), StyleY(), 176, 26, 0x0000); g.drawRect(StyleX(k), StyleY(), 176, 26, 0x7BEF);
                g.setClipRect(StyleX(k) + 2, StyleY() + 2, 172, 22); g.setTextColor(b->styleFont[k].empty() ? COL_DIM : 0xFFFF);
                g.drawString(b->styleFont[k].empty() ? (std::string(nm[k]) + ": none").c_str() : (std::string(nm[k]) + ": " + FindFont(b->styleFont[k]).label).c_str(), StyleX(k) + 5, StyleY() + 5); g.clearClipRect();
                if (!b->styleFont[k].empty()) { g.setTextColor(COL_ERR); g.drawString("x", StyleX(k) + 176 - 16, StyleY() + 5); }       // click the x to clear it
            }
        }
        // the real message box
        int py = Y + 520, ph = BtnY() - 14 - py;
        g.fillRect(RX(), py, RW(), ph, FormBg); g.drawRect(RX(), py, RW(), ph, COL_LINE);
        if (b) { DMessageBox t = *b; t.centered = false; t.x = RX() + 16; t.y = py + 28;
                 g.setClipRect(RX() + 1, py + 1, RW() - 2, ph - 2); auto m = MakeMessageBox(&g, t); m->StartEdit(); g.clearClipRect(); }
        SuperPushButton cancel(G, CancelX(), BtnY(), 90, &lgfx::fonts::Font2, "Cancel"), ok(G, OkX(), BtnY(), 90, &lgfx::fonts::Font2, "OK");
        ok.ColBorder = COL_ACCENT; cancel.Push(); ok.Push();
        if (Sub_) Sub_->Draw(ticks);
        if (FDlg) FDlg->Draw();
        if (CDlg) CDlg->Draw(ticks);
    }

private:
    enum Drag { D_NONE, D_LIST, D_TEXT } Drag = D_NONE;
    DForm F; lgfx::LGFX_Sprite* G; std::string Title; uint16_t FormBg;
    std::unique_ptr<SuperListBox> List; TextField Fld[N_FIELDS]; std::unique_ptr<ItemsDialog> Sub_; std::unique_ptr<FontDialog> FDlg; std::unique_ptr<ColorDialog> CDlg; int DlgFor = -1; std::string Msg;      // DlgFor: 0 font, 1..3 colours, 4..7 style fonts
    int Sel = -1, Focus = -1, Bad = -1, BtnH = 20, PressX = 0, PressY = 0; bool Started = false;

    DMessageBox* Cur() { return Sel >= 0 && Sel < (int)F.boxes.size() ? &F.boxes[(size_t)Sel] : nullptr; }
    std::function<int(const std::string&)> Measure() { return [this](const std::string& s) { return MeasureText(*G, s); }; }
    static std::string Trim(const std::string& s) { size_t a = s.find_first_not_of(' '), b = s.find_last_not_of(' '); return a == std::string::npos ? "" : s.substr(a, b - a + 1); }

    void Load() {                                             // the fields follow the selected box
        const DMessageBox* b = Cur();
        Fld[F_NAME].SetText(b ? b->name : ""); Fld[F_TITLE].SetText(b ? b->title : ""); Fld[F_W].SetText(b ? std::to_string(b->w) : ""); Fld[F_DEF].SetText(b ? std::to_string(b->defaultBtn) : "");
        Fld[F_X].SetText(b ? std::to_string(b->x) : ""); Fld[F_Y].SetText(b ? std::to_string(b->y) : "");
        for (int k = 0; k < 3; ++k) { bool has = b && k < (int)b->buttons.size(); Fld[F_T0 + 2 * k].SetText(has ? b->buttons[(size_t)k].text : ""); Fld[F_N0 + 2 * k].SetText(has ? b->buttons[(size_t)k].itemId : ""); }
        Bad = -1;
    }
    void Rebuild() {                                          // list and fields follow the boxes / the selection
        std::vector<std::string> names; for (const DMessageBox& b : F.boxes) names.push_back(b.name);
        if (!names.empty()) { List->SetItems(names); List->SetSelected(Sel < 0 ? 0 : Sel); List->StartEdit(); if (Sel < 0) Sel = 0; } else { List->SetItems({}); Sel = -1; }
        Load();
    }
    void Pump() {
        if (F.boxes.empty()) return;
        SuperEditState s = List->Service(); if (s == EDIT_ACCEPTED || s == EDIT_CANCELLED) List->StartEdit();
        int c = List->GetCursor(); if (c != Sel && c >= 0 && c < (int)F.boxes.size()) { Sel = c; Msg.clear(); Load(); }
    }
    void Pointer(SuperPointerEvent::Kind k, int x, int y) {
        SuperPointerEvent p; p.Type = k; p.X = (int16_t)x; p.Y = (int16_t)y; p.StartX = (int16_t)PressX; p.StartY = (int16_t)PressY; List->OnPointer(p); Pump();
    }
    bool ButtonNameTaken(const std::string& t, int skipButton) const {          // clashes with a name outside this box, this box's name, or another button of this box
        const DMessageBox& b = F.boxes[(size_t)Sel];
        if (t == b.name || F.NameTaken(t, -1, -1, true, Sel)) return true;
        for (int i = 0; i < (int)b.buttons.size(); ++i) if (i != skipButton && b.buttons[(size_t)i].itemId == t) return true;
        return false;
    }
    bool Refuse(int field, const std::string& why) { Msg = why; Bad = field; return false; }
    // Applies the text of one field to the selected box: strict, a refused value leaves the box as it was (the field turns red with the reason).
    bool Edited(int k) {
        DMessageBox* b = Cur(); if (!b) return false;
        const std::string t = Trim(Fld[k].text); Msg.clear(); Bad = -1;
        auto number = [&](int lo, int hi, int& out, const char* what) { if (t.empty() || t.size() > 5) return Refuse(k, std::string(what) + " must be a number"); long v = atol(t.c_str());
            if (v < lo || v > hi) return Refuse(k, std::string(what) + " must be " + std::to_string(lo) + " to " + std::to_string(hi)); out = (int)v; return true; };
        switch (k) {
        case F_NAME:
            if (!ValidName(t)) return Refuse(k, (t.empty() ? std::string("a name") : t) + " is not a safe C++ name (a keyword, a reserved or library name, or a macro)");
            if (t != b->name && (F.NameTaken(t, -1, -1, true, Sel) || std::any_of(b->buttons.begin(), b->buttons.end(), [&](const MsgButton& m) { return m.itemId == t; }))) return Refuse(k, t + " is already used by a control, group, menu item, message box, button name or the form's classes");
            if (t != b->name) { b->name = t; std::vector<std::string> n; for (const DMessageBox& x : F.boxes) n.push_back(x.name); List->SetItem(Sel, t); }
            return true;
        case F_TITLE: b->title = t; return true;
        case F_W: { int v; if (!number(MIN_MSG_W, MAX_MSG_W, v, "the width")) return false; b->w = v; return true; }
        case F_DEF: { int v; if (!number(0, (int)b->buttons.size() - 1, v, "the default button")) return false; b->defaultBtn = v; return true; }
        case F_X: { int v; if (!number(0, 4000, v, "X")) return false; b->x = v; return true; }
        case F_Y: { int v; if (!number(0, 4000, v, "Y")) return false; b->y = v; return true; }
        default: break;
        }
        int slot = (k - F_T0) / 2; bool isText = (k - F_T0) % 2 == 0; const int n = (int)b->buttons.size();
        if (isText) {
            if (t.empty()) {
                if (slot >= n) return true;
                if (n <= 1) return Refuse(k, "a message box needs at least one button");
                b->buttons.erase(b->buttons.begin() + slot); b->defaultBtn = std::min(b->defaultBtn, (int)b->buttons.size() - 1);
                int keep = Focus; std::string txt = Fld[k].text; Load(); Focus = keep; (void)txt; return true;      // the slots below move up
            }
            if (slot > n) return Refuse(k, "fill the buttons in order: the previous button has no text yet");
            if (slot == n) b->buttons.push_back({ t, "" }); else b->buttons[(size_t)slot].text = t;
            return true;
        }
        if (slot >= n) return Refuse(k, "give the button its text first");
        if (t.empty()) { b->buttons[(size_t)slot].itemId.clear(); return true; }
        if (!ValidName(t)) return Refuse(k, t + " is not a safe C++ name (a keyword, a reserved or library name, or a macro)");
        if (ButtonNameTaken(t, slot)) return Refuse(k, t + " is already used (a control, group, menu item, message box, another button or the form's classes)");
        b->buttons[(size_t)slot].itemId = t; return true;
    }
    void AutoNames() {                                         // fill every empty button name with MSG_<BOX>_<TEXT>
        DMessageBox* b = Cur(); if (!b) return; int added = 0;
        for (size_t i = 0; i < b->buttons.size(); ++i) if (b->buttons[i].itemId.empty()) {
            std::string id = DForm::AutoButtonName(b->name, b->buttons[i].text, [&](const std::string& s) { return ButtonNameTaken(s, (int)i) || !ValidName(s); });
            if (!id.empty() && ValidName(id) && !ButtonNameTaken(id, (int)i)) { b->buttons[i].itemId = id; ++added; }
        }
        Msg = added ? "" : "every button already has a name"; Load();
    }
    // yesNo: the ready-made confirmation box: "Confirm?" with Yes / No (named buttons MSG_<BOX>_YES / _NO), No highlighted
    void Add(bool yesNo = false) {
        if ((int)F.boxes.size() >= MAX_MSG_BOXES) { Msg = "a form has at most 20 message boxes"; return; }
        DMessageBox b; b.name = F.UniqueBoxName(); b.lines = { "Message" }; b.buttons = { { "OK", "" } };
        if (yesNo) b = F.MakeYesNoBox();
        int at = Sel >= 0 ? Sel + 1 : 0; F.boxes.insert(F.boxes.begin() + at, b); Sel = at; Msg.clear(); Rebuild(); Focus = F_NAME; Fld[F_NAME].SelectAll();
    }
    void Delete() { if (Sel < 0) return; F.boxes.erase(F.boxes.begin() + Sel); if (Sel >= (int)F.boxes.size()) Sel = (int)F.boxes.size() - 1; Msg.clear(); Rebuild(); Focus = -1; }
    void Move(int d) { int j = Sel + d; if (Sel < 0 || j < 0 || j >= (int)F.boxes.size()) return; std::swap(F.boxes[(size_t)Sel], F.boxes[(size_t)j]); Sel = j; Rebuild(); }
    void OpenSub() {
        DMessageBox* b = Cur(); if (!b) return;
        DControl p; p.type = C_LISTBOX; p.w = 300; p.list = b->lines; Sub_.reset(new ItemsDialog(G, "Message of " + b->name + " (one entry per line, at most 12)", p, F, FormBg));
        Focus = -1; Msg.clear();
    }
    void EndSub() {
        DMessageBox* b = Cur();
        if (Sub_->state == ItemsDialog::OK && b) {
            b->lines = Sub_->items; if ((int)b->lines.size() > MAX_MSG_LINES) { b->lines.resize(MAX_MSG_LINES); Msg = "a message has at most 12 lines: the rest was cut"; }
        }
        Sub_.reset(); Drag = D_NONE;
    }
    void OpenFont(int which) {                                    // 0 = the box's font, 4..7 = a style font
        DMessageBox* b = Cur(); if (!b) return;
        DControl proto; proto.type = C_LABEL; proto.text = "Sample 123"; proto.font = which == 0 ? b->font : (b->styleFont[which - 4].empty() ? b->font : b->styleFont[which - 4]); proto.fg = b->fg; proto.bg = b->bg;
        static const char* nm[4] = { "bold", "italic", "bold italic", "heading" };
        FDlg.reset(new FontDialog(G, which == 0 ? "Font of " + b->name : std::string("Style font (") + nm[which - 4] + ") of " + b->name, proto, FormBg)); DlgFor = which; Focus = -1; Msg.clear();
    }
    void EndFont() {
        DMessageBox* b = Cur();
        if (FDlg->state == FontDialog::OK && b) { if (DlgFor == 0) b->font = FDlg->font; else b->styleFont[DlgFor - 4] = FDlg->font; }
        FDlg.reset(); DlgFor = -1; Drag = D_NONE;
    }
    void OpenColor(int k) {                                       // 0 text, 1 back, 2 border
        DMessageBox* b = Cur(); if (!b) return;
        static const char* nm[3] = { "Text color", "Back color", "Border color" };
        uint16_t cur = k == 0 ? b->fg : k == 1 ? b->bg : b->border, partner = k == 0 ? b->bg : k == 1 ? b->fg : b->bg;
        CDlg.reset(new ColorDialog(G, std::string(nm[k]) + " of " + b->name, cur, partner, k == 0)); DlgFor = 1 + k; Focus = -1; Msg.clear();
    }
    void EndColor() {
        DMessageBox* b = Cur();
        if (CDlg->state == ColorDialog::OK && b) { if (DlgFor == 1) b->fg = CDlg->color; else if (DlgFor == 2) b->bg = CDlg->color; else b->border = CDlg->color; }
        CDlg.reset(); DlgFor = -1; Drag = D_NONE;
    }
    void Leave() { Focus = -1; Msg.clear(); Load(); }
    void Down(const InputEvent& e) {
        auto in = [&](int x, int y, int w, int h) { return e.x >= x && e.x < x + w && e.y >= y && e.y < y + h; };
        for (int k = 0; k < N_FIELDS; ++k) if (in(FX(k), FY(k), FW(k), 26)) {
            if (Sel < 0) return;
            if (Focus != k) { if (Focus >= 0 && Bad >= 0) Load(); Focus = k; Msg.clear(); Bad = -1; }
            Drag = D_TEXT; Fld[k].SetCaretFromX(e.x - FX(k) - 6, (SDL_GetModState() & KMOD_SHIFT) != 0, Measure());
            if (e.clicks == 2) Fld[k].SelectWord(Fld[k].caret); else if (e.clicks >= 3) Fld[k].SelectAll(); return; }
        if (Cur()) {
            if (in(FontBtnX(), LookY(), 220, 26)) { OpenFont(0); return; }
            for (int k = 0; k < 3; ++k) if (in(ColBtnX(k), LookY(), 64, 26)) { OpenColor(k); return; }
            for (int k = 0; k < 2; ++k) if (in(RichX(k), LookY(), 52, 26)) { Cur()->rich = k == 1; return; }
            for (int k = 0; k < 4; ++k) if (in(StyleX(k), StyleY(), 176, 26)) {
                if (e.x >= StyleX(k) + 156 && !Cur()->styleFont[k].empty()) Cur()->styleFont[k].clear(); else OpenFont(4 + k);        // the right end of a set font clears it
                return; }
        }
        if (in(CenterX(), CenterY(), 150, BtnH)) { if (DMessageBox* b = Cur()) { b->centered = !b->centered; Msg.clear(); } return; }
        if (in(MsgBtnX(), MsgBtnY(), 160, BtnH)) { OpenSub(); return; }
        if (in(AutoX(), AutoY() + 36 * 3 - 4, 100, BtnH)) { AutoNames(); return; }
        if (in(YesNoX(), YesNoY(), 120, BtnH)) { Add(true); return; }
        for (int k = 0; k < 4; ++k) if (in(LBX(k), LBY(), 56, BtnH)) { if (k == 0) Add(); else if (k == 1) Delete(); else Move(k == 2 ? -1 : 1); return; }
        if (!F.boxes.empty() && List->RowAt(e.x, e.y) >= 0) { Focus = -1; Drag = D_LIST; Started = false; PressX = e.x; PressY = e.y; Pointer(SuperPointerEvent::PRESS, e.x, e.y); return; }
        if (in(OkX(), BtnY(), 90, BtnH)) { state = OK; return; }
        if (in(CancelX(), BtnY(), 90, BtnH)) { state = CANCEL; return; }
    }
    void Key(const InputEvent& e) {
        bool shift = e.mod & KMOD_SHIFT;
        if (Focus >= 0) {
            if (Fld[Focus].Key(e)) { Edited(Focus); return; }
            if (e.key == SDLK_RETURN || e.key == SDLK_KP_ENTER || e.key == SDLK_ESCAPE) Leave();
            else if (e.key == SDLK_TAB) { int k = Focus; Leave(); Focus = (k + (shift ? N_FIELDS - 1 : 1)) % N_FIELDS; Fld[Focus].SelectAll(); }
            return;
        }
        SuperEditIO* io = List->EditIO();
        switch (e.key) {
        case SDLK_RETURN: case SDLK_KP_ENTER: state = OK; break;
        case SDLK_ESCAPE: state = CANCEL; break;
        case SDLK_TAB: if (Sel >= 0) { Focus = F_NAME; Fld[F_NAME].SelectAll(); } break;
        case SDLK_INSERT: Add(); break;
        case SDLK_DELETE: Delete(); break;
        case SDLK_UP:   if (!F.boxes.empty()) { io->InjectDelta(-1); Pump(); } break;
        case SDLK_DOWN: if (!F.boxes.empty()) { io->InjectDelta(1); Pump(); } break;
        default: break;
        }
    }
};
}
