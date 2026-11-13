#pragma once
// The Form Designer's code generator: <FormName>.designer.h (every time) and <FormName>.h (once)
//
// The header is regenerated on EVERY generate, so nobody edits it. It defines class <FormName>Base: the controls as public members, the
// SuperForm, InitializeComponent() (only the properties that differ from each control's own default are written) and one virtual, empty
// handler per event. The user's own class (written once by the designer, step 2) derives from it and overrides the handlers.
//
//   class Form1Base {
//   public:
//       SuperLabel title; SuperPushButton okBtn; ...          // the controls
//       SuperForm Form;
//       Form1Base(lgfx::LovyanGFX* dst, SuperEncoder* enc, SuperButton* btnA);
//       void Show();                                          // clears the screen to the form's back colour, draws everything
//       SuperEditState Service();                             // call every loop pass
//   protected:
//       virtual void okBtn_Click() {}                         // push button: accepted;  check / radio button: wifi_Change()
//   };
//
// Radio buttons with the same group name share one SuperRadioGroup (a member named after the group).
//
// <FormName>.h is the user's file: written ONCE with a class that derives from the Base and has an empty override per handler. The designer never
// changes it afterwards (it is the user's code). When the form later gets new controls, UserFileMissing() lists the handlers the file does not
// mention yet, so the designer can tell the user to add them.
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "FormModel.h"

