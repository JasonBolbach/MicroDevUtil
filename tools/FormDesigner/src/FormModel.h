#pragma once
// The Form Designer's data model: a form (size, name) with a list of controls, saved as JSON (designer only; the generated C++ comes later).
//
//   {
//     "form": { "name": "Form1", "width": 320, "height": 240, "backColor": "0x0000", "outputPath": "/home/me/project" },
//     "controls": [
//       { "type": "SuperLabel", "name": "label1", "text": "Label", "font": "Font2", "textColor": "0xFFFF", "backColor": "0x0000", "borderColor": "0xFFFF",
//         "x": 10, "y": 10, "width": 100 }
//     ]
//   }
//
// outputPath is the folder the generated code is written to. The designer checks on load that it exists and otherwise uses its working folder.
// A control's height is not stored: it follows the font (the real SuperGUI control reports it, see DesignerControls.h).
#include <cctype>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include "DesignerConfig.h"
#include "FontTable.h"
#include "ReservedNames.h"

namespace FD {

enum CType { C_LABEL, C_PUSHBUTTON, C_CHECKBOX, C_RADIO, C_PROGRESS, C_SPINBOX, C_SLIDER, C_TEXTBOX, C_INPUTBOX, C_TIMEBOX, C_DATEBOX, C_LISTBOX, C_COMBOBOX, C_TABBAR, C_MENU, C_GAUGE, C_GRAPH, C_USERDRAW, C_COLORPICKER, C_RICHTEXT, C_COUNT };
inline bool HasList(int type) { return type == C_LISTBOX || type == C_COMBOBOX || type == C_TABBAR; }      // the control has a list of items (rows / choices / tabs)
constexpr int MAX_LIST_ITEMS = 200, MAX_ITEM_LEN = 60;
constexpr int MAX_MENU_NODES = 200, MAX_MENU_DEPTH = 5, MAX_MENU_TEXT = 40, MAX_MENU_ID = 65535;

// A colour band on a gauge dial: from..to in the gauge's own units. Limits: at most 20 bands.
struct GaugeZone { double from = 0, to = 0; uint16_t color = 0x07E0; };
constexpr int MAX_GAUGE_ZONES = 20;

// ---- message boxes: modal popups the form can show (SuperMessageBox). Not on the design surface: a list in the form. Each button may have a name that
// becomes `static constexpr int <name> = <index>;` in the generated class, so code reads  if (msgDelete_Ask() == MSG_DELETE_YES). ----
struct MsgButton { std::string text, itemId; };
struct DMessageBox {
    std::string name;                                           // the generated member (and <name>_Ask / _Open / _Poll)
    int w = 220; std::string font = DEFAULT_FONT;
    std::string title;                                          // shown above the box (SetLabel); "" = none
    std::vector<std::string> lines;                             // the message, one line per entry (joined with \n)
    std::vector<MsgButton> buttons;                             // 1..3
    int defaultBtn = 0;                                         // the highlighted button
    bool centered = true; int x = 0, y = 0;                     // centred on the screen, or placed at x, y
    uint16_t fg = 0xFFFF, bg = 0x0000, border = 0xFFFF;         // text / back / border colour (the control defaults)
    bool rich = false;                                          // the message may use the rich text tags (SetRich)
    std::string styleFont[4];                                   // optional fonts of <b> <i> <b><i> <h1> ("" = none): bold, italic, bold italic, heading
};
constexpr int MAX_MSG_BOXES = 20, MAX_MSG_LINES = 12, MAX_MSG_LINE = 60, MAX_MSG_BTN_TEXT = 24, MIN_MSG_W = 60, MAX_MSG_W = 1000;

// ---- menu items: a tree. Every item that is a command (a leaf) has an itemId (a C++-safe name that becomes `static constexpr uint16_t <itemId> = <id>;`
// in the generated class) and a numeric id; a parent (an item with sub-items) may have them too. id < 0 / empty itemId = none. ----
struct MenuNode {
    std::string text, itemId;
    int id = -1;
    std::vector<MenuNode> kids;
};

// ---- per-type properties beyond the common ones ("extras"): a table drives the property panel, the JSON and the code generator ----
enum PKind { PK_INT, PK_TEXT, PK_BOOL, PK_ENUM, PK_COLOR, PK_FONT, PK_FLOAT };      // PK_FLOAT: a decimal number (lo / hi are the limits), stored as short text (12.5, 100)
struct ExtraDef {
    const char* key;          // JSON key (and the name in the generated code's docs)
    const char* label;        // property panel
    PKind       kind;
    const char* def;          // the default, as text: a number, "true" / "false", "0xFFFF" for a colour, "" for text / font. Equal to default = not saved
    int         lo, hi;       // INT: limits;  TEXT: unused
    const char* choices;      // ENUM / BOOL: "Above|Below" (BOOL: "No|Yes")
};
inline int ChoiceCount(const ExtraDef& d) { int n = 1; for (const char* p = d.choices; *p; ++p) if (*p == '|') ++n; return d.choices[0] ? n : 0; }
inline std::string ChoiceName(const ExtraDef& d, int i) {
    std::string s; int k = 0;
    for (const char* p = d.choices; ; ++p) { if (*p == '|' || !*p) { if (k == i) return s; s.clear(); ++k; if (!*p) break; } else s += *p; }
    return "";
}
inline std::string FloatText(double x) {                         // 12.5 -> "12.5", 100.0 -> "100", at most 4 decimals
    if (std::fabs(x - std::round(x)) < 1e-9) return std::to_string((long)std::lround(x));
    char b[40]; snprintf(b, sizeof b, "%.4f", x); std::string s = b; while (!s.empty() && s.back() == '0') s.pop_back(); if (!s.empty() && s.back() == '.') s.pop_back(); return s;
}
inline std::string Lower(std::string s) { for (char& c : s) c = (char)tolower((unsigned char)c); return s; }

// The label every SuperControl can carry (a caption above or below the box; with the Sleek border it sits on the frame line).
static const ExtraDef LABEL_EXTRAS[] = {
    { "label",      "Label",          PK_TEXT,  "",       0, 0, "" },
    { "labelPos",   "Label position", PK_ENUM,  "0",      0, 1, "Above|Below" },
    { "labelAlign", "Label align",    PK_ENUM,  "0",      0, 2, "Left|Center|Right" },
    { "labelFont",  "Label font",     PK_FONT,  "",       0, 0, "" },
    { "labelColor", "Label color",    PK_COLOR, "0xFFFF", 0, 0, "" },
};
static const int N_LABEL_EXTRAS = (int)(sizeof LABEL_EXTRAS / sizeof LABEL_EXTRAS[0]);
// Integers are the controls' own: a spin box or slider works on integers (a decimal point is only added when drawing, see SetDecimals).
static const ExtraDef SPIN_EXTRAS[] = {
    { "value",    "Value",       PK_INT,  "0",     -999999999, 999999999, "" },
    { "min",      "Minimum",     PK_INT,  "0",     -999999999, 999999999, "" },
    { "max",      "Maximum",     PK_INT,  "100",   -999999999, 999999999, "" },
    { "step",     "Step",        PK_INT,  "1",     1, 999999, "" },
    { "decimals", "Decimals",    PK_INT,  "0",     0, 6, "" },
    { "wrap",     "Wrap around", PK_BOOL, "false", 0, 1, "No|Yes" },
    { "prefix",   "Prefix",      PK_TEXT, "",      0, 0, "" },
    { "suffix",   "Suffix",      PK_TEXT, "",      0, 0, "" },
};
static const ExtraDef SLIDER_EXTRAS[] = {
    { "value",     "Value",      PK_INT,  "0",    -999999999, 999999999, "" },
    { "min",       "Minimum",    PK_INT,  "0",    -999999999, 999999999, "" },
    { "max",       "Maximum",    PK_INT,  "100",  -999999999, 999999999, "" },
    { "step",      "Step",       PK_INT,  "1",    1, 999999, "" },
    { "showValue", "Show value", PK_BOOL, "true", 0, 1, "No|Yes" },
};

// The initial state of a check box / radio button (set silently: no event). Only one radio button per group can be selected.
static const ExtraDef CHECK_EXTRAS[] = { { "checked", "Checked",  PK_BOOL, "false", 0, 1, "No|Yes" } };
static const ExtraDef RADIO_EXTRAS[] = { { "checked", "Selected", PK_BOOL, "false", 0, 1, "No|Yes" } };

// SuperInputBox: the template (# digit, A upper, a lower, B any letter, * any character; everything else is a literal) and the value without the literals.
static const ExtraDef INPUT_EXTRAS[] = {
    { "template",     "Template",      PK_TEXT,  "###-###-####", 0, 0, "" },
    { "value",        "Value",         PK_TEXT,  "",            0, 0, "" },
    { "trimRight",    "Trim right",    PK_BOOL,  "true",        0, 1, "No|Yes" },
    { "literalColor", "Literal color", PK_COLOR, "0xBDF7",      0, 0, "" },
};
static const ExtraDef TIME_EXTRAS[] = {
    { "seconds", "Seconds",     PK_BOOL, "true",     0, 1, "No|Yes" },
    { "value",   "Value",       PK_TEXT, "00:00:00", 0, 0, "" },
    { "wrap",    "Wrap around", PK_BOOL, "false",    0, 1, "No|Yes" },
};
static const ExtraDef DATE_EXTRAS[] = {
    { "style",     "Style",       PK_ENUM, "0",          0, 1, "Y-M-D|M-D-Y" },
    { "separator", "Separator",   PK_TEXT, "-",          0, 0, "" },
    { "yearMin",   "Year from",   PK_INT,  "1970",       0, 9999, "" },
    { "yearMax",   "Year to",     PK_INT,  "2099",       0, 9999, "" },
    { "value",     "Value",       PK_TEXT, "2000-01-01", 0, 0, "" },
    { "wrap",      "Wrap around", PK_BOOL, "false",      0, 1, "No|Yes" },
};

// Lists: the items themselves are DControl::list (edited in the items dialog); these are the other settings. "selected" = the initially selected index.
static const ExtraDef LISTBOX_EXTRAS[] = {
    { "selected", "Selected",   PK_INT,  "0",     0, 9999, "" },
    { "rows",     "Visible rows", PK_INT, "5",    1, 30, "" },
    { "wrap",     "Wrap around", PK_BOOL, "false", 0, 1, "No|Yes" },
    { "rowPad",   "Row padding", PK_INT,  "1",    0, 8, "" },
};
static const ExtraDef COMBOBOX_EXTRAS[] = {
    { "selected", "Selected",     PK_INT,  "0", 0, 9999, "" },
    { "maxRows",  "Popup rows",   PK_INT,  "5", 1, 30, "" },
    { "restore",  "Popup restore", PK_ENUM, "0", 0, 1, "Redraw form|Read back" },     // how the screen under the popup comes back (see SuperComboBox)
};
static const ExtraDef TABBAR_EXTRAS[] = {
    { "selected", "Selected",    PK_INT, "0", 0, 9999, "" },
    { "tabPad",   "Tab padding", PK_INT, "6", 0, 30, "" },
};

static const ExtraDef MENU_EXTRAS[] = {
    { "rows",     "Visible rows", PK_INT,  "5",    1, 30, "" },
    { "wrap",     "Wrap around",  PK_BOOL, "false", 0, 1, "No|Yes" },
    { "showBack", "Back row",     PK_BOOL, "true",  0, 1, "No|Yes" },        // sub-menus start with a "< Back" row (touch friendly)
    { "rowPad",   "Row padding",  PK_INT,  "1",    0, 8, "" },
    { "idPrefix", "ID prefix",    PK_TEXT, "MNU_", 0, 0, "" },                // new item ids are named <prefix><TEXT>, e.g. MNU_REBOOT
};

// Display controls (not editable: no encoder, no buttons). "bodyHeight" is the height of the picture inside the frame; the control is taller by its frame and label.
static const ExtraDef GAUGE_EXTRAS[] = {
    { "bodyHeight",  "Body height",         PK_INT,   "100",    8, 400, "" },
    { "min",         "Minimum",             PK_FLOAT, "0",      -999999, 999999, "" },
    { "max",         "Maximum",             PK_FLOAT, "100",    -999999, 999999, "" },
    { "value",       "Value",               PK_FLOAT, "0",      -999999, 999999, "" },
    { "sweepStart",  "Sweep start",   PK_INT,   "-90",    -360, 360, "" },           // degrees clockwise from 12 o'clock: -90 .. 90 is a half dial
    { "sweepEnd",    "Sweep end",     PK_INT,   "90",     -360, 360, "" },
    { "majorTicks",  "Major ticks",         PK_INT,   "4",      1, 20, "" },
    { "minorTicks",  "Minor ticks",         PK_INT,   "1",      0, 10, "" },
    { "tickLabels",  "Tick numbers",        PK_BOOL,  "true",   0, 1, "No|Yes" },
    { "decimals",    "Decimals",            PK_INT,   "0",      0, 4, "" },
    { "unit",        "Unit",                PK_TEXT,  "",       0, 0, "" },
    { "showValue",   "Show value",          PK_BOOL,  "true",   0, 1, "No|Yes" },
    { "needle",      "Needle",              PK_ENUM,  "0",      0, 1, "Taper|Line" },
    { "needleByZone","Zone needle",      PK_BOOL,  "true",   0, 1, "No|Yes" },
    { "smoothing",   "Smoothing",           PK_BOOL,  "true",   0, 1, "No|Yes" },
    { "smoothMs",    "Smooth (ms)",      PK_INT,   "250",    1, 5000, "" },
    { "trackColor",  "Track color",         PK_COLOR, "0x2104", 0, 0, "" },
    { "tickColor",   "Tick color",          PK_COLOR, "0x7BEF", 0, 0, "" },
    { "needleColor", "Needle color",        PK_COLOR, "0xFFFF", 0, 0, "" },
    { "minMaxMs",    "MinMax (ms)", PK_INT,   "0",      0, 600000, "" },           // 0 = off: lines for the lowest / highest value of the last window
    { "minMaxDeg",   "MinMax deg", PK_FLOAT, "3",     0, 90, "" },
    { "minMaxThick", "MinMax thick",   PK_INT,   "2",      1, 8, "" },
};
static const ExtraDef GRAPH_EXTRAS[] = {
    { "bodyHeight", "Body height",   PK_INT,   "80",     8, 400, "" },
    { "capacity",   "Samples",       PK_INT,   "0",      0, 2000, "" },                    // 0 = one per pixel of width (the library default)
    { "autoRange",  "Auto range",    PK_BOOL,  "true",   0, 1, "No|Yes" },
    { "rangeMin",   "Range from",    PK_FLOAT, "0",      -999999, 999999, "" },
    { "rangeMax",   "Range to",      PK_FLOAT, "100",    -999999, 999999, "" },
    { "showGrid",   "Grid",          PK_BOOL,  "true",   0, 1, "No|Yes" },
    { "showRange",  "Show range",    PK_BOOL,  "false",  0, 1, "No|Yes" },
    { "lineColor",  "Line color",    PK_COLOR, "0x07FF", 0, 0, "" },
    { "gridColor",  "Grid color",    PK_COLOR, "0x2104", 0, 0, "" },
    { "preview",    "Preview data",  PK_ENUM,  "0",      0, 3, "Sine|Rising|Noise|None" },   // the DESIGNER only: sample data so the graph shows something (not generated)
};
static const ExtraDef USERDRAW_EXTRAS[] = {
    { "bodyHeight", "Body height",       PK_INT,  "100",   1, 400, "" },
    { "persistent", "Persistent", PK_BOOL, "false", 0, 1, "No|Yes" },               // a canvas that keeps what you drew (costs width x height x 2 bytes)
    { "autoRedraw", "Redraw (ms)",  PK_INT,  "0",     0, 60000, "" },                 // 0 = only when you call Repaint()
    { "touch",      "Touch input",       PK_BOOL, "false", 0, 1, "No|Yes" },               // the control then takes focus and raises Pointer / Accept events
};

// Style fonts (optional, "" = none): the fonts <b> <i> <b><i> and <h1> use in rich text. Shared by Label, Rich text (and, as fields, message boxes).
#define FD_STYLE_FONT_EXTRAS \
    { "boldFont",       "Bold font <b>",         PK_FONT, "", 0, 0, "" }, \
    { "italicFont",     "Italic font <i>",       PK_FONT, "", 0, 0, "" }, \
    { "boldItalicFont", "Bold italic font",      PK_FONT, "", 0, 0, "" }, \
    { "headingFont",    "Heading font <h1>",     PK_FONT, "", 0, 0, "" }
constexpr const char* STYLE_FONT_KEYS[4] = { "boldFont", "italicFont", "boldItalicFont", "headingFont" };
constexpr const char* STYLE_FONT_SLOTS[4] = { "FONT_BOLD", "FONT_ITALIC", "FONT_BOLDITALIC", "FONT_HEADING" };
// SuperLabel: only the rich text option (off by default: a plain label shows every character as it is).
static const ExtraDef LABELCTL_EXTRAS[] = {
    { "rich", "Rich text", PK_BOOL, "false", 0, 1, "No|Yes" },                          // the text may use <color=..> <bg=..> <u> <s> <big> <center> <br> <tab> ... (see SuperRichCore.h)
    FD_STYLE_FONT_EXTRAS,
};
// SuperRichText: scrollable text with a few HTML-like tags. The text itself is DControl::list (one entry per line, edited in the rich text editor).
static const ExtraDef RICHTEXT_EXTRAS[] = {
    { "bodyHeight", "Body height",  PK_INT,  "120", 8, 600, "" },
    { "tabChars",   "Tab stops",    PK_INT,  "8",   1, 40,  "" },                      // a <tab> jumps to the next multiple of this many spaces
    { "lineGap",    "Line gap",     PK_INT,  "0",   0, 20,  "" },
    { "showSource", "Show source",  PK_BOOL, "false", 0, 1, "No|Yes" },                // draw the markup itself (debugging); generated as SetShowSource(true)
    FD_STYLE_FONT_EXTRAS,
};
constexpr int MAX_TAB_ORDER = 9999;
constexpr int MAX_RICH_LINES = 400, MAX_RICH_CHARS = 4000;

// SuperColorPicker: the three HSV bars with a swatch; "value" is the initial colour (RGB565).
static const ExtraDef COLORPICKER_EXTRAS[] = {
    { "value", "Color", PK_COLOR, "0xF800", 0, 0, "" },
};

struct TypeInfo {
    const char* key;       // the SuperGUI class name (also the "type" in the JSON)
    const char* title;     // shown in the toolbox
    const char* prefix;    // default names: label1, label2 ...
    int         defW;      // default width
    const char* defText;   // default caption / text
    bool        hasText;
    bool        hasAlign;           // the control has a text alignment (SetAlign)
    bool        hasLabel;           // the control can carry a label (not the ones that have a caption of their own: Label, Push button, Check box, Radio button)
    bool        editable;           // takes the encoder and buttons in its constructor
    const ExtraDef* extras;         // type-specific properties
    int         nExtras;
    bool        ctorText;           // the text is a constructor argument (else it is set with SetText after construction)
    bool        fitWidth;           // a SuperTextBox: the width can be fitted to the content (GetContentWidth)
};
#define FD_N(arr) ((int)(sizeof arr / sizeof arr[0]))
inline const TypeInfo& Info(int t) {
    static const TypeInfo T[C_COUNT] = {
        //  key                 title          prefix      w   text      hasText align  label  edit   extras          n                ctorText fit
        { "SuperLabel",       "Label",       "label",    100, "Label",  true,  true,  false, false, LABELCTL_EXTRAS, FD_N(LABELCTL_EXTRAS), true,  false },
        { "SuperPushButton",  "Push button", "button",   100, "Button", true,  true,  false, false, nullptr,       0,               true,  false },   // (no label group: its caption is the label)
        { "SuperCheckBox",    "Check box",   "check",    120, "Check",  true,  false, false, false, CHECK_EXTRAS,  FD_N(CHECK_EXTRAS),  true,  false },   // (no label group: it has its own caption)
        { "SuperRadioButton", "Radio button","radio",    120, "Option", true,  false, false, false, RADIO_EXTRAS,  FD_N(RADIO_EXTRAS),  true,  false },
        { "SuperProgressBar", "Progress bar","progress", 150, "",       false, false, true,  false, nullptr,       0,               false, false },
        { "SuperSpinBox",     "Spin box",    "spin",     100, "",       false, true,  true,  true,  SPIN_EXTRAS,   FD_N(SPIN_EXTRAS),   false, true  },
        { "SuperSlider",      "Slider",      "slider",   160, "",       false, false, true,  true,  SLIDER_EXTRAS, FD_N(SLIDER_EXTRAS), false, false },
        { "SuperTextBox",     "Text box",    "text",     120, "Text",   true,  true,  true,  false, nullptr,       0,               false, true  },
        { "SuperInputBox",    "Input box",   "input",    120, "",       false, true,  true,  true,  INPUT_EXTRAS,  FD_N(INPUT_EXTRAS),  false, true  },
        { "SuperTimeBox",     "Time box",    "time",      90, "",       false, true,  true,  true,  TIME_EXTRAS,   FD_N(TIME_EXTRAS),   false, true  },
        { "SuperDateBox",     "Date box",    "date",     110, "",       false, true,  true,  true,  DATE_EXTRAS,   FD_N(DATE_EXTRAS),   false, true  },
        { "SuperListBox",     "List box",    "list",     140, "",       false, true,  true,  true,  LISTBOX_EXTRAS, FD_N(LISTBOX_EXTRAS), false, false },
        { "SuperComboBox",    "Combo box",   "combo",    140, "",       false, true,  true,  true,  COMBOBOX_EXTRAS, FD_N(COMBOBOX_EXTRAS), false, false },
        { "SuperTabBar",      "Tab bar",     "tabs",     220, "",       false, false, true,  true,  TABBAR_EXTRAS, FD_N(TABBAR_EXTRAS), false, false },
        { "SuperMenu",        "Menu",        "menu",     160, "",       false, false, true,  true,  MENU_EXTRAS,   FD_N(MENU_EXTRAS),   false, false },
        { "SuperGauge",       "Gauge",       "gauge",    180, "",       false, false, true,  false, GAUGE_EXTRAS,  FD_N(GAUGE_EXTRAS),  false, false },
        { "SuperGraph",       "Graph",       "graph",    200, "",       false, false, true,  false, GRAPH_EXTRAS,  FD_N(GRAPH_EXTRAS),  false, false },
        { "SuperUserDraw",    "User draw",   "draw",     160, "",       false, false, true,  false, USERDRAW_EXTRAS, FD_N(USERDRAW_EXTRAS), false, false },
        { "SuperColorPicker", "Color picker","picker",   200, "",       false, false, true,  true,  COLORPICKER_EXTRAS, FD_N(COLORPICKER_EXTRAS), false, false },
        { "SuperRichText",    "Rich text",   "rich",     240, "",       false, true,  true,  true,  RICHTEXT_EXTRAS, FD_N(RICHTEXT_EXTRAS), false, false },
    };
    return T[t < 0 || t >= C_COUNT ? 0 : t];
}
// All extras of a type: its own, then the label group (when the type can have a label). Index = position in this list.
inline int ExtraCount(int type) { const TypeInfo& i = Info(type); return i.nExtras + (i.hasLabel ? N_LABEL_EXTRAS : 0); }
inline const ExtraDef& ExtraAt(int type, int idx) { const TypeInfo& i = Info(type); return idx < i.nExtras ? i.extras[idx] : LABEL_EXTRAS[idx - i.nExtras]; }
inline const ExtraDef* FindExtra(int type, const char* key) {
    for (int i = 0; i < ExtraCount(type); ++i) if (std::string(ExtraAt(type, i).key) == key) return &ExtraAt(type, i);
    return nullptr;
}

// default colours (RGB565) = the SuperGUI controls' own defaults (SuperTheme::Dark)
inline const char* StyleName(int i) { static const char* n[] = { "standard", "threed", "sleek" }; return n[i < 0 || i > 2 ? 0 : i]; }
inline const char* AlignName(int i) { static const char* n[] = { "left", "center", "right" }; return n[i < 0 || i > 2 ? 0 : i]; }
inline int NameIndex(const std::string& s, const char* (*f)(int)) { for (int i = 0; i < 3; ++i) if (s == f(i)) return i; return -1; }
constexpr int MAX_BORDER = 8, MAX_PADDING = 20;

constexpr uint16_t DEF_FG = 0xFFFF, DEF_BG = 0x0000, DEF_BORDER = 0xFFFF, DEF_FORM_BG = 0x0000;

struct DControl {
    int type = C_LABEL;
    std::string name, text;
    int x = 0, y = 0, w = 100;
    uint16_t fg = DEF_FG, bg = DEF_BG, border = DEF_BORDER;     // text / back / border colour
    std::string font = DEFAULT_FONT;                           // an lgfx::fonts name (see FontTable.h)
    std::string group = "group1";                              // radio buttons only: buttons with the same group name share one SuperRadioGroup
    // Layout overrides: -1 = the control's own default (not saved, not generated). borderStyle 0 standard / 1 3D / 2 sleek, align 0 left / 1 centre / 2 right.
    int borderStyle = -1, borderSize = -1, padding = -1, align = -1;
    int defStyle = 0, defSize = 1, defPad = 2, defAlign = 0;    // derived: the control's own defaults (not saved)
    int h = 16;                      // derived from the real control (not saved)
    std::vector<MenuNode> menu;                                 // the items of a menu (a tree)
    std::vector<GaugeZone> zones;                               // gauge: the colour bands on the dial
    std::string ctlGroup;                                       // control group: a name (generated as a uint16_t constant); "" = none (group 0, the default)
    bool visible = true, enabled = true;                        // the initial state (SetVisible / SetEnabled)
    int tabOrder = 0;                                           // focus order of the encoder (SetTabOrder): 0 = not set yet (the position in the list); once set it never changes by itself
    std::vector<std::string> pages;                             // tab bar: the control group each tab shows ("" = none), parallel to `list` (may be shorter)
    std::vector<std::string> list;                              // the items of a list box / combo box / tab bar (printable ASCII, non-empty)
    std::map<std::string, std::string> extra;                   // type-specific properties (ExtraDef): only values that differ from the default are stored
};

// ---- extras: read with the default, write only when different ----
inline std::string ExtraGet(const DControl& c, const ExtraDef& d) { auto it = c.extra.find(d.key); return it == c.extra.end() ? std::string(d.def) : it->second; }
inline bool ExtraSet(DControl& c, const ExtraDef& d, const std::string& v) {      // false when the value is not valid for the kind (nothing changes)
    std::string n = v;
    switch (d.kind) {
    case PK_INT:   { if (n.empty() || n == "-") return false; long x = strtol(n.c_str(), nullptr, 10); x = x < d.lo ? d.lo : x > d.hi ? d.hi : x; n = std::to_string(x); break; }
    case PK_FLOAT: { char* end = nullptr; double x = strtod(n.c_str(), &end); if (n.empty() || end == n.c_str() || *end) return false; x = x < d.lo ? d.lo : x > d.hi ? d.hi : x; n = FloatText(x); break; }
    case PK_BOOL:  if (n != "true" && n != "false") return false; break;
    case PK_ENUM:  { int k = atoi(n.c_str()); if (k < 0 || k >= ChoiceCount(d)) return false; n = std::to_string(k); break; }
    case PK_COLOR: { long x = strtol(n.c_str(), nullptr, 0); char b[8]; snprintf(b, sizeof b, "0x%04lX", (unsigned long)(x < 0 ? 0 : x > 0xFFFF ? 0xFFFF : x)); n = b; break; }
    default: break;                                                                  // text, font id: any
    }
    if (n == d.def) c.extra.erase(d.key); else c.extra[d.key] = n;
    return true;
}
inline long XInt(const DControl& c, const char* key);
inline void ClampSelected(DControl& c);
inline long XInt(const DControl& c, const char* key) { const ExtraDef* d = FindExtra(c.type, key); return d ? strtol(ExtraGet(c, *d).c_str(), nullptr, 0) : 0; }
inline double XFloat(const DControl& c, const char* key);
inline bool XBool(const DControl& c, const char* key) { const ExtraDef* d = FindExtra(c.type, key); return d && ExtraGet(c, *d) == "true"; }
inline double XFloat(const DControl& c, const char* key) { const ExtraDef* d = FindExtra(c.type, key); return d ? strtod(ExtraGet(c, *d).c_str(), nullptr) : 0.0; }
inline std::string XStr(const DControl& c, const char* key) { const ExtraDef* d = FindExtra(c.type, key); return d ? ExtraGet(c, *d) : ""; }
inline bool XSet(const DControl& c, const char* key) { return c.extra.count(key) > 0; }     // explicitly set (differs from the default)
// The initial selection of a list must be one of its items (an empty list has none): pulled back to the last item.
inline void ClampSelected(DControl& c) {
    const ExtraDef* d = FindExtra(c.type, "selected"); if (!d) return;
    long n = (long)c.list.size(), v = strtol(ExtraGet(c, *d).c_str(), nullptr, 10);
    if (n == 0) c.extra.erase("selected"); else if (v >= n) ExtraSet(c, *d, std::to_string(n - 1));
}

inline std::string Ascii(const std::string& s) { std::string r; for (char c : s) if (c >= 0x20 && c < 0x7F) r += c; return r; }     // printable ASCII only (the designer's character set)

// a valid name for a control, a radio group or the form: a plain identifier that is safe in the generated C++ (see ReservedNames.h)
inline bool ValidName(const std::string& s) {
    if (s.empty() || isdigit((unsigned char)s[0]) || IsReserved(s)) return false;
    for (unsigned char c : s) if (!isalnum(c) && c != '_') return false;
    return true;
}

// ---- menu helpers ----
inline std::string Trim(const std::string& s) { size_t a = s.find_first_not_of(' '), b = s.find_last_not_of(' '); return a == std::string::npos ? "" : s.substr(a, b - a + 1); }
inline int MenuNodeCount(const std::vector<MenuNode>& v) { int n = 0; for (const MenuNode& k : v) n += 1 + MenuNodeCount(k.kids); return n; }
inline int MenuMaxId(const std::vector<MenuNode>& v) { int m = -1; for (const MenuNode& k : v) m = std::max(m, std::max(k.id, MenuMaxId(k.kids))); return m; }
template <class F> void MenuForEach(std::vector<MenuNode>& v, F&& f) { for (MenuNode& k : v) { f(k); MenuForEach(k.kids, f); } }       // pre-order
template <class F> void MenuForEach(const std::vector<MenuNode>& v, F&& f) { for (const MenuNode& k : v) { f(k); MenuForEach(k.kids, f); } }
inline int MenuDepth(const std::vector<MenuNode>& v) { int d = 0; for (const MenuNode& k : v) d = std::max(d, 1 + MenuDepth(k.kids)); return d; }
// "Power off" -> "POWER_OFF"
inline std::string MenuUpper(const std::string& t) {
    std::string r; bool us = false;
    for (unsigned char c : t) { if (isalnum(c)) { if (us && !r.empty()) r += '_'; r += (char)toupper(c); us = false; } else us = true; }
    return r;
}
// A free, valid name for a new item: <prefix><TEXT>, then <prefix><TEXT>_2, _3 ...  (taken(candidate) says whether a name is used)
inline std::string AutoItemId(const std::string& prefix, const std::string& text, const std::function<bool(const std::string&)>& taken) {
    std::string base = prefix + MenuUpper(text); if (MenuUpper(text).empty()) base = prefix + "ITEM";
    std::string cand = base;
    for (int k = 2; !ValidName(cand) || taken(cand); ++k) cand = base + "_" + std::to_string(k);
    return cand;
}

// ---- a tiny JSON reader (objects, arrays, strings, integers; enough for the designer's own files) ----
struct JV {
    enum T { NUL, NUM, STR, ARR, OBJ } t = NUL;
    long n = 0; double d = 0; std::string s;                       // n: the integer part, d: the whole number
    std::vector<JV> a; std::vector<std::pair<std::string, JV>> o;
    const JV* Get(const char* k) const { for (auto& p : o) if (p.first == k) return &p.second; return nullptr; }
};
class JsonParser {
public:
    explicit JsonParser(const std::string& src) : S(src) {}
    bool Parse(JV& out, std::string& err) {
        Err.clear();
        bool ok = Value(out); Ws();
        if (ok && P != S.size()) { Fail("unexpected text after the end"); ok = false; }
        err = Err; return ok;
    }
private:
    const std::string& S; size_t P = 0; std::string Err;
    void Ws() { while (P < S.size() && isspace((unsigned char)S[P])) ++P; }
    bool Fail(const char* m) { if (Err.empty()) Err = std::string(m) + " at offset " + std::to_string(P); return false; }
    bool Str(std::string& out) {
        if (P >= S.size() || S[P] != '"') return Fail("string expected");
        ++P; out.clear();
        while (P < S.size() && S[P] != '"') {
            char c = S[P++];
            if (c == '\\') {
                if (P >= S.size()) return Fail("bad escape");
                char e = S[P++];
                if (e == 'n') c = '\n'; else if (e == 't') c = '\t'; else if (e == '"' || e == '\\' || e == '/') c = e;
                else if (e == 'u') {                                  // \uXXXX: the designer is ASCII only, so anything above 0x7F is dropped
                    if (P + 4 > S.size()) return Fail("bad \\u escape");
                    long code = strtol(S.substr(P, 4).c_str(), nullptr, 16); P += 4;
                    if (code >= 0x20 && code < 0x7F) out += (char)code;
                    continue;
                } else return Fail("unsupported escape");
            }
            out += c;
        }
        if (P >= S.size()) return Fail("unterminated string");
        ++P; return true;
    }
    bool Value(JV& v) {
        Ws();
        if (P >= S.size()) return Fail("unexpected end");
        char c = S[P];
        if (c == '{') {
            v.t = JV::OBJ; ++P; Ws();
            if (P < S.size() && S[P] == '}') { ++P; return true; }
            for (;;) {
                Ws(); std::string k; if (!Str(k)) return false;
                Ws(); if (P >= S.size() || S[P] != ':') return Fail("':' expected"); ++P;
                JV x; if (!Value(x)) return false;
                v.o.emplace_back(k, std::move(x)); Ws();
                if (P < S.size() && S[P] == ',') { ++P; continue; }
                if (P < S.size() && S[P] == '}') { ++P; return true; }
                return Fail("',' or '}' expected");
            }
        }
        if (c == '[') {
            v.t = JV::ARR; ++P; Ws();
            if (P < S.size() && S[P] == ']') { ++P; return true; }
            for (;;) {
                JV x; if (!Value(x)) return false;
                v.a.push_back(std::move(x)); Ws();
                if (P < S.size() && S[P] == ',') { ++P; continue; }
                if (P < S.size() && S[P] == ']') { ++P; return true; }
                return Fail("',' or ']' expected");
            }
        }
        if (c == '"') { v.t = JV::STR; return Str(v.s); }
        if (S.compare(P, 4, "true") == 0)  { v.t = JV::NUM; v.n = 1; P += 4; return true; }
        if (S.compare(P, 4, "null") == 0)  { v.t = JV::NUL; P += 4; return true; }
        if (S.compare(P, 5, "false") == 0) { v.t = JV::NUM; v.n = 0; P += 5; return true; }
        if (c == '-' || isdigit((unsigned char)c)) {
            char* end = nullptr; v.d = strtod(S.c_str() + P, &end); v.n = (long)v.d; v.t = JV::NUM;
            if (end == S.c_str() + P) return Fail("number expected");
            P = (size_t)(end - S.c_str()); return true;
        }
        return Fail("value expected");
    }
};

struct DForm {
    std::string name = "Form1";
    int w = DEFAULT_FORM_W, h = DEFAULT_FORM_H;
    uint16_t bg = DEF_FORM_BG;       // the form's back colour
    bool keepHidden = false;         // "Keep hidden controls hidden": a control with Visible = No stays hidden when its group is shown (SetKeepHidden); default off
    std::vector<DMessageBox> boxes;                            // message boxes (not on the surface)
    mutable std::vector<std::pair<std::string, int>> groupIds;   // control group name -> number (1, 2, 3 by first use), kept stable in the JSON
    std::string outDir;              // where the generated code goes ("" until the designer fills in a folder that exists)
    std::vector<DControl> items;

