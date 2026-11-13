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
//  * Unclickable rows: a leaf item "~" is a separator line, "~Text" a heading centred in a line, "~~Text" an ordinary item that shows "~Text" (see SuperSeparator.h). The cursor skips them.
//  * OnChange = the cursor moved / the level changed (live), OnAccept = a leaf was chosen.
//    GetResultId() / GetResultItem() describe the chosen leaf.
#include "SuperControl.h"
#include "SuperEditIO.h"
#include "SuperSeparator.h"
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
    bool ShowBack = true;               // sub-menus start with a "< Back" row (touch-friendly; B short still goes back)
    int  RowPad = 1;

    SuperMenu(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font, int rows,
              SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB)
        : SuperControl(dst, x, y, width, font), Rows(rows < 1 ? 1 : rows), IO(enc, btnA, btnB) { BackItem.Text = "< Back"; }

    void SetAbort(bool (*fn)()) { IO.Abort = fn; }
    void SetOnChange(void (*fn)(void*), void* user = nullptr) { IO.OnChange = fn; IO.OnChangeUser = user; }
    void SetOnAccept(void (*fn)(void*), void* user = nullptr) { IO.OnAccept = fn; IO.OnAcceptUser = user; }

    // The returned pointer stays valid; pass it as 'parent' to build sub-menus.
    Item* AddItem(const std::string& text, int id = 0, Item* parent = nullptr) {
        Item* it;
        if (!Free.empty()) { it = Free.back(); Free.pop_back(); *it = Item(); }           // reuse an item that RemoveItem() released
        else { Pool.push_back(Item()); it = &Pool.back(); }
        it->Text = text; it->Id = id; it->Parent = parent;
        (parent ? parent->Kids : Top.Kids).push_back(it);
        Dirty = true;
        return it;
    }
    void Clear() { Pool.clear(); Free.clear(); Top.Kids.clear(); Level = &Top; Cursor = First = 0; Result = nullptr; Dirty = true; }
    // Run-time changes (not while the menu is open; they make it start again at the top level):
    // RemoveItem removes an item with everything below it; pointers to the removed items must not be used afterwards (the memory is reused by AddItem).
    bool RemoveItem(Item* it) {
        if (!it || it == &Top || it == &BackItem) return false;
        std::vector<Item*>& sib = it->Parent ? it->Parent->Kids : Top.Kids;
        auto pos = std::find(sib.begin(), sib.end(), it);
        if (pos == sib.end()) return false;                                                   // not an item of this menu (or already removed)
        sib.erase(pos);
        std::vector<Item*> stack{ it };
        while (!stack.empty()) {                                                              // the whole subtree goes to the free list
            Item* x = stack.back(); stack.pop_back();
            if (x == Result) Result = nullptr;
            for (Item* k : x->Kids) stack.push_back(k);
            x->Kids.clear(); Free.push_back(x);
        }
        Level = &Top; Cursor = First = 0; Dirty = true;
        return true;
    }
    // The first item (depth first, in menu order) with this id, or nullptr. Parents usually have id 0; leaves carry your command ids.
    const Item* FindById(int id) const { return Find(&Top, id); }
    Item* FindById(int id) { return const_cast<Item*>(static_cast<const SuperMenu*>(this)->FindById(id)); }
    const Item* Root() const { return &Top; }                                                 // Root()->Kids are the top-level items
    int  GetResultId() const { return Result ? Result->Id : -1; }
    const Item* GetResultItem() const { return Result; }
    const Item* GetCurrentLevel() const { return Level == &Top ? nullptr : Level; }   // nullptr = top level
    // Show a sub-menu level while the menu is idle (a preview, e.g. in a design tool): the items of `parent` (with the "< Back" row if ShowBack), or the top level for nullptr / a leaf.
    // StartEdit() always begins at the top level again.
    void ShowLevel(const Item* parent) { Level = (parent && !parent->Kids.empty()) ? const_cast<Item*>(parent) : &Top; Cursor = First = 0; Dirty = true; }

    SuperEditIO* EditIO() { return &IO; }                          // the input plumbing: InjectDelta / Inject for keyboards, mice and tests
    bool IsEditing() const override { return State == EDIT_ACTIVE; }
    bool IdleTapActs() const override { return true; }          // one tap on a row opens / chooses it
    int RowAt(int sx, int sy) {
        int x0 = PosX + Inset(), y0 = GetBoxTop() + Inset(), rh = RowH();
        if (sx < x0 || sx >= x0 + InnerW() || sy < y0 || sy >= y0 + Rows * rh) return -1;
        int idx = First + (sy - y0) / rh;
        return idx < Count() ? idx : -1;
    }
    // Touch: the highlighted row is always the one under the finger. Touch a row = it highlights at once (even when idle), slide up /
    // down = the highlight follows the finger, lift = do it (open the sub-menu, go back on "< Back", or choose the leaf). A tap is
    // therefore a one-touch choice. Holding the finger above / below the visible rows keeps scrolling. Lifting off to the side or
    // tapping outside leaves the menu.
    int AutoScrollMs = 120;
    int TouchGuardMs = 250;      // touches are ignored this long after the level changes: the row under the finger just became another item
    bool StartOnPress(int, int) override { return true; }
    bool OnPointer(const SuperPointerEvent& e) override {
        if (State != EDIT_ACTIVE) return false;
        if (LevelAt && (int)(lgfx::millis() - LevelAt) < TouchGuardMs) { TouchDown = false; return true; }
        switch (e.Type) {
        case SuperPointerEvent::PRESS: {
            int i = RowAt(e.X, e.Y);
            if (i < 0) return false;
            TouchDown = true; TouchY = e.Y;
            MoveCursor(i);
            return true; }
        case SuperPointerEvent::TAP: {
            TouchDown = false;
            int i = RowAt(e.X, e.Y);
            if (i >= 0) { if (Selectable(i)) { MoveCursor(i); IO.Inject(SuperEditIO::A_SHORT); } return true; }       // (a separator row: nothing happens)
            if (HitTest(e.X, e.Y, TouchPad)) return true;                       // on the frame: nothing
            IO.Inject(SuperEditIO::B_LONG); return true; }                      // outside: leave the menu
        case SuperPointerEvent::DRAG_START:
        case SuperPointerEvent::DRAG_MOVE:
            if (!(e.Type == SuperPointerEvent::DRAG_START ? HitTest(e.StartX, e.StartY, SuperControl::TouchPad) : TouchDown)) return false;
            TouchDown = true; TouchY = e.Y;
            FollowFinger();
            return true;
        case SuperPointerEvent::DRAG_END: {
            bool was = TouchDown;
            TouchDown = false;
            if (!was) return false;
            if (e.X < PosX - SuperControl::TouchPad || e.X >= PosX + GetWidth() + SuperControl::TouchPad) IO.Inject(SuperEditIO::B_LONG);   // off to the side: leave
            else if (Selectable(RowAt(e.X, e.Y))) IO.Inject(SuperEditIO::A_SHORT);                  // lifted over an item: do it (not over a separator)
            return true; }                                                                         // (over blank space: nothing happens)
        case SuperPointerEvent::LONG_PRESS:
            TouchDown = false;
            if (HitTest(e.X, e.Y, SuperControl::TouchPad)) { IO.Inject(SuperEditIO::A_SHORT); return true; }
            return false;
        }
        return SuperPointerDefault(IO, *this, e, RowH());
    }
    bool StartEdit() override {
        if (Top.Kids.empty()) return false;
        Level = &Top; Cursor = First = 0; Result = nullptr; TouchDown = false; LevelAt = 0;
        if (!Selectable(Cursor)) { int j = NextSel(Cursor, 1); if (j < 0) return false; Cursor = j; Keep(); }      // the first row that can be chosen
        IO.Flush(); State = EDIT_ACTIVE; PushUnlessPress(); return true;
    }
    SuperEditState Service() override {
        if (State != EDIT_ACTIVE) return EDIT_IDLE;
        if (IO.Aborted()) return Finish(EDIT_CANCELLED);
        bool redraw = false;
        int n = Count();
        if (TouchDown && AutoScroll()) redraw = true;
        int32_t d = IO.Delta();
        if (d && n) {
            int nc = NextSel(Cursor, d > 0 ? 1 : -1, Wrap);                      // separators and headings are skipped
            if (nc < 0) nc = Cursor;
            if (nc != Cursor) { Cursor = nc; Keep(); redraw = true; IO.Changed(); }
        }
        switch (IO.NextKey()) {
        case SuperEditIO::A_SHORT:
        case SuperEditIO::A_LONG: {
            if (!n) break;
            if (!Selectable(Cursor)) break;
            Item* it = At(Cursor);
            if (it == &BackItem) { GoUp(); LevelAt = lgfx::millis() | 1; redraw = true; IO.Changed(); }
            else if (!it->Kids.empty()) { Level = it; Cursor = HasBack() ? 1 : 0; First = 0; if (!Selectable(Cursor)) { int j = NextSel(Cursor, 1); if (j >= 0) Cursor = j; } Keep(); LevelAt = lgfx::millis() | 1; redraw = true; IO.Changed(); }
            else { Result = it; return Finish(EDIT_ACCEPTED); }
            break; }
        case SuperEditIO::B_SHORT:
            if (Level == &Top) return Finish(EDIT_CANCELLED);
            GoUp(); LevelAt = lgfx::millis() | 1; redraw = true; IO.Changed();
            break;
        case SuperEditIO::B_LONG:  return Finish(EDIT_CANCELLED);
        default: break;
        }
        if (redraw) Push();
        return EDIT_ACTIVE;
    }

