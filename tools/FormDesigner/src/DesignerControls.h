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
#include <SuperForm.h>
#include <SuperGauge.h>
#include <SuperGraph.h>
#include <SuperUserDraw.h>
#include <SuperColorPicker.h>
#include <SuperRichText.h>
#include <SuperMessageBox.h>
#include "FormModel.h"
#include "FontTable.h"

namespace FD {
// The designer cannot run the user's drawing code: a UserDraw control shows this placeholder (diagonals and the control's name).
inline void UserDrawPlaceholder(SuperUserDraw&, lgfx::LGFX_Sprite* s, const SuperRect& r, void* user) {
    s->drawLine(r.x, r.y, r.x + r.w - 1, r.y + r.h - 1, 0x4A69); s->drawLine(r.x + r.w - 1, r.y, r.x, r.y + r.h - 1, 0x4A69);
    s->setFont(&lgfx::fonts::Font2); s->setTextDatum(lgfx::textdatum_t::middle_center); s->setTextColor(0xBDF7);
    std::string name = user ? *static_cast<std::string*>(user) : std::string("user draw"); int cx = r.x + r.w / 2, cy = r.y + r.h / 2, h = s->fontHeight();
    int w = std::max(s->textWidth(name.c_str()), s->textWidth("your code")) + 8;
    s->fillRect(cx - w / 2, cy - h - 2, w, 2 * h + 4, 0x0000);
    s->drawString(name.c_str(), cx, cy - h / 2); s->drawString("your code", cx, cy + h / 2 + 1);
}
// A message box as the real control, for the designer's preview. Centred on dst (give it a sprite as big as the form) or placed at its x, y. SetRestore with a no-op
// stops the control from reading pixels back: the designer only shows it (StartEdit draws it with its default button highlighted).
inline void NoRestore(void*) {}
inline std::unique_ptr<SuperMessageBox> MakeMessageBox(lgfx::LovyanGFX* dst, const DMessageBox& b) {
    std::unique_ptr<SuperMessageBox> m(new SuperMessageBox(dst, b.w, FindFont(b.font).font, nullptr, nullptr, nullptr));
    std::string msg; for (size_t i = 0; i < b.lines.size(); ++i) msg += (i ? "\n" : "") + b.lines[i];
    m->SetMessage(msg); for (const MsgButton& mb : b.buttons) m->AddButton(mb.text); m->SetDefault(b.defaultBtn);
    if (!b.title.empty()) m->SetLabel(b.title);
    if (!b.centered) { m->SetCentered(false); m->SetDisplayPos(b.x, b.y); }
    m->SetBorder(2, b.border); m->ColFg = b.fg; m->ColBg = b.bg;
    if (b.rich) m->SetRich(true);
    for (int k = 0; k < 4; ++k) if (!b.styleFont[k].empty()) m->SetStyleFont(k, FindFont(b.styleFont[k]).font);
    m->SetRestore(&NoRestore, nullptr);
    return m;
}
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
    case C_GAUGE:      p.reset(new SuperGauge(dst, x, y, c.w, (int)XInt(c, "bodyHeight"), font)); break;
    case C_GRAPH:      p.reset(new SuperGraph(dst, x, y, c.w, font, (int)XInt(c, "bodyHeight"))); break;
    case C_USERDRAW:   p.reset(new SuperUserDraw(dst, x, y, c.w, (int)XInt(c, "bodyHeight"), font)); break;
    case C_COLORPICKER: p.reset(new SuperColorPicker(dst, x, y, c.w, font, nullptr, nullptr, nullptr)); break;
    case C_RICHTEXT:   p.reset(new SuperRichText(dst, x, y, c.w, (int)XInt(c, "bodyHeight"), font, nullptr, nullptr, nullptr)); break;
    case C_DATEBOX:    p.reset(new SuperDateBox(dst, x, y, c.w, font, nullptr, nullptr, nullptr, (SuperDateBox::DateStyle)XInt(c, "style"))); break;
    default:           p.reset(new SuperLabel(dst, x, y, c.w, font, c.text)); break;
    }
    p->SetVisible(false, false);                                  // the setters below must not draw (SetValue would push at once)
    p->SetEnabled(c.enabled);                                     // disabled controls are drawn dimmed by the real control
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
    case C_LABEL: {
        auto* l = static_cast<SuperLabel*>(p.get());
        if (XBool(c, "rich")) l->SetRich(true);
        for (int k = 0; k < 4; ++k) { std::string f = XStr(c, STYLE_FONT_KEYS[k]); if (!f.empty()) l->SetStyleFont(k, FindFont(f).font); }
        break; }
    case C_PROGRESS: {
        auto* b = static_cast<SuperProgressBar*>(p.get());
        b->ColBar = (uint16_t)XInt(c, "barColor"); b->ShowPercent = XBool(c, "showPercent");
        b->SetPercentAlign((SuperControl::Align)XInt(c, "percentAlign"));
        b->SetValue(50);                                                           // a sample value so the bar shows something (50%: a centred percentage is half on the bar, half off it)
        break; }
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
        add(c.menu, nullptr);
        { std::string pl = XStr(c, "previewLevel");                                // a sub-menu level to look at instead of the top level (designer only: the idle control shows the top level)
          if (!pl.empty()) { SuperMenu::Item* cur = nullptr; size_t pp = 0;
            while (pp <= pl.size()) {
                size_t q = pl.find('/', pp); if (q == std::string::npos) q = pl.size();
                std::string seg = pl.substr(pp, q - pp); while (!seg.empty() && seg.front() == ' ') seg.erase(0, 1); while (!seg.empty() && seg.back() == ' ') seg.pop_back();
                SuperMenu::Item* nxt = nullptr; for (SuperMenu::Item* k : (cur ? cur->Kids : const_cast<SuperMenu::Item*>(m->Root())->Kids)) if (k->Text == seg && !k->Kids.empty()) { nxt = k; break; }
                if (!nxt) { cur = nullptr; break; } cur = nxt; pp = q + 1; }
            m->ShowLevel(cur); } }
        break; }
    case C_GAUGE: {
        auto* g = static_cast<SuperGauge*>(p.get());
        g->SetRange((float)XFloat(c, "min"), (float)XFloat(c, "max")); g->SetSweep((float)XInt(c, "sweepStart"), (float)XInt(c, "sweepEnd")); g->SetTicks((int)XInt(c, "majorTicks"), (int)XInt(c, "minorTicks"));
        g->ShowTickLabels = XBool(c, "tickLabels"); g->SetFormat((int)XInt(c, "decimals"), XStr(c, "unit")); g->ShowValue = XBool(c, "showValue");
        g->Needle = (SuperGauge::NeedleStyle)XInt(c, "needle"); g->NeedleByZone = XBool(c, "needleByZone");
        g->ColTrack = (uint16_t)XInt(c, "trackColor"); g->ColTick = (uint16_t)XInt(c, "tickColor"); g->ColNeedle = (uint16_t)XInt(c, "needleColor");
        for (const GaugeZone& z : c.zones) g->AddZone((float)z.from, (float)z.to, z.color);
        if (XInt(c, "minMaxMs") > 0) g->SetMinMax((int)XInt(c, "minMaxMs"), (float)XFloat(c, "minMaxDeg"), (int)XInt(c, "minMaxThick"));
        g->SetSmoothing(false); g->SetValue((float)XFloat(c, "value"));                      // (the picture is static: the needle is at the value at once)
        break; }
    case C_GRAPH: {
        auto* g = static_cast<SuperGraph*>(p.get());
        g->ShowGrid = XBool(c, "showGrid"); g->ShowRange = XBool(c, "showRange"); g->ColLine = (uint16_t)XInt(c, "lineColor"); g->ColGrid = (uint16_t)XInt(c, "gridColor");
        int cap = (int)XInt(c, "capacity"); if (cap > 0) g->SetCapacity(cap);
        if (!XBool(c, "autoRange")) g->SetRange((float)XFloat(c, "rangeMin"), (float)XFloat(c, "rangeMax"));
        int n = cap > 0 ? cap : c.w, kind = (int)XInt(c, "preview"); unsigned rnd = 12345;     // sample data so the designer shows a graph (not generated)
        for (int i = 0; i < n && kind != 3; ++i) {
            float v = kind == 0 ? 50.f + 40.f * std::sin(i * 0.12f) : kind == 1 ? 5.f + 90.f * i / std::max(1, n - 1) : (rnd = rnd * 1103515245u + 12345u, (float)((rnd >> 8) % 1000) / 10.f);
            g->AddSample(v, false);
        }
        break; }
    case C_COLORPICKER: static_cast<SuperColorPicker*>(p.get())->SetColor565((uint16_t)XInt(c, "value")); break;
    case C_RICHTEXT: {
        auto* r = static_cast<SuperRichText*>(p.get());
        std::string t; for (size_t i = 0; i < c.list.size(); ++i) t += (i ? "\n" : "") + c.list[i];
        for (int k = 0; k < 4; ++k) { std::string f = XStr(c, STYLE_FONT_KEYS[k]); if (!f.empty()) r->SetStyleFont(k, FindFont(f).font); }
        r->SetText(t); r->SetTabChars((int)XInt(c, "tabChars")); r->SetLineGap((int)XInt(c, "lineGap")); r->SetShowSource(XBool(c, "showSource"));
        break; }
    case C_USERDRAW: {
        auto* u = static_cast<SuperUserDraw*>(p.get());
        u->SetOnDraw(&UserDrawPlaceholder, (void*)&c.name);
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
// A gauge's value is pulled into its range (changing Minimum / Maximum moves the needle instead of refusing).
inline void ClampGaugeValue(DControl& c) {
    if (c.type != C_GAUGE) return;
    double mn = XFloat(c, "min"), mx = XFloat(c, "max"), v = XFloat(c, "value");
    if (mx > mn && (v < mn || v > mx)) ExtraSet(c, *FindExtra(C_GAUGE, "value"), FloatText(v < mn ? mn : mx));
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
    case C_GAUGE:
        if (XFloat(c, "max") <= XFloat(c, "min")) return "Maximum must be above Minimum";
        if (XFloat(c, "value") < XFloat(c, "min") || XFloat(c, "value") > XFloat(c, "max")) return "Value must be between Minimum and Maximum";
        if (XInt(c, "sweepEnd") <= XInt(c, "sweepStart")) return "the sweep must end after it starts (start -90, end 90 is a half dial)";
        for (const GaugeZone& z : c.zones) if (!(z.from < z.to)) return "a zone must end above where it starts";
        return "";
    case C_GRAPH:
        if (!XBool(c, "autoRange") && XFloat(c, "rangeMax") <= XFloat(c, "rangeMin")) return "Range to must be above Range from";
        return "";
    case C_MENU:
        if (!ValidName(XStr(c, "idPrefix") + "X")) return "the ID prefix must be letters, digits and underscores, safe in C++ (e.g. MNU_)";
        if (!MenuPathOk(c.menu, XStr(c, "previewLevel"))) return "no sub-menu called \"" + XStr(c, "previewLevel") + "\": type the text of a parent item (Settings) or a path (Settings/Sound); empty = the top level";
        return "";
    case C_LISTBOX: case C_COMBOBOX: case C_TABBAR:
        if (!c.list.empty() && XInt(c, "selected") >= (long)c.list.size()) return "the list has " + std::to_string(c.list.size()) + " items: Selected can be 0 to " + std::to_string(c.list.size() - 1);
        if (!c.list.empty() && c.type != C_TABBAR && SuperSep::IsUnclickable(c.list[(size_t)XInt(c, "selected")])) return "row " + std::to_string(XInt(c, "selected")) + " is a separator or heading (~): it cannot be selected";
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

// ---- variable height: the controls whose height the user can set (a Height row and top / bottom handles) ----
//  H_ROWS: a list box or menu: the height is a whole number of rows ("rows"), so it snaps to full rows.
//  H_BODY: gauge, graph, user draw, rich text: the height of the picture inside the frame ("bodyHeight"); rich text snaps to whole lines of text (the font's line height), the others are free.
enum HeightKind { H_FIXED = 0, H_ROWS, H_BODY };
inline int HeightKindOf(int type) { return type == C_LISTBOX || type == C_MENU ? H_ROWS : type == C_GAUGE || type == C_GRAPH || type == C_USERDRAW || type == C_RICHTEXT ? H_BODY : H_FIXED; }
struct HeightInfo { int kind = H_FIXED, unit = 1, overhead = 0, minH = 0, maxH = 0; const ExtraDef* def = nullptr; };
// total height = overhead (frame, padding, label) + n * unit, n limited by the extra's limits
inline HeightInfo HeightOf(lgfx::LovyanGFX* dst, const DControl& c) {
    HeightInfo r; r.kind = HeightKindOf(c.type); if (r.kind == H_FIXED) return r;
    r.def = FindExtra(c.type, r.kind == H_ROWS ? "rows" : "bodyHeight"); if (!r.def) { r.kind = H_FIXED; return r; }
    DControl a = c, b = c; Measure(dst, a);
    if (r.kind == H_ROWS) {
        ExtraSet(a, *r.def, std::to_string(r.def->lo)); ExtraSet(b, *r.def, std::to_string(r.def->lo + 1)); Measure(dst, a); Measure(dst, b);
        r.unit = std::max(1, b.h - a.h); r.overhead = a.h - r.unit * (int)r.def->lo; r.minH = r.overhead + r.unit * (int)r.def->lo; r.maxH = r.overhead + r.unit * (int)r.def->hi;
    } else {
        ExtraSet(a, *r.def, std::to_string(r.def->lo)); Measure(dst, a);
        r.overhead = a.h - (int)r.def->lo;
        if (c.type == C_RICHTEXT) { dst->setFont(FindFont(c.font).font); r.unit = std::max(1, (int)dst->fontHeight()); }          // whole lines of text
        r.minH = r.overhead + (int)r.def->lo; r.maxH = r.overhead + (int)r.def->hi;
    }
    return r;
}
// Sets the control's rows / body height so that its height is as near as possible to wantedH (whole rows / whole lines where the type has them). Returns the new height.
inline int SetHeightTo(lgfx::LovyanGFX* dst, DControl& c, int wantedH, bool roundDown = false) {          // roundDown: never taller than wantedH (e.g. the form's edge)
    HeightInfo h = HeightOf(dst, c); if (h.kind == H_FIXED) { Measure(dst, c); return c.h; }
    long n;
    auto rnd = [&](double v) { return roundDown ? (long)std::floor(v) : std::lround(v); };
    if (h.kind == H_ROWS) n = rnd((double)(wantedH - h.overhead) / h.unit);
    else if (h.unit > 1) n = rnd((double)(wantedH - h.overhead) / h.unit) * h.unit;               // whole lines: body height = a multiple of the line height
    else n = wantedH - h.overhead;
    n = std::max((long)h.def->lo, std::min((long)h.def->hi, n));
    if (h.kind == H_BODY && h.unit > 1 && n < h.unit) n = std::max((long)h.def->lo, (long)h.unit);
    ExtraSet(c, *h.def, std::to_string(n)); Measure(dst, c); return c.h;
}
}
