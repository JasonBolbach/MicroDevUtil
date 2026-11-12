#pragma once
// SuperMenu - a nested menu built on SuperControl, driven by a SuperEncoder and two SuperButtons.
//
//     SuperMenu menu(&lcd, 10, 10, 200, &Font2, 5, &Enc, &BtnA, &BtnB);
//     auto* settings = menu.AddItem("Settings", 0);                  // has children below
//     menu.AddItem("Brightness", 11, settings);
//     menu.AddItem("Sound",      12, settings);
//     menu.AddItem("About",      20);
//     if (menu.BeginEdit()) { int id = menu.GetResultId(); ... }     // true = a leaf item was chosen
//
//  * Items are added with AddItem(text, id, parent); a nullptr parent adds to the top level. An item with
//    children shows a ">" and opens its sub-menu; a leaf item is a command with your own id.
//  * Rows = visible rows (the list scrolls, light gray triangles show hidden items).
//  * Editing: encoder moves the cursor (stops at the ends, or wraps if Wrap is true),
//      ButtonA short/long = open a sub-menu, or choose a leaf (BeginEdit() returns true),
//      ButtonB short = back up one level (cancels the menu at the top level),
//      ButtonB long  = leave the menu from any depth (cancel).
//  * OnChange = the cursor moved / the level changed (live), OnAccept = a leaf was chosen.
//    GetResultId() / GetResultItem() describe the chosen leaf.
#include "SuperControl.h"
#include "SuperEditIO.h"
#include <deque>

class SuperMenu : public SuperControl {
public:
    struct Item {
        std::string Text;
        int  Id = 0;
        Item* Parent = nullptr;
        std::vector<Item*> Kids;
    };

    bool Wrap  = false;
    int  RowPad = 1;

    SuperMenu(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font, int rows,
              SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB)
        : SuperControl(dst, x, y, width, font), Rows(rows < 1 ? 1 : rows), IO(enc, btnA, btnB) {}

    void SetAbort(bool (*fn)()) { IO.Abort = fn; }
    void SetOnChange(void (*fn)(void*), void* user = nullptr) { IO.OnChange = fn; IO.OnChangeUser = user; }
    void SetOnAccept(void (*fn)(void*), void* user = nullptr) { IO.OnAccept = fn; IO.OnAcceptUser = user; }

    // The returned pointer stays valid; pass it as 'parent' to build sub-menus.
    Item* AddItem(const std::string& text, int id = 0, Item* parent = nullptr) {
        Pool.push_back(Item());
        Item* it = &Pool.back();
        it->Text = text; it->Id = id; it->Parent = parent;
        (parent ? parent->Kids : Top.Kids).push_back(it);
        Dirty = true;
        return it;
    }
    void Clear() { Pool.clear(); Top.Kids.clear(); Level = &Top; Cursor = First = 0; Result = nullptr; Dirty = true; }
    int  GetResultId() const { return Result ? Result->Id : -1; }
    const Item* GetResultItem() const { return Result; }
    const Item* GetCurrentLevel() const { return Level == &Top ? nullptr : Level; }   // nullptr = top level

