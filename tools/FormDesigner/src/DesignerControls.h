#pragma once
// Builds the REAL SuperGUI control for a designer control, so the design surface shows exactly what the device will draw.
// The control is created for one draw (or one height measurement) and destroyed again; the designer keeps only the model.
#include <memory>
#include <LovyanGFX.hpp>
#include <SuperLabel.h>
#include <SuperPushButton.h>
#include <SuperCheckBox.h>
#include <SuperRadioButton.h>
#include <SuperProgressBar.h>
#include "FormModel.h"
#include "FontTable.h"

namespace FD {
// dst = the sprite the designer draws into; (ox, oy) = where the form's top-left corner is in dst.
inline std::unique_ptr<SuperControl> MakeControl(lgfx::LovyanGFX* dst, const DControl& c, int ox, int oy) {
    const lgfx::IFont* font = FindFont(c.font).font;
    int x = ox + c.x, y = oy + c.y;
    std::unique_ptr<SuperControl> p;
    switch (c.type) {
    case C_PUSHBUTTON: p.reset(new SuperPushButton(dst, x, y, c.w, font, c.text)); break;
    case C_CHECKBOX:   p.reset(new SuperCheckBox(dst, x, y, c.w, font, c.text)); break;
    case C_RADIO:      p.reset(new SuperRadioButton(dst, x, y, c.w, font, c.text)); break;
    case C_PROGRESS: {
        auto* b = new SuperProgressBar(dst, x, y, c.w, font);
        b->SetVisible(false, false); b->SetValue(60); b->SetVisible(true, false);     // a sample value, without drawing yet
        p.reset(b); break; }
    default:           p.reset(new SuperLabel(dst, x, y, c.w, font, c.text)); break;
    }
    if (c.borderStyle >= 0) p->SetBorderStyle((SuperControl::BorderStyle)c.borderStyle);
    if (c.borderSize >= 0)  p->SetBorder(c.borderSize, c.border);
    if (c.padding >= 0)     p->SetPadding(c.padding);
    if (c.align >= 0 && Info(c.type).hasAlign) p->SetAlign((SuperControl::Align)c.align);
    p->ColFg = c.fg; p->ColBg = c.bg; p->ColBorder = c.border;
    // no ApplyTheme: the controls' own defaults (which equal SuperTheme::Dark) are what the device draws unless the sketch sets a theme
    return p;
}
// Fills the derived values: the control's height, and what its layout defaults are (so the designer can show them and save only overrides).
inline void Measure(lgfx::LovyanGFX* dst, DControl& c) {
    DControl d = c; d.borderStyle = d.borderSize = d.padding = d.align = -1;
    auto def = MakeControl(dst, d, 0, 0);
    c.defStyle = def->GetBorderStyle(); c.defSize = def->GetBorder(); c.defPad = def->GetPadding(); c.defAlign = def->GetAlign();
    c.h = MakeControl(dst, c, 0, 0)->GetHeight();
}
inline int ControlHeight(lgfx::LovyanGFX* dst, const DControl& c) { DControl t = c; Measure(dst, t); return t.h; }
}
