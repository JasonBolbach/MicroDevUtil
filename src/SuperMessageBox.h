#pragma once
// SuperMessageBox - a modal message with one to three buttons, built on SuperControl, answered with a
// SuperEncoder and two SuperButtons.
//
//     SuperMessageBox box(&lcd, 220, &Font2, &Enc, &BtnA, &BtnB);
//     box.SetMessage("Delete all data?\nThis cannot be undone.");
//     box.AddButton("Yes"); box.AddButton("No"); box.SetDefault(1);
//     if (box.BeginEdit()) { int which = box.GetResult(); ... }       // true = a button was chosen
//
//  * Centered on the destination (SetCentered(false) + SetDisplayPos() to place it yourself). The message is
//    word-wrapped to the width; '\n' starts a new line. SetLabel() adds a title above the box.
//  * Encoder moves between the buttons (stops at the ends), ButtonA short/long chooses the highlighted one
//    (BeginEdit() returns true, GetResult() = its index). ButtonB short cancels (returns false, GetResult() = -1).
//    ButtonB long moves back to the default button.
//  * Like the combo box popup, it puts back what it covered: pixels are read from the destination (or from
//    SetBackdrop()'s sprite) when it opens; SetRestore() lets you redraw instead.
//  * OnChange = the highlighted button changed, OnAccept = a button was chosen.
#include "SuperControl.h"
#include "SuperEditIO.h"

class SuperMessageBox : public SuperControl {
public:
    SuperMessageBox(lgfx::LovyanGFX* dst, int width, const lgfx::IFont* font,
                    SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB)
        : SuperControl(dst, 0, 0, width, font), IO(enc, btnA, btnB), Under(dst)
    { SetBorder(2, ColBorder); SetPadding(6); }

    void SetAbort(bool (*fn)()) { IO.Abort = fn; }
    void SetOnChange(void (*fn)(void*), void* user = nullptr) { IO.OnChange = fn; IO.OnChangeUser = user; }
    void SetOnAccept(void (*fn)(void*), void* user = nullptr) { IO.OnAccept = fn; IO.OnAcceptUser = user; }
    void SetRestore(void (*fn)(void*), void* user = nullptr) { RestoreFn = fn; RestoreUser = user; }
    void SetBackdrop(lgfx::LovyanGFX* src) { Backdrop = src; }
    void SetCentered(bool c) { Centered = c; }

    void SetMessage(const std::string& m) { Msg = m; LinesOk = false; Dirty = true; }
    void ClearButtons() { Btns.clear(); Dirty = true; }
    void AddButton(const std::string& s) { if (Btns.size() < 3) Btns.push_back(s); Dirty = true; }
    void SetDefault(int i) { Default = i; }
    int  GetResult() const { return Result; }

    bool IsEditing() const override { return State == EDIT_ACTIVE; }
    // Touch: tap a button = choose it, tap outside = cancel, drag = move between the buttons.
    // Touch is ignored for TouchGuardMs after the box opens (it usually opens under the finger that triggered it, and panels
    // can report a stray touch as the finger lifts), and with touch-down events available a tap only counts when its
    // finger-down happened after the box opened.
    int TouchGuardMs = 300;
    bool OnPointer(const SuperPointerEvent& e) override {
        if (State != EDIT_ACTIVE) return false;
        if ((int)(lgfx::millis() - OpenedAt) < TouchGuardMs) return true;
        if (e.Type == SuperPointerEvent::PRESS) { PressSeen = true; return true; }
        if (PressEventsOn && !PressSeen) return true;                    // part of a touch that began before the box opened
        bool end = e.Type == SuperPointerEvent::TAP || e.Type == SuperPointerEvent::LONG_PRESS || e.Type == SuperPointerEvent::DRAG_END;
        bool r = HandlePointer(e);
        if (end) PressSeen = false;
        return r;
    }
    bool HandlePointer(const SuperPointerEvent& e) {
        if (e.Type == SuperPointerEvent::TAP) {
            int i = ButtonAt(e.X, e.Y);
            if (i >= 0) { if (i != Cursor) { Cursor = i; IO.Changed(); } IO.Inject(SuperEditIO::A_SHORT); return true; }
            if (HitTest(e.X, e.Y, TouchPad)) return true;
        }
        return SuperPointerDefault(IO, *this, e, 40);
    }
    bool StartEdit() override {
        if (Btns.empty()) return false;
        LinesOk = false;
        if (Centered && Dst) { PosX = (Dst->width() - W) / 2; PosY = (Dst->height() - GetHeight()) / 2; }
        int h = GetHeight();
        Backed = false;
        lgfx::LovyanGFX* src = Backdrop ? Backdrop : Dst;
        if (!RestoreFn && src) {
            if (AllocSprite(Under, W, h)) { src->readRect(PosX, PosY, W, h, (lgfx::swap565_t*)Under.getBuffer()); Backed = true; }
        }
        Cursor = std::clamp(Default, 0, (int)Btns.size() - 1);
        Result = -1;
        OpenedAt = lgfx::millis(); PressSeen = false;
        IO.Flush();
        State = EDIT_ACTIVE;
        Push();
        return true;
    }
    SuperEditState Service() override {
        if (State != EDIT_ACTIVE) return EDIT_IDLE;
        if (IO.Aborted()) return Finish(EDIT_CANCELLED);
        bool redraw = false;
        int32_t d = IO.Delta();
        if (d) {
            int nc = std::clamp(Cursor + (d > 0 ? 1 : -1), 0, (int)Btns.size() - 1);
            if (nc != Cursor) { Cursor = nc; redraw = true; IO.Changed(); }
        }
        switch (IO.NextKey()) {
        case SuperEditIO::A_SHORT:
        case SuperEditIO::A_LONG:  Result = Cursor; return Finish(EDIT_ACCEPTED);
        case SuperEditIO::B_SHORT: return Finish(EDIT_CANCELLED);
        case SuperEditIO::B_LONG: {
            int dflt = std::clamp(Default, 0, (int)Btns.size() - 1);
            if (Cursor != dflt) { Cursor = dflt; redraw = true; IO.Changed(); }
            break; }
        default: break;
        }
        if (redraw) Push();
        return EDIT_ACTIVE;
    }

protected:
    int BtnH() { return FontH() + 6; }
    int BodyHeight() override { BuildLines(); return (int)Lines.size() * FontH() + 8 + BtnH(); }