    void Clear() { name = "Form1"; w = DEFAULT_FORM_W; h = DEFAULT_FORM_H; bg = DEF_FORM_BG; keepHidden = false; items.clear(); boxes.clear(); }   // outDir is kept: it belongs to the project folder
    // A name is taken when a control (other than `except`) or a radio group already has it: both become C++ members of the generated class.
    // The form's own class names count too: the generated classes are <name>Base and <name>, and no member may be called like its class.
    // Menu item ids (itemId) are constants of the generated class too; `exceptMenu` = a menu whose own items are not counted (the one being edited).
    // Control groups (a name per group of controls, a uint16_t constant in the generated class) count as names too; ctlGroups = false leaves them out.
    std::vector<std::string> UsedGroups() const {                  // control group names in order of first use (controls, then tab pages)
        std::vector<std::string> v;
        auto add = [&](const std::string& g) { if (!g.empty() && std::find(v.begin(), v.end(), g) == v.end()) v.push_back(g); };
        for (const DControl& c : items) add(c.ctlGroup);
        for (const DControl& c : items) if (c.type == C_TABBAR) for (const std::string& p : c.pages) add(p);
        return v;
    }
    bool IsGroupName(const std::string& n) const { auto u = UsedGroups(); return std::find(u.begin(), u.end(), n) != u.end(); }
    // The number of every used group (stable: a name keeps the number the file gave it; new names get the next one above the highest). Unused names are dropped.
    void AssignGroupIds() const {
        std::vector<std::string> used = UsedGroups(); std::vector<std::pair<std::string, int>> out; std::set<int> ids;
        for (auto& kv : groupIds) if (std::find(used.begin(), used.end(), kv.first) != used.end() && kv.second >= 1 && kv.second <= MAX_MENU_ID && ids.insert(kv.second).second) out.push_back(kv);
        int mx = ids.empty() ? 0 : *ids.rbegin();
        for (const std::string& g : used) { bool have = false; for (auto& kv : out) if (kv.first == g) have = true; if (!have) out.push_back({ g, ++mx }); }
        groupIds = out;
    }
    int GroupId(const std::string& g) const { AssignGroupIds(); for (auto& kv : groupIds) if (kv.first == g) return kv.second; return 0; }
    // Would g be a usable control group name? (a safe C++ name that no control, radio group, menu item or form class already has; other CONTROLS may share it: that is the point)
    bool GroupNameTaken(const std::string& g) const { return NameTaken(g, -1, -1, false); }
    // The ready-made confirmation box: "Confirm?" with Yes / No (named buttons MSG_<BOX>_YES / _NO), No highlighted. Its name (msgYesNo, msgYesNo2 ...) and button names are free ones.
    DMessageBox MakeYesNoBox() const {
        DMessageBox b; b.name = "msgYesNo"; for (int k = 2; NameTaken(b.name); ++k) b.name = "msgYesNo" + std::to_string(k);
        b.lines = { "Confirm?" }; b.buttons = { { "Yes", "" }, { "No", "" } }; b.defaultBtn = 1; b.w = 180;
        for (MsgButton& m : b.buttons) { std::string id = AutoButtonName(b.name, m.text, [&](const std::string& x) { return NameTaken(x) || x == b.name || !ValidName(x); }); if (ValidName(id) && !NameTaken(id) && id != b.name) m.itemId = id; }
        return b;
    }
    std::string UniqueBoxName() const { for (int k = 1;; ++k) { std::string s = "msg" + std::to_string(k); if (!NameTaken(s)) return s; } }
    // Message boxes (their member names and the names of their buttons' result constants) count too; exceptBox = a box whose own names are not counted (the one being edited).
    bool NameTaken(const std::string& n, int except = -1, int exceptMenu = -1, bool ctlGroups = true, int exceptBox = -1) const {
        if (n == name || n == name + "Base") return true;
        for (int b = 0; b < (int)boxes.size(); ++b) if (b != exceptBox) { if (boxes[b].name == n) return true; for (const MsgButton& mb : boxes[b].buttons) if (mb.itemId == n) return true; }
        if (ctlGroups && IsGroupName(n)) return true;
        for (int i = 0; i < (int)items.size(); ++i) {
            if (i != except && items[i].name == n) return true;
            if (items[i].type == C_RADIO && items[i].group == n) return true;
            if (items[i].type == C_MENU && i != exceptMenu) { bool hit = false; MenuForEach(items[i].menu, [&](const MenuNode& k) { if (k.itemId == n) hit = true; }); if (hit) return true; }
        }
        return false;
    }
    // Bring a menu into a consistent state (also used after loading a hand-edited file): empty items dropped, limits kept; itemIds valid and unique
    // (invalid or repeated ones are renamed); ids unique, 0..65535; every command (a leaf, or any item that has one of them) has both an itemId and an id;
    // missing ids are the next number above the highest id. The first appearance of a duplicate keeps its value.
    void NormalizeMenu(int mi) {
        if (mi < 0 || mi >= (int)items.size() || items[mi].type != C_MENU) return;
        DControl& c = items[mi]; std::string prefix = XStr(c, "idPrefix"); int count = 0;
        std::function<void(std::vector<MenuNode>&, int)> clean = [&](std::vector<MenuNode>& v, int depth) {
            std::vector<MenuNode> out;
            for (MenuNode& k : v) {
                k.text = Trim(Ascii(k.text)); if (k.text.size() > (size_t)MAX_MENU_TEXT) k.text.resize(MAX_MENU_TEXT);
                if (k.text.empty() || count >= MAX_MENU_NODES) continue;
                ++count; if (depth + 1 >= MAX_MENU_DEPTH) k.kids.clear(); else clean(k.kids, depth + 1);
                out.push_back(std::move(k));
            }
            v = std::move(out);
        };
        clean(c.menu, 0);
        std::set<int> usedIds; std::set<std::string> usedNames;
        // An explicit name is valid when no control, group, form class or EARLIER menu has it (the first menu in the file keeps a name two menus share).
        auto takenBefore = [&](const std::string& n2) {
            if (n2 == name || n2 == name + "Base") return true;
            for (int i = 0; i < (int)items.size(); ++i) {
                if (i != mi && items[i].name == n2) return true;
                if (items[i].name == n2 && i == mi) return true;
                if (items[i].type == C_RADIO && items[i].group == n2) return true;
                if (items[i].type == C_MENU && i < mi) { bool hit = false; MenuForEach(items[i].menu, [&](const MenuNode& k) { if (k.itemId == n2) hit = true; }); if (hit) return true; }
            }
            return false;
        };
        MenuForEach(c.menu, [&](MenuNode& k) {                                          // A: keep the first valid explicit values
            if (!k.itemId.empty() && !(ValidName(k.itemId) && !takenBefore(k.itemId) && usedNames.insert(k.itemId).second)) k.itemId.clear();
            if (k.id >= 0 && !(k.id <= MAX_MENU_ID && usedIds.insert(k.id).second)) k.id = -1;
            if (k.id > MAX_MENU_ID) k.id = -1;
        });
        int maxId = usedIds.empty() ? 0 : *usedIds.rbegin();
        MenuForEach(c.menu, [&](MenuNode& k) {                                          // B: fill in what a command needs
            if (!(k.kids.empty() || !k.itemId.empty() || k.id >= 0)) return;
            if (k.id < 0) k.id = ++maxId;
            if (k.itemId.empty()) { k.itemId = AutoItemId(prefix, k.text, [&](const std::string& s) { return NameTaken(s, -1, mi, false) || usedNames.count(s) > 0; }); usedNames.insert(k.itemId); }
        });
    }
    void NormalizeMenus() { for (int i = 0; i < (int)items.size(); ++i) NormalizeMenu(i); }
    // The tag used in automatic button names: msgDelete -> DELETE (so MSG_DELETE_YES), a box not called msg... keeps its whole name upper-cased.
    static std::string BoxTag(const std::string& boxName) {
        std::string u = MenuUpper(boxName);
        if (u.rfind("MSG_", 0) == 0 && u.size() > 4) u = u.substr(4); else if (u.rfind("MSG", 0) == 0 && u.size() > 3) u = u.substr(3);
        return u.empty() ? "BOX" : u;
    }
    static std::string AutoButtonName(const std::string& boxName, const std::string& text, const std::function<bool(const std::string&)>& taken) {
        return AutoItemId("MSG_" + BoxTag(boxName) + "_", text, taken);
    }
    // Bring the message boxes into a consistent state (also after loading a hand-edited file): names safe and unique (renamed otherwise), 1..3 buttons with text,
    // button names (optional) safe and unique (cleared otherwise), limits kept, the default button a real one.
    void NormalizeBoxes() {
        std::vector<DMessageBox> src = boxes; boxes.clear();                              // rebuilt in order: each box sees the names of the earlier ones
        for (DMessageBox b : src) {
            if ((int)boxes.size() >= MAX_MSG_BOXES) break;
            if (!ValidName(b.name) || NameTaken(b.name)) b.name = UniqueBoxName();
            b.w = std::max(MIN_MSG_W, std::min(MAX_MSG_W, b.w)); if (FontIndex(b.font) < 0) b.font = DEFAULT_FONT;
            b.title = Ascii(b.title); if (b.title.size() > (size_t)MAX_MSG_LINE) b.title.resize(MAX_MSG_LINE);
            std::vector<std::string> ln;
            for (std::string l : b.lines) { l = Ascii(l); if (l.size() > (size_t)MAX_MSG_LINE) l.resize(MAX_MSG_LINE); if ((int)ln.size() < MAX_MSG_LINES) ln.push_back(l); }
            while (!ln.empty() && Trim(ln.back()).empty()) ln.pop_back();
            b.lines = ln;
            std::vector<MsgButton> bt;
            for (MsgButton mb : b.buttons) {
                mb.text = Trim(Ascii(mb.text)); if (mb.text.size() > (size_t)MAX_MSG_BTN_TEXT) mb.text.resize(MAX_MSG_BTN_TEXT);
                if (!mb.text.empty() && bt.size() < 3) bt.push_back(mb);
            }
            if (bt.empty()) bt.push_back({ "OK", "" });
            b.buttons = bt;
            for (size_t i = 0; i < b.buttons.size(); ++i) {                                // optional button names: safe and unused, else cleared
                std::string& id = b.buttons[i].itemId; if (id.empty()) continue;
                bool dup = false; for (size_t j = 0; j < i; ++j) if (b.buttons[j].itemId == id) dup = true;
                if (!ValidName(id) || dup || id == b.name || NameTaken(id)) id.clear();
            }
            b.defaultBtn = std::max(0, std::min(b.defaultBtn, (int)b.buttons.size() - 1));
            b.x = std::max(0, std::min(b.x, 4000)); b.y = std::max(0, std::min(b.y, 4000));
            boxes.push_back(b);
        }
    }
    // The first thing wrong with the message boxes (for the export check), or "".
    std::string BoxProblem() const {
        for (int bi = 0; bi < (int)boxes.size(); ++bi) {
            const DMessageBox& b = boxes[bi];
            if (!ValidName(b.name)) return "the message box \"" + b.name + "\" is not a safe C++ name";
            if (NameTaken(b.name, -1, -1, true, bi)) return "the message box " + b.name + " has the same name as a control, a group, a menu item, another box or the form's classes";
            if (b.buttons.empty() || b.buttons.size() > 3) return b.name + ": a message box needs 1 to 3 buttons";
            std::set<std::string> seen;
            for (const MsgButton& mb : b.buttons) {
                if (mb.text.empty()) return b.name + ": a button has no text";
                if (!mb.itemId.empty() && (!ValidName(mb.itemId) || NameTaken(mb.itemId, -1, -1, true, bi) || !seen.insert(mb.itemId).second || mb.itemId == b.name)) return b.name + ": the button name " + mb.itemId + " is not a safe, unused C++ name";
            }
        }
        return "";
    }
    // The first thing wrong with the control groups (for the export check), or "".
    std::string GroupProblem() const {
        for (const std::string& g : UsedGroups()) {
            if (!ValidName(g)) return "the control group \"" + g + "\" is not a safe C++ name";
            if (GroupNameTaken(g)) return "the control group " + g + " has the same name as a control, a radio group, a menu item or the form's classes";
            int id = GroupId(g); if (id < 1 || id > MAX_MENU_ID) return "the control group " + g + " has no valid number";
        }
        return "";
    }
    // The first thing wrong with the menus (for the export check), or "".
    std::string MenuProblem() const {
        for (int mi = 0; mi < (int)items.size(); ++mi) {
            const DControl& c = items[mi]; if (c.type != C_MENU) continue;
            std::set<int> ids; std::set<std::string> names; std::string bad;
            if (!ValidName(XStr(c, "idPrefix") + "X")) return c.name + ": the ID prefix \"" + XStr(c, "idPrefix") + "\" does not make a valid name";
            MenuForEach(c.menu, [&](const MenuNode& k) {
                if (!bad.empty()) return;
                if (k.text.empty()) bad = c.name + ": a menu item has no text";
                else if (k.kids.empty() && (k.itemId.empty() || k.id < 0)) bad = c.name + ": \"" + k.text + "\" needs an item ID and an id";
                else if (!k.itemId.empty() && (!ValidName(k.itemId) || NameTaken(k.itemId, -1, mi) || !names.insert(k.itemId).second)) bad = c.name + ": the item ID " + k.itemId + " is not a safe, unused C++ name";
                else if (k.id >= 0 && (k.id > MAX_MENU_ID || !ids.insert(k.id).second)) bad = c.name + ": the id " + std::to_string(k.id) + " of \"" + k.text + "\" is used twice (or too big)";
            });
            if (!bad.empty()) return bad;
        }
        return "";
    }
    // Would renaming the form to f collide with a control or a radio group?
    bool FormNameClashes(const std::string& f, bool withGroups = true) const {
        for (const DControl& c : items) {
            if (c.name == f || c.name == f + "Base") return true;
            if (withGroups && c.type == C_RADIO && (c.group == f || c.group == f + "Base")) return true;
            if (withGroups && (c.ctlGroup == f || c.ctlGroup == f + "Base")) return true;
            if (withGroups && c.type == C_MENU) { bool hit = false; MenuForEach(c.menu, [&](const MenuNode& k) { if (k.itemId == f || k.itemId == f + "Base") hit = true; }); if (hit) return true; }
        }
        if (withGroups) for (const DMessageBox& b : boxes) { if (b.name == f || b.name == f + "Base") return true; for (const MsgButton& mb : b.buttons) if (mb.itemId == f || mb.itemId == f + "Base") return true; }
        return false;
    }
    bool ControlNamed(const std::string& n) const { for (const DControl& c : items) if (c.name == n) return true; return false; }
    // ---- tab (focus) order: a number per control, independent of the list order (= draw order). New controls get the next free number; moving a control in the list
    // does not change it; the user edits it. 0 = not assigned (a file without it): the position in the list counts. Equal numbers keep the list order (as the library does). ----
    int EffTab(size_t i) const { return items[i].tabOrder > 0 ? items[i].tabOrder : (int)i + 1; }
    int NextTabOrder() const { int m = 0; for (size_t i = 0; i < items.size(); ++i) m = std::max(m, EffTab(i)); return m + 1; }
    std::vector<int> TabSequence() const {                        // control indices in focus order (stable: equal numbers keep the list order)
        std::vector<int> v; for (size_t i = 0; i < items.size(); ++i) v.push_back((int)i);
        std::stable_sort(v.begin(), v.end(), [&](int a, int b) { return EffTab((size_t)a) < EffTab((size_t)b); }); return v;
    }
    void FreezeTabOrder() { for (size_t i = 0; i < items.size(); ++i) if (items[i].tabOrder <= 0) items[i].tabOrder = EffTab(i); }      // make every control's number explicit (a file without numbers: the list positions)
    bool TabOrderCustom() const { for (size_t i = 0; i < items.size(); ++i) if (EffTab(i) != (int)i + 1) return true; return false; }      // some control's number is not its list position
    std::string UniqueGroupName() const {
        for (int n = 1;; ++n) { std::string s = "group" + std::to_string(n); if (!NameTaken(s)) return s; }
    }
    std::string UniqueName(const char* prefix) const {
        for (int n = 1;; ++n) { std::string s = prefix + std::to_string(n); if (!NameTaken(s)) return s; }
    }