namespace FD {

inline std::string CppString(const std::string& s) {       // a C++ string literal (the designer only holds printable ASCII)
    std::string r = "\"";
    for (char c : s) { if (c == '"' || c == '\\') r += '\\'; r += c; }
    return r + "\"";
}
inline std::string HexColor(uint16_t c) { char b[8]; snprintf(b, sizeof b, "0x%04X", c); return b; }
inline std::string DesignerFileName(const DForm& f) { return f.name + ".designer.h"; }

inline const char* HeaderOf(int type) {
    static const char* h[C_COUNT] = { "SuperLabel.h", "SuperPushButton.h", "SuperCheckBox.h", "SuperRadioButton.h", "SuperProgressBar.h", "SuperSpinBox.h", "SuperSlider.h", "SuperTextBox.h", "SuperInputBox.h", "SuperTimeBox.h", "SuperTimeBox.h", "SuperListBox.h", "SuperComboBox.h", "SuperTabBar.h", "SuperMenu.h", "SuperGauge.h", "SuperGraph.h", "SuperUserDraw.h" };
    return h[type];
}
// The events a control raises: the handler is <control>_<Name>() and it is connected with control.<setter>(thunk, this).
struct EventDef { const char* name; const char* setter; const char* hint; bool idArg = false; const char* params = ""; const char* thunkParams = ""; const char* thunkArgs = ""; };       // params: the handler's parameters ("" = none); thunkParams / thunkArgs: the library callback's signature and what it passes on    // idArg: the handler receives the id of the chosen menu item      // hint: what the stub's comment tells the user (%s = the control)
inline std::vector<EventDef> EventsOf(int type) {
    switch (type) {
    case C_PUSHBUTTON: return { { "Click",  "SetOnAccept", "%s was pressed." } };
    case C_CHECKBOX:   return { { "Change", "SetOnChange", "%s changed: %s.GetChecked() is the new state." } };
    case C_RADIO:      return { { "Change", "SetOnChange", "%s changed: GROUP.GetSelected() is the selected radio button of the group." } };
    case C_SPINBOX:
    case C_SLIDER:
    case C_INPUTBOX:
    case C_TIMEBOX:
    case C_DATEBOX:    return { { "Change", "SetOnChange", "%s is being edited: %s.GetLiveValue() is the value so far." },
                                { "Accept", "SetOnAccept", "%s was accepted: %s.GetValue() is the new value." } };
    case C_LISTBOX:    return { { "Change", "SetOnChange", "%s: the highlight moved: %s.GetCursor() is the row under the cursor." },
                                { "Accept", "SetOnAccept", "%s: a row was chosen: %s.GetSelected() is its index, %s.GetSelectedText() its text." } };
    case C_COMBOBOX:   return { { "Change", "SetOnChange", "%s: the item under the cursor changed: %s.ListBox().GetCursor() is its index." },
                                { "Accept", "SetOnAccept", "%s: an item was chosen: %s.GetValue() is its text, %s.GetSelected() its index." } };
    case C_TABBAR:     return { { "Change", "SetOnChange", "%s: the cursor moved to another tab: %s.GetCursor() is its index." },
                                { "Accept", "SetOnAccept", "%s: a tab was chosen: %s.GetSelected() is its index (show that page)." } };
    case C_MENU:       return { { "Change", "SetOnChange", "%s: the cursor moved, or the menu level changed." },
                                { "Choose", "SetOnAccept", "%s: a command was chosen: id is its constant (switch on it).", true } };
    case C_USERDRAW:   return { { "Draw", "SetOnDraw", "%s: draw your picture here. spr is the sprite and body the rectangle to draw in (everything is clipped to it), e.g. spr->drawCircle(body.x + body.w / 2, body.y + body.h / 2, 20, 0x07E0);", false,
                                  "lgfx::LGFX_Sprite* spr, const SuperRect& body", "SuperUserDraw&, lgfx::LGFX_Sprite* s, const SuperRect& r, void* u", "s, r" },
                                { "Pointer", "SetOnPointer", "%s: touch. e.Type is PRESS, MOVE, RELEASE, TAP or LONG_PRESS; e.X / e.Y are relative to the body. Call %s.Repaint() to redraw.", false,
                                  "const SuperUserPointer& e", "SuperUserDraw&, const SuperUserPointer& e, void* u", "e" },
                                { "Accept", "SetOnAccept", "%s: a touch gesture ended." } };
    default:           return {};
    }
}
// The events this control really has (a user-draw control has Pointer and Accept only when Touch input is on).
inline std::vector<EventDef> EventsOf(const DControl& c) {
    std::vector<EventDef> v = EventsOf(static_cast<int>(c.type));
    if (c.type == C_USERDRAW && !XBool(c, "touch")) v.erase(std::remove_if(v.begin(), v.end(), [](const EventDef& e) { return std::string(e.name) != "Draw"; }), v.end());
    return v;
}
// every handler the Base declares, as "name_Event"
inline std::vector<std::string> HandlerNames(const DForm& f) {
    std::vector<std::string> v;
    for (const DControl& c : f.items) for (const EventDef& ev : EventsOf(c)) v.push_back(c.name + "_" + ev.name);
    return v;
}
inline bool HasPages(const DControl& c) { if (c.type != C_TABBAR) return false; for (const std::string& p : c.pages) if (!p.empty()) return true; return false; }
inline bool Focusable(const DControl& c) { return c.type != C_LABEL && c.type != C_PROGRESS && c.type != C_TEXTBOX && c.type != C_GAUGE && c.type != C_GRAPH && !(c.type == C_USERDRAW && !XBool(c, "touch")); }
inline std::string FloatLit(double x) { std::string t = FloatText(x); if (t.find('.') == std::string::npos) t += ".0"; return t + "f"; }
inline std::string CppChar(char c) { return c == '\'' ? "'\\''" : c == '\\' ? "'\\\\'" : std::string("'") + c + "'"; }

// The label (caption above / below the box) of a control: only what differs from the defaults.
inline void EmitLabel(std::ostringstream& o, const DControl& c) {
    if (!Info(c.type).hasLabel) return;
    const std::string n = c.name, lt = XStr(c, "label");
    if (!lt.empty()) o << "        " << n << ".SetLabel(" << CppString(lt) << (XInt(c, "labelPos") == 1 ? ", SuperControl::LABEL_BELOW" : "") << ");\n";
    else if (XInt(c, "labelPos") == 1) o << "        // (label position Below, but the label text is empty)\n";
    static const char* al[] = { "ALIGN_LEFT", "ALIGN_CENTER", "ALIGN_RIGHT" };
    if (XSet(c, "labelAlign")) o << "        " << n << ".SetLabelAlign(SuperControl::" << al[XInt(c, "labelAlign")] << ");\n";
    if (XSet(c, "labelFont")) o << "        " << n << ".SetLabelFont(&lgfx::fonts::" << FindFont(XStr(c, "labelFont")).id << ");\n";
    if (XSet(c, "labelColor")) o << "        " << n << ".ColLabel = " << HexColor((uint16_t)XInt(c, "labelColor")) << ";\n";
}
// The type's own settings (spin box: range, step, ...): only what differs from the control's defaults.
inline void EmitExtras(std::ostringstream& o, const DControl& c) {
    const std::string n = c.name;
    if (c.type == C_SPINBOX || c.type == C_SLIDER) {
        if (XSet(c, "min") || XSet(c, "max")) o << "        " << n << ".SetRange(" << XInt(c, "min") << ", " << XInt(c, "max") << ");\n";
        if (XSet(c, "step")) o << "        " << n << ".SetStep(" << XInt(c, "step") << ");\n";
    }
    if (c.type == C_SPINBOX) {
        if (XSet(c, "decimals")) o << "        " << n << ".SetDecimals(" << XInt(c, "decimals") << ");\n";
        if (XBool(c, "wrap")) o << "        " << n << ".SetWrap(true);\n";
        if (XSet(c, "prefix")) o << "        " << n << ".SetPrefix(" << CppString(XStr(c, "prefix")) << ");\n";
        if (XSet(c, "suffix")) o << "        " << n << ".SetSuffix(" << CppString(XStr(c, "suffix")) << ");\n";
    }
    if ((c.type == C_CHECKBOX || c.type == C_RADIO) && XBool(c, "checked")) o << "        " << n << ".SetChecked(true);\n";          // silent: no event
    if (HasList(c.type)) {
        if (c.type == C_LISTBOX) { if (XBool(c, "wrap")) o << "        " << n << ".Wrap = true;\n"; if (XSet(c, "rowPad")) o << "        " << n << ".RowPad = " << XInt(c, "rowPad") << ";\n"; }
        if (c.type == C_COMBOBOX && XSet(c, "maxRows")) o << "        " << n << ".MaxRows = " << XInt(c, "maxRows") << ";\n";
        if (c.type == C_TABBAR && XSet(c, "tabPad")) o << "        " << n << ".TabPad = " << XInt(c, "tabPad") << ";\n";
        if (!c.list.empty()) {
            o << "        " << n << ".SetItems({ ";
            for (size_t i = 0; i < c.list.size(); ++i) o << (i ? ", " : "") << CppString(c.list[i]);
            o << " });\n";
            if (XSet(c, "selected")) o << "        " << n << ".SetSelected(" << XInt(c, "selected") << ");\n";                    // after the items
        }
        if (c.type == C_COMBOBOX && XInt(c, "restore") == 0) o << "        " << n << ".SetRestore(" << n << "_RestoreThunk, this);      // the form redraws what the popup covered\n";
    }
    if (c.type == C_GAUGE) {
        if (XSet(c, "min") || XSet(c, "max")) o << "        " << n << ".SetRange(" << FloatLit(XFloat(c, "min")) << ", " << FloatLit(XFloat(c, "max")) << ");\n";
        if (XSet(c, "sweepStart") || XSet(c, "sweepEnd")) o << "        " << n << ".SetSweep(" << FloatLit((double)XInt(c, "sweepStart")) << ", " << FloatLit((double)XInt(c, "sweepEnd")) << ");\n";
        if (XSet(c, "majorTicks") || XSet(c, "minorTicks")) o << "        " << n << ".SetTicks(" << XInt(c, "majorTicks") << ", " << XInt(c, "minorTicks") << ");\n";
        if (!XBool(c, "tickLabels")) o << "        " << n << ".ShowTickLabels = false;\n";
        if (XSet(c, "decimals") || XSet(c, "unit")) o << "        " << n << ".SetFormat(" << XInt(c, "decimals") << ", " << CppString(XStr(c, "unit")) << ");\n";
        if (!XBool(c, "showValue")) o << "        " << n << ".ShowValue = false;\n";
        if (XInt(c, "needle") == 1) o << "        " << n << ".Needle = SuperGauge::NEEDLE_LINE;\n";
        if (!XBool(c, "needleByZone")) o << "        " << n << ".NeedleByZone = false;\n";
        if (XSet(c, "trackColor")) o << "        " << n << ".ColTrack = " << HexColor((uint16_t)XInt(c, "trackColor")) << ";\n";
        if (XSet(c, "tickColor")) o << "        " << n << ".ColTick = " << HexColor((uint16_t)XInt(c, "tickColor")) << ";\n";
        if (XSet(c, "needleColor")) o << "        " << n << ".ColNeedle = " << HexColor((uint16_t)XInt(c, "needleColor")) << ";\n";
        for (const GaugeZone& z : c.zones) o << "        " << n << ".AddZone(" << FloatLit(z.from) << ", " << FloatLit(z.to) << ", " << HexColor(z.color) << ");\n";
        if (XInt(c, "minMaxMs") > 0) o << "        " << n << ".SetMinMax(" << XInt(c, "minMaxMs") << ", " << FloatLit(XFloat(c, "minMaxDeg")) << ", " << XInt(c, "minMaxThick") << ");\n";
        if (!XBool(c, "smoothing")) o << "        " << n << ".SetSmoothing(false);\n";
        else if (XSet(c, "smoothMs")) o << "        " << n << ".SetSmoothing(true, " << XInt(c, "smoothMs") << ");\n";
        if (XSet(c, "value")) o << "        " << n << ".SetValue(" << FloatLit(XFloat(c, "value")) << ");\n";                       // last: the range and the zones are in place
    }
    if (c.type == C_GRAPH) {
        if (XSet(c, "capacity")) o << "        " << n << ".SetCapacity(" << XInt(c, "capacity") << ");\n";
        if (!XBool(c, "autoRange")) o << "        " << n << ".SetRange(" << FloatLit(XFloat(c, "rangeMin")) << ", " << FloatLit(XFloat(c, "rangeMax")) << ");\n";
        if (!XBool(c, "showGrid")) o << "        " << n << ".ShowGrid = false;\n";
        if (XBool(c, "showRange")) o << "        " << n << ".ShowRange = true;\n";
        if (XSet(c, "lineColor")) o << "        " << n << ".ColLine = " << HexColor((uint16_t)XInt(c, "lineColor")) << ";\n";
        if (XSet(c, "gridColor")) o << "        " << n << ".ColGrid = " << HexColor((uint16_t)XInt(c, "gridColor")) << ";\n";
    }
    if (c.type == C_USERDRAW) {
        if (XBool(c, "persistent")) o << "        " << n << ".SetPersistent(true);\n";
        if (XSet(c, "autoRedraw")) o << "        " << n << ".SetAutoRedraw(" << XInt(c, "autoRedraw") << ");\n";
    }
    if (c.type == C_MENU) {
        if (XBool(c, "wrap")) o << "        " << n << ".Wrap = true;\n";
        if (!XBool(c, "showBack")) o << "        " << n << ".ShowBack = false;\n";
        if (XSet(c, "rowPad")) o << "        " << n << ".RowPad = " << XInt(c, "rowPad") << ";\n";
        if (!c.menu.empty()) {                                                                  // the items, nested like the JSON (fd_pN = a parent's item)
            o << "        {\n"; int counter = 0;
            std::function<void(const std::vector<MenuNode>&, const std::string&)> emit = [&](const std::vector<MenuNode>& v, const std::string& parent) {
                for (const MenuNode& k : v) {
                    std::string idArg = (!k.itemId.empty() && k.id >= 0) ? k.itemId : "0";
                    o << "            ";
                    std::string var;
                    if (!k.kids.empty()) { var = "fd_p" + std::to_string(++counter); o << "auto* " << var << " = "; }
                    o << n << ".AddItem(" << CppString(k.text) << ", " << idArg << (parent.empty() ? "" : ", " + parent) << ");\n";
                    if (!k.kids.empty()) emit(k.kids, var);
                }
            };
            emit(c.menu, ""); o << "        }\n";
        }
    }
    if (c.type == C_TEXTBOX && !c.text.empty()) o << "        " << n << ".SetText(" << CppString(c.text) << ");\n";
    if (c.type == C_INPUTBOX) {
        if (XSet(c, "literalColor")) o << "        " << n << ".ColLiteral = " << HexColor((uint16_t)XInt(c, "literalColor")) << ";\n";
        if (!XBool(c, "trimRight")) o << "        " << n << ".TrimRight = false;\n";
        if (XSet(c, "value")) o << "        " << n << ".SetValue(" << CppString(XStr(c, "value")) << ");\n";
    }
    if (c.type == C_TIMEBOX || c.type == C_DATEBOX) {
        if (c.type == C_DATEBOX) {
            if (XSet(c, "separator")) o << "        " << n << ".SetSeparator(" << CppChar(XStr(c, "separator")[0]) << ");\n";
            if (XSet(c, "yearMin") || XSet(c, "yearMax")) o << "        " << n << ".SetYearRange(" << XInt(c, "yearMin") << ", " << XInt(c, "yearMax") << ");\n";
        }
        if (XBool(c, "wrap")) o << "        " << n << ".SetWrap(true);\n";
        if (XSet(c, "value")) o << "        " << n << ".SetValue(" << CppString(XStr(c, "value")) << ");\n";                 // after the year range
    }
    if (c.type == C_SLIDER && !XBool(c, "showValue")) o << "        " << n << ".ShowValue = false;\n";
    if ((c.type == C_SPINBOX || c.type == C_SLIDER) && XSet(c, "value")) o << "        " << n << ".SetValue(" << XInt(c, "value") << ");\n";     // after the range: SetValue clamps
}

inline std::string GenerateDesigner(const DForm& f) {
    const std::string base = f.name + "Base";
    std::ostringstream o;
    o << "// " << DesignerFileName(f) << " - GENERATED by the SuperGUI Form Designer. Do not edit: this file is rewritten every time the form is generated.\n"
      << "// Put your code in the file the designer wrote once next to it (it derives from " << base << " and overrides the handlers).\n"
      << "#pragma once\n#include <LovyanGFX.hpp>\n#include <SuperForm.h>\n";
    std::set<std::string> inc;
    for (const DControl& c : f.items) inc.insert(HeaderOf(c.type));
    for (const std::string& h : inc) o << "#include <" << h << ">\n";

    std::vector<std::string> groups;                           // radio groups, in order of first use
    for (const DControl& c : f.items) if (c.type == C_RADIO && std::find(groups.begin(), groups.end(), c.group) == groups.end()) groups.push_back(c.group);

    o << "\nclass " << base << " {\npublic:\n"
      << "    static constexpr int FormWidth = " << f.w << ", FormHeight = " << f.h << ";\n"
      << "    static constexpr uint16_t FormBackColor = " << HexColor(f.bg) << ";\n";
    for (const DControl& c : f.items) if (c.type == C_MENU) {                                        // the menu item ids: use them in your Choose handler
        bool any = false;
        MenuForEach(c.menu, [&](const MenuNode& k) { if (!k.itemId.empty() && k.id >= 0) { if (!any) o << "\n    // " << c.name << ": item ids\n"; any = true; o << "    static constexpr uint16_t " << k.itemId << " = " << k.id << ";\n"; } });
    }
    { auto groups = f.UsedGroups();                                                                      // control groups: SetGroup(GRP_X), Form.SetGroupVisible(GRP_X, ...)
      if (!groups.empty()) { o << "\n    // control groups (SuperControl::SetGroup; use them with Form.SetGroupVisible / SetGroupEnabled / ShowPage)\n"; for (const std::string& g : groups) o << "    static constexpr uint16_t " << g << " = " << f.GroupId(g) << ";\n"; } }
    o << "\n";
    for (const DControl& c : f.items) o << "    " << Info(c.type).key << " " << c.name << ";\n";
    for (const std::string& g : groups) o << "    SuperRadioGroup " << g << ";\n";
    o << "    SuperForm Form;\n\n";

    // constructor: every control is built with its position, width and font
    o << "    " << base << "(lgfx::LovyanGFX* dst, SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB)";
    bool first = true;
    auto sep = [&]() { o << (first ? "\n        : " : ",\n          "); first = false; };
    for (const DControl& c : f.items) {
        sep();
        const std::string fontArg = std::string("&lgfx::fonts::") + FindFont(c.font).id;
        if (c.type == C_GAUGE || c.type == C_USERDRAW) { o << c.name << "(dst, " << c.x << ", " << c.y << ", " << c.w << ", " << XInt(c, "bodyHeight") << ", " << fontArg << ")"; continue; }       // (dst, x, y, w, bodyHeight, font)
        if (c.type == C_GRAPH) { o << c.name << "(dst, " << c.x << ", " << c.y << ", " << c.w << ", " << fontArg << ", " << XInt(c, "bodyHeight") << ")"; continue; }                        // (dst, x, y, w, font, bodyHeight)
        o << c.name << "(dst, " << c.x << ", " << c.y << ", " << c.w << ", " << fontArg;
        if (Info(c.type).ctorText) o << ", " << CppString(c.text);
        if (c.type == C_LISTBOX || c.type == C_MENU) o << ", " << XInt(c, "rows");                                    // the number of visible rows is a constructor argument
        if (c.type == C_INPUTBOX) o << ", " << CppString(XStr(c, "template"));                      // the template comes before the encoder and buttons
        if (Info(c.type).editable) o << ", enc, btnA, btnB";                  // editable controls take the encoder and the two buttons
        if (c.type == C_TIMEBOX && !XBool(c, "seconds")) o << ", false";                           // (seconds = true is the default)
        if (c.type == C_DATEBOX && XInt(c, "style") == 1) o << ", SuperDateBox::DATE_MDY";
        o << ")";
    }
    sep(); o << "Form(enc, btnA)"; sep(); o << "Dst(dst)";
    o << " {\n        InitializeComponent();\n";
    for (const DControl& c : f.items) if (HasPages(c)) o << "        " << c.name << "_ShowPage(" << c.name << ".GetSelected(), false);       // the first tab's page is shown, the others are hidden\n";
    for (const DControl& c : f.items) if (!c.visible) o << "        " << c.name << ".SetVisible(false, false);       // Visible = No: hidden at start (after the first page is shown)\n";
    o << "    }\n    virtual ~" << base << "() {}\n\n";

    o << "    // Clears the screen to the form's back colour and draws every control.\n"
      << "    void Show() { Dst->fillScreen(FormBackColor); Form.Draw(); }\n"
      << "    // Call every loop pass: focus handling and editing (see SuperForm).\n"
      << "    SuperEditState Service() { return Form.Service(); }\n\n";
    for (const DControl& c : f.items) if (HasPages(c)) {                                                   // tab pages: one control group per tab
        o << "    // " << c.name << ": shows the controls of the chosen tab's group and hides the other tabs' (called when a tab is chosen)\n"
          << "    void " << c.name << "_ShowPage(int index, bool draw = true) {\n        static const uint16_t pages[] = { ";
        for (size_t i = 0; i < c.list.size(); ++i) o << (i ? ", " : "") << (i < c.pages.size() && !c.pages[i].empty() ? c.pages[i] : "0");
        o << " };\n        Form.ShowPage(pages, " << c.list.size() << ", (index >= 0 && index < " << c.list.size() << ") ? pages[index] : 0, draw);\n    }\n\n";
    }

    if (!HandlerNames(f).empty()) {
        o << "protected:\n    // Events: override these in your own class.\n";
        for (const DControl& c : f.items) for (const EventDef& ev : EventsOf(c)) o << "    virtual void " << c.name << "_" << ev.name << (ev.idArg ? "(uint16_t id) {}\n" : std::string("(") + ev.params + ") {}\n");
        o << "\n";
    }

    o << "private:\n    lgfx::LovyanGFX* Dst;\n\n    void InitializeComponent() {\n";
    for (const DControl& c : f.items) {
        o << "        // " << c.name << " (" << Info(c.type).title << ")\n";
        const std::string n = c.name;
        if (c.borderSize >= 0) o << "        " << n << ".SetBorder(" << c.borderSize << ", " << HexColor(c.border) << ");\n";
        else if (c.border != DEF_BORDER) o << "        " << n << ".ColBorder = " << HexColor(c.border) << ";\n";
        if (c.fg != DEF_FG) o << "        " << n << ".ColFg = " << HexColor(c.fg) << ";\n";
        if (c.bg != DEF_BG) o << "        " << n << ".ColBg = " << HexColor(c.bg) << ";\n";
        if (f.bg != DEF_FORM_BG) o << "        " << n << ".ColHighlightClear = FormBackColor;\n";
        if (c.borderStyle >= 0) {
            static const char* s[] = { "BORDER_STANDARD", "BORDER_THREED", "BORDER_SLEEK" };
            o << "        " << n << ".SetBorderStyle(SuperControl::" << s[c.borderStyle] << ");\n";
        }
        if (c.padding >= 0) o << "        " << n << ".SetPadding(" << c.padding << ");\n";
        if (c.align >= 0 && Info(c.type).hasAlign) {
            static const char* a[] = { "ALIGN_LEFT", "ALIGN_CENTER", "ALIGN_RIGHT" };
            o << "        " << n << ".SetAlign(SuperControl::" << a[c.align] << ");\n";
        }
        if (!c.ctlGroup.empty()) o << "        " << n << ".SetGroup(" << c.ctlGroup << ");\n";
        if (!c.enabled) o << "        " << n << ".SetEnabled(false);\n";
        EmitLabel(o, c);
        EmitExtras(o, c);
        for (const EventDef& ev : EventsOf(c)) o << "        " << n << "." << ev.setter << "(" << n << "_" << ev.name << "Thunk, this);\n";
        if (c.type == C_RADIO) o << "        " << c.group << ".Add(&" << n << ");\n";
        o << "        Form.Add(&" << n << (Focusable(c) ? "" : ", false") << ");\n";
    }
    o << "    }\n";
    for (const DControl& c : f.items) for (const EventDef& ev : EventsOf(c)) {
        if (ev.idArg) o << "    static void " << c.name << "_" << ev.name << "Thunk(void* u) { auto* s = static_cast<" << base << "*>(u); s->" << c.name << "_" << ev.name << "((uint16_t)s->" << c.name << ".GetResultId()); }\n";
        else if (ev.params[0]) o << "    static void " << c.name << "_" << ev.name << "Thunk(" << ev.thunkParams << ") { static_cast<" << base << "*>(u)->" << c.name << "_" << ev.name << "(" << ev.thunkArgs << "); }\n";
        else if (HasPages(c) && std::string(ev.name) == "Accept")
            o << "    static void " << c.name << "_AcceptThunk(void* u) { auto* s = static_cast<" << base << "*>(u); s->" << c.name << "_ShowPage(s->" << c.name << ".GetSelected()); s->" << c.name << "_Accept(); }\n";
        else o << "    static void " << c.name << "_" << ev.name << "Thunk(void* u) { static_cast<" << base << "*>(u)->" << c.name << "_" << ev.name << "(); }\n";
    }
    for (const DControl& c : f.items) if (c.type == C_COMBOBOX && XInt(c, "restore") == 0)
        o << "    static void " << c.name << "_RestoreThunk(void* u) {\n        auto* s = static_cast<" << base << "*>(u);\n"
          << "        s->Form.Redraw(s->" << c.name << ".GetPopupX(), s->" << c.name << ".GetPopupY(), s->" << c.name << ".GetPopupWidth(), s->" << c.name << ".GetPopupHeight(), FormBackColor);\n    }\n";
    o << "};\n";
    return o.str();
}

// ---- the user's file ----
inline std::string UserFileName(const DForm& f) { return f.name + ".h"; }
inline std::string GenerateUserFile(const DForm& f) {
    const std::string base = f.name + "Base";
    std::ostringstream o;
    o << "// " << UserFileName(f) << " - YOUR code for the form. The Form Designer wrote this file once and never changes it again.\n"
      << "// The controls and their layout are in " << DesignerFileName(f) << " (regenerated every time: do not edit that one).\n"
      << "//\n// In your sketch:\n"
      << "//     #include \"" << UserFileName(f) << "\"\n"
      << "//     " << f.name << " ui(&lcd, &Enc, &BtnA, &BtnB);   // lcd: your display; Enc, BtnA, BtnB: your encoder and two buttons\n"
      << "//     setup():  ui.Show();                          // draws the form\n"
      << "//     loop():   ui.Service();                       // focus, editing and the handlers below\n"
      << "#pragma once\n#include \"" << DesignerFileName(f) << "\"\n\n"
      << "class " << f.name << " : public " << base << " {\npublic:\n"
      << "    " << f.name << "(lgfx::LovyanGFX* dst, SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB) : " << base << "(dst, enc, btnA, btnB) {\n"
      << "        // Your start-up code: set initial values, e.g. " << (f.items.empty() ? "someControl.ColFg = 0xFFE0" : f.items[0].name + ".ColFg = 0xFFE0") << ".\n    }\n";
    auto h = HandlerNames(f);
    if (!h.empty()) {
        o << "\nprotected:\n";
        for (const DControl& c : f.items) for (const EventDef& ev : EventsOf(c)) {
            std::string hint = ev.hint;                                       // %s = the control, GROUP = its radio group
            for (size_t p; (p = hint.find("%s")) != std::string::npos;) hint.replace(p, 2, c.name);
            for (size_t p; (p = hint.find("GROUP")) != std::string::npos;) hint.replace(p, 5, c.group);
            if (ev.idArg) {                                                   // a menu: one case per command constant
                o << "    void " << c.name << "_" << ev.name << "(uint16_t id) override {\n        // " << hint << "\n        switch (id) {\n";
                MenuForEach(c.menu, [&](const MenuNode& k) { if (k.kids.empty() && !k.itemId.empty()) o << "        case " << k.itemId << ":    // " << k.text << "\n            break;\n"; });
                o << "        default: break;\n        }\n    }\n";
                continue;
            }
            if (HasPages(c) && std::string(ev.name) == "Accept") hint += " The page (control group) of that tab is already shown.";
            o << "    void " << c.name << "_" << ev.name << "(" << ev.params << ") override {\n        // " << hint << "\n    }\n";
        }
    }
    o << "};\n";
    return o.str();
}
// The handlers (name_Event) that the user's file does not mention yet.
inline std::vector<std::string> UserFileMissing(const DForm& f, const std::string& userText) {
    std::vector<std::string> m;
    for (const std::string& h : HandlerNames(f)) if (userText.find(h) == std::string::npos) m.push_back(h);
    return m;
}

struct GenResult { bool ok = false, unchanged = false; std::string path, error; };

// Writes <dir>/<FormName>.designer.h (through a temporary file, so a failure never leaves half a file). On success the folder is remembered
// as the form's last successful save folder (f.outDir); on failure the form is untouched.
inline GenResult WriteDesigner(DForm& f, const std::string& dir) {
    namespace fs = std::filesystem;
    GenResult r; std::error_code ec;
    { std::string problem = f.MenuProblem(); if (problem.empty()) problem = f.GroupProblem(); if (!problem.empty()) { r.error = problem; return r; } }          // never write code that would not compile
    if (dir.empty() || !fs::is_directory(dir, ec)) { r.error = "folder \"" + dir + "\" does not exist"; return r; }
    fs::path p = fs::path(dir) / DesignerFileName(f);
    r.path = p.string();
    std::string text = GenerateDesigner(f);
    { std::ifstream in(p, std::ios::binary); if (in) { std::stringstream ss; ss << in.rdbuf(); if (ss.str() == text) { r.ok = r.unchanged = true; f.outDir = dir; return r; } } }
    fs::path tmp = p; tmp += ".tmp";
    { std::ofstream out(tmp, std::ios::binary | std::ios::trunc); if (!out) { r.error = "cannot write " + tmp.string(); return r; } out << text; if (!out) { r.error = "write failed: " + tmp.string(); return r; } }
    fs::rename(tmp, p, ec);
    if (ec) { fs::remove(tmp, ec); r.error = "cannot replace " + p.string(); return r; }
    f.outDir = dir; r.ok = true; return r;
}

struct UserResult { bool ok = false, created = false; std::string path, error; std::vector<std::string> missing; };

// Writes <dir>/<FormName>.h only when it does not exist yet. An existing file is never touched: it is read, and the handlers it does not
// mention yet are returned in `missing`.
inline UserResult WriteUserFile(const DForm& f, const std::string& dir) {
    namespace fs = std::filesystem;
    UserResult r; std::error_code ec;
    if (dir.empty() || !fs::is_directory(dir, ec)) { r.error = "folder \"" + dir + "\" does not exist"; return r; }
    fs::path p = fs::path(dir) / UserFileName(f);
    r.path = p.string();
    if (fs::exists(p, ec)) {
        if (!fs::is_regular_file(p, ec)) { r.error = p.string() + " is not a file"; return r; }
        std::ifstream in(p, std::ios::binary); std::stringstream ss; ss << in.rdbuf();
        r.missing = UserFileMissing(f, ss.str()); r.ok = true; return r;
    }
    fs::path tmp = p; tmp += ".tmp";
    { std::ofstream out(tmp, std::ios::binary | std::ios::trunc); if (!out) { r.error = "cannot write " + tmp.string(); return r; } out << GenerateUserFile(f); if (!out) { r.error = "write failed: " + tmp.string(); return r; } }
    fs::rename(tmp, p, ec);
    if (ec) { fs::remove(tmp, ec); r.error = "cannot create " + p.string(); return r; }
    r.ok = r.created = true; return r;
}
}
