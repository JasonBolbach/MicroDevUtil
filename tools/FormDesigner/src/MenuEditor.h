#pragma once
// A modal menu editor for the Form Designer (SuperMenu items: a tree), drawn inside the designer window.
//
//   tree (the real SuperListBox)          |  Text      [ ........ ]   the selected item
//     System                              |  Item ID   [ MNU_REBOOT ] a C++-safe name: static constexpr uint16_t MNU_REBOOT = 7;
//         Reboot        [MNU_REBOOT = 7]  |  Id        [ 7 ]          unique within the menu, 0..65535
//         Power off     [MNU_POWER_OFF = 8]  [Add] [Add child] [Delete] [Move up] [Move down]
//     About             [MNU_ABOUT = 20]     [Indent] [Outdent] [Paste outline] [Clear all]
//                                         |  preview: the real SuperMenu
//
// Every command (a leaf) has an Item ID and an Id. New items get the next free number and a name made from their text (<prefix><TEXT>, e.g.
// MNU_REBOOT) that follows the text until you edit it yourself. A parent item (it has sub-items) may have an Item ID and an Id too, or neither.
// Strict: an Item ID that is not a safe C++ name, or is already used, and an Id that is used twice or out of range are refused (red, with the reason).
// "Paste outline": every non-empty clipboard line becomes an item, indentation (spaces or tabs) makes sub-items; they are added at the end.
// Keys: Up / Down / Home / End / PageUp / PageDown in the tree, Insert = Add, Delete, Tab = next field, in the Text field Enter adds the next sibling,
//       Esc in a field = back to the tree, Esc in the tree = Cancel, Enter in the tree = OK.
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

namespace FD {
class MenuEditor {
public:
    enum State { OPEN, OK, CANCEL } state = OPEN;
    std::vector<MenuNode> nodes;                              // the tree being edited (the result when state == OK)

    // proto: the menu control (colours, font, idPrefix ...); form + menuIndex: where it lives (item ids must not clash with the form's other names)
    MenuEditor(lgfx::LGFX_Sprite* g, const std::string& title, const DControl& proto, const DForm& form, int menuIndex, uint16_t formBg)
        : nodes(proto.menu), G(g), Title(title), Proto(proto), Form(form), Mi(menuIndex), FormBg(formBg), Prefix(XStr(proto, "idPrefix")) {
        List.reset(new SuperListBox(G, X + 16, Y + 44, LIST_W, &lgfx::fonts::Font2, ROWS, nullptr, nullptr, nullptr));
        FText.maxLen = MAX_MENU_TEXT; FId.maxLen = 24; FNum.maxLen = 5; FNum.filter = TextField::DIGITS; FId.filter = TextField::IDENT;
        SuperPushButton probe(G, 0, 0, 90, &lgfx::fonts::Font2, "OK"); BtnH = probe.GetHeight();
        Sel = nodes.empty() ? -1 : 0; Rebuild();
    }

    static constexpr int W = 1040, H = 620, X = (FD::WIN_W - W) / 2, Y = (FD::WIN_H - H) / 2, LIST_W = 470, ROWS = 17;
    // geometry (public: the self-test aims at it)
    int RX() const { return X + 16 + LIST_W + 16; }
    int RW() const { return W - 16 - (16 + LIST_W + 16); }
    int FieldY(int k) const { return Y + 64 + k * 56; }                  // 0 Text, 1 Item ID, 2 Id
    int FieldW(int k) const { return k == 2 ? 120 : RW(); }
    int Row1Y() const { return Y + 262; }
    int Row2Y() const { return Y + 302; }
    int BtnX(int row, int k) const { return RX() + k * (row == 1 ? 106 : 132); }
    int BtnW(int row) const { return row == 1 ? 100 : 126; }
    int BtnY() const { return Y + H - 16 - BtnH; }
    int OkX() const { return X + W - 16 - 90; }
    int CancelX() const { return OkX() - 10 - 90; }
    SuperListBox* Lb() { return List.get(); }
    int Selected() const { return Sel; }
    int Rows() const { return (int)Flat.size(); }
    std::string PreviewLevel() const { return PreviewPath(); }           // the sub-menu level the preview shows (the selected item's level; "" = top)
    std::string RowText(int i) const { return i >= 0 && i < (int)Flat.size() ? Label(i) : ""; }
    int FocusedField() const { return Focus; }                         // -1 tree, 0 Text, 1 Item ID, 2 Id
    const std::string& FieldText(int k) const { return (k == 0 ? FText : k == 1 ? FId : FNum).text; }
    const std::string& Message() const { return Msg; }
    const MenuNode* SelectedNode() const { return Sel >= 0 ? At(Flat[Sel]) : nullptr; }

