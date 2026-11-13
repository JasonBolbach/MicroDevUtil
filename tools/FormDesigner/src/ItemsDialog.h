#pragma once
// A modal list editor for the Form Designer (the items of a list box, combo box or tab bar), drawn inside the designer window.
//
//   items (the real SuperListBox)   |  Item text: [ ........................ ]      the selected item, changed live
//                                   |  [Add] [Insert] [Delete] [Move up] [Move down]
//                                   |  [Sort A-Z] [Paste lines] [Clear all]          N items
//                                   |  preview: the real control with these items
//
// A tab bar also has a "Page group" per tab: the control group whose controls are shown when that tab is chosen (empty = none). A group name is a safe C++ name
// that no control, radio group, menu item or form class has; several tabs may not need the same group, but they may share one.
// Items are printable ASCII, 1..60 characters, at most 200. An empty item is refused (the field turns red, the old text stays).
// Mouse: click an item. Keys: Up / Down / Home / End / PageUp / PageDown in the list, Insert = Add, Delete = Delete, Tab = list <-> text field,
//        in the text field Enter = Add the next item (fast entry), Esc = back to the list; in the list Esc = Cancel, Enter = OK.
// "Paste lines" adds every non-empty line of the clipboard after the selected item.
#include <algorithm>
#include <memory>
#include <LovyanGFX.hpp>
#include <SuperListBox.h>
#include <SuperPushButton.h>
#include "DesignerInput.h"
#include "DesignerTextField.h"
#include "DesignerWidgets.h"
#include "DesignerControls.h"

namespace FD {
class ItemsDialog {
public:
    enum State { OPEN, OK, CANCEL } state = OPEN;
    std::vector<std::string> items;                           // the list being edited (the result when state == OK)
    std::vector<std::string> pages;                           // tab bar: the page group of each tab (same size as items)

    ItemsDialog(lgfx::LGFX_Sprite* g, const std::string& title, const DControl& proto, const DForm& form, uint16_t formBg)
        : items(proto.list), pages(proto.pages), G(g), Title(title), Proto(proto), Form(form), FormBg(formBg), Tabs(proto.type == C_TABBAR) {
        pages.resize(items.size()); PageField.maxLen = 32; PageField.filter = TextField::IDENT;
        List.reset(new SuperListBox(G, X + 16, Y + 44, LIST_W, &lgfx::fonts::Font2, ROWS, nullptr, nullptr, nullptr));
        Field.maxLen = MAX_ITEM_LEN;
        SuperPushButton probe(G, 0, 0, 90, &lgfx::fonts::Font2, "OK"); BtnH = probe.GetHeight();
        Sel = items.empty() ? -1 : std::min((int)XInt(proto, "selected"), (int)items.size() - 1);
        Rebuild(); Focus = F_LIST;
    }

    static constexpr int W = 860, H = 480, X = (FD::WIN_W - W) / 2, Y = (FD::WIN_H - H) / 2, LIST_W = 300, ROWS = 14;
    // geometry (public: the self-test aims at it)
    int RX() const { return X + 16 + LIST_W + 16; }
    int RW() const { return W - 16 - (16 + LIST_W + 16); }
    int FieldY() const { return Y + 64; }
    int PageY() const { return Y + 124; }
    int Row1Y() const { return Y + 104 + (Tabs ? 60 : 0); }
    int Row2Y() const { return Y + 144 + (Tabs ? 60 : 0); }
    int BtnX(int row, int k) const { static const int w1[5] = { 0, 102, 204, 306, 408 }, w2[3] = { 0, 118, 236 }; return RX() + (row == 1 ? w1[k] : w2[k]); }
    int BtnW(int row) const { return row == 1 ? 96 : 112; }
    int BtnY() const { return Y + H - 16 - BtnH; }
    int OkX() const { return X + W - 16 - 90; }
    int CancelX() const { return OkX() - 10 - 90; }
    SuperListBox* Lb() { return List.get(); }
    int Selected() const { return Sel; }
    const std::string& FieldText() const { return Field.text; }
    bool FieldFocused() const { return Focus == F_FIELD; }
    bool PageFocused() const { return Focus == F_PAGE; }
    const std::string& PageText() const { return PageField.text; }
    const std::string& Message() const { return Msg; }