    static std::string Layout(const DControl& c) {              // only the overrides, e.g. , "borderStyle": "sleek", "padding": 4
        std::string s;
        if (HasList(c.type) && !c.list.empty()) {                         // "items": ["Red", "Green"]; a tab with a page group is { "text": "Home", "group": "GRP_HOME" }
            s += ", \"items\": [";
            for (size_t i = 0; i < c.list.size(); ++i) {
                std::string v; for (char ch : c.list[i]) { if (ch == '"' || ch == '\\') v += '\\'; v += ch; }
                std::string pg = c.type == C_TABBAR && i < c.pages.size() ? c.pages[i] : "";
                s += (i ? ", " : "") + (pg.empty() ? "\"" + v + "\"" : "{ \"text\": \"" + v + "\", \"group\": \"" + pg + "\" }");
            }
            s += "]";
        }
        if (c.type == C_RICHTEXT && !c.list.empty()) {                     // "richText": ["line one", "line two"]: readable, one entry per line
            s += ", \"richText\": [";
            for (size_t i = 0; i < c.list.size(); ++i) { std::string v; for (char ch : c.list[i]) { if (ch == '"' || ch == '\\') v += '\\'; v += ch; } s += (i ? ", " : "") + std::string("\"") + v + "\""; }
            s += "]";
        }
        if (c.type == C_GAUGE && !c.zones.empty()) {                       // "zones": [ { "from": 0, "to": 60, "color": "0x07E0" }, ... ]
            s += ", \"zones\": [";
            for (size_t i = 0; i < c.zones.size(); ++i) s += (i ? ", " : "") + std::string("{ \"from\": ") + FloatText(c.zones[i].from) + ", \"to\": " + FloatText(c.zones[i].to) + ", \"color\": \"" + Hex(c.zones[i].color) + "\" }";
            s += "]";
        }
        if (!c.ctlGroup.empty()) s += ", \"controlGroup\": \"" + c.ctlGroup + "\"";
        if (!c.visible) s += ", \"visible\": false";
        if (!c.enabled) s += ", \"enabled\": false";
        if (c.type == C_RADIO) s += ", \"group\": \"" + c.group + "\"";
        if (c.borderStyle >= 0) s += std::string(", \"borderStyle\": \"") + StyleName(c.borderStyle) + "\"";
        if (c.borderSize >= 0)  s += ", \"borderSize\": " + std::to_string(c.borderSize);
        if (c.padding >= 0)     s += ", \"padding\": " + std::to_string(c.padding);
        if (c.align >= 0)       s += std::string(", \"align\": \"") + AlignName(c.align) + "\"";
        auto esc = [](const std::string& v) { std::string r; for (char ch : v) { if (ch == '"' || ch == '\\') r += '\\'; r += ch; } return r; };
        for (int i = 0; i < ExtraCount(c.type); ++i) {                    // the type's extras that differ from their default
            const ExtraDef& d = ExtraAt(c.type, i); auto it = c.extra.find(d.key); if (it == c.extra.end()) continue;
            s += std::string(", \"") + d.key + "\": ";
            switch (d.kind) {
            case PK_INT:   s += it->second; break;
            case PK_FLOAT: s += it->second; break;                                  // a number: 12.5
            case PK_BOOL:  s += it->second; break;                                 // true / false
            case PK_ENUM:  s += "\"" + Lower(ChoiceName(d, atoi(it->second.c_str()))) + "\""; break;
            default:       s += "\"" + esc(it->second) + "\""; break;                // colour "0x...." , text, font id
            }
        }
        return s;
    }
    static std::string Hex(uint16_t c) { char b[8]; snprintf(b, sizeof b, "0x%04X", c); return b; }