    void RenderBody(int x, int y, int w, int h) override {
        BuildLines();
        Spr.setFont(Font);
        Spr.setTextColor(ColFg);
        Spr.setTextDatum(lgfx::textdatum_t::top_center);
        int fh = FontH(), ty = y;
        for (auto& l : Lines) { Spr.drawString(l.c_str(), x + w / 2, ty); ty += fh; }
        int n = (int)Btns.size(), gap = 6, bw = (w - (n - 1) * gap) / n, by = y + h - BtnH();
        Spr.setTextDatum(lgfx::textdatum_t::middle_center);
        for (int i = 0; i < n; ++i) {
            int bx = x + i * (bw + gap);
            bool cur = State == EDIT_ACTIVE && i == Cursor;
            if (cur) Spr.fillRect(bx, by, bw, BtnH(), ColCursor);
            Spr.drawRect(bx, by, bw, BtnH(), cur ? ColCursor : ColFg);
            Spr.setTextColor(cur ? ColCursorFg : ColFg);
            Spr.setClipRect(bx, by, bw, BtnH());
            Spr.drawString(Btns[i].c_str(), bx + bw / 2, by + BtnH() / 2);
            Spr.clearClipRect();
        }
    }

private:
    SuperEditIO IO;
    lgfx::LGFX_Sprite Under;
    lgfx::LovyanGFX* Backdrop = nullptr;
    void (*RestoreFn)(void*) = nullptr; void* RestoreUser = nullptr;
    bool Backed = false, Centered = true, LinesOk = false;
    int  Cursor = 0, Default = 0, Result = -1;
    uint32_t OpenedAt = 0;
    bool PressSeen = false;
    std::string Msg;
    std::vector<std::string> Btns, Lines;
    SuperEditState State = EDIT_IDLE;

    void BuildLines() {                                  // word-wrap Msg to the inner width
        if (LinesOk) return;
        Lines.clear();
        Spr.setFont(Font);
        int maxw = InnerW();
        size_t pos = 0;
        while (pos <= Msg.size()) {
            size_t nl = Msg.find('\n', pos);
            std::string para = Msg.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos), cur;
            size_t p = 0;
            while (p < para.size()) {
                size_t sp = para.find(' ', p);
                std::string word = para.substr(p, sp == std::string::npos ? std::string::npos : sp - p + 1);
                std::string trial = cur + word;
                std::string trimmed = trial; while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
                if (!cur.empty() && (int)Spr.textWidth(trimmed.c_str()) > maxw) { Lines.push_back(cur); cur = word; }
                else cur = trial;
                p += word.size();
            }
            while (!cur.empty() && cur.back() == ' ') cur.pop_back();
            Lines.push_back(cur);
            if (nl == std::string::npos) break;
            pos = nl + 1;
        }
        if (Lines.empty()) Lines.push_back("");
        LinesOk = true;
    }
    int ButtonAt(int sx, int sy) {
        int x0 = PosX + Inset(), h = BodyHeight(), by = GetBoxTop() + Inset() + h - BtnH(), w = InnerW();
        int n = (int)Btns.size(), gap = 6, bw = (w - (n - 1) * gap) / std::max(1, n);
        if (sy < by - TouchPad || sy >= by + BtnH() + TouchPad) return -1;
        for (int i = 0; i < n; ++i) {
            int bx = x0 + i * (bw + gap);
            if (sx >= bx - gap / 2 && sx < bx + bw + gap / 2) return i;
        }
        return -1;
    }
    SuperEditState Finish(SuperEditState r) {
        State = EDIT_IDLE;
        if (RestoreFn) RestoreFn(RestoreUser);
        else if (Backed && Dst) Under.pushSprite(Dst, PosX, PosY);
        Under.deleteSprite();
        ReleaseBuffer();
        Backed = false;
        if (r == EDIT_ACCEPTED) IO.Accepted();
        return r;
    }
};