    void Event(const InputEvent& e) {
        switch (e.type) {
        case InputEvent::DOWN: if (e.button == SDL_BUTTON_LEFT) Down(e); break;
        case InputEvent::MOVE:
            if (Drag == D_LIST) { Pointer(Started ? SuperPointerEvent::DRAG_MOVE : SuperPointerEvent::DRAG_START, e.x, e.y); Started = true; }
            else if (Drag == D_TEXT) { if (Focus == F_PAGE) PageField.SetCaretFromX(e.x - RX() - 6, true, Measure()); else { Field.SetCaretFromX(e.x - RX() - 6, true, Measure()); Reveal(); } }
            break;
        case InputEvent::UP: if (Drag == D_LIST) Pointer(SuperPointerEvent::DRAG_END, e.x, e.y); Drag = D_NONE; break;
        case InputEvent::WHEEL: if (e.x >= X && e.x < X + W && e.y >= Y && e.y < Y + H) { List->EditIO()->InjectDelta(-e.wheel * 3); Pump(); } break;
        case InputEvent::KEY_DOWN: Key(e); break;
        case InputEvent::TEXT: if (Focus == F_FIELD) { Field.Insert(e.text); Edited(); Reveal(); } else if (Focus == F_PAGE) { PageField.Insert(e.text); PageEdited(); } break;
        default: break;
        }
    }

    void Draw(uint32_t ticks) {
        lgfx::LGFX_Sprite& g = *G;
        g.fillRectAlpha(0, 0, FD::WIN_W, FD::WIN_H, 150, 0x0000);
        g.fillRect(X, Y, W, H, COL_PANEL); g.drawRect(X, Y, W, H, COL_ACCENT); g.drawRect(X + 1, Y + 1, W - 2, H - 2, COL_ACCENT);
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left);
        g.setTextColor(COL_ACCENT); g.drawString(Title.c_str(), X + 16, Y + 12);
        if (items.empty()) { g.setTextColor(COL_DIM); g.drawString("(no items: click Add, or Paste lines)", X + 24, Y + 52); }
        List->Push();
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left); g.setTextColor(0xFFFF);
        g.drawString("Item text:", RX(), FieldY() - 22);
        bool bad = Sel >= 0 && Field.text != items[Sel];                    // typed text that was refused (empty)
        DrawField(g, Field, RX(), FieldY(), RW(), 26, Focus == F_FIELD, bad, ticks);
        if (Tabs) {
            g.setTextColor(0xFFFF); g.drawString("Page group (control group shown with this tab, empty = none):", RX(), PageY() - 22);
            bool pbad = Sel >= 0 && PageField.text != pages[(size_t)Sel];
            DrawField(g, PageField, RX(), PageY(), 300, 26, Focus == F_PAGE, pbad, ticks);
        }
        static const char* l1[5] = { "Add", "Insert", "Delete", "Move up", "Move down" }; static const char* l2[3] = { "Sort A-Z", "Paste lines", "Clear all" };
        for (int k = 0; k < 5; ++k) { SuperPushButton b(G, BtnX(1, k), Row1Y(), BtnW(1), &lgfx::fonts::Font2, l1[k]); b.Push(); }
        for (int k = 0; k < 3; ++k) { SuperPushButton b(G, BtnX(2, k), Row2Y(), BtnW(2), &lgfx::fonts::Font2, l2[k]); b.Push(); }
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left);
        char b[80]; snprintf(b, sizeof b, "%d item%s", (int)items.size(), items.size() == 1 ? "" : "s");
        g.setTextColor(0xFFFF); g.drawString(b, RX() + 360, Row2Y() + 6);
        g.setTextColor(COL_DIM); g.drawString("at most 200 items, each 1 to 60 characters (printable ASCII)", RX(), Row2Y() + 34);
        if (!Msg.empty()) { g.setTextColor(COL_ERR); g.drawString(Msg.c_str(), RX(), Row2Y() + 52); }
        // the real control with these items
        int py = Y + 210 + (Tabs ? 60 : 0), ph = BtnY() - 14 - py;
        g.fillRect(RX(), py, RW(), ph, FormBg); g.drawRect(RX(), py, RW(), ph, COL_LINE);
        DControl t = Proto; t.list = items; t.pages = pages; t.x = 0; t.y = 0; t.w = std::min(RW() - 24, std::max(Proto.w, 120));
        if (Sel >= 0) FD::ExtraSet(t, *FindExtra(t.type, "selected"), std::to_string(Sel));
        g.setClipRect(RX() + 1, py + 1, RW() - 2, ph - 2);
        MakeControl(&g, t, RX() + 12, py + 12)->Push();
        g.clearClipRect();
        SuperPushButton cancel(G, CancelX(), BtnY(), 90, &lgfx::fonts::Font2, "Cancel"), ok(G, OkX(), BtnY(), 90, &lgfx::fonts::Font2, "OK");
        ok.ColBorder = COL_ACCENT; cancel.Push(); ok.Push();
    }

