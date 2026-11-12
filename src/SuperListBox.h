#pragma once
// SuperListBox - a list of text items with a fixed number of visible rows, built on SuperControl
// and edited with a SuperEncoder and two SuperButtons (see SuperEditIO.h).
//
//  * Height = rows * (font height + 2 * RowPad) + border/padding (+ label).
//  * The selected item is drawn with ColSel/ColSelFg. While editing, the cursor row is drawn with
//    ColCursor/ColCursorFg and the list scrolls to keep it in view. Light gray triangles at the right
//    edge show that items are hidden above / below.
//
// Editing (BeginEdit() blocks and returns true if accepted; StartEdit()/Service() is the non-blocking form):
//   Encoder          = move the cursor (stops at the ends, or wraps if Wrap is true)
//   ButtonA short/long = accept: the cursor item becomes the selected item
//   ButtonB short    = cancel
//   ButtonB long     = move the cursor back to the selected item and keep editing
#include "SuperControl.h"
#include "SuperEditIO.h"

class SuperListBox : public SuperControl {
public:
    bool     Wrap        = false;       // cursor wraps from last to first item
    int      RowPad      = 1;           // extra pixels above and below each row's text

    SuperListBox(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font, int rows,
                 SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB)
        : SuperControl(dst, x, y, width, font), Rows(rows < 1 ? 1 : rows), IO(enc, btnA, btnB) {}

    void SetAbort(bool (*fn)()) { IO.Abort = fn; }
    // OnChange: the cursor moved (live). OnAccept: an item was accepted (GetSelected() is new).
    void SetOnChange(void (*fn)(void*), void* user = nullptr) { IO.OnChange = fn; IO.OnChangeUser = user; }
    void SetOnAccept(void (*fn)(void*), void* user = nullptr) { IO.OnAccept = fn; IO.OnAcceptUser = user; }

    // ---- items ----
    void SetRows(int r) { Rows = r < 1 ? 1 : r; Dirty = true; }
    int  GetRows() const { return Rows; }
    void AddItem(const std::string& s) { Items.push_back(s); if (Sel < 0) Sel = Cursor = 0; Dirty = true; }
    void SetItems(const std::vector<std::string>& v) {
        Items = v; First = 0;
        Sel = Items.empty() ? -1 : std::clamp(Sel, 0, (int)Items.size() - 1);
        Cursor = Sel; Keep(Sel); Dirty = true;
    }
    void ClearItems() { Items.clear(); Sel = Cursor = -1; First = 0; Dirty = true; }
    int  Count() const { return (int)Items.size(); }
    const std::string& GetItem(int i) const { static const std::string none; return (i >= 0 && i < Count()) ? Items[i] : none; }

    void SetSelected(int i) {
        Sel = Items.empty() ? -1 : std::clamp(i, 0, (int)Items.size() - 1);
        Cursor = Sel; Keep(Sel); Dirty = true;
    }
    int  GetSelected() const { return Sel; }
    const std::string& GetSelectedText() const { return GetItem(Sel); }
    int  GetCursor() const { return Cursor; }

    // ---- editing ----
    bool IsEditing() const override { return State == EDIT_ACTIVE; }
    SuperEditIO* EditIO() { return &IO; }
    // Item under a screen point (inside the rows), or -1.
    int RowAt(int sx, int sy) {
        int x0 = PosX + Inset(), y0 = GetBoxTop() + Inset(), rh = RowH();
        if (sx < x0 || sx >= x0 + InnerW() || sy < y0 || sy >= y0 + Rows * rh) return -1;
        int idx = First + (sy - y0) / rh;
        return idx < Count() ? idx : -1;
    }
    // Touch: tap a row = choose it, long press = accept, drag = scroll the cursor, tap outside = cancel.
    bool OnPointer(const SuperPointerEvent& e) override {
        if (State != EDIT_ACTIVE) return false;
        if (e.Type == SuperPointerEvent::TAP) {
            int i = RowAt(e.X, e.Y);
            if (i >= 0) { if (i != Cursor) { Cursor = i; Keep(i); IO.Changed(); } IO.Inject(SuperEditIO::A_SHORT); return true; }
        }
        return SuperPointerDefault(IO, *this, e, RowH());
    }

