#pragma once
// SuperComboBox - a drop-down list built on SuperTextBox, edited with a SuperEncoder and two SuperButtons
// (see SuperEditIO.h). Closed, it looks like a text box showing the selected item with a drop-down arrow.
// While editing, a SuperListBox pops open below it (or above, if there is no room below) and the box
// shows the item under the cursor live.
//
//  * The popup shows up to MaxRows items. Style it through ListBox() (colours, border, RowPad, Wrap).
//  * When the popup closes, what was underneath is put back. By default the pixels under the popup are
//    read back from the destination first (readRect). Panels that can't be read (most SPI panels): keep the
//    background in a sprite and pass it to SetBackdrop(), draw into a sprite, or call SetRestore().
//
// Editing (BeginEdit() blocks and returns true if accepted; StartEdit()/Service() is the non-blocking form):
//   Encoder            = move through the list
//   ButtonA short/long = accept the item under the cursor
//   ButtonB short      = cancel
//   ButtonB long       = back to the selected item, popup stays open
#include "SuperTextBox.h"
#include "SuperListBox.h"

class SuperComboBox : public SuperTextBox {
public:
    int      MaxRows     = 5;           // popup rows (fewer if the list is shorter)

    SuperComboBox(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font,
                  SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB)
        : SuperTextBox(dst, x, y, width, font), List(dst, x, y, width, font, 5, enc, btnA, btnB), Under(dst)
    {
        ReserveRight = FontH();
        List.SetBorder(1, 0xFFFF);
        SyncText();
    }

    SuperListBox& ListBox() { return List; }                       // style the popup
    void ApplyTheme(const SuperTheme& t) override { SuperTextBox::ApplyTheme(t); List.ApplyTheme(t); }
    void SetAbort(bool (*fn)()) { List.SetAbort(fn); }
    // OnChange: the cursor moved (live). OnAccept: an item was accepted (GetSelected() is new).
    void SetOnChange(void (*fn)(void*), void* user = nullptr) { List.SetOnChange(fn, user); }
    void SetOnAccept(void (*fn)(void*), void* user = nullptr) { AcceptFn = fn; AcceptUser = user; }
    // Where the pixels under the popup are read from when it opens. Default: the destination.
    // Most SPI panels can't be read back, so draw your screen into a sprite and pass that sprite here
    // (or as the destination): the popup is saved from it and put back to the destination on close.
    void SetBackdrop(lgfx::LovyanGFX* src) { Backdrop = src; }
    // Instead of reading back the pixels under the popup, call this to let you redraw them.
    void SetRestore(void (*fn)(void*), void* user = nullptr) { RestoreFn = fn; RestoreUser = user; }

    // ---- items ----
    void AddItem(const std::string& s) { List.AddItem(s); SyncText(); }
    void SetItems(const std::vector<std::string>& v) { List.SetItems(v); SyncText(); }
    void ClearItems() { List.ClearItems(); SyncText(); }
    int  Count() const { return List.Count(); }
    void SetSelected(int i) { List.SetSelected(i); SyncText(); Refresh(); }
    int  GetSelected() const { return List.GetSelected(); }
    const std::string& GetValue() const { return List.GetSelectedText(); }

    // ---- editing ----
    bool IsEditing() const override { return State == EDIT_ACTIVE; }
    // While open, the popup counts as part of the control.
    bool HitTest(int x, int y, int pad = 0) override {
        return SuperTextBox::HitTest(x, y, pad) || (State == EDIT_ACTIVE && List.HitTest(x, y, pad));
    }
    // Touch: tap a popup row = choose it, tap anywhere else = cancel, drag in the popup = move the cursor.
    bool OnPointer(const SuperPointerEvent& e) override {
        if (State != EDIT_ACTIVE) return false;
        if (e.Type == SuperPointerEvent::TAP && !List.HitTest(e.X, e.Y, TouchPad)) { List.EditIO()->Inject(SuperEditIO::B_SHORT); return true; }
        return List.OnPointer(e);
    }

    bool StartEdit() {                                             // non-blocking: then call Service()
        if (List.Count() == 0) return false;
        List.SetRows(std::min(List.Count(), std::max(1, MaxRows)));
        int ph = List.GetHeight(), below = GetBoxTop() + GetBoxHeight(), py = below;
        if (Dst && below + ph > Dst->height() && GetBoxTop() - ph >= 0) py = GetBoxTop() - ph;   // flip above
        List.SetDisplayPos(GetX(), py);
        PopX = GetX(); PopY = py; PopH = ph;
        Backed = false;
        lgfx::LovyanGFX* src = Backdrop ? Backdrop : Dst;
        if (!RestoreFn && src) {
            Under.setColorDepth(16);
            if (Under.createSprite(GetWidth(), ph)) {
                src->readRect(PopX, PopY, GetWidth(), ph, (lgfx::swap565_t*)Under.getBuffer());
                Backed = true;
            }
        }
        State = EDIT_ACTIVE;
        ShownCursor = -1;
        if (!List.StartEdit()) { State = EDIT_IDLE; Close(); return false; }
        SyncOpenText();
        Push();
        return true;
    }

    // Returns EDIT_ACTIVE while editing, then EDIT_ACCEPTED or EDIT_CANCELLED once, then EDIT_IDLE.
    SuperEditState Service() {
        if (State != EDIT_ACTIVE) return EDIT_IDLE;
        SuperEditState r = List.Service();
        if (r == EDIT_ACTIVE) {
            if (SyncOpenText()) Refresh();
            return EDIT_ACTIVE;
        }
        State = EDIT_IDLE;
        Close();
        if (r == EDIT_IDLE) r = EDIT_CANCELLED;
        SyncText();
        Push();
        if (r == EDIT_ACCEPTED && AcceptFn) AcceptFn(AcceptUser);
        return r;
    }

protected:
    void CellStyleAt(int, CellStyle& st) override {
        if (State == EDIT_ACTIVE) { st.fill = true; st.bg = ColCursor; st.fg = ColCursorFg; }
    }
    void RenderBody(int x, int y, int w, int h) override {
        SuperTextBox::RenderBody(x, y, w, h);
        int s = std::max(2, ReserveRight / 4), cx = x + w - ReserveRight / 2, cy = y + h / 2;
        if (State == EDIT_ACTIVE) Spr.fillTriangle(cx, cy - s, cx - s, cy + s / 2 + 1, cx + s, cy + s / 2 + 1, ColArrow);   // open: up
        else                      Spr.fillTriangle(cx, cy + s, cx - s, cy - s / 2 - 1, cx + s, cy - s / 2 - 1, ColArrow);   // closed: down
    }

private:
    SuperListBox      List;
    lgfx::LGFX_Sprite Under;
    void (*RestoreFn)(void*) = nullptr;
    void* RestoreUser = nullptr;
    void (*AcceptFn)(void*) = nullptr;
    void* AcceptUser = nullptr;
    lgfx::LovyanGFX* Backdrop = nullptr;
    bool  Backed = false;
    int   PopX = 0, PopY = 0, PopH = 0, ShownCursor = -1;
    SuperEditState State = EDIT_IDLE;

    void SyncText() { SetText(List.GetSelectedText()); }
    bool SyncOpenText() {                                          // show the cursor item while open
        int c = List.GetCursor();
        if (c == ShownCursor) return false;
        ShownCursor = c;
        SetText(List.GetItem(c));
        return true;
    }
    void Close() {
        if (RestoreFn) RestoreFn(RestoreUser);
        else if (Backed && Dst) Under.pushSprite(Dst, PopX, PopY);
        Under.deleteSprite();
        List.ReleaseBuffer();
        Backed = false;
    }
};