    static std::string BoxJson(const DMessageBox& b) {
        std::string s = "{ \"name\": \"" + JsonEsc(b.name) + "\"";
        if (b.w != 220) s += ", \"width\": " + std::to_string(b.w);
        if (b.font != DEFAULT_FONT) s += ", \"font\": \"" + JsonEsc(b.font) + "\"";
        if (b.rich) s += ", \"rich\": true";
        for (int k = 0; k < 4; ++k) if (!b.styleFont[k].empty()) s += std::string(", \"") + STYLE_FONT_KEYS[k] + "\": \"" + JsonEsc(b.styleFont[k]) + "\"";
        if (!b.title.empty()) s += ", \"title\": \"" + JsonEsc(b.title) + "\"";
        if (!b.centered) s += ", \"centered\": false, \"x\": " + std::to_string(b.x) + ", \"y\": " + std::to_string(b.y);
        if (b.fg != 0xFFFF) s += ", \"textColor\": \"" + Hex(b.fg) + "\"";
        if (b.bg != 0x0000) s += ", \"backColor\": \"" + Hex(b.bg) + "\"";
        if (b.border != 0xFFFF) s += ", \"borderColor\": \"" + Hex(b.border) + "\"";
        s += ",\n      \"message\": [";
        for (size_t i = 0; i < b.lines.size(); ++i) s += (i ? ", \"" : "\"") + JsonEsc(b.lines[i]) + "\"";
        s += "],\n      \"buttons\": [";
        for (size_t i = 0; i < b.buttons.size(); ++i) {
            const MsgButton& mb = b.buttons[i];
            s += (i ? ", " : "") + (mb.itemId.empty() ? "\"" + JsonEsc(mb.text) + "\"" : "{ \"text\": \"" + JsonEsc(mb.text) + "\", \"itemId\": \"" + JsonEsc(mb.itemId) + "\" }");
        }
        s += "]";
        if (b.defaultBtn != 0) s += ", \"default\": " + std::to_string(b.defaultBtn);
        return s + " }";
    }
    static std::string JsonEsc(const std::string& s) { std::string r; for (char c : s) { if (c == '"' || c == '\\') r += '\\'; r += c; } return r; }
    // A menu as nested, readable, hand-editable JSON:  [ { "text": "System", "items": [ { "text": "Reboot", "itemId": "MNU_REBOOT", "id": 7 } ] }, ... ]
    static std::string MenuJson(const std::vector<MenuNode>& v, int ind) {
        if (v.empty()) return "[]";
        std::string pad((size_t)ind + 2, ' '), s = "[\n";
        for (size_t i = 0; i < v.size(); ++i) {
            const MenuNode& k = v[i];
            s += pad + "{ \"text\": \"" + JsonEsc(k.text) + "\"";
            if (!k.itemId.empty()) s += ", \"itemId\": \"" + JsonEsc(k.itemId) + "\"";
            if (k.id >= 0) s += ", \"id\": " + std::to_string(k.id);
            if (!k.kids.empty()) s += ", \"items\": " + MenuJson(k.kids, ind + 2);
            s += i + 1 < v.size() ? " },\n" : " }\n";
        }
        return s + std::string((size_t)ind, ' ') + "]";
    }
    std::string GroupIdsJson() const {                            // , "groupIds": { "GRP_PAGE1": 1, "GRP_PAGE2": 2 }
        AssignGroupIds(); if (groupIds.empty()) return "";
        std::string s = ", \"groupIds\": { "; for (size_t i = 0; i < groupIds.size(); ++i) s += (i ? ", \"" : "\"") + groupIds[i].first + "\": " + std::to_string(groupIds[i].second);
        return s + " }";
    }
    std::string ToJson() const {
        auto esc = [](const std::string& s) { std::string r; for (char c : s) { if (c == '"' || c == '\\') r += '\\'; r += c; } return r; };
        std::string j = "{\n  \"form\": { \"name\": \"" + esc(name) + "\", \"width\": " + std::to_string(w) + ", \"height\": " + std::to_string(h) + ", \"backColor\": \"" + Hex(bg) + "\", \"outputPath\": \"" + esc(outDir) + "\"" + (keepHidden ? ", \"keepHidden\": true" : "") + GroupIdsJson() + " },\n  \"controls\": [";
        const bool custom = TabOrderCustom();                         // the focus numbers are saved only when some differ from the list positions
        for (size_t i = 0; i < items.size(); ++i) {
            const DControl& c = items[i];
            j += i ? ",\n    " : "\n    ";
            j += "{ \"type\": \"" + std::string(Info(c.type).key) + "\", \"name\": \"" + esc(c.name) + "\", \"text\": \"" + esc(c.text) +
                 "\", \"font\": \"" + esc(c.font) + "\", \"textColor\": \"" + Hex(c.fg) + "\", \"backColor\": \"" + Hex(c.bg) + "\", \"borderColor\": \"" + Hex(c.border) +
                 "\"" + Layout(c) + (custom ? ", \"tabOrder\": " + std::to_string(EffTab(i)) : std::string()) + ", \"x\": " + std::to_string(c.x) + ", \"y\": " + std::to_string(c.y) + ", \"width\": " + std::to_string(c.w) +
                 (c.type == C_MENU ? ",\n      \"items\": " + MenuJson(c.menu, 6) + "\n    }" : " }");
        }
        j += items.empty() ? "]" : "\n  ]";
        if (!boxes.empty()) {                                                          // "messageBoxes": [ { name, title, message: [lines], buttons: [...], default } ]
            j += ",\n  \"messageBoxes\": [\n";
            for (size_t i = 0; i < boxes.size(); ++i) j += "    " + BoxJson(boxes[i]) + (i + 1 < boxes.size() ? ",\n" : "\n");
            j += "  ]";
        }
        j += "\n}\n";
        return j;
    }