    bool IsEditing() const override { return State == EDIT_ACTIVE; }
    int RowAt(int sx, int sy) {
        int x0 = PosX + Inset(), y0 = GetBoxTop() + Inset(), rh = RowH();
        if (sx < x0 || sx >= x0 + InnerW() || sy < y0 || sy >= y0 + Rows * rh) return -1;
        int idx = First + (sy - y0) / rh;
        return idx < (int)Level->Kids.size() ? idx : -1;
    }
    // Touch: tap a row = open / choose it, long press = back one level, tap outside = leave the menu, drag = move the cursor.
    bool OnPointer(const SuperPointerEvent& e) override {
        if (State != EDIT_ACTIVE) return false;
        bool in = HitTest(e.X, e.Y, TouchPad);
        if (e.Type == SuperPointerEvent::TAP) {
            int i = RowAt(e.X, e.Y);
            if (i >= 0) { if (i != Cursor) { Cursor = i; Keep(); IO.Changed(); } IO.Inject(SuperEditIO::A_SHORT); return true; }
            if (!in) { IO.Inject(SuperEditIO::B_LONG); return true; }
            return true;
        }
        if (e.Type == SuperPointerEvent::LONG_PRESS) { if (in) IO.Inject(SuperEditIO::B_SHORT); return in; }
        return SuperPointerDefault(IO, *this, e, RowH());
    }
    bool StartEdit() override {
        if (Top.Kids.empty()) return false;
        Level = &Top; Cursor = First = 0; Result = nullptr;
        IO.Flush(); State = EDIT_ACTIVE; Push(); return true;
    }
    SuperEditState Service() override {
        if (State != EDIT_ACTIVE) return EDIT_IDLE;
        if (IO.Aborted()) return Finish(EDIT_CANCELLED);
        bool redraw = false;
        int n = (int)Level->Kids.size();
        int32_t d = IO.Delta();
        if (d && n) {
            int step = d > 0 ? 1 : -1;
            int nc = Wrap ? ((Cursor + step) % n + n) % n : std::clamp(Cursor + step, 0, n - 1);
            if (nc != Cursor) { Cursor = nc; Keep(); redraw = true; IO.Changed(); }
        }
        switch (IO.NextKey()) {
        case SuperEditIO::A_SHORT:
        case SuperEditIO::A_LONG: {
            if (!n) break;
            Item* it = Level->Kids[Cursor];
            if (!it->Kids.empty()) { Level = it; Cursor = First = 0; redraw = true; IO.Changed(); }
            else { Result = it; return Finish(EDIT_ACCEPTED); }
            break; }
        case SuperEditIO::B_SHORT:
            if (Level == &Top) return Finish(EDIT_CANCELLED);
            { Item* from = Level; Level = Level->Parent ? Level->Parent : &Top;
              Cursor = 0;
              for (int i = 0; i < (int)Level->Kids.size(); ++i) if (Level->Kids[i] == from) Cursor = i;
              First = 0; Keep(); redraw = true; IO.Changed(); }
            break;
        case SuperEditIO::B_LONG:  return Finish(EDIT_CANCELLED);
        default: break;
        }
        if (redraw) Push();
        return EDIT_ACTIVE;
    }

protected:
    int RowH() { return FontH() + 2 * RowPad; }
    int BodyHeight() override { return Rows * RowH(); }

    void RenderBody(int x, int y, int w, int h) override {
        int rh = RowH(), n = (int)Level->Kids.size();
        bool more = n > Rows;
        int gw = more ? rh / 2 + 3 : 0, tw = std::max(1, w - gw);
        bool editing = State == EDIT_ACTIVE;
        int mk = rh / 4 + 1;                                           // size of the ">" marker
        Spr.setFont(Font);
        Spr.setTextDatum(lgfx::textdatum_t::middle_left);
        for (int r = 0; r < Rows; ++r) {
            int idx = First + r;
            if (idx >= n) break;
            int ry = y + r * rh;
            Item* it = Level->Kids[idx];
            uint16_t fg = ColFg;
            if (editing && idx == Cursor) { Spr.fillRect(x, ry, tw, rh, ColCursor); fg = ColCursorFg; }
            Spr.setClipRect(x, ry, tw - (it->Kids.empty() ? 0 : mk * 2 + 2), rh);
            Spr.setTextColor(fg);
            Spr.drawString(it->Text.c_str(), x + 2, ry + rh / 2);
            Spr.clearClipRect();
            if (!it->Kids.empty()) {
                int cx = x + tw - mk - 2, cy = ry + rh / 2;
                Spr.fillTriangle(cx - mk / 2, cy - mk, cx - mk / 2, cy + mk, cx + mk / 2 + 1, cy, fg);
            }
        }
        if (more) {
            int s = std::max(2, rh / 4), cx = x + tw + gw / 2;
            if (First > 0)            Spr.fillTriangle(cx, y + rh / 2 - s, cx - s, y + rh / 2 + s, cx + s, y + rh / 2 + s, ColArrow);
            if (First + Rows < n)     Spr.fillTriangle(cx, y + h - rh / 2 + s, cx - s, y + h - rh / 2 - s, cx + s, y + h - rh / 2 - s, ColArrow);
        }
    }

private:
    int Rows;
    SuperEditIO IO;
    std::deque<Item> Pool;
    Item Top;
    Item* Level = &Top;
    Item* Result = nullptr;
    int Cursor = 0, First = 0;
    SuperEditState State = EDIT_IDLE;

    void Keep() {
        if (Cursor < First) First = Cursor;
        else if (Cursor >= First + Rows) First = Cursor - Rows + 1;
        First = std::max(0, First);
    }
    SuperEditState Finish(SuperEditState r) {
        State = EDIT_IDLE; Level = &Top; Cursor = First = 0; Push();
        if (r == EDIT_ACCEPTED) IO.Accepted();
        return r;
    }
};