    bool StartEdit() {                                             // non-blocking: then call Service()
        if (Items.empty()) return false;
        Cursor = Sel < 0 ? 0 : Sel;
        IO.Flush();
        State = EDIT_ACTIVE;
        Keep(Cursor);
        Push();
        return true;
    }

    // Returns EDIT_ACTIVE while editing, then EDIT_ACCEPTED or EDIT_CANCELLED once, then EDIT_IDLE.
    SuperEditState Service() {
        if (State != EDIT_ACTIVE) return EDIT_IDLE;
        if (IO.Aborted()) return Finish(EDIT_CANCELLED);
        bool redraw = false;
        int n = Count();

        int32_t d = IO.Delta();
        if (d) {
            int step = (n <= 20) ? (d > 0 ? 1 : -1) : (int)d;     // fast spin only skips in long lists
            int nc = Wrap ? (((Cursor + step) % n) + n) % n : std::clamp(Cursor + step, 0, n - 1);
            if (nc != Cursor) { Cursor = nc; Keep(Cursor); redraw = true; IO.Changed(); }
        }
        switch (IO.NextKey()) {
        case SuperEditIO::A_SHORT:
        case SuperEditIO::A_LONG:  Sel = Cursor; return Finish(EDIT_ACCEPTED);
        case SuperEditIO::B_SHORT: return Finish(EDIT_CANCELLED);
        case SuperEditIO::B_LONG:  if (Cursor != Sel) { Cursor = Sel; Keep(Cursor); redraw = true; IO.Changed(); } break;
        default: break;
        }
        if (redraw) Push();
        return EDIT_ACTIVE;
    }

protected:
    int RowH() { return FontH() + 2 * RowPad; }
    int BodyHeight() override { return Rows * RowH(); }

    void RenderBody(int x, int y, int w, int h) override {
        int rh = RowH(), n = Count();
        bool more = n > Rows;
        int gw = more ? rh / 2 + 3 : 0, tw = std::max(1, w - gw);
        bool editing = (State == EDIT_ACTIVE);
        Spr.setTextDatum(lgfx::textdatum_t::middle_left);
        for (int r = 0; r < Rows; ++r) {
            int idx = First + r;
            if (idx >= n) break;
            int ry = y + r * rh;
            uint16_t fg = ColFg;
            if (editing && idx == Cursor)  { Spr.fillRect(x, ry, tw, rh, ColCursor); fg = ColCursorFg; }
            else if (idx == Sel)           { Spr.fillRect(x, ry, tw, rh, ColSel);    fg = ColSelFg; }
            Spr.setClipRect(x, ry, tw, rh);
            Spr.setTextColor(fg);
            Spr.drawString(Items[idx].c_str(), x + 2, ry + rh / 2);
            Spr.clearClipRect();
        }
        if (more) {
            int s = std::max(2, rh / 4), cx = x + tw + gw / 2;
            if (First > 0) {
                int cy = y + rh / 2;
                Spr.fillTriangle(cx, cy - s, cx - s, cy + s, cx + s, cy + s, ColArrow);
            }
            if (First + Rows < n) {
                int cy = y + h - rh / 2;
                Spr.fillTriangle(cx, cy + s, cx - s, cy - s, cx + s, cy - s, ColArrow);
            }
        }
    }

private:
    int  Rows;
    SuperEditIO IO;
    std::vector<std::string> Items;
    int  Sel = -1, Cursor = -1, First = 0;
    SuperEditState State = EDIT_IDLE;

    void Keep(int i) {
        if (i < 0) return;
        if (i < First) First = i;
        else if (i >= First + Rows) First = i - Rows + 1;
        First = std::max(0, First);
    }
    SuperEditState Finish(SuperEditState r) {
        State = EDIT_IDLE;
        Cursor = Sel; Keep(Sel);
        Push();
        if (r == EDIT_ACCEPTED) IO.Accepted();
        return r;
    }
};
