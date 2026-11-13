#pragma once
// Builds the REAL SuperGUI control for a designer control, so the design surface shows exactly what the device will draw.
// The control is created for one draw (or one height measurement) and destroyed again; the designer keeps only the model.
#include <algorithm>
#include <functional>
#include <memory>
#include <LovyanGFX.hpp>
#include <SuperLabel.h>
#include <SuperPushButton.h>
#include <SuperCheckBox.h>
#include <SuperRadioButton.h>
#include <SuperProgressBar.h>
#include <SuperSpinBox.h>
#include <SuperSlider.h>
#include <SuperTextBox.h>
#include <SuperInputBox.h>
#include <SuperTimeBox.h>
#include <SuperListBox.h>
#include <SuperComboBox.h>
#include <SuperTabBar.h>
#include <SuperMenu.h>
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
    case C_PROGRESS:   p.reset(new SuperProgressBar(dst, x, y, c.w, font)); break;
    case C_SPINBOX:    p.reset(new SuperSpinBox(dst, x, y, c.w, font, nullptr, nullptr, nullptr)); break;       // no encoder / buttons in the designer
    case C_SLIDER:     p.reset(new SuperSlider(dst, x, y, c.w, font, nullptr, nullptr, nullptr)); break;
    case C_TEXTBOX:    p.reset(new SuperTextBox(dst, x, y, c.w, font)); break;
    case C_INPUTBOX:   p.reset(new SuperInputBox(dst, x, y, c.w, font, XStr(c, "template").c_str(), nullptr, nullptr, nullptr)); break;      // (the template is copied)
    case C_TIMEBOX:    p.reset(new SuperTimeBox(dst, x, y, c.w, font, nullptr, nullptr, nullptr, XBool(c, "seconds"))); break;
    case C_LISTBOX:    p.reset(new SuperListBox(dst, x, y, c.w, font, (int)XInt(c, "rows"), nullptr, nullptr, nullptr)); break;
    case C_COMBOBOX:   p.reset(new SuperComboBox(dst, x, y, c.w, font, nullptr, nullptr, nullptr)); break;
    case C_TABBAR:     p.reset(new SuperTabBar(dst, x, y, c.w, font, nullptr, nullptr, nullptr)); break;
    case C_MENU:       p.reset(new SuperMenu(dst, x, y, c.w, font, (int)XInt(c, "rows"), nullptr, nullptr, nullptr)); break;
    case C_DATEBOX:    p.reset(new SuperDateBox(dst, x, y, c.w, font, nullptr, nullptr, nullptr, (SuperDateBox::DateStyle)XInt(c, "style"))); break;
    default:           p.reset(new SuperLabel(dst, x, y, c.w, font, c.text)); break;
    }
    p->SetVisible(false, false);                                  // the setters below must not draw (SetValue would push at once)
    if (c.borderStyle >= 0) p->SetBorderStyle((SuperControl::BorderStyle)c.borderStyle);
    if (c.borderSize >= 0)  p->SetBorder(c.borderSize, c.border);
    if (c.padding >= 0)     p->SetPadding(c.padding);
    if (c.align >= 0 && Info(c.type).hasAlign) p->SetAlign((SuperControl::Align)c.align);
    p->ColFg = c.fg; p->ColBg = c.bg; p->ColBorder = c.border;
    // the label (caption above / below the box)
    if (Info(c.type).hasLabel) {
        std::string lt = XStr(c, "label");
        if (!lt.empty()) p->SetLabel(lt, (SuperControl::LabelPos)XInt(c, "labelPos"));
        p->SetLabelAlign((SuperControl::Align)XInt(c, "labelAlign"));
        std::string lf = XStr(c, "labelFont"); if (!lf.empty()) p->SetLabelFont(FindFont(lf).font);
        p->ColLabel = (uint16_t)XInt(c, "labelColor");
    }
    // the type's own settings
    switch (c.type) {
    case C_PROGRESS: static_cast<SuperProgressBar*>(p.get())->SetValue(60); break;                 // a sample value so the bar shows something
    case C_SPINBOX: {
        auto* b = static_cast<SuperSpinBox*>(p.get());
        b->SetRange((int32_t)XInt(c, "min"), (int32_t)XInt(c, "max")); b->SetStep((int32_t)XInt(c, "step")); b->SetDecimals((int)XInt(c, "decimals"));
        b->SetWrap(XBool(c, "wrap")); b->SetPrefix(XStr(c, "prefix")); b->SetSuffix(XStr(c, "suffix")); b->SetValue((int32_t)XInt(c, "value"));
        break; }
    case C_SLIDER: {
        auto* b = static_cast<SuperSlider*>(p.get());
        b->SetRange((int32_t)XInt(c, "min"), (int32_t)XInt(c, "max")); b->SetStep((int32_t)XInt(c, "step")); b->SetValue((int32_t)XInt(c, "value"));
        b->ShowValue = XBool(c, "showValue");
        break; }
    case C_CHECKBOX:
    case C_RADIO:   static_cast<SuperCheckBox*>(p.get())->SetChecked(XBool(c, "checked")); break;           // (silent)
    case C_LISTBOX: {
        auto* b = static_cast<SuperListBox*>(p.get());
        b->Wrap = XBool(c, "wrap"); b->RowPad = (int)XInt(c, "rowPad"); b->SetItems(c.list); if (!c.list.empty()) b->SetSelected((int)XInt(c, "selected"));
        break; }
    case C_COMBOBOX: {
        auto* b = static_cast<SuperComboBox*>(p.get());
        b->MaxRows = (int)XInt(c, "maxRows"); b->SetItems(c.list); if (!c.list.empty()) b->SetSelected((int)XInt(c, "selected"));
        break; }
    case C_TABBAR: {
        auto* b = static_cast<SuperTabBar*>(p.get());
        b->TabPad = (int)XInt(c, "tabPad"); b->SetItems(c.list); if (!c.list.empty()) b->SetSelected((int)XInt(c, "selected"));
        break; }
    case C_MENU: {
        auto* m = static_cast<SuperMenu*>(p.get());
        m->Wrap = XBool(c, "wrap"); m->ShowBack = XBool(c, "showBack"); m->RowPad = (int)XInt(c, "rowPad");
        std::function<void(const std::vector<MenuNode>&, SuperMenu::Item*)> add = [&](const std::vector<MenuNode>& nodes, SuperMenu::Item* parent) {
            for (const MenuNode& k : nodes) { SuperMenu::Item* it = m->AddItem(k.text, k.id < 0 ? 0 : k.id, parent); add(k.kids, it); }
        };
        add(c.menu, nullptr);                                                      // (the idle menu shows the top level)
        break; }
    case C_TEXTBOX: static_cast<SuperTextBox*>(p.get())->SetText(c.text); break;
    case C_INPUTBOX: {
        auto* b = static_cast<SuperInputBox*>(p.get());
        b->ColLiteral = (uint16_t)XInt(c, "literalColor"); b->TrimRight = XBool(c, "trimRight");
        if (XSet(c, "value")) b->SetValue(XStr(c, "value"));                      // (a value that does not fit is ignored here: the designer refuses it earlier)
        break; }
    case C_TIMEBOX: {
        auto* b = static_cast<SuperTimeBox*>(p.get());
        b->SetWrap(XBool(c, "wrap")); if (XSet(c, "value")) b->SetValue(XStr(c, "value"));
        break; }
    case C_DATEBOX: {
        auto* b = static_cast<SuperDateBox*>(p.get());
        std::string sep = XStr(c, "separator"); if (!sep.empty()) b->SetSeparator(sep[0]);
        b->SetYearRange((int)XInt(c, "yearMin"), (int)XInt(c, "yearMax")); b->SetWrap(XBool(c, "wrap"));
        if (XSet(c, "value")) b->SetValue(XStr(c, "value"));                      // (after the year range)
        break; }
    default: break;
    }
    p->SetVisible(true, false);
    // no ApplyTheme: the controls' own defaults (which equal SuperTheme::Dark) are what the device draws unless the sketch sets a theme
    return p;
}
// "" when the control's own settings make sense, else a message for the user (the real control is the judge). Used to refuse bad input.
inline std::string Validate(lgfx::LovyanGFX* dst, const DControl& c) {
    switch (c.type) {
    case C_INPUTBOX: {
        auto p = MakeControl(dst, c, 0, 0); auto* b = static_cast<SuperInputBox*>(p.get());
        if (b->SlotCount() == 0) return "the template needs at least one slot: # A a B or *";
        if (XSet(c, "value") && !XStr(c, "value").empty()) {
            SuperInputBox t(dst, 0, 0, 100, &lgfx::fonts::Font2, XStr(c, "template").c_str(), nullptr, nullptr, nullptr);
            t.SetVisible(false, false);
            if (!t.SetValue(XStr(c, "value"))) return "the value does not fit the template (too long, or a character the slot cannot hold)";
        }
        return ""; }
    case C_TIMEBOX: {
        SuperTimeBox t(dst, 0, 0, 100, &lgfx::fonts::Font2, nullptr, nullptr, nullptr, XBool(c, "seconds")); t.SetVisible(false, false);
        std::string v = XStr(c, "value"); size_t colons = (size_t)std::count(v.begin(), v.end(), ':');
        for (char ch : v) if (!isdigit((unsigned char)ch) && ch != ':') return "a time looks like 07:30:00 (HH:MM:SS) or 07:30";
        if (colons < 1 || colons > 2 || !t.SetValue(v)) return "not a valid time: hours 00-23, minutes and seconds 00-59";
        return ""; }
    case C_DATEBOX: {
        std::string sep = XStr(c, "separator");
        if (sep.size() != 1 || !isprint((unsigned char)sep[0]) || isdigit((unsigned char)sep[0]) || sep[0] == ' ') return "the separator is one visible character that is not a digit";
        if (XInt(c, "yearMin") > XInt(c, "yearMax")) return "\"Year from\" must not be after \"Year to\"";
        SuperDateBox t(dst, 0, 0, 100, &lgfx::fonts::Font2, nullptr, nullptr, nullptr); t.SetVisible(false, false);
        t.SetYearRange((int)XInt(c, "yearMin"), (int)XInt(c, "yearMax"));
        if (!t.SetValue(XStr(c, "value"))) return "not a valid date inside the year range: YYYY-MM-DD, e.g. 2026-12-31 (2026-02-30 does not exist)";
        return ""; }
    case C_MENU:
        if (!ValidName(XStr(c, "idPrefix") + "X")) return "the ID prefix must be letters, digits and underscores, safe in C++ (e.g. MNU_)";
        return "";
    case C_LISTBOX: case C_COMBOBOX: case C_TABBAR:
        if (!c.list.empty() && XInt(c, "selected") >= (long)c.list.size()) return "the list has " + std::to_string(c.list.size()) + " items: Selected can be 0 to " + std::to_string(c.list.size() - 1);
        return "";
    default: return "";
    }
}
// After a change: store the value the way the control prints it (07:5 -> 07:05:00; 42 in ### -> 042 is stored literal-free as 042).
inline void Normalize(lgfx::LovyanGFX* dst, DControl& c) {
    if (!XSet(c, "value")) return;
    auto p = MakeControl(dst, c, 0, 0);
    const ExtraDef* d = FindExtra(c.type, "value"); if (!d) return;
    if (c.type == C_INPUTBOX) ExtraSet(c, *d, static_cast<SuperInputBox*>(p.get())->GetValue());
    else if (c.type == C_TIMEBOX) ExtraSet(c, *d, static_cast<SuperTimeBox*>(p.get())->GetValue());
    else if (c.type == C_DATEBOX) ExtraSet(c, *d, static_cast<SuperDateBox*>(p.get())->GetValue());
}
// The width at which the whole text of a SuperTextBox-based control fits (Info(type).fitWidth), measured on the real control.
inline int FitWidth(lgfx::LovyanGFX* dst, const DControl& c) {
    DControl t = c; t.w = 1000;
    auto p = MakeControl(dst, t, 0, 0);
    return static_cast<SuperTextBox*>(p.get())->GetContentWidth();
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