    void Event(const InputEvent& e) {
        switch (e.type) {
        case InputEvent::DOWN: if (e.button == SDL_BUTTON_LEFT) Down(e); break;
        case InputEvent::MOVE:
            if (Drag == D_LIST) { Pointer(Started ? SuperPointerEvent::DRAG_MOVE : SuperPointerEvent::DRAG_START, e.x, e.y); Started = true; }
            else if (Drag == D_TEXT && Focus >= 0) { Cur()->SetCaretFromX(e.x - RX() - 6, true, Measure()); }
            break;
        case InputEvent::UP: if (Drag == D_LIST) Pointer(SuperPointerEvent::DRAG_END, e.x, e.y); Drag = D_NONE; break;
        case InputEvent::WHEEL: if (e.x >= X && e.x < X + W && e.y >= Y && e.y < Y + H) { List->EditIO()->InjectDelta(-e.wheel * 3); Pump(); } break;
        case InputEvent::KEY_DOWN: Key(e); break;
        case InputEvent::TEXT: if (Focus >= 0) { Cur()->Insert(e.text); Edited(); } break;
        default: break;
        }
    }

    void Draw(uint32_t ticks) {
        lgfx::LGFX_Sprite& g = *G;
        g.fillRectAlpha(0, 0, FD::WIN_W, FD::WIN_H, 150, 0x0000);
        g.fillRect(X, Y, W, H, COL_PANEL); g.drawRect(X, Y, W, H, COL_ACCENT); g.drawRect(X + 1, Y + 1, W - 2, H - 2, COL_ACCENT);
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left);
        g.setTextColor(COL_ACCENT); g.drawString(Title.c_str(), X + 16, Y + 12);
        if (nodes.empty()) { g.setTextColor(COL_DIM); g.drawString("(no items: Add, or Paste outline)", X + 24, Y + 52); }
        List->Push();
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left); g.setTextColor(0xFFFF);
        static const char* lab[3] = { "Text", "Item ID (C++ name of the constant)", "Id (number)" };
        for (int k = 0; k < 3; ++k) {
            g.setTextColor(0xFFFF); g.drawString(lab[k], RX(), FieldY(k) - 22);
            TextField& f = k == 0 ? FText : k == 1 ? FId : FNum; const MenuNode* n = SelectedNode();
            std::string model = !n ? "" : k == 0 ? n->text : k == 1 ? n->itemId : (n->id >= 0 ? std::to_string(n->id) : "");
            DrawField(g, f, RX(), FieldY(k), FieldW(k), 26, Focus == k, n && f.text != model, ticks);
        }
        if (!Msg.empty()) { g.setTextColor(COL_ERR); g.drawString(Msg.c_str(), RX(), FieldY(2) + 32); }
        static const char* l1[5] = { "Add", "Add child", "Delete", "Move up", "Move down" }, *l2[4] = { "Indent", "Outdent", "Paste outline", "Clear all" };
        for (int k = 0; k < 5; ++k) { SuperPushButton b(G, BtnX(1, k), Row1Y(), BtnW(1), &lgfx::fonts::Font2, l1[k]); b.Push(); }
        for (int k = 0; k < 4; ++k) { SuperPushButton b(G, BtnX(2, k), Row2Y(), BtnW(2), &lgfx::fonts::Font2, l2[k]); b.Push(); }
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left); g.setTextColor(COL_DIM);
        char b[100]; snprintf(b, sizeof b, "%d items (%d commands), ID prefix %s", MenuNodeCount(nodes), Commands(), Prefix.c_str());
        g.drawString(b, RX(), Row2Y() + 36);
        g.drawString("~ = separator line   ~Text = heading (no IDs)   ~~Text = a real item starting with ~", RX(), Row2Y() + 54);
        // the real menu
        int py = Y + 380, ph = BtnY() - 14 - py;
        g.fillRect(RX(), py, RW(), ph, FormBg); g.drawRect(RX(), py, RW(), ph, COL_LINE);
        DControl t = Proto; t.menu = nodes; t.extra.erase("previewLevel"); { std::string lv = PreviewPath(); if (!lv.empty()) t.extra["previewLevel"] = lv; } t.x = 0; t.y = 0; t.w = std::min(RW() - 24, std::max(Proto.w, 140));
        g.setClipRect(RX() + 1, py + 1, RW() - 2, ph - 2); MakeControl(&g, t, RX() + 12, py + 12)->Push(); g.clearClipRect();
        SuperPushButton cancel(G, CancelX(), BtnY(), 90, &lgfx::fonts::Font2, "Cancel"), ok(G, OkX(), BtnY(), 90, &lgfx::fonts::Font2, "OK");
        ok.ColBorder = COL_ACCENT; cancel.Push(); ok.Push();
    }

