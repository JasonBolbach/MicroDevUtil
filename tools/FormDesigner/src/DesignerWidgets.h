#pragma once
// Drawing helpers shared by the designer's panels and its dialogs.
#include <LovyanGFX.hpp>
#include <string>
#include "DesignerTextField.h"

namespace FD {
constexpr uint16_t COL_PANEL = 0x18C3, COL_LINE = 0x4A69, COL_ACCENT = 0x07FF, COL_SEL = 0x2B5F, COL_DIM = 0x8410, COL_ERR = 0xF800;

inline int MeasureText(lgfx::LGFX_Sprite& g, const std::string& s) { g.setFont(&lgfx::fonts::Font2); return g.textWidth(s.c_str()); }

// A text field box: frame, selection, text, blinking caret. `bad` draws it red (invalid content).
inline void DrawField(lgfx::LGFX_Sprite& g, TextField& f, int x, int y, int w, int h, bool focused, bool bad, uint32_t ticks) {
    g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left);
    int tx = x + 6 - f.scrollX;
    g.drawRect(x, y, w, h, bad ? COL_ERR : focused ? COL_ACCENT : 0x7BEF);
    g.setClipRect(x + 2, y + 2, w - 4, h - 4);
    if (f.HasSel()) {
        int x0 = tx + MeasureText(g, f.text.substr(0, f.SelBegin())), x1 = tx + MeasureText(g, f.text.substr(0, f.SelEnd()));
        g.fillRect(x0, y + 3, x1 - x0, h - 6, COL_SEL);
    }
    g.setTextColor(bad ? COL_ERR : 0xFFFF); g.drawString(f.text.c_str(), tx, y + (h - g.fontHeight()) / 2);
    if (focused && ((ticks / 500) % 2 == 0 || f.HasSel())) g.drawFastVLine(tx + MeasureText(g, f.text.substr(0, f.caret)), y + 4, h - 8, 0xFFFF);
    g.clearClipRect();
}
}
