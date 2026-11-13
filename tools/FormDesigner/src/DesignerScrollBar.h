#pragma once
// DesignerScrollBar - a scroll bar, vertical or horizontal, for the Form Designer (desktop only for now).
//
// It knows nothing about SDL or the designer: give it a rectangle, the size of the content and of the visible part, and feed it pointer positions.
// That keeps it ready to become a SuperGUI control later (the API would stay the same; only Draw() uses LovyanGFX).
//
//   ScrollBar bar(ScrollBar::VERTICAL, 12);
//   bar.SetGeometry(x, y, length);              // the track: x, y, thickness x length (HORIZONTAL: length x thickness)
//   bar.SetRange(total, page);  bar.SetPos(p);  // content size and visible size in the same unit (pixels or lines); p = the first visible unit
//   bar.Draw(gfx, trackColour, thumbColour, activeColour);
//   if (bar.Down(x, y)) ... ; bar.Move(x, y) (while the button is down: true when Pos() changed) ; bar.Up();
//   Pos() is always inside 0 .. MaxPos() (= total - page).
//
// Mouse: drag the thumb; click the track above / below (left / right of) the thumb to move one page toward the click.
#include <LovyanGFX.hpp>
#include <algorithm>

namespace FD {
class ScrollBar {
public:
    enum Orient { VERTICAL, HORIZONTAL };

    explicit ScrollBar(Orient o = VERTICAL, int thickness = 12, int minThumb = 24) : Or(o), Th(thickness), MinThumb(minThumb) {}

    // ---- set-up ----
    void SetGeometry(int x, int y, int length) { X = x; Y = y; Len = std::max(1, length); }
    void SetRange(int total, int page) { Tot = std::max(0, total); Pg = std::max(1, page); Pos_ = Clamp(Pos_); }
    void SetPos(int p) { Pos_ = Clamp(p); }

    // ---- state ----
    Orient Orientation() const { return Or; }
    int  Thickness() const { return Th; }
    int  Pos() const { return Pos_; }
    int  Total() const { return Tot; }
    int  PageSize() const { return Pg; }
    int  MaxPos() const { return std::max(0, Tot - Pg); }
    bool Needed() const { return Tot > Pg; }                    // false: everything is visible, draw and click nothing
    bool Dragging() const { return Drag; }
    int  TrackX() const { return X; }
    int  TrackY() const { return Y; }
    int  TrackW() const { return Or == VERTICAL ? Th : Len; }
    int  TrackH() const { return Or == VERTICAL ? Len : Th; }
    int  ThumbLen() const { return Needed() ? std::min(Len, std::max(MinThumb, (int)((long long)Len * Pg / Tot))) : Len; }
    int  ThumbStart() const { int room = Len - ThumbLen(); return MaxPos() ? (int)((long long)room * Pos_ / MaxPos()) : 0; }     // from the track start

    bool Contains(int px, int py) const { return Needed() && px >= X && px < X + TrackW() && py >= Y && py < Y + TrackH(); }

    // ---- pointer ----
    // A press: returns true when the bar used it (the thumb is grabbed, or the page moved: check Pos()).
    bool Down(int px, int py) {
        if (!Contains(px, py)) return false;
        int m = Along(px, py) - (Or == VERTICAL ? Y : X), ts = ThumbStart(), tl = ThumbLen();
        if (m >= ts && m < ts + tl) { Drag = true; Grab = m - ts; }
        else SetPos(Pos_ + (m < ts ? -Pg : Pg));
        return true;
    }
    // The mouse moved with the button down; true when Pos() changed.
    bool Move(int px, int py) {
        if (!Drag) return false;
        int room = Len - ThumbLen(), m = Along(px, py) - (Or == VERTICAL ? Y : X) - Grab, old = Pos_;
        Pos_ = room > 0 ? Clamp((int)((long long)std::max(0, std::min(m, room)) * MaxPos() / room)) : 0;
        return Pos_ != old;
    }
    void Up() { Drag = false; }

    // ---- drawing ----
    void Draw(lgfx::LGFXBase& g, uint16_t track, uint16_t thumb, uint16_t active) const {
        if (!Needed()) return;
        g.fillRect(X, Y, TrackW(), TrackH(), track);
        int ts = ThumbStart(), tl = ThumbLen(), in = 2;                                   // the thumb sits 2 pixels inside the track
        uint16_t c = Drag ? active : thumb;
        if (Or == VERTICAL) g.fillRect(X + in, Y + ts, Th - 2 * in, tl, c); else g.fillRect(X + ts, Y + in, tl, Th - 2 * in, c);
    }

private:
    Orient Or; int Th, MinThumb;
    int X = 0, Y = 0, Len = 1, Tot = 0, Pg = 1, Pos_ = 0, Grab = 0; bool Drag = false;
    int Clamp(int p) const { return std::max(0, std::min(p, MaxPos())); }
    int Along(int px, int py) const { return Or == VERTICAL ? py : px; }
};
}
