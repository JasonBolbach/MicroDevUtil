#pragma once
// Rows that cannot be chosen, for SuperListBox, SuperComboBox and SuperMenu: an item whose text starts with '~'.
//   "~"        a separator line across the row
//   "~Text"    a heading: the text centred in a line,   ---- Text ----   (dim; no cursor on it, never selected or accepted)
//   "~~Text"   an ordinary item that shows "~Text" (the escape for a real leading ~)
// The cursor skips these rows (encoder, touch and drag); a list that has nothing but such rows cannot be edited.
// A heading that is too long is cut (characters are dropped, no "..."), so that about one character width plus 3 pixels of line is always left on each side.
#include <LovyanGFX.hpp>
#include <string>

namespace SuperSep {
constexpr int SIDE_CHARS = 1;      // the line on each side of a heading is at least this many character widths ...
constexpr int SIDE_PAD   = 3;      // ... plus this many pixels
constexpr int TEXT_GAP   = 3;      // blank pixels between the heading text and its line
inline bool IsUnclickable(const std::string& t) { return !t.empty() && t[0] == '~' && !(t.size() > 1 && t[1] == '~'); }
inline bool IsLine(const std::string& t) { return t.size() == 1 && t[0] == '~'; }
inline const char* Shown(const std::string& t) { return (t.size() > 1 && t[0] == '~') ? t.c_str() + 1 : t.c_str(); }       // the text to draw ("~Head" -> "Head", "~~x" -> "~x")

// The heading text that fits in a row of w pixels (the control's font must be set on g).
inline std::string Fit(lgfx::LGFXBase& g, const std::string& item, int w) {
    std::string t = Shown(item); int cw = (int)g.textWidth("0"), avail = w - 2 * (SIDE_CHARS * cw + SIDE_PAD);
    while (!t.empty() && (int)g.textWidth(t.c_str()) > avail) t.pop_back();
    return t;
}
// Draws a separator row (x, y, w, h; the control's font is set on g): a full line for "~", else the heading centred in a line.
inline void Draw(lgfx::LGFXBase& g, const std::string& item, int x, int y, int w, int h, uint16_t color) {
    int my = y + h / 2;
    if (IsLine(item)) { g.drawFastHLine(x + 2, my, w - 4, color); return; }
    std::string t = Fit(g, item, w);
    if (t.empty()) { g.drawFastHLine(x + 2, my, w - 4, color); return; }
    int tw = (int)g.textWidth(t.c_str()), tx = x + (w - tw) / 2;
    g.drawFastHLine(x + 2, my, std::max(0, tx - TEXT_GAP - (x + 2)), color);
    g.drawFastHLine(tx + tw + TEXT_GAP, my, std::max(0, x + w - 2 - (tx + tw + TEXT_GAP)), color);
    g.setTextDatum(lgfx::textdatum_t::middle_left); g.setTextColor(color); g.drawString(t.c_str(), tx, my);
}
}