    // Replaces this form; on failure the form is left unchanged and err says why.
    bool FromJson(const std::string& src, std::string& err) {
        JV root; JsonParser p(src);
        if (!p.Parse(root, err)) return false;
        const JV* f = root.Get("form"); const JV* cs = root.Get("controls");
        if (root.t != JV::OBJ || !f || f->t != JV::OBJ || !cs || cs->t != JV::ARR) { err = "not a Form Designer file (\"form\" / \"controls\" missing)"; return false; }
        DForm n;
        auto col = [](const JV* v, uint16_t def) -> uint16_t {            // a colour: "0xF800" or a plain number
            long x = def;
            if (v && v->t == JV::NUM) x = v->n; else if (v && v->t == JV::STR) x = strtol(v->s.c_str(), nullptr, 0);
            return (uint16_t)(x < 0 ? 0 : x > 0xFFFF ? 0xFFFF : x); };
        auto num = [](const JV* v, int def, int lo, int hi) { int x = v && v->t == JV::NUM ? (int)v->n : def; return x < lo ? lo : x > hi ? hi : x; };
        if (const JV* v = f->Get("name")) if (v->t == JV::STR && ValidName(v->s)) n.name = v->s;
        n.w = num(f->Get("width"), DEFAULT_FORM_W, MIN_FORM_W, MAX_FORM_W);
        n.h = num(f->Get("height"), DEFAULT_FORM_H, MIN_FORM_H, MAX_FORM_H);
        n.bg = col(f->Get("backColor"), DEF_FORM_BG);
        if (const JV* v = f->Get("outputPath")) if (v->t == JV::STR) n.outDir = v->s;
        if (const JV* v = f->Get("keepHidden")) if (v->t == JV::NUM) n.keepHidden = v->n != 0;
        for (const JV& c : cs->a) {
            const JV* ty = c.Get("type");
            int t = -1;
            if (ty && ty->t == JV::STR) for (int i = 0; i < C_COUNT; ++i) if (ty->s == Info(i).key) t = i;
            if (t < 0) { err = "unknown control type \"" + (ty ? ty->s : std::string("?")) + "\""; return false; }
            DControl d; d.type = t;
            const JV* nm = c.Get("name"); const JV* tx = c.Get("text");
            d.name = nm && nm->t == JV::STR && ValidName(nm->s) && !n.ControlNamed(nm->s) ? nm->s : n.UniqueName(Info(t).prefix);   // groups are repaired after the loop
            d.text = tx && tx->t == JV::STR ? Ascii(tx->s) : Info(t).defText;
            if (const JV* fv = c.Get("font")) if (fv->t == JV::STR && FontIndex(fv->s) >= 0) d.font = fv->s;       // unknown fonts fall back to the default
            d.fg = col(c.Get("textColor"), DEF_FG); d.bg = col(c.Get("backColor"), DEF_BG); d.border = col(c.Get("borderColor"), DEF_BORDER);
            if (const JV* v = c.Get("borderStyle")) if (v->t == JV::STR) d.borderStyle = NameIndex(v->s, StyleName);
            if (const JV* v = c.Get("align")) if (v->t == JV::STR && Info(t).hasAlign) d.align = NameIndex(v->s, AlignName);
            if (const JV* v = c.Get("borderSize")) if (v->t == JV::NUM) d.borderSize = num(v, 0, 0, MAX_BORDER);
            if (const JV* v = c.Get("padding")) if (v->t == JV::NUM) d.padding = num(v, 0, 0, MAX_PADDING);
            if (const JV* gv = c.Get("group")) if (gv->t == JV::STR && ValidName(gv->s) && !n.ControlNamed(gv->s)) d.group = gv->s;
            for (int ei = 0; ei < ExtraCount(t); ++ei) {                   // the type's extras (bad values are ignored: the default stays)
                const ExtraDef& ed = ExtraAt(t, ei); const JV* v = c.Get(ed.key); if (!v) continue;
                switch (ed.kind) {
                case PK_INT:   if (v->t == JV::NUM) ExtraSet(d, ed, std::to_string(v->n)); break;
                case PK_FLOAT: if (v->t == JV::NUM) ExtraSet(d, ed, FloatText(v->d)); break;
                case PK_BOOL:  if (v->t == JV::NUM) ExtraSet(d, ed, v->n ? "true" : "false"); break;
                case PK_ENUM:  if (v->t == JV::NUM) ExtraSet(d, ed, std::to_string(v->n));
                               else if (v->t == JV::STR) for (int k = 0; k < ChoiceCount(ed); ++k) if (Lower(ChoiceName(ed, k)) == Lower(v->s)) ExtraSet(d, ed, std::to_string(k));
                               break;
                case PK_COLOR: if (v->t == JV::STR) ExtraSet(d, ed, v->s); else if (v->t == JV::NUM) ExtraSet(d, ed, std::to_string(v->n)); break;
                case PK_FONT:  if (v->t == JV::STR && (v->s.empty() || FontIndex(v->s) >= 0)) ExtraSet(d, ed, v->s); break;
                default:       if (v->t == JV::STR) ExtraSet(d, ed, Ascii(v->s)); break;                    // (text: printable ASCII only)
                }
            }
            if (HasList(t)) if (const JV* lv = c.Get("items")) if (lv->t == JV::ARR)                  // items: printable ASCII, non-empty, a sensible number and length
                for (const JV& it : lv->a) if ((it.t == JV::STR || it.t == JV::OBJ) && (int)d.list.size() < MAX_LIST_ITEMS) {
                    std::string raw = it.t == JV::STR ? it.s : (it.Get("text") && it.Get("text")->t == JV::STR ? it.Get("text")->s : "");
                    std::string v; for (char ch : raw) if (ch >= 0x20 && ch < 0x7F) v += ch;
                    if (v.size() > (size_t)MAX_ITEM_LEN) v.resize(MAX_ITEM_LEN);
                    if (v.empty()) continue;
                    d.list.push_back(v);
                    if (t == C_TABBAR) { const JV* gv = it.t == JV::OBJ ? it.Get("group") : nullptr; d.pages.resize(d.list.size()); if (gv && gv->t == JV::STR) d.pages.back() = gv->s; }
                }
            if (t == C_RICHTEXT) if (const JV* rv = c.Get("richText")) {                          // lines of markup (a single string with \n is accepted too): printable ASCII, bounded
                std::vector<std::string> raw;
                if (rv->t == JV::ARR) { for (const JV& l : rv->a) if (l.t == JV::STR) raw.push_back(l.s); }
                else if (rv->t == JV::STR) { std::string cur; for (char ch : rv->s) { if (ch == '\n') { raw.push_back(cur); cur.clear(); } else cur += ch; } raw.push_back(cur); }
                int chars = 0;
                for (std::string& l : raw) { std::string v; for (char ch : l) if (ch >= 0x20 && ch < 0x7F) v += ch; if ((int)d.list.size() >= MAX_RICH_LINES || chars + (int)v.size() > MAX_RICH_CHARS) break; chars += (int)v.size() + 1; d.list.push_back(v); }
            }
            if (t == C_GAUGE) if (const JV* zv = c.Get("zones")) if (zv->t == JV::ARR)
                for (const JV& z : zv->a) if (z.t == JV::OBJ && (int)d.zones.size() < MAX_GAUGE_ZONES) {                          // a band needs from < to; the colour is 0xRRRR or a number
                    const JV* fv = z.Get("from"); const JV* tv = z.Get("to"); const JV* cv = z.Get("color");
                    if (!fv || !tv || fv->t != JV::NUM || tv->t != JV::NUM || !(fv->d < tv->d)) continue;
                    GaugeZone gz; gz.from = fv->d; gz.to = tv->d;
                    if (cv && cv->t == JV::STR) gz.color = (uint16_t)std::min(65535L, std::max(0L, strtol(cv->s.c_str(), nullptr, 0))); else if (cv && cv->t == JV::NUM) gz.color = (uint16_t)std::min(65535L, std::max(0L, cv->n));
                    d.zones.push_back(gz);
                }
            if (const JV* v = c.Get("controlGroup")) if (v->t == JV::STR) d.ctlGroup = v->s;
            if (const JV* v = c.Get("visible")) if (v->t == JV::NUM) d.visible = v->n != 0;
            if (const JV* v = c.Get("enabled")) if (v->t == JV::NUM) d.enabled = v->n != 0;
            if (t == C_MENU) if (const JV* mv = c.Get("items")) if (mv->t == JV::ARR) {                      // nested items: text, optional itemId / id, optional items
                int budget = 1000;
                std::function<void(const JV&, std::vector<MenuNode>&, int)> rd = [&](const JV& arr, std::vector<MenuNode>& out, int depth) {
                    for (const JV& it : arr.a) {
                        if (budget-- <= 0 || depth >= MAX_MENU_DEPTH + 1) return;
                        MenuNode k;
                        if (it.t == JV::STR) k.text = it.s;                                        // shorthand: just the text
                        else if (it.t == JV::OBJ) {
                            if (const JV* v = it.Get("text")) if (v->t == JV::STR) k.text = v->s;
                            if (const JV* v = it.Get("itemId")) if (v->t == JV::STR) k.itemId = v->s;
                            if (const JV* v = it.Get("id")) if (v->t == JV::NUM) k.id = (int)std::max(-1L, std::min(v->n, 1000000L));
                            if (const JV* v = it.Get("items")) if (v->t == JV::ARR) rd(*v, k.kids, depth + 1);
                        } else continue;
                        out.push_back(std::move(k));
                    }
                };
                rd(*mv, d.menu, 0);
            }
            if (const JV* v = c.Get("tabOrder")) if (v->t == JV::NUM) d.tabOrder = (int)std::max(0L, std::min(v->n, (long)MAX_TAB_ORDER));
            d.x = num(c.Get("x"), 0, 0, MAX_FORM_W); d.y = num(c.Get("y"), 0, 0, MAX_FORM_H);
            d.w = num(c.Get("width"), Info(t).defW, MIN_CONTROL_W, MAX_FORM_W);
            n.items.push_back(d);
        }
        for (int k = 1; n.FormNameClashes(n.name, false); ++k) n.name = "Form" + std::to_string(k);   // the form's class names must not clash with a control (a group gives way to the form, below)
        for (DControl& c : n.items)                                   // a group name must not be a control name or the form's class (all are C++ names)
            if (c.type == C_RADIO && (n.ControlNamed(c.group) || c.group == n.name || c.group == n.name + "Base")) c.group = n.UniqueGroupName();
        n.NormalizeMenus();                                            // item ids: valid, unique, numbered (after the control names are final)
        if (const JV* bv = root.Get("messageBoxes")) if (bv->t == JV::ARR) {         // message boxes: name, title, message lines, buttons, default, position, colours
            auto colr = [&](const JV* v, uint16_t def) -> uint16_t { long x = def; if (v && v->t == JV::NUM) x = v->n; else if (v && v->t == JV::STR) x = strtol(v->s.c_str(), nullptr, 0); return (uint16_t)(x < 0 ? 0 : x > 0xFFFF ? 0xFFFF : x); };
            for (const JV& bj : bv->a) if (bj.t == JV::OBJ && (int)n.boxes.size() < MAX_MSG_BOXES * 2) {
                DMessageBox b;
                if (const JV* v = bj.Get("name")) if (v->t == JV::STR) b.name = v->s;
                if (const JV* v = bj.Get("width")) if (v->t == JV::NUM) b.w = (int)std::max(-1L, std::min(v->n, 100000L));
                if (const JV* v = bj.Get("font")) if (v->t == JV::STR) b.font = v->s;
                if (const JV* v = bj.Get("rich")) if (v->t == JV::NUM) b.rich = v->n != 0;
                for (int k = 0; k < 4; ++k) if (const JV* v = bj.Get(STYLE_FONT_KEYS[k])) if (v->t == JV::STR && FontIndex(v->s) >= 0) b.styleFont[k] = v->s;
                if (const JV* v = bj.Get("title")) if (v->t == JV::STR) b.title = v->s;
                if (const JV* v = bj.Get("centered")) if (v->t == JV::NUM) b.centered = v->n != 0;
                if (const JV* v = bj.Get("x")) if (v->t == JV::NUM) b.x = (int)std::max(-1L, std::min(v->n, 100000L));
                if (const JV* v = bj.Get("y")) if (v->t == JV::NUM) b.y = (int)std::max(-1L, std::min(v->n, 100000L));
                b.fg = colr(bj.Get("textColor"), 0xFFFF); b.bg = colr(bj.Get("backColor"), 0x0000); b.border = colr(bj.Get("borderColor"), 0xFFFF);
                if (const JV* v = bj.Get("message")) {
                    if (v->t == JV::ARR) { for (const JV& l : v->a) if (l.t == JV::STR) b.lines.push_back(l.s); }
                    else if (v->t == JV::STR) { std::string cur; for (char ch : v->s) { if (ch == '\n') { b.lines.push_back(cur); cur.clear(); } else cur += ch; } b.lines.push_back(cur); }     // a plain string with \n is accepted too
                }
                if (const JV* v = bj.Get("buttons")) if (v->t == JV::ARR) for (const JV& k : v->a) {
                    MsgButton mb;
                    if (k.t == JV::STR) mb.text = k.s;
                    else if (k.t == JV::OBJ) { if (const JV* t = k.Get("text")) if (t->t == JV::STR) mb.text = t->s; if (const JV* t = k.Get("itemId")) if (t->t == JV::STR) mb.itemId = t->s; }
                    else continue;
                    b.buttons.push_back(mb);
                }
                if (const JV* v = bj.Get("default")) if (v->t == JV::NUM) b.defaultBtn = (int)std::max(0L, std::min(v->n, 100L));
                n.boxes.push_back(b);
            }
            n.NormalizeBoxes();
        }
        for (DControl& c : n.items) {                                  // control groups: safe names that no control, radio group, menu item or form class has
            if (!c.ctlGroup.empty() && (!ValidName(c.ctlGroup) || n.GroupNameTaken(c.ctlGroup))) c.ctlGroup.clear();
            if (c.type == C_TABBAR) { c.pages.resize(c.list.size()); for (std::string& p : c.pages) if (!p.empty() && (!ValidName(p) || n.GroupNameTaken(p))) p.clear(); }
        }
        if (const JV* gv = f->Get("groupIds")) if (gv->t == JV::OBJ) for (auto& kv : gv->o) if (kv.second.t == JV::NUM) n.groupIds.push_back({ kv.first, (int)kv.second.n });
        n.AssignGroupIds();                                            // numbers: the file's where valid, else the next free
        for (DControl& c : n.items) if (HasList(c.type)) ClampSelected(c);                 // the initial selection must be an item
        { std::set<std::string> seen;                                 // only one radio button per group can be selected: the first one wins
          for (DControl& c : n.items) if (c.type == C_RADIO && XBool(c, "checked") && !seen.insert(c.group).second) c.extra.erase("checked"); }
        n.FreezeTabOrder();                                            // numbers missing in the file become the list positions, and stay with the controls from now on
        *this = n; err.clear(); return true;
    }
};
}