private:
    enum FocusId { F_LIST, F_FIELD, F_PAGE } Focus = F_LIST;
    enum Drag { D_NONE, D_LIST, D_TEXT } Drag = D_NONE;
    lgfx::LGFX_Sprite* G; std::string Title; DControl Proto; const DForm& Form; uint16_t FormBg; bool Tabs;
    std::unique_ptr<SuperListBox> List; TextField Field, PageField; std::string Msg;
    int Sel = -1, BtnH = 20, PressX = 0, PressY = 0; bool Started = false;

    std::function<int(const std::string&)> Measure() { return [this](const std::string& s) { return MeasureText(*G, s); }; }
    void Reveal() { Field.EnsureVisible(Measure(), RW() - 12); }
    static std::string Clean(const std::string& s) {          // printable ASCII, trimmed, at most MAX_ITEM_LEN
        std::string v; for (char c : s) if (c >= 0x20 && c < 0x7F) v += c;
        size_t a = v.find_first_not_of(' '), b = v.find_last_not_of(' ');
        v = a == std::string::npos ? "" : v.substr(a, b - a + 1);
        if (v.size() > (size_t)MAX_ITEM_LEN) v.resize(MAX_ITEM_LEN);
        return v;
    }
    void Rebuild() {                                          // list box and field follow `items` / `Sel`
        if (!items.empty()) { List->SetItems(items); List->SetSelected(Sel < 0 ? 0 : Sel); List->StartEdit(); if (Sel < 0) Sel = 0; }
        else { List->SetItems({}); Sel = -1; }
        Field.SetText(Sel >= 0 ? items[Sel] : ""); Field.SelectAll(); PageField.SetText(Sel >= 0 ? pages[(size_t)Sel] : "");
    }
    void Pump() {                                             // run the list box; a lift "accepts" a row, so keep editing; follow its cursor
        if (items.empty()) return;
        SuperEditState s = List->Service();
        if (s == EDIT_ACCEPTED || s == EDIT_CANCELLED) List->StartEdit();
        int c = List->GetCursor();
        if (c != Sel && c >= 0 && c < (int)items.size()) { Sel = c; Msg.clear(); Field.SetText(items[Sel]); Field.SelectAll(); PageField.SetText(pages[(size_t)Sel]); }
    }
    void Pointer(SuperPointerEvent::Kind k, int x, int y) {
        SuperPointerEvent p; p.Type = k; p.X = (int16_t)x; p.Y = (int16_t)y; p.StartX = (int16_t)PressX; p.StartY = (int16_t)PressY;
        List->OnPointer(p); Pump();
    }
    bool Full() { if ((int)items.size() >= MAX_LIST_ITEMS) { Msg = "a list has at most 200 items"; return true; } return false; }
    void Edited() {                                           // the text field changed: apply it to the selected item (strict: not empty)
        if (Sel < 0) return;
        std::string v = Clean(Field.text);
        if (v.empty()) { Msg = "an item cannot be empty"; return; }
        Msg.clear(); items[Sel] = v; List->SetItem(Sel, v);
    }
    void PageEdited() {                                       // the page group of the selected tab (strict: a safe name nobody else has, or empty)
        if (Sel < 0) return; const std::string& v = PageField.text;
        if (!v.empty() && !ValidName(v)) { Msg = v + " is not a safe C++ name (a keyword, a reserved or library name, or a macro)"; return; }
        if (!v.empty() && Form.GroupNameTaken(v)) { Msg = v + " is already used by a control, a radio group, a menu item or the form's classes"; return; }
        Msg.clear(); pages[(size_t)Sel] = v;
    }
    void Add(int at, const char* text) {                      // a new item before index `at` (items.size() = at the end)
        if (Full()) return;
        std::string t = text; if (t.empty()) { int n = (int)items.size() + 1; do { t = "Item " + std::to_string(n++); } while (std::find(items.begin(), items.end(), t) != items.end()); }
        at = std::max(0, std::min(at, (int)items.size())); items.insert(items.begin() + at, t); pages.insert(pages.begin() + at, ""); Sel = at; Msg.clear(); Rebuild(); Focus = F_FIELD;
    }
    void Delete() { if (Sel < 0) return; items.erase(items.begin() + Sel); pages.erase(pages.begin() + Sel); if (Sel >= (int)items.size()) Sel = (int)items.size() - 1; Msg.clear(); Rebuild(); }
    void Move(int d) { int j = Sel + d; if (Sel < 0 || j < 0 || j >= (int)items.size()) return; std::swap(items[Sel], items[j]); std::swap(pages[(size_t)Sel], pages[(size_t)j]); Sel = j; Rebuild(); }
    void Sort() {
        if (items.size() < 2) return;
        std::string keep = Sel >= 0 ? items[Sel] : "";
        std::vector<int> idx(items.size()); for (size_t i = 0; i < idx.size(); ++i) idx[i] = (int)i;
        auto lower = [](std::string x) { for (char& c : x) c = (char)tolower((unsigned char)c); return x; };
        std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) { return lower(items[(size_t)a]) < lower(items[(size_t)b]); });
        std::vector<std::string> ni, np; for (int i : idx) { ni.push_back(items[(size_t)i]); np.push_back(pages[(size_t)i]); } items = ni; pages = np;
        Sel = (int)(std::find(items.begin(), items.end(), keep) - items.begin()); if (Sel >= (int)items.size()) Sel = 0; Rebuild();
    }
    void PasteLines() {
        char* clip = SDL_GetClipboardText(); if (!clip) return;
        std::string all = clip; SDL_free(clip);
        int added = 0, at = Sel >= 0 ? Sel + 1 : (int)items.size(); size_t p = 0;
        while (p <= all.size()) {
            size_t q = all.find_first_of("\r\n", p); if (q == std::string::npos) q = all.size();
            std::string line = Clean(all.substr(p, q - p)); p = q + 1;
            if (line.empty()) continue;
            if ((int)items.size() >= MAX_LIST_ITEMS) { Msg = "a list has at most 200 items: the rest was not added"; break; }
            items.insert(items.begin() + at, line); pages.insert(pages.begin() + at, ""); ++at; ++added;
        }
        if (added) { Sel = at - 1; if (Msg.empty()) Msg.clear(); Rebuild(); } else if (Msg.empty()) Msg = "the clipboard has no lines to add";
    }
    void ClearAll() { items.clear(); pages.clear(); Sel = -1; Msg.clear(); Rebuild(); Focus = F_LIST; }
    void Down(const InputEvent& e) {
        auto in = [&](int x, int y, int w, int h) { return e.x >= x && e.x < x + w && e.y >= y && e.y < y + h; };
        if (Tabs && in(RX(), PageY(), 300, 26)) { if (Sel >= 0) { Focus = F_PAGE; Drag = D_TEXT; PageField.SetCaretFromX(e.x - RX() - 6, (SDL_GetModState() & KMOD_SHIFT) != 0, Measure()); if (e.clicks >= 3) PageField.SelectAll(); } return; }
        if (in(RX(), FieldY(), RW(), 26)) { if (Sel >= 0) { Focus = F_FIELD; Drag = D_TEXT; Field.SetCaretFromX(e.x - RX() - 6, (SDL_GetModState() & KMOD_SHIFT) != 0, Measure());
                                                     if (e.clicks == 2) Field.SelectWord(Field.caret); else if (e.clicks >= 3) Field.SelectAll(); Reveal(); } return; }
        for (int k = 0; k < 5; ++k) if (in(BtnX(1, k), Row1Y(), BtnW(1), BtnH)) {
            if (k == 0) Add(Sel >= 0 ? Sel + 1 : 0, ""); else if (k == 1) Add(Sel >= 0 ? Sel : 0, ""); else if (k == 2) Delete(); else Move(k == 3 ? -1 : 1);
            return; }
        for (int k = 0; k < 3; ++k) if (in(BtnX(2, k), Row2Y(), BtnW(2), BtnH)) { if (k == 0) Sort(); else if (k == 1) PasteLines(); else ClearAll(); return; }
        if (!items.empty() && List->RowAt(e.x, e.y) >= 0) { Focus = F_LIST; Drag = D_LIST; Started = false; PressX = e.x; PressY = e.y; Pointer(SuperPointerEvent::PRESS, e.x, e.y); return; }
        if (in(OkX(), BtnY(), 90, BtnH)) { state = OK; return; }
        if (in(CancelX(), BtnY(), 90, BtnH)) { state = CANCEL; return; }
    }
    void Key(const InputEvent& e) {
        bool shift = e.mod & KMOD_SHIFT;
        if (Focus == F_PAGE) {
            if (PageField.Key(e)) { PageEdited(); return; }
            if (e.key == SDLK_RETURN || e.key == SDLK_KP_ENTER || e.key == SDLK_ESCAPE) { Focus = F_LIST; if (Sel >= 0) PageField.SetText(pages[(size_t)Sel]); Msg.clear(); }
            else if (e.key == SDLK_TAB) { Focus = F_LIST; if (Sel >= 0) PageField.SetText(pages[(size_t)Sel]); Msg.clear(); }
            return;
        }
        if (Focus == F_FIELD) {
            if (Field.Key(e)) { Edited(); Reveal(); return; }
            if (e.key == SDLK_RETURN || e.key == SDLK_KP_ENTER) { Add(Sel + 1, ""); return; }          // fast entry: the next item
            if (e.key == SDLK_TAB && Tabs) { Focus = F_PAGE; PageField.SelectAll(); Msg.clear(); if (Sel >= 0) Field.SetText(items[Sel]); return; }
            if (e.key == SDLK_ESCAPE || e.key == SDLK_TAB) { Focus = F_LIST; if (Sel >= 0) Field.SetText(items[Sel]); Msg.clear(); }
            return;
        }
        SuperEditIO* io = List->EditIO();
        switch (e.key) {
        case SDLK_RETURN: case SDLK_KP_ENTER: state = OK; break;
        case SDLK_ESCAPE: state = CANCEL; break;
        case SDLK_TAB: if (Sel >= 0) { Focus = F_FIELD; Field.SelectAll(); } (void)shift; break;
        case SDLK_INSERT: Add(Sel >= 0 ? Sel + 1 : 0, ""); break;
        case SDLK_DELETE: Delete(); break;
        case SDLK_UP:       if (!items.empty()) { io->InjectDelta(-1); Pump(); } break;
        case SDLK_DOWN:     if (!items.empty()) { io->InjectDelta(1); Pump(); } break;
        case SDLK_PAGEUP:   if (!items.empty()) { io->InjectDelta(-(ROWS - 1)); Pump(); } break;
        case SDLK_PAGEDOWN: if (!items.empty()) { io->InjectDelta(ROWS - 1); Pump(); } break;
        case SDLK_HOME:     if (!items.empty()) { io->InjectDelta(-100000); Pump(); } break;
        case SDLK_END:      if (!items.empty()) { io->InjectDelta(100000); Pump(); } break;
        default: break;
        }
    }
};
}