private:
    using Path = std::vector<int>;
    enum Drag { D_NONE, D_LIST, D_TEXT } Drag = D_NONE;
    lgfx::LGFX_Sprite* G; std::string Title; DControl Proto; const DForm& Form; int Mi; uint16_t FormBg; std::string Prefix;
    std::unique_ptr<SuperListBox> List; TextField FText, FId, FNum; std::string Msg;
    std::vector<Path> Flat; int Sel = -1, Focus = -1, BtnH = 20, PressX = 0, PressY = 0; bool Started = false;

    // ---- the tree ----
    MenuNode* At(const Path& p) { std::vector<MenuNode>* v = &nodes; MenuNode* n = nullptr; for (int i : p) { n = &(*v)[(size_t)i]; v = &n->kids; } return n; }
    const MenuNode* At(const Path& p) const { return const_cast<MenuEditor*>(this)->At(p); }
    std::vector<MenuNode>& Sibs(const Path& p) { Path up(p.begin(), p.end() - 1); return up.empty() ? nodes : At(up)->kids; }
    void Flatten(const std::vector<MenuNode>& v, Path& cur) { for (size_t i = 0; i < v.size(); ++i) { cur.push_back((int)i); Flat.push_back(cur); Flatten(v[i].kids, cur); cur.pop_back(); } }
    std::string Label(int i) const {
        const MenuNode* n = At(Flat[(size_t)i]); std::string s((Flat[(size_t)i].size() - 1) * 4, ' ');
        s += n->kids.empty() ? "  " : "> "; s += n->text;
        if (!n->itemId.empty()) s += "   [" + n->itemId + " = " + std::to_string(n->id) + "]";
        return s;
    }
    // The level of the real menu to preview: the one that holds the selected item (its siblings, with the Back row), as a path of parent texts; "" = the top level.
    std::string PreviewPath() const {
        if (Sel < 0 || Sel >= (int)Flat.size()) return "";
        const Path& p = Flat[(size_t)Sel]; std::string r; const std::vector<MenuNode>* v = &nodes;
        for (size_t i = 0; i + 1 < p.size(); ++i) { const MenuNode& n = (*v)[(size_t)p[i]]; if (n.text.find('/') != std::string::npos) return ""; r += (r.empty() ? "" : "/") + n.text; v = &n.kids; }
        return r;
    }
    int Commands() const { int c = 0; MenuForEach(nodes, [&](const MenuNode& k) { if (!k.itemId.empty()) ++c; }); return c; }
    void Rebuild(const Path* select = nullptr) {                 // tree -> flat list -> list box, fields
        Flat.clear(); Path cur; Flatten(nodes, cur);
        if (select) for (size_t i = 0; i < Flat.size(); ++i) if (Flat[i] == *select) Sel = (int)i;
        if (Flat.empty()) Sel = -1; else Sel = std::max(0, std::min(Sel, (int)Flat.size() - 1));
        std::vector<std::string> items; for (int i = 0; i < (int)Flat.size(); ++i) items.push_back(Label(i));
        if (!items.empty()) { List->SetItems(items); List->SetSelected(Sel); List->StartEdit(); } else List->SetItems({});
        SyncFields();
    }
    void SyncFields() {
        const MenuNode* n = SelectedNode();
        FText.SetText(n ? n->text : ""); FId.SetText(n ? n->itemId : ""); FNum.SetText(n && n->id >= 0 ? std::to_string(n->id) : "");
    }
    bool NameInTree(const std::string& s, const MenuNode* self) const { bool hit = false; MenuForEach(nodes, [&](const MenuNode& k) { if (&k != self && k.itemId == s) hit = true; }); return hit; }
    bool IdInTree(int id, const MenuNode* self) const { bool hit = false; MenuForEach(nodes, [&](const MenuNode& k) { if (&k != self && k.id == id) hit = true; }); return hit; }
    std::string Auto(const std::string& text, const MenuNode* self) const {
        return AutoItemId(Prefix, text, [&](const std::string& s) { return Form.NameTaken(s, -1, Mi) || NameInTree(s, self); });
    }
    int NextId() const { return std::max(0, MenuMaxId(nodes)) + 1; }
    void MakeCommand(MenuNode& n) { if (IsSepNode(n)) { n.itemId.clear(); n.id = -1; return; } if (n.id < 0) n.id = NextId(); if (n.itemId.empty()) n.itemId = Auto(n.text, &n); }

    // ---- editing ----
    TextField* Cur() { return Focus == 0 ? &FText : Focus == 1 ? &FId : Focus == 2 ? &FNum : nullptr; }
    std::function<int(const std::string&)> Measure() { return [this](const std::string& s) { return MeasureText(*G, s); }; }
    void Edited() {                                           // a field changed: apply it to the selected item if it is valid (strict: otherwise the old value stays)
        MenuNode* n = Sel >= 0 ? At(Flat[(size_t)Sel]) : nullptr; if (!n) return;
        Msg.clear();
        if (Focus == 0) {
            std::string v = Trim(Ascii(FText.text)); if (v.size() > (size_t)MAX_MENU_TEXT) v.resize(MAX_MENU_TEXT);
            if (v.empty()) { Msg = "an item needs text"; return; }
            bool follows = !n->itemId.empty() && n->itemId == Auto(n->text, n);              // an automatic name follows the text
            n->text = v; if (follows) n->itemId = Auto(v, n);
            if (IsSepNode(*n)) { n->itemId.clear(); n->id = -1; }                             // "~" / "~Text": a separator or heading, not a command
            else if (n->kids.empty() && (n->itemId.empty() || n->id < 0)) MakeCommand(*n);      // (it was one: now it is an ordinary command again)
        } else if (IsSepNode(*n)) {
            Msg = "a separator or heading (~) is not a command: it has no Item ID or id";
        } else if (Focus == 1) {
            std::string v = FId.text;
            if (v.empty()) { if (!n->kids.empty()) { n->itemId.clear(); n->id = -1; } else Msg = "a command needs an Item ID"; }       // only a parent may have none
            else if (!ValidName(v)) Msg = v + " is not a safe C++ name (a keyword, a reserved or library name, or a macro)";
            else if (Form.NameTaken(v, -1, Mi) || NameInTree(v, n)) Msg = v + " is already used (a control, a group, the form's classes, or another item)";
            else { n->itemId = v; if (n->id < 0) n->id = NextId(); }
        } else if (Focus == 2) {
            if (FNum.text.empty()) { if (!n->kids.empty()) { n->itemId.clear(); n->id = -1; } else Msg = "a command needs an id"; }
            else { long id = strtol(FNum.text.c_str(), nullptr, 10);
                   if (id > MAX_MENU_ID) Msg = "an id is 0 to 65535";
                   else if (IdInTree((int)id, n)) Msg = "the id " + std::to_string(id) + " is already used in this menu";
                   else { n->id = (int)id; if (n->itemId.empty()) n->itemId = Auto(n->text, n); } }
        }
        std::vector<std::string> items; for (int i = 0; i < (int)Flat.size(); ++i) items.push_back(Label(i));
        List->SetItem(Sel, items[(size_t)Sel]);
    }
    void Pump() {
        if (Flat.empty()) return;
        SuperEditState s = List->Service();
        if (s == EDIT_ACCEPTED || s == EDIT_CANCELLED) List->StartEdit();
        int c = List->GetCursor();
        if (c != Sel && c >= 0 && c < (int)Flat.size()) { Sel = c; Msg.clear(); SyncFields(); }
    }
    void Pointer(SuperPointerEvent::Kind k, int x, int y) {
        SuperPointerEvent p; p.Type = k; p.X = (int16_t)x; p.Y = (int16_t)y; p.StartX = (int16_t)PressX; p.StartY = (int16_t)PressY;
        List->OnPointer(p); Pump();
    }
    bool Full() { if (MenuNodeCount(nodes) >= MAX_MENU_NODES) { Msg = "a menu has at most 200 items"; return true; } return false; }
    MenuNode NewNode() { MenuNode n; int k = MenuNodeCount(nodes) + 1; do { n.text = "Item " + std::to_string(k++); } while (false); MakeCommand(n); return n; }
    void Add(bool child) {
        if (Full()) return; Msg.clear(); MenuNode n = NewNode();
        if (Sel < 0) { nodes.push_back(n); Path p{ (int)nodes.size() - 1 }; Rebuild(&p); }
        else if (child) {
            Path p = Flat[(size_t)Sel]; if ((int)p.size() >= MAX_MENU_DEPTH) { Msg = "menus can be nested up to 5 levels"; return; }
            MenuNode* par = At(p); par->kids.push_back(n); p.push_back((int)par->kids.size() - 1); Rebuild(&p);
        } else { Path p = Flat[(size_t)Sel]; std::vector<MenuNode>& sb = Sibs(p); sb.insert(sb.begin() + p.back() + 1, n); p.back()++; Rebuild(&p); }
        Focus = 0; FText.SelectAll();
    }
    void Delete() {
        if (Sel < 0) return; Path p = Flat[(size_t)Sel]; std::vector<MenuNode>& sb = Sibs(p); sb.erase(sb.begin() + p.back()); Msg.clear();
        if (!sb.empty()) p.back() = std::min(p.back(), (int)sb.size() - 1); else if (p.size() > 1) p.pop_back(); else p.clear();
        if (p.empty()) { Sel = -1; Rebuild(); } else Rebuild(&p);
    }
    void MoveBy(int d) {
        if (Sel < 0) return; Path p = Flat[(size_t)Sel]; std::vector<MenuNode>& sb = Sibs(p); int j = p.back() + d;
        if (j < 0 || j >= (int)sb.size()) return; std::swap(sb[(size_t)p.back()], sb[(size_t)j]); p.back() = j; Rebuild(&p);
    }
    void Indent() {                                           // becomes the last child of the sibling above
        if (Sel < 0) return; Path p = Flat[(size_t)Sel]; if (p.back() == 0) { Msg = "there is no item above to become the parent"; return; }
        if ((int)p.size() >= MAX_MENU_DEPTH || MenuDepth(At(p)->kids) + (int)p.size() >= MAX_MENU_DEPTH) { Msg = "menus can be nested up to 5 levels"; return; }
        std::vector<MenuNode>& sb = Sibs(p); MenuNode n = sb[(size_t)p.back()]; sb.erase(sb.begin() + p.back());
        MenuNode& up = sb[(size_t)p.back() - 1]; up.kids.push_back(n); p.back()--; p.push_back((int)up.kids.size() - 1); Msg.clear(); Rebuild(&p);
    }
    void Outdent() {                                          // becomes a sibling after its parent
        if (Sel < 0) return; Path p = Flat[(size_t)Sel]; if (p.size() < 2) return;
        std::vector<MenuNode>& sb = Sibs(p); MenuNode n = sb[(size_t)p.back()]; sb.erase(sb.begin() + p.back());
        p.pop_back(); std::vector<MenuNode>& ps = Sibs(p); ps.insert(ps.begin() + p.back() + 1, n); p.back()++; Msg.clear(); Rebuild(&p);
    }
    void PasteOutline() {
        char* clip = SDL_GetClipboardText(); if (!clip) return; std::string all = clip; SDL_free(clip);
        struct L { int tabs, spaces; std::string text; }; std::vector<L> lines; size_t p0 = 0; int unit = 0;                 // a tab is one level; spaces: the smallest indent is one level
        while (p0 <= all.size()) {
            size_t q = all.find_first_of("\r\n", p0); if (q == std::string::npos) q = all.size();
            std::string raw = all.substr(p0, q - p0); p0 = q + 1; int tabs = 0, sp = 0; size_t i = 0;
            for (; i < raw.size() && (raw[i] == ' ' || raw[i] == '\t'); ++i) { if (raw[i] == '\t') ++tabs; else ++sp; }
            std::string t = Trim(Ascii(raw.substr(i))); if (t.size() > (size_t)MAX_MENU_TEXT) t.resize(MAX_MENU_TEXT);
            if (t.empty()) continue; lines.push_back({ tabs, sp, t }); if (sp > 0 && (unit == 0 || sp < unit)) unit = sp;
        }
        if (lines.empty()) { Msg = "the clipboard has no lines to add"; return; }
        int added = 0; std::vector<int> stack;                  // stack of indent levels -> path of the last item at each level
        std::vector<Path> paths; Path last;
        for (const L& l : lines) {
            if (MenuNodeCount(nodes) >= MAX_MENU_NODES) { Msg = "a menu has at most 200 items: the rest was not added"; break; }
            int depth = l.tabs + (unit ? l.spaces / unit : 0); depth = std::min(depth, (int)last.size());     // at most one level deeper than the previous item
            if (depth >= MAX_MENU_DEPTH) depth = MAX_MENU_DEPTH - 1;
            MenuNode n; n.text = l.text;
            if (depth == 0) { nodes.push_back(n); last = { (int)nodes.size() - 1 }; }
            else { last.resize((size_t)depth); MenuNode* par = At(last); par->kids.push_back(n); last.push_back((int)par->kids.size() - 1); }
            ++added;
        }
        MenuForEach(nodes, [&](MenuNode& k) { if (k.kids.empty() || !k.itemId.empty()) MakeCommand(k); });   // leaves become commands (the next ids, automatic names)
        // a pasted item that became a parent keeps no id of its own (it only opens a sub-menu)
        if (added) { Msg.clear(); Sel = std::max(0, Sel); Rebuild(); } 
    }
    void ClearAll() { nodes.clear(); Sel = -1; Msg.clear(); Focus = -1; Rebuild(); }
    void Down(const InputEvent& e) {
        auto in = [&](int x, int y, int w, int h) { return e.x >= x && e.x < x + w && e.y >= y && e.y < y + h; };
        for (int k = 0; k < 3; ++k) if (in(RX(), FieldY(k), FieldW(k), 26)) {
            if (Sel >= 0) { Focus = k; Drag = D_TEXT; TextField* f = Cur(); f->SetCaretFromX(e.x - RX() - 6, (SDL_GetModState() & KMOD_SHIFT) != 0, Measure());
                            if (e.clicks == 2) f->SelectWord(f->caret); else if (e.clicks >= 3) f->SelectAll(); }
            return; }
        for (int k = 0; k < 5; ++k) if (in(BtnX(1, k), Row1Y(), BtnW(1), BtnH)) { Focus = -1; if (k == 0) Add(false); else if (k == 1) Add(true); else if (k == 2) Delete(); else MoveBy(k == 3 ? -1 : 1); return; }
        for (int k = 0; k < 4; ++k) if (in(BtnX(2, k), Row2Y(), BtnW(2), BtnH)) { Focus = -1; if (k == 0) Indent(); else if (k == 1) Outdent(); else if (k == 2) PasteOutline(); else ClearAll(); return; }
        if (!Flat.empty() && List->RowAt(e.x, e.y) >= 0) { Focus = -1; Drag = D_LIST; Started = false; PressX = e.x; PressY = e.y; Pointer(SuperPointerEvent::PRESS, e.x, e.y); return; }
        if (in(OkX(), BtnY(), 90, BtnH)) { state = OK; return; }
        if (in(CancelX(), BtnY(), 90, BtnH)) { state = CANCEL; return; }
    }
    void Key(const InputEvent& e) {
        bool shift = e.mod & KMOD_SHIFT;
        if (Focus >= 0) {
            if (Cur()->Key(e)) { Edited(); return; }
            if (e.key == SDLK_RETURN || e.key == SDLK_KP_ENTER) { if (Focus == 0) Add(false); else { Focus = -1; SyncFields(); } return; }       // Enter in the Text field: the next sibling
            if (e.key == SDLK_ESCAPE) { Focus = -1; Msg.clear(); SyncFields(); return; }
            if (e.key == SDLK_TAB) { Focus = (Focus + (shift ? 2 : 1)) % 3; SyncFields(); Msg.clear(); Cur()->SelectAll(); }
            return;
        }
        SuperEditIO* io = List->EditIO();
        switch (e.key) {
        case SDLK_RETURN: case SDLK_KP_ENTER: state = OK; break;
        case SDLK_ESCAPE: state = CANCEL; break;
        case SDLK_TAB: if (Sel >= 0) { Focus = shift ? 2 : 0; Cur()->SelectAll(); } break;
        case SDLK_INSERT: Add(shift); break;
        case SDLK_DELETE: Delete(); break;
        case SDLK_UP:       if (!Flat.empty()) { io->InjectDelta(-1); Pump(); } break;
        case SDLK_DOWN:     if (!Flat.empty()) { io->InjectDelta(1); Pump(); } break;
        case SDLK_PAGEUP:   if (!Flat.empty()) { io->InjectDelta(-(ROWS - 1)); Pump(); } break;
        case SDLK_PAGEDOWN: if (!Flat.empty()) { io->InjectDelta(ROWS - 1); Pump(); } break;
        case SDLK_HOME:     if (!Flat.empty()) { io->InjectDelta(-100000); Pump(); } break;
        case SDLK_END:      if (!Flat.empty()) { io->InjectDelta(100000); Pump(); } break;
        default: break;
        }
    }
};
}
