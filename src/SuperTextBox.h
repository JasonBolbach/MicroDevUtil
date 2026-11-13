#pragma once
// SuperTextBox - a single-line text display built on SuperControl (see SuperControl.h for the
// frame: border styles, padding, label, colours, DrawPreview).
//
//  * Shows only whole characters. If the text is wider than the box, light gray < / > arrows
//    in reserved gutters at the edges show that text is hidden on that side.
//  * Alignment (SetAlign) applies when the text fits.
//
// Subclasses can customise per-character width (CellWidth) and style (CellStyleAt).
#include "SuperControl.h"

class SuperTextBox : public SuperControl {
public:
    struct CellStyle {
        uint16_t fg;
        uint16_t bg = 0;
        bool     fill = false;          // fill the cell with bg behind the glyph
        bool     outline = false;       // draw a 1 px rectangle around the cell
        uint16_t outlineColor = 0;
    };

    SuperTextBox(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font)
        : SuperControl(dst, x, y, width, font) {}

    void SetText(const std::string& t, bool keepScroll = false) {
        Text = t; LayoutDirty = Dirty = true;
        if (!keepScroll) First = 0;
    }
    const std::string& GetText() const { return Text; }

    // ---- scrolling ----
    int  GetFirstVisible() const { return First; }
    void ScrollBy(int n) { Layout(); First += n; ClampFirst(); Dirty = true; }
    void ScrollTo(int i) { Layout(); First = i;  ClampFirst(); Dirty = true; }

    // Scroll (minimally) so characters lo..hi are all visible. If that range is wider than
    // the box, only 'focus' is guaranteed. Used to force the neighbours of a cursor into view.
    void EnsureVisible(int lo, int hi, int focus) {
        Layout();
        int n = (int)Text.size();
        if (n == 0) return;
        lo = std::clamp(lo, 0, n - 1); hi = std::clamp(hi, 0, n - 1); focus = std::clamp(focus, 0, n - 1);
        if (lo > hi) std::swap(lo, hi);
        if (!Scrolling()) { First = 0; return; }
        int vp = Viewport();
        if (Pre[hi + 1] - Pre[lo] > vp) lo = hi = focus;
        if (lo < First) First = lo;
        else while (First < lo && Pre[hi + 1] - Pre[First] > vp) ++First;
        while (First > 0 && Pre[n] - Pre[First - 1] <= vp) --First;   // don't leave a gap on the right
        Dirty = true;
    }
    void EnsureVisible(int i) { EnsureVisible(i, i, i); }

    // The control width at which the whole text fits without scrolling arrows (text + frame + padding). For "fit to content" (the Form Designer).
    int GetContentWidth() { Layout(); return Pre.back() + 2 * Inset() + ReserveRight; }

    // Hit test for touch: which character is under screen x? Returns its index, or CHAR_NONE,
    // CHAR_ARROW_LEFT / CHAR_ARROW_RIGHT (the "<" / ">" shown while the text scrolls).
    enum { CHAR_NONE = -1, CHAR_ARROW_LEFT = -2, CHAR_ARROW_RIGHT = -3 };
    int CharAt(int sx) {
        Layout(); ClampFirst();
        int n = (int)Text.size(), x0 = PosX + Inset(), areaW = TextW();
        bool sc = Scrolling();
        int x, endX;
        if (sc) {
            if (sx < x0 + ArrowW) return CHAR_ARROW_LEFT;
            if (sx >= x0 + areaW - ArrowW) return CHAR_ARROW_RIGHT;
            x = x0 + ArrowW; endX = x0 + areaW - ArrowW;
        } else {
            int total = Pre.back();
            x = x0 + (Alignment == ALIGN_CENTER ? (areaW - total) / 2 : Alignment == ALIGN_RIGHT ? areaW - total : 0);
            endX = x0 + areaW;
        }
        for (int i = First; i < n; ++i) {                  // same walk as RenderBody
            int w = Pre[i + 1] - Pre[i];
            if (i > First && x + w > endX) break;
            if (sx >= x && sx < x + w) return i;
            x += w;
        }
        return CHAR_NONE;
    }

protected:
    // Pixel width of character i of the current text (at least 1).
    virtual int CellWidth(int i) {
        char s[2] = { Text[i], 0 };
        return Spr.textWidth(s);
    }
    virtual void CellStyleAt(int /*i*/, CellStyle& /*style*/) {}

    void OnFontChanged() override { LayoutDirty = true; }

    // Pixels at the right edge of the body that the text must not use (e.g. a drop-down arrow).
    int ReserveRight = 0;

    void RenderBody(int x0, int y0, int innerW, int h) override {
        Layout();
        ClampFirst();
        int n = (int)Text.size(), cy = y0 + h / 2;
        int areaW = TextW();
        bool sc = Scrolling();
        int x, endX;
        if (sc) {
            x = x0 + ArrowW;
            endX = x0 + areaW - ArrowW;
        } else {
            int total = Pre.back();
            x = x0 + (Alignment == ALIGN_CENTER ? (areaW - total) / 2 : Alignment == ALIGN_RIGHT ? areaW - total : 0);
            endX = x0 + areaW;
        }

        Spr.setTextDatum(lgfx::textdatum_t::middle_center);
        if (sc) {
            Spr.setTextColor(ColArrow);
            if (First > 0)                  Spr.drawString("<", x0 + ArrowW / 2, cy);
            if (LastVisible(First) < n - 1) Spr.drawString(">", x0 + areaW - ArrowW / 2, cy);
        }
        for (int i = First; i < n; ++i) {
            int w = Pre[i + 1] - Pre[i];
            if (i > First && x + w > endX) break;
            CellStyle st; st.fg = ColFg;
            CellStyleAt(i, st);
            if (st.fill)    Spr.fillRect(x, y0, w, h, st.bg);
            if (st.outline) Spr.drawRect(x, y0, w, h, st.outlineColor);
            char s[2] = { Text[i], 0 };
            Spr.setTextColor(st.fg);
            Spr.drawString(s, x + w / 2, cy);
            x += w;
        }
    }

private:
    std::string        Text;
    int                First = 0;
    bool               LayoutDirty = true;
    std::vector<int>   Pre;        // Pre[i] = pixel x of char i; Pre[n] = total width
    int                ArrowW = 0;

    int  TextW() { return std::max(1, InnerW() - ReserveRight); }

    void Layout() {
        if (!LayoutDirty) return;
        LayoutDirty = false;
        Spr.setFont(Font);
        ArrowW = std::max(1, (int)Spr.textWidth("<"));
        int n = (int)Text.size();
        Pre.assign(n + 1, 0);
        for (int i = 0; i < n; ++i) Pre[i + 1] = Pre[i] + std::max(1, CellWidth(i));
        ClampFirst();
    }
    bool Scrolling() { Layout(); return Pre.back() > TextW(); }
    int  Viewport()  { return Scrolling() ? std::max(1, TextW() - 2 * ArrowW) : TextW(); }
    int  MaxFirst() {
        int n = (int)Text.size();
        if (!Scrolling() || n == 0) return 0;
        int vp = Viewport();
        for (int f = 0; f < n - 1; ++f) if (Pre[n] - Pre[f] <= vp) return f;
        return n - 1;
    }
    void ClampFirst() { First = std::clamp(First, 0, MaxFirst()); }
    int  LastVisible(int first) {
        int n = (int)Text.size(), vp = Viewport(), l = first;
        while (l + 1 < n && Pre[l + 2] - Pre[first] <= vp) ++l;
        return l;
    }
};
