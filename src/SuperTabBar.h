#pragma once
// SuperTabBar - a row of tabs built on SuperControl, edited with a SuperEncoder and two SuperButtons.
//  * The bar only draws the tabs and tracks the selected one; you draw / show the page below it.
//    Use OnAccept (or GetSelected() after BeginEdit()) to switch pages.
//  * Tabs are as wide as their text (plus TabPad each side). When they don't fit, the bar scrolls and light
//    gray < > arrows show that tabs are hidden.
//  * Editing: encoder moves the cursor along the tabs (stops at the ends), ButtonA short/long selects the
//    tab under the cursor, ButtonB short = cancel, ButtonB long = cursor back to the selected tab.
//    OnChange = the cursor moved (live), OnAccept = a tab was selected.
#include "SuperControl.h"
#include "SuperEditIO.h"

class SuperTabBar : public SuperControl {
public:
    int TabPad = 6;                                  // padding left and right of each tab's text

    SuperTabBar(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font,
                SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB)
        : SuperControl(dst, x, y, width, font), IO(enc, btnA, btnB) {}

    SuperEditIO* EditIO() { return &IO; }                          // the input plumbing: InjectDelta / Inject for keyboards, mice and tests
    void SetAbort(bool (*fn)()) { IO.Abort = fn; }
    void SetOnChange(void (*fn)(void*), void* user = nullptr) { IO.OnChange = fn; IO.OnChangeUser = user; }
    void SetOnAccept(void (*fn)(void*), void* user = nullptr) { IO.OnAccept = fn; IO.OnAcceptUser = user; }

    void AddTab(const std::string& s) { Tabs.push_back(s); if (Sel < 0) Sel = Cursor = 0; Dirty = true; }
    void ClearTabs() { Tabs.clear(); Sel = Cursor = -1; First = 0; Dirty = true; }
    // The same list API as SuperListBox / SuperComboBox (GetTab is the same as GetItem). Change tabs at run time, not while the bar is being edited.
    const std::string& GetItem(int i) const { return GetTab(i); }
    void SetItems(const std::vector<std::string>& v) {
        Tabs = v; First = 0;
        Sel = Tabs.empty() ? -1 : std::clamp(Sel, 0, (int)Tabs.size() - 1);
        Cursor = Sel; Dirty = true;
    }
    void InsertItem(int i, const std::string& s) {                 // before index i (i >= Count() appends); the selection stays on its tab
        i = std::clamp(i, 0, Count());
        Tabs.insert(Tabs.begin() + i, s);
        if (Sel < 0) { Sel = Cursor = 0; }
        else { if (i <= Sel) ++Sel; if (i <= Cursor) ++Cursor; }
        Dirty = true;
    }
    void RemoveItem(int i) {                                       // if it was the selected tab, the selection moves to the tab now at that index
        if (i < 0 || i >= Count()) return;
        Tabs.erase(Tabs.begin() + i);
        if (Tabs.empty()) { Sel = Cursor = -1; First = 0; }
        else {
            if (i < Sel) --Sel; else if (i == Sel) Sel = std::min(Sel, Count() - 1);
            if (i < Cursor) --Cursor; else if (i == Cursor) Cursor = std::min(Cursor, Count() - 1);
            First = std::min(First, std::max(0, Count() - 1));
        }
        Dirty = true;
    }
    void SetItem(int i, const std::string& s) { if (i >= 0 && i < Count()) { Tabs[i] = s; Dirty = true; } }
    int  Count() const { return (int)Tabs.size(); }
    const std::string& GetTab(int i) const { static const std::string none; return (i >= 0 && i < Count()) ? Tabs[i] : none; }
    void SetSelected(int i) { Sel = Tabs.empty() ? -1 : std::clamp(i, 0, Count() - 1); Cursor = Sel; Dirty = true; Refresh(); }
    int  GetSelected() const { return Sel; }
    int  GetCursor() const { return Cursor; }