protected:
    bool TouchDown = false; int TouchY = 0; uint32_t LastAuto = 0, LevelAt = 0;
    void MoveCursor(int i) { if (!Selectable(i)) return; if (i != Cursor) { Cursor = i; Keep(); IO.Changed(); Push(); } }
    void FollowFinger() {                                      // cursor = the visible row under the finger
        int rh = RowH(), y0 = GetBoxTop() + Inset();
        if (TouchY >= y0 && TouchY < y0 + Rows * rh && First + (TouchY - y0) / rh >= Count()) return;   // over blank space: keep the last row
        int r = TouchY < y0 ? 0 : std::clamp((TouchY - y0) / rh, 0, Rows - 1);
        MoveCursor(std::clamp(First + r, 0, Count() - 1));
    }
    bool AutoScroll() {                                        // finger held above / below the visible rows
        int rh = RowH(), y0 = GetBoxTop() + Inset(), n = Count();
        int dir = TouchY < y0 ? -1 : (TouchY >= y0 + Rows * rh ? 1 : 0);
        if (!dir || n == 0) return false;
        uint32_t now = lgfx::millis();
        if (now - LastAuto < (uint32_t)AutoScrollMs) return false;
        LastAuto = now;
        int nc = NextSel(Cursor, dir, false);
        if (nc < 0 || nc == Cursor) return false;
        Cursor = nc; Keep(); IO.Changed();
        return true;
    }
    int RowH() { return FontH() + 2 * RowPad; }
    int BodyHeight() override { return Rows * RowH(); }

    void RenderBody(int x, int y, int w, int h) override {
        int rh = RowH(), n = Count();
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
            Item* it = At(idx);
            uint16_t fg = ColFg;
            if (!Selectable(idx)) {                                    // a separator line or a heading: dim, never highlighted
                Spr.setClipRect(x, ry, tw, rh); SuperSep::Draw(Spr, it->Text, x, ry, tw, rh, ColArrow); Spr.clearClipRect();
                continue;
            }
            if (editing && idx == Cursor) { Spr.fillRect(x, ry, tw, rh, ColCursor); fg = ColCursorFg; }
            Spr.setClipRect(x, ry, tw - (it->Kids.empty() ? 0 : mk * 2 + 2), rh);
            Spr.setTextColor(fg);
            Spr.drawString(SuperSep::Shown(it->Text), x + 2, ry + rh / 2);
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
    std::vector<Item*> Free;                                   // items released by RemoveItem(), reused by AddItem()
    static const Item* Find(const Item* from, int id) {
        for (const Item* k : from->Kids) { if (k->Id == id) return k; if (const Item* f = Find(k, id)) return f; }
        return nullptr;
    }
    Item Top;
    Item* Level = &Top;
    Item* Result = nullptr;
    Item BackItem;
    int Cursor = 0, First = 0;
    SuperEditState State = EDIT_IDLE;

    bool HasBack() const { return ShowBack && Level != &Top; }
    int  Count() const { return (int)Level->Kids.size() + (HasBack() ? 1 : 0); }
    Item* At(int i) { return HasBack() ? (i == 0 ? &BackItem : Level->Kids[i - 1]) : Level->Kids[i]; }
    bool Selectable(int i) { return i >= 0 && i < Count() && !(At(i)->Kids.empty() && At(i) != &BackItem && SuperSep::IsUnclickable(At(i)->Text)); }
    int  NextSel(int from, int dir, bool wrap = false) {              // the next row that can be chosen; -1 = none
        int n = Count(); if (n == 0) return -1;
        for (int k = 1; k <= n; ++k) {
            int j = from + dir * k;
            if (wrap) j = ((j % n) + n) % n; else if (j < 0 || j >= n) return -1;
            if (Selectable(j)) return j;
        }
        return -1;
    }
    void GoUp() {                                              // back one level, cursor on the item we came from
        Item* from = Level; Level = Level->Parent ? Level->Parent : &Top;
        Cursor = 0;
        for (int i = 0; i < Count(); ++i) if (At(i) == from) Cursor = i;
        First = 0; Keep();
    }
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