    bool IsEditing() const override { return State == EDIT_ACTIVE; }
    bool IdleTapActs() const override { return true; }          // one tap on a tab selects it
    // Touch: tap a tab = select it, long press = accept the cursor tab, tap outside = cancel, drag = move the cursor.
    bool OnPointer(const SuperPointerEvent& e) override {
        if (State != EDIT_ACTIVE) return false;
        if (e.Type == SuperPointerEvent::TAP) {
            int i = TabAt(e.X, e.Y);
            if (i >= 0) { if (i != Cursor) { Cursor = i; IO.Changed(); } IO.Inject(SuperEditIO::A_SHORT); return true; }
            if (HitTest(e.X, e.Y, TouchPad)) return true;                 // between tabs / on an arrow: ignore
        }
        return SuperPointerDefault(IO, *this, e, 28);
    }
    bool StartEdit() override {
        if (Tabs.empty()) return false;
        Cursor = Sel < 0 ? 0 : Sel; IO.Flush(); State = EDIT_ACTIVE; Push(); return true;
    }
    SuperEditState Service() override {
        if (State != EDIT_ACTIVE) return EDIT_IDLE;
        if (IO.Aborted()) return Finish(EDIT_CANCELLED);
        bool redraw = false;
        int32_t d = IO.Delta();
        if (d) {
            int nc = std::clamp(Cursor + (d > 0 ? 1 : -1), 0, Count() - 1);
            if (nc != Cursor) { Cursor = nc; redraw = true; IO.Changed(); }
        }
        switch (IO.NextKey()) {
        case SuperEditIO::A_SHORT:
        case SuperEditIO::A_LONG:  Sel = Cursor; return Finish(EDIT_ACCEPTED);
        case SuperEditIO::B_SHORT: return Finish(EDIT_CANCELLED);
        case SuperEditIO::B_LONG:  if (Cursor != Sel) { Cursor = Sel; redraw = true; IO.Changed(); } break;
        default: break;
        }
        if (redraw) Push();
        return EDIT_ACTIVE;
    }

protected:
    int BodyHeight() override { return FontH() + 4; }

    void RenderBody(int x, int y, int w, int h) override {
        int n = Count();
        if (n == 0) return;
        Spr.setFont(Font);
        std::vector<int> tw(n);
        int total = 0;
        for (int i = 0; i < n; ++i) { tw[i] = (int)Spr.textWidth(Tabs[i].c_str()) + 2 * TabPad; total += tw[i]; }
        int ar = h / 2 + 2;                                          // room for a scroll arrow
        bool scroll = total > w;
        int focus = State == EDIT_ACTIVE ? Cursor : Sel;
        if (scroll) {                                                // keep 'focus' (and a neighbour) in view
            int vw = w - 2 * ar;
            if (First > focus) First = focus;
            auto span = [&](int a, int b) { int s = 0; for (int i = a; i <= b; ++i) s += tw[i]; return s; };
            while (First < focus && span(First, focus) > vw) ++First;
        } else First = 0;
        int x0 = scroll ? x + ar : x, x1 = scroll ? x + w - ar : x + w;
        Spr.setClipRect(x0, y, x1 - x0, h);
        Spr.setTextDatum(lgfx::textdatum_t::middle_center);
        TabX.assign(n, -1); TabW.assign(n, 0);
        int px = x0;
        int last = First;
        for (int i = First; i < n && px < x1; ++i, ++px) {           // 1 px between tabs
            last = i;
            TabX[i] = px; TabW[i] = tw[i];
            bool cur = State == EDIT_ACTIVE && i == Cursor;
            if (cur)            { Spr.fillRect(px, y, tw[i], h, ColCursor); Spr.setTextColor(ColCursorFg); }
            else if (i == Sel)  { Spr.fillRect(px, y, tw[i], h, ColSel);    Spr.setTextColor(ColSelFg); }
            else                { Spr.setTextColor(ColFg); }
            Spr.drawString(Tabs[i].c_str(), px + tw[i] / 2, y + h / 2);
            if (i == Sel) Spr.fillRect(px, y + h - 2, tw[i], 2, ColHighlight);   // underline marks the open page
            px += tw[i];
        }
        Spr.clearClipRect();
        if (scroll) {
            int s = std::max(2, h / 4), cy = y + h / 2;
            if (First > 0)    Spr.fillTriangle(x + ar / 2 - s / 2, cy, x + ar / 2 + s / 2, cy - s, x + ar / 2 + s / 2, cy + s, ColArrow);
            if (last < n - 1) Spr.fillTriangle(x + w - ar / 2 + s / 2, cy, x + w - ar / 2 - s / 2, cy - s, x + w - ar / 2 - s / 2, cy + s, ColArrow);
        }
    }

private:
    SuperEditIO IO;
    std::vector<int> TabX, TabW;                     // where each tab was last drawn (sprite x), -1 = not visible
    int TabAt(int sx, int sy) {
        int by = GetBoxTop(), pad = SuperControl::TouchPad;
        if (sy < by - pad || sy >= by + GetBoxHeight() + pad) return -1;       // forgiving vertically (a fingertip)
        int rx = sx - PosX;
        for (size_t i = 0; i < TabX.size(); ++i)                              // 1 px slack on each side closes the gaps
            if (TabX[i] >= 0 && rx >= TabX[i] - 1 && rx < TabX[i] + TabW[i] + 1) return (int)i;
        return -1;
    }
    std::vector<std::string> Tabs;
    int Sel = -1, Cursor = -1, First = 0;
    SuperEditState State = EDIT_IDLE;
    SuperEditState Finish(SuperEditState r) {
        State = EDIT_IDLE; Cursor = Sel; Push();
        if (r == EDIT_ACCEPTED) IO.Accepted();
        return r;
    }
};
