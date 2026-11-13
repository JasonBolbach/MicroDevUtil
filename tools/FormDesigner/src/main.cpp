// SuperGUI Form Designer - step 2: the form model and the design surface.
// Toolbox (left) -> design surface (middle, shows the REAL SuperGUI controls) -> property panel (right). The form is saved as JSON.
// Mouse:  click a toolbox item, then click the form to place it; click a control to select it; drag to move; drag the handles at its left /
//         right edge to resize; wheel scrolls the surface (Shift = sideways); click a field on the right to edit a property.
// Keys:   Ctrl/Cmd+O open, +S save (both always show the file dialog), +X export <Form>.designer.h and <Form>.h (Shift: choose the folder), arrows nudge the selection (Shift = one grid step), Delete removes it, Tab selects the next control, G changes the grid,
//         Ctrl/Cmd+S saves, Ctrl/Cmd+O opens (form.json, or the file named in FD_FORM), Esc cancels / deselects.
// Run with FD_SELFTEST=1 for an automatic check (input layer, text field, model, and the whole editing flow with synthetic events).
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include <algorithm>
#include <csignal>
#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <cstdlib>
#ifndef SIGBUS
#define SIGBUS SIGSEGV                                   // no SIGBUS on Windows
#endif
static inline int setenv(const char* n, const char* v, int overwrite) { if (!overwrite && std::getenv(n)) return 0; return _putenv_s(n, v); }
static inline int unsetenv(const char* n) { return _putenv_s(n, ""); }
#else
#include <unistd.h>
#endif
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include "DesignerConfig.h"
#include "DesignerInput.h"
#include "DesignerTextField.h"
#include "FormModel.h"
#include "DesignerControls.h"
#include "DesignerWidgets.h"
#include "ColorDialog.h"
#include "FontDialog.h"
#include "CodeGen.h"
#include "FileDialog.h"
#include "PresetDialog.h"
#include "TemplateBuilder.h"
#include "ItemsDialog.h"
#include "MenuEditor.h"
#include "ZonesDialog.h"
#include "MessageBoxEditor.h"
#include "HelpDialog.h"
#include "RichTextEditor.h"
#include "DesignerScrollBar.h"
#include "AppSettings.h"
#include <SuperSpinBox.h>
#include <vector>

extern bool* g_running;                              // set in sdl_main.cpp: false once the window is closed
using FD::Input; using FD::InputEvent; using FD::DControl;

static LGFX* lcd = nullptr;
static lgfx::LGFX_Sprite scr;                        // everything is drawn here, then pushed to the window in one go
static bool dirty = true;
static int winW = 0, winH = 0;

// ---- layout (logical pixels) ----
static const int TOOL_Y0 = 84, TOOL_H = 25;                              // toolbox rows
static const int TREE_Y0 = TOOL_Y0 + FD::C_COUNT * TOOL_H + 68, TREE_H = 20, TREE_BOT = 806;   // "controls on form" list
static int TreeRows() { return std::max(1, (TREE_BOT - TREE_Y0) / TREE_H); }
static const int ORD_BTN_W = 64, ORD_BTN_GAP = 6;                         // the order buttons above the list: Up, Down, Front, Back
static int OrdBtnX(int k) { return 18 + k * (ORD_BTN_W + ORD_BTN_GAP); }
static int OrdBtnY() { return TREE_Y0 - 28; }
static int treeFirst = 0;                                                  // the first visible row of that list (it scrolls)
static const int VX = 320, VY = 84, VW = 660, VH = 716;                  // the design surface's viewport
static const int PAD = 20;                                               // space around the form inside the viewport
static const int FIELD_X = 1105, FIELD_Y0 = 96, FIELD_W = 276, FIELD_H = 26, FIELD_GAP = 34, FIELD_PAD = 6;

// ---- editor state ----
static FD::DForm form;
static int sel = -1;                                 // selected control (index), -1 = the form itself
static int tool = -1;                                // toolbox item waiting to be placed, -1 = none
static int grid = 8;                                 // snap step in pixels (1 = off)
static int scrollX = 0, scrollY = 0;                 // design surface scroll (in screen pixels, at the current zoom)
static std::string viewGroup;                        // design view: "" = every control, else only the controls of this control group (and those without a group) are shown
static int zoom = 100;                               // design surface zoom in percent (10 .. 800): the form is shown at zoom / 100
static std::string status; static Uint32 statusAt = 0;
enum DragMode { D_NONE, D_MOVE, D_RESIZE_L, D_RESIZE_R, D_TEXT };
static DragMode drag = D_NONE; static int dragDX = 0, dragDY = 0, dragRight = 0;

// ---- property fields ----
enum Prop { P_NAME, P_TEXT, P_X, P_Y, P_W, P_BSIZE, P_PAD, P_GROUP, P_FNAME, P_FW, P_FH, P_FG, P_BG, P_BORDER, P_FBG, P_FONT, P_BSTYLE, P_ALIGN, P_CGROUP, P_VISIBLE, P_ENABLED,
            P_TABORDER = 93,                       // the focus (tab) order number of a control
            P_KEEPHIDDEN = 94,                     // form: keep controls with Visible = No hidden when their group is shown
            P_RICH = 95,                           // the text of a rich text control (opens the rich text editor)
            P_MSGBOXES = 96,                       // the form's message boxes (opens the message box editor)
            P_ZONES = 97,                          // the colour zones of a gauge (opens the zones dialog)
            P_ITEMS = 98,                          // the items of a list box / combo box / tab bar (opens the items dialog)
            P_HINT = 99,                           // a row that only shows a hint (the template legend)
            P_EXTRA = 100 };                       // P_EXTRA + i = the selected control's i-th extra property (FD::ExtraAt), see FormModel.h
static const int MAX_FIELDS = 64;                    // (a gauge has about 42 rows)
static FD::TextField fields[MAX_FIELDS];
static int fieldProp[MAX_FIELDS], nFields = 0;
static int propScroll = 0;                           // the property panel scrolls when a control has more rows than fit
static const int PANEL_TOP = 90, PANEL_BOT = 806;    // the visible band of the property rows
static int FieldY(int i) { return FIELD_Y0 + i * FIELD_GAP - propScroll; }
static int FieldAt(int x, int y) {
    for (int i = 0; i < nFields; ++i) if (x >= FIELD_X && x < FIELD_X + FIELD_W && y >= FieldY(i) && y < FieldY(i) + FIELD_H && y >= PANEL_TOP && y < PANEL_BOT) return i;
    return -1;
}
static int MaxPropScroll() { return std::max(0, FIELD_Y0 + nFields * FIELD_GAP + 64 - PANEL_BOT); }       // (64: the info lines under the rows)
static void ClampPropScroll() { propScroll = std::min(std::max(propScroll, 0), MaxPropScroll()); }
static void EnsureRowVisible(int i) {
    if (i < 0 || i >= nFields) return;
    if (FieldY(i) < PANEL_TOP) propScroll -= PANEL_TOP - FieldY(i) + 4;
    else if (FieldY(i) + FIELD_H > PANEL_BOT) propScroll += FieldY(i) + FIELD_H - PANEL_BOT + 4;
    ClampPropScroll();
}
static int focus = -1;                               // focused field, -1 = none

static void Status(const char* fmt, ...) {
    char b[200]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    status = b; statusAt = SDL_GetTicks(); dirty = true;
}
static FD::AppSettings settings;                     // application-level: the folders the Open / Save dialogs were last looking at
static std::string currentFile;                      // the form file chosen with Open / Save As ("" = none yet: FD_FORM, else form.json)
static const char* FormPath() {
    if (!currentFile.empty()) return currentFile.c_str();
    const char* p = getenv("FD_FORM"); return p && *p ? p : "form.json";
}
static bool FileKnown() { return !currentFile.empty() || (getenv("FD_FORM") && *getenv("FD_FORM")); }   // a form that has a file name (Save As / Open / FD_FORM)

// ---- model helpers ----
// ---- zoom: form pixels <-> screen pixels ----
static const int ZOOM_MIN = 10, ZOOM_MAX = 800, ZOOM_STEP = 10;
static int Zc(int v) { return (v * zoom + 99) / 100; }                          // a form length / offset in screen pixels (rounded up: the first screen pixel that shows it)
static int ZW() { return Zc(form.w); }
static int ZH() { return Zc(form.h); }
static int FloorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
// A form smaller than the viewport is centred; a larger one starts PAD inside and scrolls.
static int FormOX() { return ZW() + 2 * PAD <= VW ? VX + (VW - ZW()) / 2 : VX + PAD - scrollX; }
static int FormOY() { return ZH() + 2 * PAD <= VH ? VY + (VH - ZH()) / 2 : VY + PAD - scrollY; }
static int ToFX(int sx) { return FloorDiv((sx - FormOX()) * 100, zoom); }      // a screen x as a form x
static int ToFY(int sy) { return FloorDiv((sy - FormOY()) * 100, zoom); }
static int Snap(int v) { if (v < 0) v = 0; return grid > 1 ? (v + grid / 2) / grid * grid : v; }
// ---- scroll bars: one for the property panel, two for the design surface (they overlay the edge of the viewport: PAD is wider than a bar, so they never hide the form) ----
static const int SB = 12;
static FD::ScrollBar treeBar(FD::ScrollBar::VERTICAL, 8), propBar(FD::ScrollBar::VERTICAL, 8), surfV(FD::ScrollBar::VERTICAL, SB), surfH(FD::ScrollBar::HORIZONTAL, SB);
static FD::ScrollBar* barDrag = nullptr;             // the bar being dragged with the mouse
static void ClampScroll() {
    int mx = std::max(0, ZW() + 2 * PAD - VW), my = std::max(0, ZH() + 2 * PAD - VH);
    scrollX = std::min(std::max(scrollX, 0), mx); scrollY = std::min(std::max(scrollY, 0), my);
}
static void ClampTree() { treeFirst = std::max(0, std::min(treeFirst, std::max(0, (int)form.items.size() - TreeRows()))); }
static void SyncBars() {                             // geometry, range and position of the four bars follow the model
    ClampTree(); treeBar.SetGeometry(284, TREE_Y0, TreeRows() * TREE_H); treeBar.SetRange((int)form.items.size(), TreeRows()); treeBar.SetPos(treeFirst);
    int th = PANEL_BOT - PANEL_TOP;
    propBar.SetGeometry(1382, PANEL_TOP, th); propBar.SetRange(th + MaxPropScroll(), th); propBar.SetPos(propScroll);
    bool needV = ZH() + 2 * PAD > VH, needH = ZW() + 2 * PAD > VW;
    surfV.SetGeometry(VX + VW - SB, VY, VH - (needH ? SB : 0)); surfV.SetRange(ZH() + 2 * PAD, VH); surfV.SetPos(scrollY);
    surfH.SetGeometry(VX, VY + VH - SB, VW - (needV ? SB : 0)); surfH.SetRange(ZW() + 2 * PAD, VW); surfH.SetPos(scrollX);
}
static void RefreshGeometry() { for (auto& c : form.items) FD::Measure(&scr, c); }
static void ClampPos(DControl& c) {
    c.w = std::min(std::max(c.w, FD::MIN_CONTROL_W), std::max(FD::MIN_CONTROL_W, form.w));
    c.x = std::min(std::max(c.x, 0), std::max(0, form.w - c.w));
    c.y = std::min(std::max(c.y, 0), std::max(0, form.h - c.h));
}
static DControl* Sel() { return sel >= 0 && sel < (int)form.items.size() ? &form.items[sel] : nullptr; }

// ---- property rows: what each row is and how it is edited ----
static bool IsExtra(int p) { return p >= P_EXTRA; }
static const FD::ExtraDef* ExtraOf(int p) {           // the schema entry behind an extra property of the selected control
    DControl* c = Sel(); if (!c || !IsExtra(p)) return nullptr;
    int i = p - P_EXTRA; return i < FD::ExtraCount(c->type) ? &FD::ExtraAt(c->type, i) : nullptr;
}
enum RowKind { RK_TEXT, RK_COLOR, RK_FONT, RK_SEG, RK_HINT, RK_ITEMS };   // text field / swatch (colour dialog) / font dialog / segments to click
static int RowKindOf(int p) {
    if (IsExtra(p)) {
        const FD::ExtraDef* d = ExtraOf(p); if (!d) return RK_TEXT;
        switch (d->kind) { case FD::PK_COLOR: return RK_COLOR; case FD::PK_FONT: return RK_FONT; case FD::PK_BOOL: case FD::PK_ENUM: return RK_SEG; default: return RK_TEXT; }
    }
    if (p == P_HINT) return RK_HINT;
    if (p == P_VISIBLE || p == P_ENABLED || p == P_KEEPHIDDEN) return RK_SEG;
    if (p == P_ITEMS || p == P_ZONES || p == P_MSGBOXES || p == P_RICH) return RK_ITEMS;
    if (p == P_FONT) return RK_FONT;
    if (p == P_BSTYLE || p == P_ALIGN) return RK_SEG;
    if (p >= P_FG && p <= P_FBG) return RK_COLOR;
    return RK_TEXT;
}
static bool IsColorProp(int p) { return RowKindOf(p) == RK_COLOR; }
static bool IsPickRow(int p) { return RowKindOf(p) != RK_TEXT; }                  // no text field: swatch, font or segments
static std::string PropLabel(int p) {
    if (const FD::ExtraDef* d = ExtraOf(p)) return d->label;
    static const char* L[] = { "Name", "Text", "X", "Y", "Width", "Border size", "Padding", "Group", "Name", "Width", "Height", "Text color", "Back color", "Border color", "Back color", "Font", "Border style", "Align", "Control group", "Visible", "Enabled" };
    if (p == P_ZONES) return "Zones";
    if (p == P_MSGBOXES) return "Message boxes";
    if (p == P_TABORDER) return "Tab order";
    if (p == P_KEEPHIDDEN) return "Keep hidden";
    if (p == P_RICH) return "Text";
    if (p == P_ITEMS) return Sel() && Sel()->type == FD::C_TABBAR ? "Tabs" : Sel() && Sel()->type == FD::C_MENU ? "Menu items" : "Items";
    return p >= 0 && p <= P_ENABLED ? L[p] : "";
}
// segments of a RK_SEG row
static int SegCount(int p) {
    if (const FD::ExtraDef* d = ExtraOf(p)) return FD::ChoiceCount(*d);
    if (p == P_VISIBLE || p == P_ENABLED || p == P_KEEPHIDDEN) return 2;
    return 3;
}
static std::string SegLabel(int p, int k) {
    static const char* S[] = { "Standard", "3D", "Sleek" }; static const char* A[] = { "Left", "Center", "Right" };
    if (const FD::ExtraDef* d = ExtraOf(p)) return FD::ChoiceName(*d, k);
    if (p == P_VISIBLE || p == P_ENABLED || p == P_KEEPHIDDEN) return k ? "Yes" : "No";
    return p == P_BSTYLE ? S[k] : A[k];
}
static int SegCurrent(int p) {
    if (p == P_KEEPHIDDEN) return form.keepHidden ? 1 : 0;
    const DControl* c = Sel(); if (!c) return 0;
    if (const FD::ExtraDef* d = ExtraOf(p)) { std::string v = FD::ExtraGet(*c, *d); return d->kind == FD::PK_BOOL ? (v == "true" ? 1 : 0) : atoi(v.c_str()); }
    if (p == P_VISIBLE) return c->visible ? 1 : 0;
    if (p == P_ENABLED) return c->enabled ? 1 : 0;
    return p == P_BSTYLE ? (c->borderStyle >= 0 ? c->borderStyle : c->defStyle) : (c->align >= 0 ? c->align : c->defAlign);
}

// ---- the last successful save folder of the generated code (kept in the form's JSON as outputPath, not shown in the designer) ----
static bool DirExists(const std::string& p) { std::error_code ec; return !p.empty() && std::filesystem::is_directory(p, ec); }
static std::string WorkDir() { std::error_code ec; return std::filesystem::current_path(ec).string(); }     // the executable's working folder
// The folder the FormDesigner program itself lives in (the default export folder); the working folder if SDL cannot tell.
static std::string appDirOverride;                   // (the self-test points the program folder somewhere else)
static std::string AppDir() {
    if (!appDirOverride.empty()) return appDirOverride;
    std::string d; if (char* b = SDL_GetBasePath()) { d = b; SDL_free(b); }
    while (d.size() > 1 && (d.back() == '/' || d.back() == '\\')) d.pop_back();
    return DirExists(d) ? d : WorkDir();
}
// A quick check after loading (and at start-up): a missing export folder falls back to the FormDesigner application folder. Returns true when it had to change.
// The code generator will set form.outDir after every successful save of the generated files.
static bool FixOutDir() {
    if (DirExists(form.outDir)) return false;
    form.outDir = AppDir(); return true;
}

static bool GetColorProp(int p, uint16_t& out) {      // the model colour a colour row edits
    DControl* c = Sel();
    if (const FD::ExtraDef* d = ExtraOf(p)) { if (d->kind != FD::PK_COLOR) return false; out = (uint16_t)strtol(FD::ExtraGet(*c, *d).c_str(), nullptr, 0); return true; }
    switch (p) { case P_FG: if (!c) return false; out = c->fg; return true; case P_BG: if (!c) return false; out = c->bg; return true;
                 case P_BORDER: if (!c) return false; out = c->border; return true; case P_FBG: out = form.bg; return true; }
    return false;
}
static void SetColorProp(int p, uint16_t v) {
    DControl* c = Sel();
    if (const FD::ExtraDef* d = ExtraOf(p)) { char b[8]; snprintf(b, sizeof b, "0x%04X", v); FD::ExtraSet(*c, *d, b); return; }
    switch (p) { case P_FG: if (c) c->fg = v; break; case P_BG: if (c) c->bg = v; break; case P_BORDER: if (c) c->border = v; break; case P_FBG: form.bg = v; break; }
}
static bool IsTextColor(int p) { const FD::ExtraDef* d = ExtraOf(p); return p == P_FG || (d && std::string(d->key) == "labelColor"); }   // drawn as text on a partner colour
static uint16_t PartnerColor(int p) { DControl* c = Sel(); return p == P_FG && c ? c->bg : p == P_BG && c ? c->fg : p == P_BORDER && c ? c->bg : IsExtra(p) && c ? form.bg : form.bg; }
// fonts: the control's own font, or an extra font (the label font: "" = the same as the control)
static std::string GetFontProp(int p) {
    DControl* c = Sel(); if (!c) return "";
    if (const FD::ExtraDef* d = ExtraOf(p)) return FD::ExtraGet(*c, *d);
    return c->font;
}
static void SetFontProp(int p, const std::string& id) {
    DControl* c = Sel(); if (!c) return;
    if (const FD::ExtraDef* d = ExtraOf(p)) { FD::ExtraSet(*c, *d, id); return; }
    c->font = id;
}

// ---- buttons at the right end of some rows: Fit (width of text-box controls), Presets / Build (the input box template) ----
struct RowBtn { const char* label; int x, w; };
static int RowButtons(int p, RowBtn* out) {
    DControl* c = Sel(); int n = 0; if (!c) return 0;
    if (p == P_W && FD::Info(c->type).fitWidth) out[n++] = { "Fit", FIELD_X + FIELD_W - 46, 46 };
    if (const FD::ExtraDef* d = ExtraOf(p)) if (std::string(d->key) == "template") { out[n++] = { "Presets", FIELD_X + FIELD_W - 150, 74 }; out[n++] = { "Build", FIELD_X + FIELD_W - 70, 70 }; }
    return n;
}
static int RowFieldW(int p) { RowBtn b[2]; int n = RowButtons(p, b); return n ? b[0].x - 4 - FIELD_X : FIELD_W; }
static std::string rejectMsg;                         // why the last typed value was refused (shown under the rows, red)

static std::string PropValue(int p) {
    DControl* c = Sel();
    if (const FD::ExtraDef* d = ExtraOf(p)) return FD::ExtraGet(*c, *d);       // (text and number extras)
    switch (p) {
    case P_NAME: return c ? c->name : ""; case P_TEXT: return c ? c->text : "";
    case P_BSIZE: return c ? std::to_string(c->borderSize >= 0 ? c->borderSize : c->defSize) : "";
    case P_PAD: return c ? std::to_string(c->padding >= 0 ? c->padding : c->defPad) : "";
    case P_GROUP: return c ? c->group : "";
    case P_CGROUP: return c ? c->ctlGroup : "";
    case P_TABORDER: return c && sel >= 0 ? std::to_string(form.EffTab((size_t)sel)) : "";
    case P_X: return c ? std::to_string(c->x) : ""; case P_Y: return c ? std::to_string(c->y) : ""; case P_W: return c ? std::to_string(c->w) : "";
    case P_FNAME: return form.name; case P_FW: return std::to_string(form.w); case P_FH: return std::to_string(form.h);
    }
    return "";
}
// A change to one of the control's extra properties, tried on a copy: the real control judges whether it makes sense (refused values keep the old one).
static bool ApplyExtra(int p, const std::string& v) {
    DControl* c = Sel(); const FD::ExtraDef* d = ExtraOf(p); if (!c || !d) return false;
    DControl cand = *c;
    if (!FD::ExtraSet(cand, *d, v.empty() && (d->kind == FD::PK_INT || d->kind == FD::PK_FLOAT) ? std::string(d->def) : v)) return false;
    if (std::string(d->key) != "value") FD::ClampGaugeValue(cand);                          // a gauge: a new range moves the needle into it      // (an empty number = the default)
    std::string key = d->key, msg = FD::Validate(&scr, cand);
    if (!msg.empty() && key == "template" && XSet(cand, "value")) {          // a new template that the old value no longer fits: the value is cleared
        DControl t2 = cand; t2.extra.erase("value");
        if (FD::Validate(&scr, t2).empty()) { cand = t2; msg.clear(); Status("the value no longer fits the new template and was cleared"); }
    }
    if (!msg.empty()) { rejectMsg = msg; Status("%s", msg.c_str()); dirty = true; return false; }       // strict: refused, the model keeps the old value, the row turns red
    rejectMsg.clear();
    FD::Normalize(&scr, cand);                                                // stored the way the control prints it
    *c = cand;
    if (c->type == FD::C_RADIO && key == "checked" && XBool(*c, "checked"))             // selecting a radio button deselects the others of its group
        for (DControl& o : form.items) if (&o != c && o.type == FD::C_RADIO && o.group == c->group) o.extra.erase("checked");
    return true;
}
// Applies a typed value to the model (live, on every change). Returns false when the value is not (yet) valid; the model is then unchanged.
static bool ApplyProp(int p, const std::string& v) {
    DControl* c = Sel();
    if (ExtraOf(p)) return ApplyExtra(p, v);
    if ((p <= P_GROUP || (p >= P_CGROUP && p <= P_ENABLED) || p == P_TABORDER) != (c != nullptr)) return false;
    if (p == P_TABORDER) { if (v.empty()) return false; int n = std::min(atoi(v.c_str()), FD::MAX_TAB_ORDER); if (n < 1) return false; c->tabOrder = n; return true; }       // a number from 1: the user decides the focus order
    if (p == P_BSIZE || p == P_PAD) {                    // empty = back to the control's own default; a value equal to the default is not an override
        bool size = p == P_BSIZE; int n = v.empty() ? -1 : std::min(atoi(v.c_str()), size ? FD::MAX_BORDER : FD::MAX_PADDING);
        (size ? c->borderSize : c->padding) = n == (size ? c->defSize : c->defPad) ? -1 : n; return true;
    }
    bool num = p == P_X || p == P_Y || p == P_W || p == P_FW || p == P_FH;
    int n = num && !v.empty() ? atoi(v.c_str()) : 0;
    if (num && v.empty()) return false;
    switch (p) {
    case P_NAME:  if (!FD::ValidName(v) || form.NameTaken(v, sel)) return false; c->name = v; return true;
    case P_TEXT:  c->text = v; return true;
    case P_CGROUP:                                                                      // a control group: a name (a uint16_t constant in the generated class), "" = none
        if (v.empty()) { c->ctlGroup.clear(); return true; }
        if (!FD::ValidName(v)) { Status("%s is not a safe C++ name (a keyword, a reserved or library name, or a macro)", v.c_str()); return false; }
        if (form.GroupNameTaken(v)) { Status("%s is already used by a control, a radio group, a menu item or the form's classes", v.c_str()); return false; }
        c->ctlGroup = v; if (!viewGroup.empty()) viewGroup = v; return true;
    case P_GROUP: if (!FD::ValidName(v) || form.ControlNamed(v) || form.IsGroupName(v) || v == form.name || v == form.name + "Base") return false; c->group = v;
                  if (XBool(*c, "checked")) for (const DControl& o : form.items) if (&o != c && o.type == FD::C_RADIO && o.group == v && XBool(o, "checked")) { c->extra.erase("checked"); break; }     // joining a group that already has a selection: it does not take it over
                  return true;     // an identifier that is not a control's name
    case P_X:     c->x = n; ClampPos(*c); return true;
    case P_Y:     c->y = n; ClampPos(*c); return true;
    case P_W:     c->w = n; ClampPos(*c); return true;
    case P_FNAME: if (!FD::ValidName(v) || form.FormNameClashes(v)) return false; form.name = v; return true;     // not a keyword, not a control / group name
    case P_FW:    form.w = std::min(std::max(n, FD::MIN_FORM_W), FD::MAX_FORM_W); ClampScroll(); return true;
    case P_FH:    form.h = std::min(std::max(n, FD::MIN_FORM_H), FD::MAX_FORM_H); ClampScroll(); return true;
    }
    return false;
}
// Which fields the property panel shows (rebuilt when the selection changes) and their text (refreshed from the model).
static void BuildFields() {
    nFields = 0;
    auto add = [](int p) { if (nFields >= MAX_FIELDS) return; FD::TextField& f = fields[nFields]; fieldProp[nFields++] = p; f.filter = FD::TextField::ANY; f.maxLen = 64;
                           if (p == P_NAME || p == P_FNAME || p == P_GROUP || p == P_CGROUP) { f.filter = FD::TextField::IDENT; if (p == P_CGROUP) f.maxLen = 32; }
                           if (p == P_TABORDER) { f.filter = FD::TextField::DIGITS; f.maxLen = 4; }
                           if (p == P_X || p == P_Y || p == P_W || p == P_FW || p == P_FH) { f.filter = FD::TextField::DIGITS; f.maxLen = 4; }
                           if (p == P_BSIZE || p == P_PAD) { f.filter = FD::TextField::DIGITS; f.maxLen = 2; }
                           if (const FD::ExtraDef* d = ExtraOf(p)) { if (d->kind == FD::PK_INT) { f.filter = d->lo < 0 ? FD::TextField::SIGNED : FD::TextField::DIGITS; f.maxLen = 11; } else if (d->kind == FD::PK_FLOAT) { f.filter = FD::TextField::DECIMAL; f.maxLen = 12; } else f.maxLen = 40; } };
    if (Sel()) {
        const FD::TypeInfo& ti = FD::Info(Sel()->type);
        add(P_NAME); if (ti.hasText) add(P_TEXT); if (Sel()->type == FD::C_RADIO) add(P_GROUP); add(P_X); add(P_Y); add(P_W); if (FD::Focusable(*Sel())) add(P_TABORDER); add(P_CGROUP); add(P_VISIBLE); add(P_ENABLED);
        if (FD::HasList(Sel()->type) || Sel()->type == FD::C_MENU) add(P_ITEMS);
        if (Sel()->type == FD::C_GAUGE) add(P_ZONES);
        if (Sel()->type == FD::C_RICHTEXT) add(P_RICH);
        for (int i = 0; i < ti.nExtras; ++i) { add(P_EXTRA + i); if (std::string(ti.extras[i].key) == "template") add(P_HINT); }   // the type's own properties (+ the template legend)
        add(P_FONT); add(P_FG); add(P_BG); add(P_BORDER); add(P_BSTYLE); add(P_BSIZE); add(P_PAD); if (ti.hasAlign) add(P_ALIGN);
        if (ti.hasLabel) for (int i = 0; i < FD::N_LABEL_EXTRAS; ++i) add(P_EXTRA + ti.nExtras + i);   // the label group
    }
    else       { add(P_FNAME); add(P_FW); add(P_FH); add(P_FBG); add(P_KEEPHIDDEN); add(P_MSGBOXES); }
    focus = -1; drag = drag == D_TEXT ? D_NONE : drag; propScroll = 0;
    for (int i = 0; i < nFields; ++i) if (!IsPickRow(fieldProp[i])) fields[i].SetText(PropValue(fieldProp[i]));
    dirty = true;
}
static void SyncFields() {                           // model -> fields (the field being typed in keeps its text)
    for (int i = 0; i < nFields; ++i) if (i != focus && !IsPickRow(fieldProp[i])) fields[i].SetText(PropValue(fieldProp[i]));
    dirty = true;
}
static bool InView(const DControl& c) { return viewGroup.empty() || c.ctlGroup.empty() || c.ctlGroup == viewGroup; }
// ---- the order of the controls = the order they are added to the form = draw order (the last one is in front) and focus order ----
static void MoveSel(int how) {                       // 0 up (one earlier), 1 down (one later), 2 to the front (last), 3 to the back (first)
    if (sel < 0 || sel >= (int)form.items.size()) return;
    int n = (int)form.items.size(), to = how == 0 ? sel - 1 : how == 1 ? sel + 1 : how == 2 ? n - 1 : 0;
    if (to < 0 || to >= n || to == sel) { Status("%s is already %s", form.items[sel].name.c_str(), how == 0 || how == 3 ? "the first (at the back)" : "the last (in front)"); return; }
    form.FreezeTabOrder();                           // a control's focus number is fixed before anything moves: the move changes the draw order only
    DControl c = form.items[sel]; form.items.erase(form.items.begin() + sel); form.items.insert(form.items.begin() + to, c); sel = to;
    if (sel < treeFirst) treeFirst = sel; else if (sel >= treeFirst + TreeRows()) treeFirst = sel - TreeRows() + 1;
    BuildFields(); dirty = true;
    static const char* what[4] = { "moved one step toward the back", "moved one step toward the front", "brought to the front", "sent to the back" };
    Status("%s %s (position %d of %d: drawn %s; its tab number %d is unchanged)", c.name.c_str(), what[how], sel + 1, n, sel == n - 1 ? "on top" : sel == 0 ? "at the bottom" : "in between", form.EffTab((size_t)sel));
}
static void Select(int i) {
    if (i >= 0 && i < (int)form.items.size() && !InView(form.items[i])) viewGroup = form.items[i].ctlGroup;        // selecting a control of another page switches the view to it
    if (i != sel) { sel = i; BuildFields(); }
    if (i >= 0) { if (i < treeFirst) treeFirst = i; else if (i >= treeFirst + TreeRows()) treeFirst = i - TreeRows() + 1; ClampTree(); }      // the list shows the selected control
    dirty = true;
}
static void SetFocus(int i) {
    int old = focus; focus = i; EnsureRowVisible(i); if (old != i) rejectMsg.clear();
    if (old >= 0 && old != i) SyncFields();          // leaving a field: show what the model really holds (clamped, or the old valid name)
    dirty = true;
}
static void FieldEdited(int i) { if (IsPickRow(fieldProp[i])) return; ApplyProp(fieldProp[i], fields[i].text); RefreshGeometry(); SyncFields(); }

// ---- the colour dialog (modal) ----
static FD::ColorDialog* colorDlg = nullptr; static int colorDlgProp = -1;
static void OpenColorDialog(int prop) {
    uint16_t cur = 0; if (!GetColorProp(prop, cur) || colorDlg) return;
    std::string what = PropLabel(prop) + (Sel() ? " of " + Sel()->name : " of the form");
    colorDlg = new FD::ColorDialog(&scr, what, cur, PartnerColor(prop), IsTextColor(prop));
    colorDlgProp = prop; focus = -1; drag = D_NONE; dirty = true;
}
static void CloseColorDialog() {
    if (!colorDlg) return;
    if (colorDlg->state == FD::ColorDialog::OK) { SetColorProp(colorDlgProp, colorDlg->color); RefreshGeometry(); SyncFields(); Status("%s = 0x%04X", PropLabel(colorDlgProp).c_str(), colorDlg->color); }
    delete colorDlg; colorDlg = nullptr; drag = D_NONE; dirty = true;
}

// ---- the font dialog (modal) ----
static FD::FontDialog* fontDlg = nullptr; static int fontDlgProp = P_FONT;
static void OpenFontDialog(int prop) {
    DControl* c = Sel(); if (!c || fontDlg || colorDlg) return;
    DControl proto = *c; std::string cur = GetFontProp(prop); if (!cur.empty()) proto.font = cur;          // (the label font "" = the control's own font)
    fontDlgProp = prop;
    fontDlg = new FD::FontDialog(&scr, PropLabel(prop) + " of " + c->name, proto, form.bg);
    focus = -1; drag = D_NONE; dirty = true;
}
static void CloseFontDialog() {
    if (!fontDlg) return;
    if (fontDlg->state == FD::FontDialog::OK) if (DControl* c = Sel()) {
        SetFontProp(fontDlgProp, fontDlg->font); RefreshGeometry(); ClampPos(*c); SyncFields(); Status("%s %s = %s", c->name.c_str(), PropLabel(fontDlgProp).c_str(), fontDlg->font.c_str());
    }
    delete fontDlg; fontDlg = nullptr; drag = D_NONE; dirty = true;
}

// ---- the input box template: the presets list and the builder (both modal), and Fit width ----
static FD::PresetDialog* presetDlg = nullptr; static FD::TemplateBuilder* builderDlg = nullptr; static int templateProp = -1;
static int TemplateRowProp() { for (int i = 0; i < nFields; ++i) if (const FD::ExtraDef* d = ExtraOf(fieldProp[i])) if (std::string(d->key) == "template") return fieldProp[i]; return -1; }
static bool AnyModal();
static FD::MessageBoxEditor* boxDlg = nullptr;
static FD::HelpDialog* helpDlg = nullptr;
static FD::RichTextEditor* richDlg = nullptr;
static constexpr bool HELP_SOURCE_TOGGLE = true;        // the help window has a "Show source" check box (debugging the rich text control); set false to remove it
static FD::ItemsDialog* itemsDlg = nullptr; static FD::MenuEditor* menuDlg = nullptr; static FD::ZonesDialog* zonesDlg = nullptr;
static void OpenItemsDialog() {
    if (!Sel() && !AnyModal()) { boxDlg = new FD::MessageBoxEditor(&scr, "Message boxes of " + form.name, form, form.bg); focus = -1; drag = D_NONE; dirty = true; return; }
    DControl* c = Sel(); if (!c || AnyModal()) return;
    if (c->type == FD::C_RICHTEXT) { richDlg = new FD::RichTextEditor(&scr, "Text of " + c->name, *c, form.bg); focus = -1; drag = D_NONE; dirty = true; return; }
    if (c->type == FD::C_GAUGE) { zonesDlg = new FD::ZonesDialog(&scr, "Zones of " + c->name, *c, form.bg); focus = -1; drag = D_NONE; dirty = true; return; }
    if (c->type == FD::C_MENU) { menuDlg = new FD::MenuEditor(&scr, "Menu items of " + c->name, *c, form, sel, form.bg); focus = -1; drag = D_NONE; dirty = true; return; }
    if (!FD::HasList(c->type)) return;
    itemsDlg = new FD::ItemsDialog(&scr, std::string(c->type == FD::C_TABBAR ? "Tabs of " : "Items of ") + c->name, *c, form, form.bg); focus = -1; drag = D_NONE; dirty = true;
}
static void CloseRichDialog() {
    bool ok = richDlg->state == FD::RichTextEditor::OK; std::vector<std::string> r = richDlg->Lines();
    delete richDlg; richDlg = nullptr; drag = D_NONE; dirty = true;
    if (!ok) return;
    if (DControl* c = Sel()) {
        std::vector<std::string> v; int chars = 0;                                         // the same bounds as a file load
        for (std::string l : r) { if ((int)v.size() >= FD::MAX_RICH_LINES || chars + (int)l.size() > FD::MAX_RICH_CHARS) break; chars += (int)l.size() + 1; v.push_back(l); }
        while (!v.empty() && v.back().empty()) v.pop_back();
        c->list = v; RefreshGeometry(); SyncFields(); Status("%s: %d lines", c->name.c_str(), (int)v.size());
    }
}
static void CloseBoxDialog() {
    bool ok = boxDlg->state == FD::MessageBoxEditor::OK; std::vector<FD::DMessageBox> r = boxDlg->Boxes();
    delete boxDlg; boxDlg = nullptr; drag = D_NONE; dirty = true;
    if (!ok) return;
    form.boxes = r; form.NormalizeBoxes(); SyncFields(); Status("%d message box%s", (int)form.boxes.size(), form.boxes.size() == 1 ? "" : "es");
}
static void CloseZonesDialog() {
    bool ok = zonesDlg->state == FD::ZonesDialog::OK; std::vector<FD::GaugeZone> r = zonesDlg->zones;
    delete zonesDlg; zonesDlg = nullptr; drag = D_NONE; dirty = true;
    if (!ok) return;
    if (DControl* c = Sel()) { c->zones = r; RefreshGeometry(); SyncFields(); Status("%s: %d zones", c->name.c_str(), (int)r.size()); }
}
static void CloseMenuDialog() {
    bool ok = menuDlg->state == FD::MenuEditor::OK; std::vector<FD::MenuNode> r = menuDlg->nodes;
    delete menuDlg; menuDlg = nullptr; drag = D_NONE; dirty = true;
    if (!ok) return;
    if (DControl* c = Sel()) { c->menu = r; form.NormalizeMenu(sel); RefreshGeometry(); ClampPos(*c); SyncFields(); Status("%s: %d items", c->name.c_str(), FD::MenuNodeCount(c->menu)); }
}
static void CloseItemsDialog() {
    bool ok = itemsDlg->state == FD::ItemsDialog::OK; std::vector<std::string> r = itemsDlg->items, pg = itemsDlg->pages;
    delete itemsDlg; itemsDlg = nullptr; drag = D_NONE; dirty = true;
    if (!ok) return;
    if (DControl* c = Sel()) { c->list = r; if (c->type == FD::C_TABBAR) { c->pages = pg; c->pages.resize(c->list.size()); } FD::ClampSelected(*c); RefreshGeometry(); ClampPos(*c); SyncFields(); Status("%s: %d %s", c->name.c_str(), (int)r.size(), c->type == FD::C_TABBAR ? "tabs" : "items"); }
}
static void OpenPresetDialog() {
    DControl* c = Sel(); templateProp = TemplateRowProp(); if (!c || templateProp < 0 || AnyModal()) return;
    presetDlg = new FD::PresetDialog(&scr, "Template presets for " + c->name, *c, form.bg); focus = -1; drag = D_NONE; dirty = true;
}
static void OpenBuilderDialog() {
    DControl* c = Sel(); templateProp = TemplateRowProp(); if (!c || templateProp < 0 || AnyModal()) return;
    builderDlg = new FD::TemplateBuilder(&scr, "Template builder for " + c->name, *c, form.bg, XStr(*c, "template")); focus = -1; drag = D_NONE; dirty = true;
}
static void FinishTemplateDialog(bool ok, const std::string& result) {
    if (ok && Sel() && templateProp >= 0 && ApplyProp(templateProp, result)) { RefreshGeometry(); SyncFields(); Status("template = %s", result.c_str()); }
    dirty = true;
}
static void ClosePresetDialog() { std::string r = presetDlg->tmpl; bool ok = presetDlg->state == FD::PresetDialog::OK; delete presetDlg; presetDlg = nullptr; drag = D_NONE; FinishTemplateDialog(ok, r); }
static void CloseBuilderDialog() { std::string r = builderDlg->tmpl; bool ok = builderDlg->state == FD::TemplateBuilder::OK; delete builderDlg; builderDlg = nullptr; drag = D_NONE; FinishTemplateDialog(ok, r); }
static void FitSel() {                                // Fit: the control is exactly as wide as its content
    DControl* c = Sel(); if (!c || !FD::Info(c->type).fitWidth) return;
    c->w = FD::FitWidth(&scr, *c); ClampPos(*c); RefreshGeometry(); SyncFields(); Status("%s: width %d fits its content", c->name.c_str(), c->w);
}

// ---- the file dialog (modal): Open, Save As, and the folder the code is generated into ----
static bool SaveForm(); static bool OpenForm(); static void ExportForm(const std::string& dir); static std::string StartupForm();
enum FilePurpose { FP_OPEN, FP_SAVE, FP_EXPORT };
static FD::FileDialog* fileDlg = nullptr; static int filePurpose = FP_OPEN;
static bool AnyModal() { return fileDlg || colorDlg || fontDlg || presetDlg || builderDlg || itemsDlg || menuDlg || zonesDlg || boxDlg || helpDlg || richDlg; }
static bool DirOk(const std::string& d) { std::error_code ec; return !d.empty() && std::filesystem::is_directory(d, ec); }
static void OpenFileDialog(int purpose) {
    if (AnyModal()) return;
    std::error_code ec; namespace fs = std::filesystem;
    std::string known = FileKnown() ? fs::absolute(FormPath(), ec).string() : "";
    std::string start;
    if (purpose == FP_EXPORT) start = form.outDir;
    else if (purpose == FP_OPEN)                         // always the dialog, where it was last looking (an application setting)
        start = DirOk(settings.lastOpenDir) ? settings.lastOpenDir : !known.empty() ? fs::path(known).parent_path().string() : WorkDir();
    else                                                 // Save: always the dialog too, on the form's file; a form without one starts in the last Save folder
        start = !known.empty() ? known : (fs::path(DirOk(settings.lastSaveDir) ? settings.lastSaveDir : WorkDir()) / (form.name + ".json")).string();
    const char* title = purpose == FP_OPEN ? "Open form" : purpose == FP_SAVE ? "Save form" : "Export code into folder";
    fileDlg = new FD::FileDialog(&scr, purpose == FP_OPEN ? FD::FileDialog::K_OPEN : purpose == FP_SAVE ? FD::FileDialog::K_SAVE : FD::FileDialog::K_FOLDER, title, start);
    if (purpose == FP_SAVE && !known.empty()) fileDlg->SetNoConfirmPath(known);       // saving over the form's own file needs no "Replace it?"
    filePurpose = purpose; focus = -1; drag = D_NONE; dirty = true;
}
static void CloseFileDialog() {
    if (!fileDlg) return;
    std::string path = fileDlg->path, lastDir = fileDlg->Dir(); bool ok = fileDlg->state == FD::FileDialog::OK;
    delete fileDlg; fileDlg = nullptr; drag = D_NONE; dirty = true;
    if (DirOk(lastDir) && filePurpose != FP_EXPORT) {    // remember where the dialog was looking (OK or Cancel)
        (filePurpose == FP_OPEN ? settings.lastOpenDir : settings.lastSaveDir) = lastDir;
        settings.Save();
    }
    if (!ok) return;
    std::string old = currentFile;
    auto remember = [&]() { std::error_code ec; settings.lastForm = std::filesystem::absolute(currentFile, ec).string(); settings.Save(); };     // reopened at the next start
    if (filePurpose == FP_OPEN) { currentFile = path; if (!OpenForm()) currentFile = old; else remember(); }
    else if (filePurpose == FP_SAVE) { currentFile = path; if (!SaveForm()) currentFile = old; else remember(); }      // a changed name becomes the form's file name
    else ExportForm(path);
}

// a segmented row (border style / alignment): a value equal to the control's own default is stored as "no override"
static void SetEnumProp(int p, int v) {
    if (p == P_KEEPHIDDEN) { form.keepHidden = v != 0; SyncFields(); return; }
    DControl* c = Sel(); if (!c) return;
    if (const FD::ExtraDef* d = ExtraOf(p)) ApplyExtra(p, d->kind == FD::PK_BOOL ? (v ? "true" : "false") : std::to_string(v));
    else if (p == P_VISIBLE) c->visible = v != 0;
    else if (p == P_ENABLED) c->enabled = v != 0;
    else if (p == P_BSTYLE) c->borderStyle = v == c->defStyle ? -1 : v;
    else if (p == P_ALIGN)  c->align = v == c->defAlign ? -1 : v;
    RefreshGeometry(); ClampPos(*c); SyncFields();
}

// ---- model operations ----
static void Place(int type, int fx, int fy) {        // fx, fy in form coordinates
    DControl c; c.type = type; c.name = form.UniqueName(FD::Info(type).prefix); c.text = FD::Info(type).defText;
    c.w = FD::Info(type).defW; c.x = Snap(fx); c.y = Snap(fy); c.tabOrder = form.NextTabOrder();          // the focus number is set once, here: later list moves do not change it
    if (type == FD::C_RADIO) { c.group = form.UniqueGroupName(); for (const DControl& o : form.items) if (o.type == FD::C_RADIO) { c.group = o.group; break; } }   // joins the form's first group
    if (type == FD::C_RICHTEXT) c.list = { "<center><color=yellow><big>Rich</big> text</color></center>", "<hr>", "Plain, <u>underlined</u>, <color=#0F0>green</color>." };
    if (FD::HasList(type)) for (int i = 1; i <= 3; ++i) c.list.push_back(std::string(type == FD::C_TABBAR ? "Tab " : "Item ") + std::to_string(i));      // a few sample items
    if (type == FD::C_MENU) {                                                    // a small sample menu; ids and names are filled in below
        FD::MenuNode sys; sys.text = "System"; FD::MenuNode a, b, about; a.text = "Reboot"; b.text = "Power off"; about.text = "About"; sys.kids = { a, b }; c.menu = { sys, about };
    }
    if (type == FD::C_GAUGE) {                                                   // a sample: green / yellow / red bands and a value, so a new gauge shows something
        c.zones = { { 0, 60, 0x07E0 }, { 60, 85, 0xFFE0 }, { 85, 100, 0xF800 } }; FD::ExtraSet(c, *FD::FindExtra(FD::C_GAUGE, "value"), "42");
    }
    FD::Measure(&scr, c);
    if (type == FD::C_INPUTBOX || type == FD::C_TIMEBOX || type == FD::C_DATEBOX) c.w = std::max(FD::MIN_CONTROL_W, FD::FitWidth(&scr, c));      // exactly as wide as its text
    FD::Measure(&scr, c); ClampPos(c);
    form.items.push_back(c); sel = (int)form.items.size() - 1; form.NormalizeMenu(sel); BuildFields();
    Status("added %s", c.name.c_str());
}
static void DeleteSel() {
    if (!Sel()) return;
    Status("deleted %s", Sel()->name.c_str());
    form.items.erase(form.items.begin() + sel); sel = -1; BuildFields();
}
static void Nudge(int dx, int dy) { if (DControl* c = Sel()) { c->x += dx; c->y += dy; ClampPos(*c); SyncFields(); } }
static int HitControl(int fx, int fy) {              // topmost control under a form coordinate
    for (int i = (int)form.items.size() - 1; i >= 0; --i) {
        const DControl& c = form.items[i];
        if (!InView(c)) continue;
        if (fx >= c.x && fx < c.x + c.w && fy >= c.y && fy < c.y + c.h) return i;
    }
    return -1;
}
static int HitHandle(int fx, int fy) {               // -1 left handle, +1 right handle, 0 none (selected control only)
    DControl* c = Sel(); if (!c) return 0;
    int my = c->y + c->h / 2;                           // the handles are 8 screen pixels, wherever the zoom is
    if (std::abs(Zc(fy) - Zc(my)) <= 7) { if (std::abs(Zc(fx) - Zc(c->x)) <= 6) return -1; if (std::abs(Zc(fx) - Zc(c->x + c->w)) <= 6) return 1; }
    return 0;
}
static bool SaveForm() {
    FILE* f = fopen(FormPath(), "wb");
    if (!f) { Status("cannot write %s", FormPath()); return false; }
    std::string j = form.ToJson(); fwrite(j.data(), 1, j.size(), f); fclose(f);
    Status("saved %s (%d controls)", FormPath(), (int)form.items.size());
    return true;
}
// Ctrl/Cmd+X (Export): writes <Form>.designer.h into the last successful save folder (the working folder the first time), then saves the form's JSON so
// that folder is remembered (outputPath).
static void ExportForm(const std::string& dir) {
    if (dir.empty()) FixOutDir();
    FD::GenResult r = FD::WriteDesigner(form, dir.empty() ? form.outDir : dir);
    if (!r.ok) { Status("not exported: %s", r.error.c_str()); return; }
    FD::UserResult u = FD::WriteUserFile(form, form.outDir);                    // <Form>.h: created once, never overwritten
    if (!SaveForm()) return;
    std::string note = !u.ok ? "  (" + u.error + ")" : u.created ? "  + created " + FD::UserFileName(form) : "  (" + FD::UserFileName(form) + " kept" + (u.backup.empty() ? "" : ", copy " + std::filesystem::path(u.backup).filename().string()) + ")";
    if (u.ok && !u.missing.empty()) { note += "; add to it: " + u.missing[0]; if (u.missing.size() > 1) note += " +" + std::to_string(u.missing.size() - 1) + " more"; }
    Status("%s %s%s", r.unchanged ? "up to date:" : "exported", r.path.c_str(), note.c_str());
}
static bool OpenForm() {
    FILE* f = fopen(FormPath(), "rb");
    if (!f) { Status("cannot open %s", FormPath()); return false; }
    std::string j; char b[4096]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) j.append(b, n); fclose(f);
    std::string err;
    if (!form.FromJson(j, err)) { Status("%s: %s", FormPath(), err.c_str()); return false; }
    std::string missing = form.outDir;
    bool moved = FixOutDir();
    sel = -1; scrollX = scrollY = 0; ClampScroll(); RefreshGeometry(); BuildFields();
    if (moved && !missing.empty()) Status("opened %s; output folder \"%s\" not found, using %s", FormPath(), missing.c_str(), form.outDir.c_str());
    else Status("opened %s (%d controls)", FormPath(), (int)form.items.size());
    return true;
}

// ---- drawing ----
static const uint16_t C_BG = 0x1082, C_PANEL = 0x18C3, C_LINE = 0x4A69, C_ACCENT = 0x07FF, C_SEL = 0x2B5F, C_YELLOW = 0xFFE0;
static void Frame(int x, int y, int w, int h, const char* title) {
    scr.fillRect(x, y, w, h, C_PANEL); scr.drawRect(x, y, w, h, C_LINE);
    scr.setFont(&lgfx::fonts::Font2); scr.setTextColor(C_ACCENT); scr.setTextDatum(lgfx::textdatum_t::top_left);
    scr.drawString(title, x + 8, y + 6);
}
static int MeasureText(const std::string& s) { scr.setFont(&lgfx::fonts::Font2); return scr.textWidth(s.c_str()); }
static void Reveal(int i) { fields[i].EnsureVisible(MeasureText, RowFieldW(fieldProp[i]) - 2 * FIELD_PAD); dirty = true; }

// ---- zoom controls: a real SuperSpinBox (drag, click the left / right third, wheel) with Fit and 1:1 buttons ----
static const int VIEW_X = 548, VIEW_W = 156;      // the design-view button: All -> each control group -> All ...
static const int ZSPIN_X = 770, ZSPIN_W = 96, ZFIT_X = 874, ZFIT_W = 50, ZONE_X = 930, ZONE_W = 50, ZROW_Y = 59;
static SuperSpinBox* zoomSpin = nullptr;
static int ZRowH() { return zoomSpin ? zoomSpin->GetHeight() : 24; }
static void SetZoom(int z, int ax, int ay) {            // (ax, ay): the screen point that stays where it is (e.g. the mouse)
    z = std::min(std::max(z, ZOOM_MIN), ZOOM_MAX);
    if (z != zoom) {
        double fx = (ax - FormOX()) * 100.0 / zoom, fy = (ay - FormOY()) * 100.0 / zoom;
        zoom = z;
        scrollX = VX + PAD - (ax - (int)std::lround(fx * zoom / 100.0));     // (only used when the zoomed form is larger than the viewport)
        scrollY = VY + PAD - (ay - (int)std::lround(fy * zoom / 100.0));
        ClampScroll();
    }
    if (zoomSpin) zoomSpin->SetValue(zoom);
    dirty = true;
}
static void ZoomCentre(int z) { SetZoom(z, VX + VW / 2, VY + VH / 2); }
static void ZoomFit() {                                 // the whole form inside the viewport (a small form is enlarged)
    int z = std::min((VW - 2 * PAD) * 100 / form.w, (VH - 2 * PAD) * 100 / form.h);
    ZoomCentre(z); scrollX = scrollY = 0; ClampScroll(); dirty = true;
}
static bool spinDrag = false, spinStarted = false; static int spinPX = 0, spinPY = 0;
static void SpinPump() {                                // run the spin box; the zoom follows its live value
    SuperEditState st = zoomSpin->Service();
    int v = st == EDIT_ACTIVE ? zoomSpin->GetLiveValue() : zoomSpin->GetValue();
    if (v != zoom) { int keep = zoom; (void)keep; ZoomCentre(v); }
}
static void SpinPointer(SuperPointerEvent::Kind k, int x, int y) {
    SuperPointerEvent p; p.Type = k; p.X = (int16_t)x; p.Y = (int16_t)y; p.StartX = (int16_t)spinPX; p.StartY = (int16_t)spinPY;
    zoomSpin->OnPointer(p); SpinPump();
}
static bool InRect(int x, int y, int rx, int ry, int rw, int rh) { return x >= rx && x < rx + rw && y >= ry && y < ry + rh; }

// ---- the form on the surface: drawn once at 1:1 by the real controls, then scaled pixel-exactly ----
static lgfx::LGFX_Sprite formSpr; static uint16_t formBgRaw = 0;
static void RenderFormSprite() {
    if (formSpr.width() != form.w || formSpr.height() != form.h) { formSpr.deleteSprite(); formSpr.setColorDepth(16); formSpr.createSprite(form.w, form.h); }
    formSpr.fillSprite(form.bg);
    formBgRaw = ((uint16_t*)formSpr.getBuffer())[0];                    // the background as stored in the buffer (grid dots go on this colour only)
    for (auto& c : form.items) if (InView(c)) FD::MakeControl(&formSpr, c, 0, 0)->Push();
}
static void BlitForm(int ox, int oy) {                                   // nearest-neighbour scale of the form sprite onto the viewport
    int x0 = std::max(ox, VX), y0 = std::max(oy, VY), x1 = std::min(ox + ZW(), VX + VW), y1 = std::min(oy + ZH(), VY + VH);
    if (x1 <= x0 || y1 <= y0) return;
    uint16_t* d = (uint16_t*)scr.getBuffer(); const uint16_t* sp = (const uint16_t*)formSpr.getBuffer();
    int dw = scr.width(), sw = formSpr.width();
    std::vector<int> cols(x1 - x0);
    for (int x = x0; x < x1; ++x) cols[x - x0] = std::min(sw - 1, (x - ox) * 100 / zoom);
    for (int y = y0; y < y1; ++y) {
        const uint16_t* sr = sp + (size_t)std::min(form.h - 1, (y - oy) * 100 / zoom) * sw; uint16_t* dr = d + (size_t)y * dw;
        for (int x = x0; x < x1; ++x) dr[x] = sr[cols[x - x0]];
    }
}

static void Render() {
    RefreshGeometry();
    scr.fillSprite(C_BG);
    scr.setFont(&lgfx::fonts::Font2); scr.setTextDatum(lgfx::textdatum_t::top_left);
    scr.fillRect(0, 0, FD::WIN_W, 44, C_PANEL); scr.drawFastHLine(0, 44, FD::WIN_W, C_LINE);
    scr.setTextColor(0xFFFF); scr.drawString("SuperGUI Form Designer", 10, 14);
    { std::string fn = FileKnown() ? std::filesystem::path(FormPath()).filename().string() : "(not saved yet)"; scr.setTextColor(C_ACCENT); scr.drawString(fn.c_str(), 190, 14); }
    if (!status.empty() && SDL_GetTicks() - statusAt < 6000) { scr.setTextColor(C_YELLOW); scr.drawString(status.c_str(), 380, 14); }
    else scr.setTextColor(0x8410), scr.drawString("Ctrl/Cmd+S save  O open  X export   G grid  + - 0 F zoom  Del remove", 380, 14);

    // toolbox
    Frame(10, 56, 290, 754, "Toolbox");
    for (int i = 0; i < FD::C_COUNT; ++i) {
        int y = TOOL_Y0 + i * TOOL_H;
        if (tool == i) scr.fillRect(18, y, 274, TOOL_H - 4, C_SEL);
        scr.drawRect(18, y, 274, TOOL_H - 4, tool == i ? C_ACCENT : C_LINE);
        scr.setTextColor(0xFFFF); scr.drawString(FD::Info(i).title, 28, y + 5);
        scr.setTextColor(0x8410); scr.drawString(FD::Info(i).key, 150, y + 5);
    }
    scr.setTextColor(C_ACCENT); scr.drawString("Controls on form", 18, TREE_Y0 - 50);
    scr.setTextColor(0x8410); scr.drawString("(# = tab order)", 150, TREE_Y0 - 50);
    { static const char* ol[4] = { "Up", "Down", "Front", "Back" };
      for (int k = 0; k < 4; ++k) { SuperPushButton b(&scr, OrdBtnX(k), OrdBtnY(), ORD_BTN_W, &lgfx::fonts::Font2, ol[k]); if (sel < 0) b.SetEnabled(false); b.Push(); } }
    ClampTree();
    for (int i = treeFirst; i < (int)form.items.size() && i < treeFirst + TreeRows(); ++i) {
        int y = TREE_Y0 + (i - treeFirst) * TREE_H;
        if (i == sel) scr.fillRect(18, y, 274, TREE_H - 2, C_SEL);
        scr.setClipRect(18, y, 264, TREE_H);
        if (FD::Focusable(form.items[i])) { scr.setTextColor(C_ACCENT); scr.drawString(std::to_string(form.EffTab((size_t)i)).c_str(), 22, y + 2); }          // the focus (tab) number
        scr.setTextColor(InView(form.items[i]) ? 0xFFFF : 0x4A69); scr.drawString(form.items[i].name.c_str(), 46, y + 2);        // (dim: on another page than the one in view)
        if (!form.items[i].ctlGroup.empty()) { scr.setTextColor(0x8410); scr.drawString(form.items[i].ctlGroup.c_str(), 116, y + 2); }
        scr.setTextColor(0x8410); scr.drawString(FD::Info(form.items[i].type).title, form.items[i].ctlGroup.empty() ? 190 : 214, y + 2);
        scr.clearClipRect();
    }

    // design surface
    Frame(310, 56, 680, 754, "Design surface");
    char b[160];
    snprintf(b, sizeof b, "%d x %d  grid %d", form.w, form.h, grid);
    scr.setTextColor(0x8410); scr.drawString(b, 430, 62);
    if (!viewGroup.empty() && !form.IsGroupName(viewGroup)) viewGroup.clear();                // the view group disappeared (no control uses it any more)
    { std::string vl = "View: " + (viewGroup.empty() ? std::string("All") : viewGroup); if (vl.size() > 17) vl = vl.substr(0, 16) + "~";
      SuperPushButton vb(&scr, VIEW_X, ZROW_Y, VIEW_W, &lgfx::fonts::Font2, vl); if (!viewGroup.empty()) vb.ColBorder = C_YELLOW; vb.Push(); }
    scr.setTextColor(0xFFFF); scr.drawString("Zoom", ZSPIN_X - 50, 62);
    zoomSpin->Push();
    { SuperPushButton fit(&scr, ZFIT_X, ZROW_Y, ZFIT_W, &lgfx::fonts::Font2, "Fit"), one(&scr, ZONE_X, ZROW_Y, ZONE_W, &lgfx::fonts::Font2, "1:1"); fit.Push(); one.Push(); }
    scr.setClipRect(VX, VY, VW, VH);
    scr.fillRect(VX, VY, VW, VH, 0x2124);
    int ox = FormOX(), oy = FormOY();
    RenderFormSprite(); BlitForm(ox, oy);
    scr.drawRect(ox - 1, oy - 1, ZW() + 2, ZH() + 2, C_YELLOW);
    if (grid * zoom >= 400) {                                            // grid dots on the empty form background only (never over a control)
        const uint16_t* dp = (const uint16_t*)scr.getBuffer(); int dw = scr.width();
        for (int gy = 0; gy < form.h; gy += grid) {
            int py = oy + Zc(gy); if (py < VY || py >= VY + VH) continue;
            for (int gx = 0; gx < form.w; gx += grid) { int px = ox + Zc(gx); if (px >= VX && px < VX + VW && dp[(size_t)py * dw + px] == formBgRaw) scr.drawPixel(px, py, 0x2945); }
        }
    }
    for (const DControl& c : form.items) if (!c.visible && InView(c))                    // initially hidden: faded, with an H in the corner
        { int x0 = ox + Zc(c.x), y0 = oy + Zc(c.y), w = Zc(c.x + c.w) - Zc(c.x), h = Zc(c.y + c.h) - Zc(c.y); scr.fillRectAlpha(x0, y0, w, h, 160, form.bg); scr.setTextColor(C_YELLOW); scr.setFont(&lgfx::fonts::Font2); scr.setTextDatum(lgfx::textdatum_t::top_left); scr.drawString("H", x0 + 2, y0 + 1); }
    if (DControl* c = Sel()) {                                       // selection box + the two resize handles
        int x0 = ox + Zc(c->x), x1 = ox + Zc(c->x + c->w), y0 = oy + Zc(c->y), y1 = oy + Zc(c->y + c->h), my = oy + Zc(c->y + c->h / 2);
        scr.drawRect(x0 - 1, y0 - 1, x1 - x0 + 2, y1 - y0 + 2, C_ACCENT);
        scr.fillRect(x0 - 4, my - 4, 8, 8, C_ACCENT); scr.fillRect(x1 - 4, my - 4, 8, 8, C_ACCENT);
    }
    int mx = ToFX(Input::MouseX()), my2 = ToFY(Input::MouseY());
    if (tool >= 0 && mx >= 0 && my2 >= 0 && mx < form.w && my2 < form.h) {   // where a click would place the new control
        int px = ox + Zc(Snap(mx)), py = oy + Zc(Snap(my2));
        scr.drawFastHLine(px - 6, py, 13, C_ACCENT); scr.drawFastVLine(px, py - 6, 13, C_ACCENT);
        scr.setTextColor(C_ACCENT); scr.drawString(FD::Info(tool).title, px + 8, py + 4);
    }
    scr.clearClipRect();
    SyncBars(); surfV.Draw(scr, 0x18C3, 0x4A69, C_ACCENT); surfH.Draw(scr, 0x18C3, 0x4A69, C_ACCENT);        // the surface's bars (only when the zoomed form is larger than the viewport)

    // property panel
    Frame(1000, 56, 390, 754, Sel() ? ("Properties: " + Sel()->name).c_str() : ("Properties: form " + form.name).c_str());
    for (int i = 0; i < nFields; ++i) {
        int fy = FieldY(i);
        if (fy + FIELD_H <= PANEL_TOP || fy >= PANEL_BOT) continue;                          // scrolled out
        scr.setClipRect(1002, PANEL_TOP, 386, PANEL_BOT - PANEL_TOP);                        // (DrawField clears the clip, so set it per row)
        scr.setFont(&lgfx::fonts::Font2); scr.setTextDatum(lgfx::textdatum_t::top_left);
        scr.setTextColor(0xFFFF); scr.drawString(PropLabel(fieldProp[i]).c_str(), 1010, fy + 6);
        int kind = RowKindOf(fieldProp[i]);
        if (kind == RK_SEG) {                                                                // segments: click one
            int n = SegCount(fieldProp[i]), cur = SegCurrent(fieldProp[i]), segw = FIELD_W / n;
            scr.setTextDatum(lgfx::textdatum_t::middle_center);
            for (int k = 0; k < n; ++k) {
                int sx = FIELD_X + k * segw;
                scr.fillRect(sx, fy, segw, FIELD_H, k == cur ? C_SEL : 0x0000); scr.drawRect(sx, fy, segw, FIELD_H, k == cur ? C_ACCENT : 0x7BEF);
                scr.setTextColor(k == cur ? 0xFFFF : 0x8410); scr.drawString(SegLabel(fieldProp[i], k).c_str(), sx + segw / 2, fy + FIELD_H / 2);
            }
            scr.setTextDatum(lgfx::textdatum_t::top_left);
        } else if (kind == RK_FONT) {                                                        // the font name: click opens the font dialog
            std::string id = GetFontProp(fieldProp[i]); const FD::ExtraDef* xd = ExtraOf(fieldProp[i]);
            bool optional = xd != nullptr;                                                   // an extra font ("" = same as the control) can be cleared with the x
            scr.fillRect(FIELD_X, fy, FIELD_W, FIELD_H, 0x0000); scr.drawRect(FIELD_X, fy, FIELD_W, FIELD_H, 0x7BEF);
            scr.setTextColor(id.empty() ? 0x8410 : 0xFFFF); scr.setFont(&lgfx::fonts::Font2);
            scr.drawString(id.empty() ? "(same as the control)" : FD::FindFont(id).label, FIELD_X + 6, fy + 6);
            if (optional && !id.empty()) { scr.setTextColor(0xF800); scr.drawString("x", FIELD_X + FIELD_W - 16, fy + 6); }
            else if (!optional) { scr.setTextColor(0x8410); scr.drawString("click to change", FIELD_X + FIELD_W - 100, fy + 6); }
        } else if (kind == RK_ITEMS) {                                                       // a summary of the list; the Edit button (or a click) opens the items dialog
            const DControl* ic = Sel(); std::string sm;
            if (fieldProp[i] == P_RICH) { sm = std::to_string(ic->list.size()) + (ic->list.size() == 1 ? " line" : " lines"); for (size_t k = 0; k < ic->list.size() && sm.size() < 60; ++k) sm += (k ? " / " : ": ") + ic->list[k]; }
            else if (fieldProp[i] == P_MSGBOXES) { sm = std::to_string(form.boxes.size()) + (form.boxes.size() == 1 ? " box" : " boxes"); for (size_t k = 0; k < form.boxes.size() && sm.size() < 60; ++k) sm += (k ? ", " : ": ") + form.boxes[k].name; }
            else if (fieldProp[i] == P_ZONES) { sm = std::to_string(ic->zones.size()) + (ic->zones.size() == 1 ? " zone" : " zones"); for (size_t k = 0; k < ic->zones.size() && sm.size() < 60; ++k) sm += (k ? ", " : ": ") + FD::FloatText(ic->zones[k].from) + "-" + FD::FloatText(ic->zones[k].to); }
            else if (ic->type == FD::C_MENU) { int cmds = 0; FD::MenuForEach(ic->menu, [&](const FD::MenuNode& k) { if (!k.itemId.empty()) ++cmds; }); sm = std::to_string(FD::MenuNodeCount(ic->menu)) + " items, " + std::to_string(cmds) + " ids: ";
                                          for (size_t k = 0; k < ic->menu.size() && sm.size() < 60; ++k) sm += (k ? ", " : "") + ic->menu[k].text; }
            else { sm = std::to_string(ic->list.size()) + (ic->type == FD::C_TABBAR ? " tabs: " : " items: ");
                   for (size_t k = 0; k < ic->list.size() && sm.size() < 60; ++k) sm += (k ? ", " : "") + ic->list[k]; }
            scr.fillRect(FIELD_X, fy, FIELD_W - 80, FIELD_H, 0x0000); scr.drawRect(FIELD_X, fy, FIELD_W - 80, FIELD_H, 0x7BEF);
            scr.setClipRect(FIELD_X + 2, fy + 2, FIELD_W - 84, FIELD_H - 4); scr.setTextColor(0xC618); scr.drawString(sm.c_str(), FIELD_X + 6, fy + 6);
            scr.setClipRect(1002, PANEL_TOP, 386, PANEL_BOT - PANEL_TOP);
            SuperPushButton eb(&scr, FIELD_X + FIELD_W - 74, fy, 74, &lgfx::fonts::Font2, "Edit..."); eb.Push();
        } else if (kind == RK_HINT) {                                                        // the template legend
            scr.setTextColor(0x8410); scr.drawString("# digit  A upper  a lower  B letter", 1010, fy - 2); scr.drawString("* any   other characters are literals", 1010, fy + 14);
        } else if (kind == RK_COLOR) {                                                       // a swatch: click opens the colour dialog
            uint16_t col = 0; GetColorProp(fieldProp[i], col);
            scr.fillRect(FIELD_X, fy, 70, FIELD_H, col); scr.drawRect(FIELD_X, fy, 70, FIELD_H, 0xFFFF);
            snprintf(b, sizeof b, "0x%04X   (click to change)", col);
            scr.setTextColor(0xC618); scr.drawString(b, FIELD_X + 80, fy + 6);
        } else {
            const FD::ExtraDef* xd = ExtraOf(fieldProp[i]);
            bool bad = focus == i && (fieldProp[i] == P_NAME || fieldProp[i] == P_FNAME || fieldProp[i] == P_GROUP || fieldProp[i] == P_CGROUP || (xd && xd->kind == FD::PK_TEXT)) && fields[i].text != PropValue(fieldProp[i]);
            FD::DrawField(scr, fields[i], FIELD_X, fy, RowFieldW(fieldProp[i]), FIELD_H, focus == i, bad, SDL_GetTicks());
        }
        scr.setClipRect(1002, PANEL_TOP, 386, PANEL_BOT - PANEL_TOP);
        { RowBtn rb[2]; int nb = RowButtons(fieldProp[i], rb); for (int k = 0; k < nb; ++k) { SuperPushButton btn(&scr, rb[k].x, fy, rb[k].w, &lgfx::fonts::Font2, rb[k].label); btn.Push(); } }
        scr.clearClipRect();
    }
    scr.setClipRect(1002, PANEL_TOP, 386, PANEL_BOT - PANEL_TOP);
    scr.setTextDatum(lgfx::textdatum_t::top_left); scr.setFont(&lgfx::fonts::Font2); scr.setTextColor(0x8410);
    { int iy = FieldY(nFields) + 6;
      if (Sel()) { snprintf(b, sizeof b, FD::FindExtra(Sel()->type, "bodyHeight") ? "Height %d (the body height + the frame)" : "Height %d (follows the font)", Sel()->h); scr.drawString(b, 1010, iy); }
      else scr.drawString("Select a control to edit it.", 1010, iy);
      if (focus >= 0) scr.drawString("Enter / Esc: done    Tab: next field", 1010, iy + 24);
      if (!rejectMsg.empty()) {                                                          // why the last value was refused (red, wrapped)
          scr.setTextColor(0xF800); std::string m = rejectMsg; int yy = iy + 48;
          while (!m.empty() && yy < PANEL_BOT) { size_t cut = m.size() > 48 ? m.rfind(' ', 48) : std::string::npos; if (m.size() <= 48) cut = m.size(); else if (cut == std::string::npos) cut = 48;
              scr.drawString(m.substr(0, cut).c_str(), 1010, yy); m = m.size() > cut ? m.substr(cut + (m[cut] == ' ' ? 1 : 0)) : ""; yy += 18; }
      } }
    scr.clearClipRect();
    SyncBars(); treeBar.Draw(scr, 0x2124, 0x4A69, C_ACCENT); propBar.Draw(scr, 0x2124, 0x4A69, C_ACCENT);                                   // the property panel's bar
    if (colorDlg) colorDlg->Draw(SDL_GetTicks());
    if (fontDlg) fontDlg->Draw();
    if (fileDlg) fileDlg->Draw(SDL_GetTicks());
    if (presetDlg) presetDlg->Draw();
    if (builderDlg) builderDlg->Draw();
    if (itemsDlg) itemsDlg->Draw(SDL_GetTicks());
    if (menuDlg) menuDlg->Draw(SDL_GetTicks());
    if (zonesDlg) zonesDlg->Draw(SDL_GetTicks());
    if (boxDlg) boxDlg->Draw(SDL_GetTicks());
    if (helpDlg) helpDlg->Draw(SDL_GetTicks());
    if (richDlg) richDlg->Draw(SDL_GetTicks());
    scr.pushSprite(lcd, 0, 0);
    dirty = false;
}

// ---- input ----
static void Handle(const InputEvent& e) {
    if (richDlg) { richDlg->Event(e); dirty = true; if (richDlg->state != FD::RichTextEditor::OPEN) CloseRichDialog(); return; }
    if (helpDlg) { helpDlg->Event(e); dirty = true; if (!helpDlg->open) { delete helpDlg; helpDlg = nullptr; } return; }
    if (boxDlg) { boxDlg->Event(e); dirty = true; if (boxDlg->state != FD::MessageBoxEditor::OPEN) CloseBoxDialog(); return; }
    if (zonesDlg) { zonesDlg->Event(e); dirty = true; if (zonesDlg->state != FD::ZonesDialog::OPEN) CloseZonesDialog(); return; }
    if (menuDlg) { menuDlg->Event(e); dirty = true; if (menuDlg->state != FD::MenuEditor::OPEN) CloseMenuDialog(); return; }
    if (itemsDlg) { itemsDlg->Event(e); dirty = true; if (itemsDlg->state != FD::ItemsDialog::OPEN) CloseItemsDialog(); return; }
    if (presetDlg) { presetDlg->Event(e); dirty = true; if (presetDlg->state != FD::PresetDialog::OPEN) ClosePresetDialog(); return; }
    if (builderDlg) { builderDlg->Event(e); dirty = true; if (builderDlg->state != FD::TemplateBuilder::OPEN) CloseBuilderDialog(); return; }
    if (fileDlg) {
        fileDlg->Event(e); dirty = true;
        if (fileDlg->state != FD::FileDialog::OPEN) CloseFileDialog();
        return;
    }
    if (fontDlg) {
        fontDlg->Event(e); dirty = true;
        if (fontDlg->state != FD::FontDialog::OPEN) CloseFontDialog();
        return;
    }
    if (colorDlg) {                                      // a modal dialog gets every event
        colorDlg->Event(e); dirty = true;
        if (colorDlg->state != FD::ColorDialog::OPEN) CloseColorDialog();
        return;
    }
    switch (e.type) {
    case InputEvent::MOVE: {
        dirty = true;
        if (barDrag) { if (barDrag->Move(e.x, e.y)) { if (barDrag == &treeBar) treeFirst = barDrag->Pos(); else if (barDrag == &propBar) propScroll = barDrag->Pos(); else if (barDrag == &surfV) scrollY = barDrag->Pos(); else scrollX = barDrag->Pos(); ClampPropScroll(); ClampScroll(); } break; }
        if (spinDrag) { SpinPointer(spinStarted ? SuperPointerEvent::DRAG_MOVE : SuperPointerEvent::DRAG_START, e.x, e.y); spinStarted = true; break; }
        if (drag == D_TEXT && focus >= 0) { fields[focus].SetCaretFromX(e.x - FIELD_X - FIELD_PAD, true, MeasureText); Reveal(focus); break; }
        DControl* c = Sel(); if (!c || drag == D_NONE) break;
        int fx = ToFX(e.x), fy = ToFY(e.y);
        if (drag == D_MOVE) { c->x = Snap(fx - dragDX); c->y = Snap(fy - dragDY); }
        else if (drag == D_RESIZE_R) { c->w = Snap(fx) - c->x; }
        else if (drag == D_RESIZE_L) { int nx = std::min(Snap(fx), dragRight - FD::MIN_CONTROL_W); c->x = nx; c->w = dragRight - nx; }
        ClampPos(*c);
        if (drag == D_RESIZE_L) c->x = std::max(0, dragRight - c->w);        // the right edge stays put
        SyncFields();
        break; }
    case InputEvent::DOWN: {
        if (e.button != SDL_BUTTON_LEFT) break;
        if (InRect(e.x, e.y, VIEW_X, ZROW_Y, VIEW_W, ZRowH())) {              // the design view: All -> each control group in turn -> All
            SetFocus(-1); std::vector<std::string> gs = form.UsedGroups(); size_t at = 0;
            for (size_t k = 0; k < gs.size(); ++k) if (gs[k] == viewGroup) at = k + 1;
            viewGroup = at < gs.size() ? gs[at] : ""; if (sel >= 0 && sel < (int)form.items.size() && !InView(form.items[sel])) { sel = -1; BuildFields(); } dirty = true; break;
        }
        if (InRect(e.x, e.y, ZSPIN_X, ZROW_Y, ZSPIN_W, ZRowH())) {          // the zoom spin box: click the left / right third, or drag up / down
            SetFocus(-1); spinPX = e.x; spinPY = e.y; spinDrag = true; spinStarted = false;
            zoomSpin->StartEdit(); SpinPointer(SuperPointerEvent::PRESS, e.x, e.y); break;
        }
        if (InRect(e.x, e.y, ZFIT_X, ZROW_Y, ZFIT_W, ZRowH())) { SetFocus(-1); ZoomFit(); break; }
        if (InRect(e.x, e.y, ZONE_X, ZROW_Y, ZONE_W, ZRowH())) { SetFocus(-1); ZoomCentre(100); break; }
        int fi = FieldAt(e.x, e.y);
        if (fi >= 0) {                                                       // a button at the end of a row (Fit, Presets, Build)
            RowBtn rb[2]; int nb = RowButtons(fieldProp[fi], rb); bool hit = false;
            for (int k = 0; k < nb && !hit; ++k) if (e.x >= rb[k].x && e.x < rb[k].x + rb[k].w) {
                std::string l = rb[k].label; SetFocus(-1); hit = true;
                if (l == "Fit") FitSel(); else if (l == "Presets") OpenPresetDialog(); else OpenBuilderDialog();
            }
            if (hit) break;
        }
        if (fi >= 0 && RowKindOf(fieldProp[fi]) == RK_ITEMS) { SetFocus(-1); OpenItemsDialog(); break; }
        if (fi >= 0 && RowKindOf(fieldProp[fi]) == RK_SEG) { int n = SegCount(fieldProp[fi]); SetFocus(-1); SetEnumProp(fieldProp[fi], std::min(n - 1, (e.x - FIELD_X) * n / FIELD_W)); break; }
        if (fi >= 0 && RowKindOf(fieldProp[fi]) == RK_FONT) {
            SetFocus(-1);
            if (IsExtra(fieldProp[fi]) && !GetFontProp(fieldProp[fi]).empty() && e.x >= FIELD_X + FIELD_W - 26) { SetFontProp(fieldProp[fi], ""); RefreshGeometry(); SyncFields(); break; }    // the x: back to "same as the control"
            OpenFontDialog(fieldProp[fi]); break;
        }
        if (fi >= 0 && IsColorProp(fieldProp[fi])) { SetFocus(-1); OpenColorDialog(fieldProp[fi]); break; }
        if (fi >= 0) {                                                       // a property field: caret / selection
            SetFocus(fi); drag = D_TEXT;
            FD::TextField& f = fields[fi];
            f.SetCaretFromX(e.x - FIELD_X - FIELD_PAD, (SDL_GetModState() & KMOD_SHIFT) != 0, MeasureText);
            if (e.clicks == 2) f.SelectWord(f.caret); else if (e.clicks >= 3) f.SelectAll();
            Reveal(fi); break;
        }
        SetFocus(-1);
        for (int k = 0; k < 4; ++k) if (e.x >= OrdBtnX(k) && e.x < OrdBtnX(k) + ORD_BTN_W && e.y >= OrdBtnY() && e.y < OrdBtnY() + 26) { MoveSel(k); return; }       // the order buttons
        SyncBars();
        for (FD::ScrollBar* b : { &treeBar, &propBar, &surfV, &surfH }) if (b->Down(e.x, e.y)) {                // a scroll bar: grab the thumb or page toward the click
            barDrag = b->Dragging() ? b : nullptr;
            if (b == &treeBar) treeFirst = b->Pos(); else if (b == &propBar) propScroll = b->Pos(); else if (b == &surfV) scrollY = b->Pos(); else scrollX = b->Pos();
            ClampPropScroll(); ClampScroll(); dirty = true; return;
        }
        if (e.x >= 18 && e.x < 292 && e.y >= TOOL_Y0 && e.y < TOOL_Y0 + FD::C_COUNT * TOOL_H) {   // toolbox
            int t = (e.y - TOOL_Y0) / TOOL_H; tool = tool == t ? -1 : t; dirty = true; break;
        }
        if (e.x >= 18 && e.x < 292 && e.y >= TREE_Y0 && e.y < TREE_Y0 + TreeRows() * TREE_H) {    // controls-on-form list (only the rows that are drawn)
            int i = (e.y - TREE_Y0) / TREE_H + treeFirst; if (i < (int)form.items.size()) Select(i); break;
        }
        if (e.x >= VX && e.x < VX + VW && e.y >= VY && e.y < VY + VH) {                           // design surface
            int fx = ToFX(e.x), fy = ToFY(e.y);
            bool inForm = fx >= 0 && fy >= 0 && fx < form.w && fy < form.h;
            if (tool >= 0) { if (inForm) { Place(tool, fx, fy); tool = -1; } break; }
            if (int hnd = HitHandle(fx, fy)) { drag = hnd < 0 ? D_RESIZE_L : D_RESIZE_R; dragRight = Sel()->x + Sel()->w; break; }
            int i = HitControl(fx, fy);
            Select(i);
            if (i >= 0) { drag = D_MOVE; dragDX = fx - form.items[i].x; dragDY = fy - form.items[i].y; }
        }
        break; }
    case InputEvent::UP:
        if (spinDrag) { SpinPointer(spinStarted ? SuperPointerEvent::DRAG_END : SuperPointerEvent::TAP, e.x, e.y); SpinPump(); spinDrag = false; }
        if (barDrag) { barDrag->Up(); barDrag = nullptr; }
        drag = D_NONE; break;
    case InputEvent::WHEEL:
        if (e.x >= 18 && e.x < 292 && e.y >= TREE_Y0 - 22 && e.y < TREE_BOT) { treeFirst -= e.wheel; ClampTree(); dirty = true; break; }      // the controls-on-form list scrolls
        if (e.x >= 1000 && e.x < 1390 && e.y >= 56 && e.y < 810) { propScroll -= e.wheel * FIELD_GAP * 2; ClampPropScroll(); dirty = true; break; }      // the property panel scrolls
        if (InRect(e.x, e.y, ZSPIN_X, ZROW_Y, ZSPIN_W, ZRowH())) { ZoomCentre(zoom + (e.wheel > 0 ? ZOOM_STEP : -ZOOM_STEP)); break; }
        if (e.x >= VX && e.x < VX + VW && e.y >= VY && e.y < VY + VH) {
            if (SDL_GetModState() & (KMOD_CTRL | KMOD_GUI)) SetZoom(zoom + (e.wheel > 0 ? ZOOM_STEP : -ZOOM_STEP) * std::abs(e.wheel), e.x, e.y);   // Ctrl / Cmd + wheel: zoom around the mouse
            else { if (SDL_GetModState() & KMOD_SHIFT) scrollX -= e.wheel * 40; else scrollY -= e.wheel * 40; ClampScroll(); dirty = true; }
        }
        break;
    case InputEvent::KEY_DOWN: {
        bool cmd = e.mod & (KMOD_CTRL | KMOD_GUI), shift = e.mod & KMOD_SHIFT;
        if (focus >= 0) {
            if (fields[focus].Key(e)) { FieldEdited(focus); Reveal(focus); break; }
            if (e.key == SDLK_TAB) { int n = focus; do n = (n + (shift ? nFields - 1 : 1)) % nFields; while (IsPickRow(fieldProp[n]) && n != focus); SetFocus(n); fields[n].SelectAll(); Reveal(n); break; }
            if (e.key == SDLK_RETURN || e.key == SDLK_KP_ENTER || e.key == SDLK_ESCAPE) { SetFocus(-1); break; }
            break;
        }
        if (!cmd && (e.key == SDLK_QUESTION || e.key == SDLK_F1 || (e.key == SDLK_SLASH && shift))) { if (!AnyModal()) { helpDlg = new FD::HelpDialog(&scr, HELP_SOURCE_TOGGLE); drag = D_NONE; dirty = true; } break; }       // ? = the keyboard help
        if (cmd && e.key == SDLK_s) { OpenFileDialog(FP_SAVE); break; }          // Save always shows the dialog (a new name becomes the form's file)
        if (cmd && e.key == SDLK_o) { OpenFileDialog(FP_OPEN); break; }          // Open always shows the dialog, where it was last looking
        if (cmd && e.key == SDLK_x) { OpenFileDialog(FP_EXPORT); break; }   // Export: always asks for the folder (starting at the one saved in the form), then writes the code
        if (cmd) break;                                                          // no other Ctrl / Cmd shortcuts (so Ctrl+G is not the grid key)
        int step = shift ? grid : 1;
        switch (e.key) {
        case SDLK_ESCAPE: if (tool >= 0) tool = -1; else Select(-1); dirty = true; break;
        case SDLK_DELETE: case SDLK_BACKSPACE: DeleteSel(); break;
        case SDLK_LEFT:  Nudge(-step, 0); break;
        case SDLK_RIGHT: Nudge(step, 0); break;
        case SDLK_UP:    Nudge(0, -step); break;
        case SDLK_DOWN:  Nudge(0, step); break;
        case SDLK_PLUS: case SDLK_EQUALS: case SDLK_KP_PLUS: ZoomCentre(zoom + ZOOM_STEP); break;
        case SDLK_MINUS: case SDLK_KP_MINUS: ZoomCentre(zoom - ZOOM_STEP); break;
        case SDLK_0:     ZoomCentre(100); break;
        case SDLK_f:     ZoomFit(); break;
        case SDLK_g:     grid = grid == 1 ? 4 : grid == 4 ? 8 : grid == 8 ? 16 : 1; Status("grid %d", grid); break;
        case SDLK_TAB:   if (!form.items.empty()) { int n = (int)form.items.size(); Select(sel < 0 ? (shift ? n - 1 : 0) : (sel + (shift ? n - 1 : 1)) % n); } break;
        default: break;
        }
        break; }
    case InputEvent::TEXT: if (focus >= 0) { fields[focus].Insert(e.text); FieldEdited(focus); Reveal(focus); } break;
    case InputEvent::RESIZE: winW = e.w; winH = e.h; dirty = true; break;
    default: break;
    }
}

// ---- synthetic-event self test (FD_SELFTEST=1) ----
static bool ToJsonHas(const FD::DForm& f, const char* what) { return f.ToJson().find(what) != std::string::npos; }
static int fails = 0;
static const char* lastCheck = "(none)";
static void CrashHandler(int sig) {                    // self-test only: report the last check and leave quietly (no macOS crash dialog)
    char b[300]; int n = snprintf(b, sizeof b, "\nSELFTEST CRASHED (signal %d) after the check: %s\n", sig, lastCheck); if (n > 0) {
#ifdef _WIN32
        int r = _write(2, b, (unsigned)n); (void)r;
#else
        ssize_t r = write(2, b, (size_t)n); (void)r;
#endif
    }
    _exit(139);
}
static void Check(bool ok, const char* what) { lastCheck = what; printf("%s  %s\n", ok ? "ok  " : "FAIL", what); fflush(stdout); if (!ok) ++fails; }       // (flushed: a crash shows the last check)
static bool Next(InputEvent& e) { return Input::Poll(e); }
static void SelfTest() {
    signal(SIGSEGV, CrashHandler); signal(SIGBUS, CrashHandler); signal(SIGABRT, CrashHandler); signal(SIGILL, CrashHandler); signal(SIGFPE, CrashHandler);
    SDL_Window* w = nullptr;
    for (int i = 0; i < 100 && !(w = Input::Window()); ++i) SDL_Delay(50);   // the window is created on the main thread
    Check(w != nullptr, "SDL window found");
    if (!w) exit(1);
    InputEvent e;
    while (Next(e)) {}                                        // drop events from start-up
    auto motion = [&](int x, int y) { SDL_Event s{}; s.type = SDL_MOUSEMOTION; s.motion.windowID = Input::WindowId(); s.motion.x = x; s.motion.y = y; SDL_PushEvent(&s); };
    int ww, wh; SDL_GetWindowSize(w, &ww, &wh);
    motion(ww / 2, wh / 2);
    Check(Next(e) && e.type == InputEvent::MOVE && std::abs(e.x - FD::WIN_W / 2) <= 1 && std::abs(e.y - FD::WIN_H / 2) <= 1, "mouse at window centre -> logical centre");
    motion(0, 0);
    Check(Next(e) && e.x == 0 && e.y == 0, "mouse at the top-left corner -> 0,0");
    // The panel scales the frame in 0.5 steps and centres it (no stretching); the mapping is checked as a pure function.
    int lx, ly;
    Input::Scale(700, 410, 1400, 820, 1400, 820, FD::WIN_W, FD::WIN_H, lx, ly);
    Check(lx == 700 && ly == 410, "window = logical size: identity");
    Input::Scale(1000, 500, 1400, 820, 2800, 1640, FD::WIN_W, FD::WIN_H, lx, ly);
    Check(lx == 1000 && ly == 500, "hi-dpi (2x output, same point size): points still equal logical pixels");
    // maximized 1920x1080 window: scale = (2*1920/1400)/2 = 1.0 (integer maths), image 1400x820 centred, bars 260 / 130
    Input::Scale(260, 130, 1920, 1080, 1920, 1080, FD::WIN_W, FD::WIN_H, lx, ly);
    Check(lx == 0 && ly == 0, "maximized: the image's top-left corner (after the centring bars) is logical 0,0");
    Input::Scale(960, 540, 1920, 1080, 1920, 1080, FD::WIN_W, FD::WIN_H, lx, ly);
    Check(lx == 700 && ly == 410, "maximized: the window centre is the logical centre");
    Input::Scale(10, 10, 1920, 1080, 1920, 1080, FD::WIN_W, FD::WIN_H, lx, ly);
    Check(lx < 0 && ly < 0, "maximized: the bars are outside the logical frame");
    Input::Scale(5, 5, 0, 0, 0, 0, FD::WIN_W, FD::WIN_H, lx, ly);
    Check(true, "a zero-size window does not crash");
    SDL_Event b{}; b.type = SDL_MOUSEBUTTONDOWN; b.button.windowID = Input::WindowId(); b.button.button = SDL_BUTTON_RIGHT; b.button.clicks = 2; b.button.x = ww / 2; b.button.y = wh / 2; SDL_PushEvent(&b);
    Check(Next(e) && e.type == InputEvent::DOWN && e.button == SDL_BUTTON_RIGHT && e.clicks == 2 && Input::ButtonHeld(SDL_BUTTON_RIGHT), "right button down (double click count, held state)");
    b.type = SDL_MOUSEBUTTONUP; SDL_PushEvent(&b);
    Check(Next(e) && e.type == InputEvent::UP && !Input::ButtonHeld(SDL_BUTTON_RIGHT), "right button up clears the held state");
    SDL_Event wh2{}; wh2.type = SDL_MOUSEWHEEL; wh2.wheel.windowID = Input::WindowId(); wh2.wheel.y = 3; wh2.wheel.preciseY = 3.0f; Input::Feed(&wh2);
    Check(Next(e) && e.type == InputEvent::WHEEL && e.wheel == 3, "wheel notches");
    SDL_Event k{}; k.type = SDL_KEYDOWN; k.key.windowID = Input::WindowId(); k.key.keysym.sym = SDLK_a; k.key.keysym.mod = KMOD_LSHIFT; SDL_PushEvent(&k);
    Check(Next(e) && e.type == InputEvent::KEY_DOWN && e.key == SDLK_a && (e.mod & KMOD_SHIFT), "key down with modifiers");
    SDL_Event t{}; t.type = SDL_TEXTINPUT; t.text.windowID = Input::WindowId(); strcpy(t.text.text, "A\xC3\xA9"); Input::Feed(&t);
    Check(Next(e) && e.type == InputEvent::TEXT && std::string(e.text) == "A\xC3\xA9", "typed text arrives as UTF-8");
    // ---- the text field model (the system clipboard is saved and restored) ----
    {
        char* oldClip = SDL_GetClipboardText();
        auto M = [](const std::string& t) { return (int)t.size() * 8; };            // 8 px per byte
        auto K = [](SDL_Keycode k, Uint16 mod = 0) { InputEvent ev; ev.type = InputEvent::KEY_DOWN; ev.key = k; ev.mod = mod; return ev; };
        const Uint16 CMD = KMOD_LCTRL, SH = KMOD_LSHIFT, ALT = KMOD_LALT;
        FD::TextField f;
        f.Insert("hello world");
        Check(f.text == "hello world" && f.caret == 11 && !f.HasSel(), "text field: typing puts the caret at the end");
        f.Key(K(SDLK_LEFT)); Check(f.caret == 10, "text field: Left moves one character");
        f.Key(K(SDLK_HOME)); Check(f.caret == 0, "text field: Home");
        f.Key(K(SDLK_END)); Check(f.caret == 11, "text field: End");
        for (int i = 0; i < 5; ++i) f.Key(K(SDLK_LEFT, SH));
        Check(f.HasSel() && f.Selected() == "world", "text field: Shift+Left x5 selects \"world\"");
        f.Key(K(SDLK_c, CMD)); char* c = SDL_GetClipboardText();
        Check(c && std::string(c) == "world", "text field: Ctrl+C puts the selection on the system clipboard"); if (c) SDL_free(c);
        f.Insert("X"); Check(f.text == "hello X", "text field: typing replaces the selection");
        f.Key(K(SDLK_z, CMD)); Check(f.text == "hello world" && f.Selected() == "world", "text field: undo restores text and selection");
        f.Key(K(SDLK_z, CMD | SH)); Check(f.text == "hello X", "text field: redo");
        f.Key(K(SDLK_END)); f.Key(K(SDLK_LEFT, ALT)); Check(f.caret == 6, "text field: Alt+Left jumps a word");
        f.Key(K(SDLK_LEFT, ALT)); Check(f.caret == 0, "text field: Alt+Left again reaches the start");
        f.Key(K(SDLK_RIGHT, ALT)); Check(f.caret == 5, "text field: Alt+Right jumps to the end of the word");
        f.Key(K(SDLK_a, CMD)); Check(f.Selected() == "hello X", "text field: Ctrl+A selects all");
        f.Key(K(SDLK_v, CMD)); Check(f.text == "world" && f.caret == 5, "text field: Ctrl+V pastes over the selection");
        f.Key(K(SDLK_x, CMD)); Check(f.text == "world", "text field: Ctrl+X with no selection does nothing");
        f.Key(K(SDLK_a, CMD)); f.Key(K(SDLK_x, CMD)); char* c2 = SDL_GetClipboardText();
        Check(f.text.empty() && c2 && std::string(c2) == "world", "text field: Ctrl+X cuts to the clipboard"); if (c2) SDL_free(c2);
        f.SetText("hello world"); f.Key(K(SDLK_BACKSPACE, ALT)); Check(f.text == "hello ", "text field: Alt+Backspace deletes a word");
        f.Key(K(SDLK_BACKSPACE, KMOD_LGUI)); Check(f.text.empty(), "text field: Cmd+Backspace deletes to the line start");
        f.SetText(""); f.Insert("a\xC3\xA9z\x01"); Check(f.text == "az", "text field: non-ASCII bytes and control codes are dropped");
        f.SetText(""); f.Insert("a\nb\r"); Check(f.text == "a b", "text field: newlines become spaces, CR is dropped (single line)");
        f.SetText("abcd"); f.SetCaretFromX(13, false, M); Check(f.caret == 2, "text field: click at x=13 -> nearest boundary (2)");
        f.SetCaretFromX(1000, true, M); Check(f.caret == 4 && f.anchor == 2, "text field: dragging past the end extends to the end");
        f.SetText("one two"); f.SelectWord(5); Check(f.Selected() == "two", "text field: double click selects the word");
        f.maxLen = 4; f.SetText("abcd"); f.Insert("ef"); Check(f.text == "abcd", "text field: the length limit holds");
        f.maxLen = 256; f.SetText(std::string(40, 'x')); f.EnsureVisible(M, 100);
        Check(f.scrollX > 0 && M(f.text) - f.scrollX <= 100, "text field: a long text scrolls so the caret is visible");
        if (oldClip) { SDL_SetClipboardText(oldClip); SDL_free(oldClip); }
    }
    // ---- the form model and the editing flow, driven with synthetic events through the same Handle() the window uses ----
    {
        using namespace FD;
        form.Clear(); FixOutDir(); sel = -1; tool = -1; grid = 8; scrollX = scrollY = 0; BuildFields();
        auto ev = [](InputEvent::Type t, int x, int y, int clicks = 1) { InputEvent v; v.type = t; v.x = x; v.y = y; v.button = SDL_BUTTON_LEFT; v.clicks = clicks; return v; };
        auto Click = [&](int x, int y, int clicks = 1) { Handle(ev(InputEvent::DOWN, x, y, clicks)); Handle(ev(InputEvent::UP, x, y, clicks)); };
        auto Key = [&](SDL_Keycode k, Uint16 mod = 0) { InputEvent v; v.type = InputEvent::KEY_DOWN; v.key = k; v.mod = mod; Handle(v); };
        auto Type = [&](const char* t) { InputEvent v; v.type = InputEvent::TEXT; strncpy(v.text, t, sizeof v.text - 1); Handle(v); };
        int ox = FormOX(), oy = FormOY();
        Click(40, TOOL_Y0 + 5);
        Check(tool == C_LABEL, "designer: clicking a toolbox row arms that control");
        Click(ox + 26, oy + 21);
        Check(form.items.size() == 1 && sel == 0 && tool == -1, "designer: clicking the form places it, selects it, and disarms the tool");
        Check(form.items[0].name == "label1" && form.items[0].x == 24 && form.items[0].y == 24, "designer: default name, position snapped to the 8 px grid");
        Check(form.items[0].h > 8 && form.items[0].h < 40, "designer: the height comes from the real SuperLabel");
        // drag to move
        Handle(ev(InputEvent::DOWN, ox + 40, oy + 30)); Handle(ev(InputEvent::MOVE, ox + 56, oy + 38)); Handle(ev(InputEvent::UP, ox + 56, oy + 38));
        Check(form.items[0].x == 40 && form.items[0].y == 32, "designer: dragging moves the control (snapped)");
        Handle(ev(InputEvent::DOWN, ox + 60, oy + 40)); Handle(ev(InputEvent::MOVE, ox - 500, oy - 500)); Handle(ev(InputEvent::UP, 0, 0));
        Check(form.items[0].x == 0 && form.items[0].y == 0, "designer: a control cannot be dragged outside the form");
        form.items[0].x = 40; form.items[0].y = 32;
        // resize with the right handle, then the left one
        int hy = oy + form.items[0].y + form.items[0].h / 2;
        Handle(ev(InputEvent::DOWN, ox + 140, hy)); Handle(ev(InputEvent::MOVE, ox + 200, hy)); Handle(ev(InputEvent::UP, 0, 0));
        Check(form.items[0].x == 40 && form.items[0].w == 160, "designer: right handle resizes to the snapped position");
        Handle(ev(InputEvent::DOWN, ox + 40, hy)); Handle(ev(InputEvent::MOVE, ox + 72, hy)); Handle(ev(InputEvent::UP, 0, 0));
        Check(form.items[0].x == 72 && form.items[0].w == 128, "designer: left handle keeps the right edge fixed");
        Handle(ev(InputEvent::DOWN, ox + 200, hy)); Handle(ev(InputEvent::MOVE, ox + 73, hy)); Handle(ev(InputEvent::UP, 0, 0));
        Check(form.items[0].w >= MIN_CONTROL_W, "designer: a control cannot shrink below the minimum width");
        // keys
        form.items[0].x = 40; form.items[0].w = 100;
        Key(SDLK_RIGHT); Check(form.items[0].x == 41, "designer: arrow nudges 1 px");
        Key(SDLK_DOWN, KMOD_LSHIFT); Check(form.items[0].y == 40, "designer: Shift+arrow nudges one grid step");
        // property fields
        Click(FIELD_X + 20, FieldY(1) + 5);
        Check(focus == 1 && fieldProp[1] == P_TEXT, "designer: clicking the Text field focuses it");
        Key(SDLK_a, KMOD_LCTRL); Type("Hi there");
        Check(form.items[0].text == "Hi there", "designer: typing in the Text field changes the control live");
        Click(FIELD_X + 20, FieldY(2) + 5); Key(SDLK_a, KMOD_LCTRL); Type("x9"); Type("9999");
        Check(fields[2].text == "9999" && form.items[0].x + form.items[0].w <= form.w, "designer: X accepts digits only and is clamped into the form");
        Key(SDLK_RETURN);
        Check(focus == -1 && fields[2].text == std::to_string(form.items[0].x), "designer: leaving the field shows the clamped value");
        Click(FIELD_X + 20, FieldY(0) + 5); Key(SDLK_a, KMOD_LCTRL); Type("bad name!");
        Check(form.items[0].name == "badname", "designer: a name keeps only identifier characters");
        Key(SDLK_RETURN);
        // a second control, duplicate names, tab
        Click(40, TOOL_Y0 + 4 * TOOL_H + 5); Click(ox + 10, oy + 100);
        Check(form.items.size() == 2 && form.items[1].type == C_PROGRESS && form.items[1].name == "progress1", "designer: a progress bar is added with its own name prefix");
        Click(FIELD_X + 20, FieldY(0) + 5); Key(SDLK_a, KMOD_LCTRL); Type("badname");
        Check(form.items[1].name == "progress1", "designer: a name already in use is refused");
        Key(SDLK_RETURN);
        Key(SDLK_TAB); Check(sel == 0, "designer: Tab selects the next control (wraps)");
        Click(10, 700); Click(ox + 280, oy + 220); Check(sel == -1 && form.items.size() == 2, "designer: clicking empty form space deselects");
        // form properties
        Check(nFields == 6 && fieldProp[1] == P_FW && fieldProp[3] == P_FBG && fieldProp[4] == P_KEEPHIDDEN && fieldProp[5] == P_MSGBOXES, "designer: with nothing selected the property panel edits the form");
        Click(FIELD_X + 20, FieldY(1) + 5); Key(SDLK_a, KMOD_LCTRL); Type("4000"); Key(SDLK_RETURN);
        Check(form.w == MAX_FORM_W, "designer: form width is limited to the maximum");
        Click(FIELD_X + 20, FieldY(1) + 5); Key(SDLK_a, KMOD_LCTRL); Type("2"); Key(SDLK_RETURN);
        Check(form.w == MIN_FORM_W, "designer: form width is limited to the minimum");
        Click(FIELD_X + 20, FieldY(1) + 5); Key(SDLK_a, KMOD_LCTRL); Type("320"); Key(SDLK_RETURN);
        // scrolling
        Click(FIELD_X + 20, FieldY(2) + 5); Key(SDLK_a, KMOD_LCTRL); Type("768"); Key(SDLK_RETURN);
        { InputEvent w; w.type = InputEvent::WHEEL; w.x = VX + 50; w.y = VY + 50; w.wheel = -3; Handle(w); }
        Check(scrollY > 0 && scrollY <= form.h + 2 * PAD - VH, "designer: the wheel scrolls a tall form, within its limits");
        Click(FIELD_X + 20, FieldY(2) + 5); Key(SDLK_a, KMOD_LCTRL); Type("240"); Key(SDLK_RETURN);
        Check(scrollY == 0, "designer: scrolling is reset when the form fits again");
        // JSON
        form.items[0].text = "say \"hi\" \\ ok";
        std::string j = form.ToJson(), err; DForm g;
        Check(g.FromJson(j, err) && g.items.size() == 2 && g.items[0].text == form.items[0].text && g.w == 320 && g.h == 240 && g.items[1].type == C_PROGRESS && g.ToJson() == j, "model: JSON round trip (quotes and backslashes included)");
        Check(!g.FromJson("{\"form\":{},\"controls\":[{\"type\":\"SuperNope\"}]}", err) && g.items.size() == 2, "model: an unknown control type is an error and leaves the form unchanged");
        Check(!g.FromJson("not json", err) && !err.empty(), "model: invalid JSON gives an error message");
        Check(g.FromJson("{\"form\":{\"width\":99999,\"height\":1},\"controls\":[{\"type\":\"SuperLabel\",\"name\":\"a b\",\"x\":-5}]}", err) && g.w == MAX_FORM_W && g.h == MIN_FORM_H && g.items[0].name == "label1" && g.items[0].x == 0, "model: loading clamps sizes and repairs names");
        // save / open
        setenv("FD_FORM", "/tmp/fd_selftest_form.json", 1);
        SaveForm(); form.Clear(); sel = -1; OpenForm();
        Check(form.items.size() == 2 && form.items[0].text == "say \"hi\" \\ ok", "designer: Ctrl+S / Ctrl+O round trip through a file");
        remove("/tmp/fd_selftest_form.json");
        // output folder: stored in the JSON, a missing folder falls back to the working folder on load
        Check(DirExists(form.outDir) && form.outDir == AppDir(), "designer: a new form's output folder is the FormDesigner application folder");
        { std::string e2; DForm h; h.outDir = "/some \"dir\""; Check(g.FromJson(h.ToJson(), e2) && g.outDir == h.outDir, "model: outputPath survives a JSON round trip (quotes escaped)"); }
        { FILE* f = fopen("/tmp/fd_selftest_form.json", "wb"); fputs("{\"form\":{\"outputPath\":\"/no/such/folder/anywhere\"},\"controls\":[]}", f); fclose(f); }
        OpenForm(); Check(form.outDir == AppDir(), "designer: a saved output folder that no longer exists is replaced by the application folder");
        { FILE* f = fopen("/tmp/fd_selftest_form.json", "wb"); fputs("{\"form\":{\"outputPath\":\"/tmp\"},\"controls\":[]}", f); fclose(f); }
        OpenForm(); Check(form.outDir == "/tmp", "designer: a saved output folder that exists is kept");
        remove("/tmp/fd_selftest_form.json"); setenv("FD_FORM", "/tmp/fd_selftest_form.json", 1);
        // ---- colours and the colour dialog ----
        form.Clear(); FixOutDir(); sel = -1; BuildFields();
        Place(C_LABEL, 8, 8);
        int fgRow = -1, bgRow = -1; for (int i = 0; i < nFields; ++i) { if (fieldProp[i] == P_FG) fgRow = i; if (fieldProp[i] == P_BG) bgRow = i; }
        Check(fgRow >= 0 && IsColorProp(P_FG) && !IsColorProp(P_X), "colors: a control has Text / Back / Border color rows");
        Click(FIELD_X + 10, FieldY(fgRow) + 5);
        Check(colorDlg != nullptr && colorDlg->color == DEF_FG, "colors: clicking a color row opens the dialog with the current color");
        Handle(ev(InputEvent::DOWN, 5, 5));
        Key(SDLK_DELETE);
        Check(colorDlg != nullptr && form.items.size() == 1, "colors: the dialog is modal (clicks and keys behind it do nothing)");
        auto D = [&]() -> ColorDialog& { return *colorDlg; };
        auto Pal = [&](int c, int r) { Click(D().PalX() + c * (D().CELL + D().GAP) + 4, D().PalY() + r * (D().CELL + D().GAP) + 4); };
        Pal(0, 1);
        Check(D().color == 0xF800 && D().FieldText(0) == "255" && D().FieldText(1) == "0" && D().FieldText(3) == "FF0000", "colors: a palette cell sets the color and the R / G / B / hex fields");
        Click(D().FieldX(1) + 10, D().FieldY() + 5); Key(SDLK_a, KMOD_LCTRL); Type("255");
        Check(D().color == 0xFFE0 && D().FieldText(3) == "FFFF00", "colors: typing in a channel field changes the color (R 255 G 255 B 0 = yellow)");
        Click(D().FieldX(3) + 10, D().FieldY() + 5); Key(SDLK_a, KMOD_LCTRL); Type("00ff"); Check(D().color == 0xFFE0, "colors: a partial hex value is ignored until it is complete");
        Type("ZZ"); Check(D().FieldText(3) == "00FF", "colors: the hex field takes hex digits only");
        Type("FF"); Check(D().color == 0x07FF && D().FieldText(3) == "00FFFF", "colors: a complete hex value sets the color (00FFFF = cyan)");
        Type("ABC"); Check(D().FieldText(3) == "00FFFF", "colors: the hex field takes no more than 6 digits");
        Key(SDLK_RETURN); Click(D().FieldX(3) + 10, D().FieldY() + 5); Key(SDLK_a, KMOD_LCTRL); Type("0000FF"); Key(SDLK_ESCAPE);
        Check(colorDlg != nullptr && D().color == 0x001F, "colors: Esc / Enter in a field leaves the field, not the dialog");
        SuperColorPicker* pk = D().Pk(); int hh, ss, vv;
        int px = pk->GetX() + pk->GetWidth() / 2, py = pk->GetY() + pk->GetBorder() + pk->GetPadding() + 8;
        Handle(ev(InputEvent::DOWN, px, py));
        pk->GetLiveHSV(hh, ss, vv);
        Check(hh > 100 && hh < 180 && D().color != 0x001F, "colors: pressing the HSV picker's hue bar sets the hue (about 140 degrees, a green)");
        Handle(ev(InputEvent::MOVE, px + 60, py + 40)); Handle(ev(InputEvent::UP, px + 60, py + 40));
        int h2, s2, v2; pk->GetLiveHSV(h2, s2, v2);
        Check(h2 > hh && s2 == ss && v2 == vv, "colors: dragging along the hue bar changes only the hue (even when the pointer leaves the bar)");
        uint16_t c1 = D().color; Key(SDLK_RIGHT, KMOD_LSHIFT);
        Check(D().color != c1, "colors: Shift+Right moves the picker's active bar by 10");
        Click(D().OldX() + 5, D().PalY() + 5); Check(D().color == DEF_FG, "colors: clicking Old restores the starting color");
        Click(D().CancelX() + 5, D().BtnY() + 3);
        Check(colorDlg == nullptr && form.items[0].fg == DEF_FG, "colors: Cancel closes the dialog and keeps the old color");
        Click(FIELD_X + 10, FieldY(fgRow) + 5); Pal(0, 1); Click(D().OkX() + 5, D().BtnY() + 3);
        Check(colorDlg == nullptr && form.items[0].fg == 0xF800, "colors: OK stores the color in the model");
        Click(FIELD_X + 10, FieldY(bgRow) + 5); Pal(9, 1); Key(SDLK_ESCAPE);
        Check(colorDlg == nullptr && form.items[0].bg == DEF_BG, "colors: Esc cancels");
        Click(FIELD_X + 10, FieldY(bgRow) + 5); Pal(9, 1); Key(SDLK_RETURN);
        Check(form.items[0].bg != DEF_BG && form.items[0].bg == ColorDialog::Palette(9, 1), "colors: Enter accepts");
        { auto lbl = MakeControl(&scr, form.items[0], 0, 0); Check(lbl->ColFg == 0xF800 && lbl->ColBg == form.items[0].bg, "colors: the real control gets the model's colors"); }
        form.items[0].border = 0x07E0; form.bg = 0x2104;
        std::string cj = form.ToJson(), cerr; DForm cg;
        Check(cg.FromJson(cj, cerr) && cg.items[0].fg == 0xF800 && cg.items[0].border == 0x07E0 && cg.bg == 0x2104 && cj.find("\"0xF800\"") != std::string::npos, "model: colors survive the JSON round trip (as 0xRRRR strings)");
        Check(cg.FromJson("{\"form\":{},\"controls\":[{\"type\":\"SuperLabel\",\"textColor\":63488}]}", cerr) && cg.items[0].fg == 0xF800 && cg.items[0].bg == DEF_BG && cg.bg == DEF_FORM_BG, "model: colors may be plain numbers; missing colors take the defaults");
        sel = -1; BuildFields(); Click(FIELD_X + 10, FieldY(3) + 5);
        Check(colorDlg != nullptr, "colors: the form's Back color row opens the dialog too"); Click(D().CancelX() + 5, D().BtnY() + 3);
        // ---- fonts and the font dialog ----
        {
            const auto& ft = Fonts(); bool uniq = true;
            for (size_t i = 0; i < ft.size(); ++i) for (size_t j = i + 1; j < ft.size(); ++j) if (std::string(ft[i].id) == ft[j].id) uniq = false;
            Check(ft.size() > 60 && uniq && FontIndex(DEFAULT_FONT) >= 0 && FindFont("nonsense").font == FindFont(DEFAULT_FONT).font, "fonts: the table is unique, has the default, and unknown names fall back to it");
        }
        form.Clear(); FixOutDir(); sel = -1; BuildFields(); Place(C_PUSHBUTTON, 8, 8);
        int fontRow = -1; for (int i = 0; i < nFields; ++i) if (fieldProp[i] == P_FONT) fontRow = i;
        int h0 = form.items[0].h;
        Check(fontRow >= 0 && form.items[0].font == "Font2", "fonts: a control has a Font row, default Font2");
        Click(FIELD_X + 10, FieldY(fontRow) + 5);
        Check(fontDlg != nullptr && fontDlg->font == "Font2", "fonts: clicking the Font row opens the dialog on the current font");
        Key(SDLK_DELETE); Check(fontDlg != nullptr && form.items.size() == 1, "fonts: the dialog is modal");
        Key(SDLK_DOWN); Check(fontDlg->font == "Font4", "fonts: Down moves to the next font");
        Key(SDLK_END); Check(fontDlg->font == Fonts().back().id, "fonts: End goes to the last font");
        Key(SDLK_HOME); Check(fontDlg->font == "Font0", "fonts: Home goes to the first font");
        Key(SDLK_PAGEDOWN); Check(fontDlg->Lb()->GetCursor() == FontDialog::ROWS - 1, "fonts: Page Down moves one page");
        { InputEvent w; w.type = InputEvent::WHEEL; w.x = FontDialog::X + 50; w.y = FontDialog::Y + 100; w.wheel = -2; Handle(w); }
        Check(fontDlg->Lb()->GetCursor() == FontDialog::ROWS - 1 + 6, "fonts: the wheel scrolls (3 rows per notch)");
        Key(SDLK_HOME);
        SuperListBox* lb = fontDlg->Lb(); int rx = lb->GetX() + lb->GetBorder() + lb->GetPadding() + 6, ry0 = lb->GetBoxTop() + lb->GetBorder() + lb->GetPadding() + 2;
        Click(rx, ry0 + 3 * 18); Check(fontDlg->font == Fonts()[3].id && fontDlg != nullptr, "fonts: clicking a row selects it");
        Handle(ev(InputEvent::DOWN, rx, ry0 + 1 * 18)); Handle(ev(InputEvent::MOVE, rx, ry0 + 5 * 18)); Handle(ev(InputEvent::UP, rx, ry0 + 5 * 18));
        Check(fontDlg != nullptr && fontDlg->font == Fonts()[5].id, "fonts: dragging follows the pointer and keeps the dialog open after the lift");
        Click(fontDlg->CancelX() + 5, fontDlg->BtnY() + 3);
        Check(fontDlg == nullptr && form.items[0].font == "Font2", "fonts: Cancel keeps the old font");
        Click(FIELD_X + 10, FieldY(fontRow) + 5); Key(SDLK_DOWN); Key(SDLK_ESCAPE);
        Check(fontDlg == nullptr && form.items[0].font == "Font2", "fonts: Esc cancels");
        Click(FIELD_X + 10, FieldY(fontRow) + 5); Key(SDLK_DOWN); Key(SDLK_RETURN);
        Check(fontDlg == nullptr && form.items[0].font == "Font4" && form.items[0].h > h0, "fonts: Enter accepts, and the control's height follows the font (SuperPushButton)");
        Click(FIELD_X + 10, FieldY(fontRow) + 5); lb = fontDlg->Lb(); rx = lb->GetX() + lb->GetBorder() + lb->GetPadding() + 6; ry0 = lb->GetBoxTop() + lb->GetBorder() + lb->GetPadding() + 2;
        Key(SDLK_HOME); Handle(ev(InputEvent::DOWN, rx, ry0 + 3 * 18, 2)); Handle(ev(InputEvent::UP, rx, ry0 + 3 * 18));
        Check(fontDlg == nullptr && form.items[0].font == Fonts()[3].id, "fonts: a double click accepts that font");
        { std::string e3; DForm fg2; form.items[0].font = "FreeSansBold12pt7b";
          Check(fg2.FromJson(form.ToJson(), e3) && fg2.items[0].font == "FreeSansBold12pt7b", "model: the font survives the JSON round trip");
          Check(fg2.FromJson("{\"form\":{},\"controls\":[{\"type\":\"SuperLabel\",\"font\":\"NoSuchFont\"}]}", e3) && fg2.items[0].font == DEFAULT_FONT, "model: an unknown font name in a file falls back to the default"); }
        // ---- border style / size, padding, alignment ----
        form.Clear(); FixOutDir(); sel = -1; BuildFields(); Place(C_LABEL, 8, 8);
        auto Row = [&](int prop) { for (int i = 0; i < nFields; ++i) if (fieldProp[i] == prop) return i; return -1; };
        auto Seg = [&](int prop, int k) { Click(FIELD_X + k * (FIELD_W / 3) + 6, FieldY(Row(prop)) + 5); };
        Check(Row(P_BSTYLE) >= 0 && Row(P_BSIZE) >= 0 && Row(P_PAD) >= 0 && Row(P_ALIGN) >= 0, "layout: a label has border style / size, padding and alignment rows");
        DControl& L0 = form.items[0];
        Check(L0.borderStyle == -1 && L0.borderSize == -1 && L0.padding == -1 && L0.align == -1 && IsPickRow(P_BSTYLE) && !IsPickRow(P_PAD), "layout: nothing is overridden by default");
        Check(fields[Row(P_BSIZE)].text == std::to_string(L0.defSize) && fields[Row(P_PAD)].text == std::to_string(L0.defPad), "layout: the fields show the control's own defaults");
        int hBefore = L0.h;
        Seg(P_BSTYLE, 2); Check(L0.borderStyle == 2 && ToJsonHas(form, "\"borderStyle\": \"sleek\""), "layout: clicking Sleek sets the border style (and it is saved)");
        { auto c = MakeControl(&scr, L0, 0, 0); Check(c->GetBorderStyle() == SuperControl::BORDER_SLEEK, "layout: the real control gets the border style"); }
        Seg(P_BSTYLE, 0); Check(L0.borderStyle == -1 && !ToJsonHas(form, "borderStyle"), "layout: choosing the default again removes the override");
        Seg(P_ALIGN, 1); Check(L0.align == 1, "layout: clicking Center sets the alignment");
        Seg(P_ALIGN, 0); Check(L0.align == -1, "layout: choosing the default alignment removes the override");
        Click(FIELD_X + 20, FieldY(Row(P_BSIZE)) + 5); Key(SDLK_a, KMOD_LCTRL); Type("3x"); Key(SDLK_RETURN);
        Check(L0.borderSize == 3 && fields[Row(P_BSIZE)].text == "3", "layout: typing a border size sets it (digits only)");
        Click(FIELD_X + 20, FieldY(Row(P_PAD)) + 5); Key(SDLK_a, KMOD_LCTRL); Type("99"); Key(SDLK_RETURN);
        Check(L0.padding == MAX_PADDING && fields[Row(P_PAD)].text == std::to_string(MAX_PADDING), "layout: padding is limited to the maximum");
        Check(L0.h > hBefore, "layout: the control's height follows border size and padding");
        Click(FIELD_X + 20, FieldY(Row(P_BSIZE)) + 5); Key(SDLK_a, KMOD_LCTRL); Key(SDLK_BACKSPACE); Key(SDLK_RETURN);
        Check(L0.borderSize == -1 && fields[Row(P_BSIZE)].text == std::to_string(L0.defSize), "layout: an empty field goes back to the default");
        L0.borderStyle = 1; L0.padding = 4; L0.align = 2;
        { std::string e4; DForm lf; Check(lf.FromJson(form.ToJson(), e4) && lf.items[0].borderStyle == 1 && lf.items[0].padding == 4 && lf.items[0].align == 2 && lf.items[0].borderSize == -1, "model: layout overrides survive the JSON round trip");
          Check(lf.FromJson("{\"form\":{},\"controls\":[{\"type\":\"SuperLabel\",\"borderStyle\":\"wavy\",\"borderSize\":99,\"align\":\"center\"},{\"type\":\"SuperProgressBar\",\"align\":\"right\"}]}", e4)
                && lf.items[0].borderStyle == -1 && lf.items[0].borderSize == MAX_BORDER && lf.items[0].align == 1 && lf.items[1].align == -1, "model: bad values in a file are repaired (unknown style ignored, size clamped, align ignored where unsupported)"); }
        Place(C_CHECKBOX, 8, 60); Check(Row(P_ALIGN) < 0 && Row(P_BSTYLE) >= 0, "layout: a check box has no alignment row");
        Place(C_PUSHBUTTON, 8, 100); Seg(P_ALIGN, 0); Check(form.items[2].align == 0 && form.items[2].defAlign == 1, "layout: a push button defaults to centre, so Left is an override");
        // ---- the code generator ----
        form.Clear(); FixOutDir(); sel = -1; BuildFields();
        form.name = "Demo";
        { DControl c; c.type = C_LABEL; c.name = "title"; c.text = "Say \"hi\" \\ now"; c.x = 16; c.y = 8; c.w = 200; c.fg = 0x07FF; c.borderStyle = 2; c.padding = 4; c.align = 1; form.items.push_back(c); }
        { DControl c; c.type = C_PUSHBUTTON; c.name = "okBtn"; c.text = "OK"; c.x = 16; c.y = 48; c.w = 80; c.font = "FreeSansBold12pt7b"; c.bg = 0x001F; c.border = 0x07FF; c.borderSize = 2; form.items.push_back(c); }
        { DControl c; c.type = C_CHECKBOX; c.name = "wifi"; c.text = "Wi-Fi"; c.x = 16; c.y = 96; c.w = 140; form.items.push_back(c); }
        { DControl c; c.type = C_RADIO; c.name = "modeA"; c.text = "A"; c.x = 16; c.y = 128; c.w = 140; form.items.push_back(c); }
        { DControl c; c.type = C_PROGRESS; c.name = "prog"; c.x = 16; c.y = 176; c.w = 200; form.items.push_back(c); }
        form.bg = 0x2104;
        std::string gen = GenerateDesigner(form);
        auto has = [&](const char* x) { return gen.find(x) != std::string::npos; };
        Check(has("class DemoBase {") && has("#pragma once") && has("GENERATED"), "generator: the header defines <Form>Base and says it is generated");
        Check(has("#include <SuperLabel.h>") && has("#include <SuperPushButton.h>") && has("#include <SuperProgressBar.h>") && has("#include <SuperRadioButton.h>") && has("#include <SuperCheckBox.h>"), "generator: includes the header of every control type used");
        { form.items.pop_back(); form.items.pop_back(); std::string g2 = GenerateDesigner(form); Check(g2.find("SuperProgressBar") == std::string::npos && g2.find("SuperRadioGroup") == std::string::npos, "generator: unused control types are not included (and no radio group without radio buttons)"); }
        form.items.push_back([] { DControl c; c.type = C_RADIO; c.name = "modeA"; c.text = "A"; c.x = 16; c.y = 128; c.w = 140; return c; }());
        form.items.push_back([] { DControl c; c.type = C_PROGRESS; c.name = "prog"; c.x = 16; c.y = 176; c.w = 200; return c; }());
        gen = GenerateDesigner(form);
        Check(has("title(dst, 16, 8, 200, &lgfx::fonts::Font2, \"Say \\\"hi\\\" \\\\ now\")"), "generator: constructor arguments, text escaped for C++");
        Check(has("okBtn(dst, 16, 48, 80, &lgfx::fonts::FreeSansBold12pt7b, \"OK\")") && has("prog(dst, 16, 176, 200, &lgfx::fonts::Font2)"), "generator: the font by its lgfx name; a progress bar has no text argument");
        Check(has("title.ColFg = 0x07FF;") && has("title.SetBorderStyle(SuperControl::BORDER_SLEEK);") && has("title.SetPadding(4);") && has("title.SetAlign(SuperControl::ALIGN_CENTER);"), "generator: overrides are written");
        Check(has("okBtn.SetBorder(2, 0x07FF);") && has("okBtn.ColBg = 0x001F;"), "generator: a border size override carries the border colour");
        Check(!has("wifi.ColFg") && !has("wifi.SetBorderStyle") && !has("wifi.SetPadding") && !has("wifi.SetAlign") && !has("prog.ColBg") && !has("prog.SetBorder"), "generator: defaults are not written");
        Check(has("FormBackColor = 0x2104") && has("wifi.ColHighlightClear = FormBackColor;") && has("Dst->fillScreen(FormBackColor)"), "generator: a non-black form colour is used for the screen and the highlight erase colour");
        Check(has("virtual void okBtn_Click() {}") && has("virtual void wifi_Change() {}") && has("virtual void modeA_Change() {}") && !has("title_") && !has("prog_"), "generator: one virtual handler per event (button / check / radio), none for labels and progress bars");
        Check(has("okBtn.SetOnAccept(okBtn_ClickThunk, this);") && has("wifi.SetOnChange(wifi_ChangeThunk, this);") && has("SuperRadioGroup group1;") && has("group1.Add(&modeA);"), "generator: events and the radio group are wired");
        Check(has("Form.Add(&title, false);") && has("Form.Add(&okBtn);") && has("Form.Add(&prog, false);"), "generator: labels and progress bars are not focusable");
        Check(GenerateDesigner(form) == gen, "generator: the output is deterministic");
        Check(!ValidName("int") && !ValidName("Form") && ValidName("Form1") && !ValidName("Show"), "model: C++ keywords and the generator's own names are not valid control names");
        // writing
        std::error_code fec; std::string gdir = std::filesystem::temp_directory_path(fec).string() + "/fd_gen_test"; std::filesystem::remove_all(gdir, fec); std::filesystem::create_directories(gdir, fec);
        form.outDir = "/old"; GenResult wr = WriteDesigner(form, gdir);
        Check(wr.ok && !wr.unchanged && form.outDir == gdir && std::filesystem::exists(gdir + "/Demo.designer.h") && !std::filesystem::exists(gdir + "/Demo.designer.h.tmp"), "generator: writes <Form>.designer.h (no temporary file left) and remembers the folder");
        { std::ifstream in(gdir + "/Demo.designer.h", std::ios::binary); std::stringstream ss; ss << in.rdbuf(); Check(ss.str() == gen, "generator: the file holds exactly the generated text"); }
        wr = WriteDesigner(form, gdir); Check(wr.ok && wr.unchanged, "generator: an unchanged form leaves the file alone");
        form.outDir = "/keep"; wr = WriteDesigner(form, gdir + "/nope");
        Check(!wr.ok && !wr.error.empty() && form.outDir == "/keep", "generator: a folder that does not exist is an error and the remembered folder stays");
        // the whole key: Ctrl+X (Export), with the JSON saved so outputPath is remembered
        setenv("FD_FORM", (gdir + "/demo.json").c_str(), 1); form.outDir = gdir; std::filesystem::remove(gdir + "/Demo.designer.h", fec);
        Key(SDLK_g, KMOD_LCTRL); Check(!std::filesystem::exists(gdir + "/Demo.designer.h") && grid == 8, "export: Ctrl+G does nothing any more (no export, no grid change)");
        Key(SDLK_x, KMOD_LCTRL);
        Check(fileDlg != nullptr && fileDlg->Dir() == std::filesystem::path(gdir).lexically_normal().string() && !std::filesystem::exists(gdir + "/Demo.designer.h"), "export: Ctrl+X opens the folder dialog on the folder saved in the form (nothing is written yet)");
        Handle(ev(InputEvent::DOWN, fileDlg->OkX() + 5, fileDlg->BtnY() + 3)); Handle(ev(InputEvent::UP, 0, 0));
        { std::ifstream in(gdir + "/demo.json"); std::stringstream ss; ss << in.rdbuf();
          Check(std::filesystem::exists(gdir + "/Demo.designer.h") && ss.str().find("\"outputPath\": \"" + std::filesystem::path(gdir).lexically_normal().string() + "\"") != std::string::npos && grid == 8, "export: Ctrl+X writes the code and saves the form with outputPath"); }
        std::filesystem::remove_all(gdir, fec); unsetenv("FD_FORM"); form.Clear(); FixOutDir();
        // ---- the file dialog (a real folder tree in the temp folder) ----
        {
            namespace fs = std::filesystem; std::error_code fe;
            std::string root = "/tmp/fd_ft"; fs::remove_all(root, fe);      // short: SDL text events carry at most 31 bytes
            fs::create_directories(root + "/Alpha", fe); fs::create_directories(root + "/beta/inner", fe); fs::create_directories(root + "/.hidden", fe);
            auto touch = [&](const std::string& p, const char* body = "{}") { FILE* f = fopen(p.c_str(), "wb"); if (f) { fputs(body, f); fclose(f); } };
            touch(root + "/x.json"); touch(root + "/Zed.JSON"); touch(root + "/notes.txt"); touch(root + "/.secret.json"); touch(root + "/beta/inner/deep.json");
            FileDialog* d = nullptr;
            auto Mk = [&](FileDialog::Kind k, const std::string& start) { delete d; d = new FileDialog(&scr, k, "test", start); };
            auto Row = [&](int i) { return d->RowText(i); };
            auto RowXY = [&](int i, int& x, int& y) { SuperListBox* lb = d->Lb(); x = lb->GetX() + lb->GetBorder() + lb->GetPadding() + 8; y = lb->GetBoxTop() + lb->GetBorder() + lb->GetPadding() + 2 + i * 18; };
            auto ClickRow = [&](int i, int clicks = 1) { int x, y; RowXY(i, x, y); d->Event(ev(InputEvent::DOWN, x, y, clicks)); d->Event(ev(InputEvent::UP, x, y, clicks)); };
            auto K2 = [&](SDL_Keycode k, Uint16 m = 0) { InputEvent v; v.type = InputEvent::KEY_DOWN; v.key = k; v.mod = m; d->Event(v); };
            auto T2 = [&](const char* t2) { InputEvent v; v.type = InputEvent::TEXT; strncpy(v.text, t2, sizeof v.text - 1); d->Event(v); };
            auto Btn = [&](int x, int y) { d->Event(ev(InputEvent::DOWN, x + 5, y + 3)); d->Event(ev(InputEvent::UP, x + 5, y + 3)); };
            Mk(FileDialog::K_OPEN, root);
            Check(d->Count() == 5 && Row(0) == ".." && Row(1) == "Alpha/" && Row(2) == "beta/" && Row(3) == "x.json" && Row(4) == "Zed.JSON", "files: folders first, then files that match *.json, sorted ignoring case");
            Check(d->PathText() == root && d->Dir() == root, "files: the path line shows the folder");
            ClickRow(3); Check(d->NameText() == "x.json" && d->state == FileDialog::OPEN, "files: clicking a file puts its name in the name field");
            ClickRow(2, 2); Check(d->Dir() == root + "/beta" && Row(1) == "inner/", "files: double-clicking a folder opens it");
            K2(SDLK_BACKSPACE); Check(d->Dir() == root, "files: Backspace goes up one folder");
            K2(SDLK_DOWN); K2(SDLK_RETURN); Check(d->Dir() == root + "/Alpha", "files: Down, Enter opens the highlighted folder");
            Btn(d->NavX(0), d->BtnRowY()); Check(d->Dir() == root, "files: the Up button goes up");
            ClickRow(4, 2); Check(d->state == FileDialog::OK && d->path == root + "/Zed.JSON", "files: double-clicking a file opens it (extension matched ignoring case)");
            Mk(FileDialog::K_OPEN, root + "/x.json");
            Check(d->NameText() == "x.json" && d->Lb()->GetCursor() == 3, "files: starting on a file shows its folder and highlights it");
            d->Event(ev(InputEvent::DOWN, d->NameX() + 10, d->NameY() + 5)); K2(SDLK_a, KMOD_LCTRL); T2("missing.json"); K2(SDLK_RETURN);
            Check(d->state == FileDialog::OPEN && d->ErrorText().find("no such file") != std::string::npos, "files: opening a file that does not exist is an error and the dialog stays");
            K2(SDLK_a, KMOD_LCTRL); T2("x.json"); K2(SDLK_RETURN); Check(d->state == FileDialog::OK && d->path == root + "/x.json", "files: Enter in the name field opens that file");
            // path line
            Mk(FileDialog::K_OPEN, root);
            d->Event(ev(InputEvent::DOWN, FileDialog::X + 40, d->PathY() + 5)); K2(SDLK_a, KMOD_LCTRL); T2((root + "/beta").c_str()); K2(SDLK_RETURN);
            Check(d->Dir() == root + "/beta", "files: typing a folder in the path line and pressing Enter goes there");
            K2(SDLK_TAB); K2(SDLK_TAB); K2(SDLK_a, KMOD_LCTRL); T2((root + "/no/where").c_str()); K2(SDLK_RETURN); Check(d->Dir() == root + "/beta" && !d->ErrorText().empty(), "files: a path that does not exist is an error (the folder stays)");
            K2(SDLK_a, KMOD_LCTRL); T2((root + "/x.json").c_str()); K2(SDLK_RETURN); Check(d->Dir() == root && d->NameText() == "x.json", "files: typing a file path shows its folder and name");
            // save
            Mk(FileDialog::K_SAVE, root + "/form1.json");
            Check(d->NameText() == "form1.json", "files: Save starts with the suggested name");
            K2(SDLK_a, KMOD_LCTRL); T2("a:b"); K2(SDLK_RETURN); Check(d->state == FileDialog::OPEN && !d->ErrorText().empty(), "files: a name with : / \\ * ? \" < > | is refused");
            K2(SDLK_a, KMOD_LCTRL); T2("brand_new"); K2(SDLK_RETURN);
            Check(d->state == FileDialog::OK && d->path == root + "/brand_new.json", "files: Save adds the extension");
            Mk(FileDialog::K_SAVE, root + "/x.json"); K2(SDLK_RETURN);
            Check(d->Confirming() && d->state == FileDialog::OPEN, "files: saving over an existing file asks first");
            K2(SDLK_n); Check(!d->Confirming() && d->state == FileDialog::OPEN, "files: N keeps the dialog open without saving");
            K2(SDLK_RETURN); K2(SDLK_y); Check(d->state == FileDialog::OK && d->path == root + "/x.json", "files: Y confirms the replacement");
            // folder chooser + new folder
            Mk(FileDialog::K_FOLDER, root);
            Check(d->Count() == 3 && Row(1) == "Alpha/" && Row(2) == "beta/", "files: the folder chooser lists folders only");
            ClickRow(2, 2); Btn(d->OkX(), d->BtnY()); Check(d->state == FileDialog::OK && d->path == root + "/beta", "files: Choose returns the folder that is shown");
            Mk(FileDialog::K_FOLDER, root); Btn(d->NavX(2), d->BtnRowY());
            Check(d->Naming(), "files: New folder asks for a name");
            T2("made/x"); K2(SDLK_RETURN); Check(d->Naming() && !d->ErrorText().empty(), "files: an invalid folder name is refused");
            K2(SDLK_a, KMOD_LCTRL); T2("made"); K2(SDLK_RETURN);
            Check(!d->Naming() && fs::is_directory(root + "/made", fe) && d->Dir() == root + "/made", "files: New folder creates the folder and opens it");
            Btn(d->NavX(2), d->BtnRowY()); T2("made"); T2(""); K2(SDLK_ESCAPE); Check(!d->Naming() && d->state == FileDialog::OPEN, "files: Esc leaves New folder, not the dialog");
            Btn(d->NavX(0), d->BtnRowY()); Check(d->Dir() == root, "files: the new folder is where Up leads back");
            K2(SDLK_ESCAPE); Check(d->state == FileDialog::CANCEL, "files: Esc cancels");
            Mk(FileDialog::K_OPEN, root); Btn(d->CancelX(), d->BtnY()); Check(d->state == FileDialog::CANCEL, "files: the Cancel button cancels");
            delete d; d = nullptr;

            // ---- wired into the designer: Open / Save / Save As / Generate into a folder ----
            form.Clear(); FixOutDir(); sel = -1; BuildFields(); unsetenv("FD_FORM"); currentFile.clear(); form.name = "Pick";
            Place(C_LABEL, 8, 8); form.items[0].text = "from file";
            auto Kd = [&](SDL_Keycode k, Uint16 m = 0) { InputEvent v; v.type = InputEvent::KEY_DOWN; v.key = k; v.mod = m; Handle(v); };
            auto Td = [&](const char* t2) { InputEvent v; v.type = InputEvent::TEXT; strncpy(v.text, t2, sizeof v.text - 1); Handle(v); };
            setenv("FD_SETTINGS", (root + "_settings.json").c_str(), 1); settings = AppSettings(); remove((root + "_settings.json").c_str());
            Key(SDLK_s, KMOD_LCTRL);
            Check(fileDlg != nullptr && fileDlg->NameText() == "Pick.json", "designer: Ctrl+S with no file yet opens the Save dialog, suggesting <FormName>.json");
            Kd(SDLK_ESCAPE); Check(fileDlg == nullptr && !FileKnown(), "designer: cancelling Save leaves the form without a file");
            currentFile = root + "/mine.json";
            Check(SaveForm() && fs::exists(root + "/mine.json", fe) && FileKnown(), "designer: Save writes the file");
            Key(SDLK_s, KMOD_LCTRL);
            Check(fileDlg != nullptr && fileDlg->Dir() == root && fileDlg->NameText() == "mine.json", "designer: Ctrl+S always shows the Save dialog, on the form's own file");
            Kd(SDLK_RETURN); Check(fileDlg == nullptr && currentFile == root + "/mine.json", "designer: saving over the form's own file needs no \"Replace it?\"");
            Key(SDLK_s, KMOD_LCTRL | KMOD_LSHIFT); Check(fileDlg != nullptr && fileDlg->NameText() == "mine.json", "designer: Ctrl+Shift+S is the same Save dialog");
            Kd(SDLK_a, KMOD_LCTRL); Td("renamed"); Kd(SDLK_RETURN);
            Check(fileDlg == nullptr && currentFile == root + "/renamed.json" && fs::exists(root + "/renamed.json", fe) && std::string(FormPath()) == root + "/renamed.json", "designer: a changed name is saved and remembered as the form's file (as Save As)");
            Key(SDLK_s, KMOD_LCTRL); Check(fileDlg != nullptr && fileDlg->NameText() == "renamed.json", "designer: the next Save starts with the remembered name");
            Kd(SDLK_ESCAPE);
            Check(settings.lastSaveDir == root, "settings: the folder the Save dialog was looking at is remembered");
            currentFile = root + "/mine.json";
            form.Clear(); form.name = "Other"; sel = -1; BuildFields();
            Key(SDLK_o, KMOD_LCTRL);
            Check(fileDlg != nullptr && fileDlg->Dir() == root, "designer: Ctrl+O always shows the Open dialog (here: the folder the file is in, nothing was remembered yet)");
            { int row = -1; for (int i = 0; i < fileDlg->Count(); ++i) if (fileDlg->RowText(i) == "mine.json") row = i; fileDlg->Lb()->SetSelected(row); fileDlg->Lb()->StartEdit(); }
            Kd(SDLK_TAB); Kd(SDLK_TAB); Kd(SDLK_TAB);                        // (focus goes round: name, path, list)
            Kd(SDLK_BACKSPACE); Check(fileDlg != nullptr && fileDlg->Dir() == fs::path(root).parent_path().string(), "designer: (Backspace in the list goes up)");
            Kd(SDLK_ESCAPE);
            { AppSettings back; Check(back.Load() && back.lastOpenDir == fs::path(root).parent_path().string() && settings.lastOpenDir == back.lastOpenDir, "settings: the folder the Open dialog was looking at when it was closed is remembered, and saved to the settings file"); }
            Key(SDLK_o, KMOD_LCTRL); Check(fileDlg != nullptr && fileDlg->Dir() == fs::path(root).parent_path().string(), "designer: Ctrl+O opens where it was last looking (an application-level setting)");
            Kd(SDLK_ESCAPE); settings.lastOpenDir = root;
            Key(SDLK_o, KMOD_LCTRL); { int row = -1; for (int i = 0; i < fileDlg->Count(); ++i) if (fileDlg->RowText(i) == "mine.json") row = i; fileDlg->Lb()->SetSelected(row); fileDlg->Lb()->StartEdit(); }
            Kd(SDLK_RETURN);                                                 // Enter on the list row: the highlighted file opens
            Check(fileDlg == nullptr && form.items.size() == 1 && form.items[0].text == "from file" && form.name == "Pick", "designer: Open loads the chosen file");
            touch(root + "/broken.json", "not json"); currentFile = root + "/mine.json";
            OpenFileDialog(FP_OPEN); { FileDialog* fd = fileDlg; for (int i = 0; i < fd->Count(); ++i) if (fd->RowText(i) == "broken.json") { fd->Lb()->SetSelected(i); fd->Lb()->StartEdit(); } }
            { InputEvent v; v.type = InputEvent::KEY_DOWN; v.key = SDLK_TAB; Handle(v); v.key = SDLK_a; v.mod = KMOD_LCTRL; Handle(v); Td("broken.json"); v.key = SDLK_RETURN; v.mod = 0; Handle(v); }
            Check(fileDlg == nullptr && currentFile == root + "/mine.json" && form.name == "Pick", "designer: opening a file that is not a form leaves the form and the current file alone");
            // generate into a chosen folder
            fs::create_directories(root + "/out", fe);
            Key(SDLK_x, KMOD_LCTRL | KMOD_LSHIFT);
            Check(fileDlg != nullptr && fileDlg->Dir() == form.outDir, "designer: Ctrl+Shift+X opens the folder chooser at the last export folder");
            Handle(ev(InputEvent::DOWN, FileDialog::X + 40, fileDlg->PathY() + 5)); Handle(ev(InputEvent::UP, 0, 0));
            Kd(SDLK_a, KMOD_LCTRL); Td((root + "/out").c_str()); Kd(SDLK_RETURN);
            Check(fileDlg != nullptr && fileDlg->Dir() == root + "/out", "designer: the folder chooser follows the typed path");
            Handle(ev(InputEvent::DOWN, fileDlg->OkX() + 5, fileDlg->BtnY() + 3)); Handle(ev(InputEvent::UP, 0, 0));
            Check(fileDlg == nullptr && form.outDir == root + "/out" && fs::exists(root + "/out/Pick.designer.h", fe), "designer: choosing the folder generates there and remembers it (outputPath)");
            currentFile.clear(); fs::remove_all(root, fe); remove((root + "_settings.json").c_str()); unsetenv("FD_SETTINGS");
            form.Clear(); FixOutDir(); sel = -1; BuildFields();
        }
        // ---- the user's file ----
        form.Clear(); FixOutDir(); sel = -1; BuildFields(); form.name = "Demo";
        { DControl c; c.type = C_PUSHBUTTON; c.name = "okBtn"; form.items.push_back(c); }
        { DControl c; c.type = C_CHECKBOX; c.name = "wifi"; form.items.push_back(c); }
        { DControl c; c.type = C_LABEL; c.name = "title"; form.items.push_back(c); }
        std::string uf = GenerateUserFile(form);
        auto uhas = [&](const char* x) { return uf.find(x) != std::string::npos; };
        Check(uhas("#include \"Demo.designer.h\"") && uhas("class Demo : public DemoBase {") && uhas("Demo(lgfx::LovyanGFX* dst, SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB) : DemoBase(dst, enc, btnA, btnB)"), "user file: includes the designer header and derives from the Base with the same constructor");
        Check(uhas("void okBtn_Click() override {") && uhas("void wifi_Change() override {") && !uhas("title_") && uhas("wifi.GetChecked()"), "user file: one override stub per handler (none for labels)");
        Check(uhas("never changes it again") && uhas("ui.Show();") && uhas("ui.Service();") && uhas("#pragma once"), "user file: says it is yours and how to use it in a sketch");
        Check(GenerateUserFile(form) == uf, "user file: deterministic");
        { DForm e2; e2.name = "Empty"; std::string ue = GenerateUserFile(e2); Check(ue.find("protected:") == std::string::npos && ue.find("class Empty : public EmptyBase") != std::string::npos, "user file: a form without events has no handler section"); }
        std::error_code ufe; std::string udir = "/tmp/fd_uf"; std::filesystem::remove_all(udir, ufe); std::filesystem::create_directories(udir, ufe);
        UserResult ur = WriteUserFile(form, udir);
        Check(ur.ok && ur.created && ur.missing.empty() && std::filesystem::exists(udir + "/Demo.h", ufe) && !std::filesystem::exists(udir + "/Demo.h.tmp", ufe), "user file: created the first time (no temporary file left)");
        { FILE* f = fopen((udir + "/Demo.h").c_str(), "ab"); fputs("// MY OWN CODE\n", f); fclose(f); }
        ur = WriteUserFile(form, udir);
        { std::ifstream in(udir + "/Demo.h", std::ios::binary); std::stringstream ss; ss << in.rdbuf();
          Check(ur.ok && !ur.created && ss.str() == uf + "// MY OWN CODE\n", "user file: an existing file is never touched (the user's edits stay)"); }
        { std::string bn = std::filesystem::path(ur.backup).filename().string(); std::ifstream in(ur.backup, std::ios::binary); std::stringstream ss; ss << in.rdbuf();
          bool shape = bn.size() == std::string("Demo.2026_11_13_09_30_00.h.txt").size() && bn.substr(0, 5) == "Demo." && bn.substr(bn.size() - 6) == ".h.txt" && bn[9] == '_' && bn[12] == '_' && bn[15] == '_' && bn[18] == '_' && bn[21] == '_';
          Check(!ur.backup.empty() && shape && ss.str() == uf + "// MY OWN CODE\n", "export: an existing user file is copied to <Form>.yyyy_mm_dd_hh_mm_ss.h.txt (identical copy)");
          Check(ur.backup.find(udir) == 0 && std::filesystem::exists(udir + "/Demo.h", ufe), "export: the backup is next to the file and the file itself stays"); }
        { std::filesystem::remove_all(udir + "_new", ufe); std::filesystem::create_directories(udir + "_new", ufe); UserResult fresh = WriteUserFile(form, udir + "_new"); Check(fresh.created && fresh.backup.empty(), "export: a file that was just created needs no copy"); std::filesystem::remove_all(udir + "_new", ufe); }
        { DControl c; c.type = C_RADIO; c.name = "modeA"; form.items.push_back(c); }
        { DControl c; c.type = C_PUSHBUTTON; c.name = "cancelBtn"; form.items.push_back(c); }
        ur = WriteUserFile(form, udir);
        Check(ur.ok && ur.missing.size() == 2 && ur.missing[0] == "modeA_Change" && ur.missing[1] == "cancelBtn_Click", "user file: handlers for controls added later are reported as missing (the file is not edited)");
        ur = WriteUserFile(form, udir + "/nope"); Check(!ur.ok && !ur.error.empty(), "user file: a missing folder is an error");
        setenv("FD_FORM", (udir + "/demo.json").c_str(), 1); form.outDir = udir; ExportForm("");
        Check(std::filesystem::exists(udir + "/Demo.designer.h", ufe) && std::filesystem::exists(udir + "/Demo.h", ufe) && status.find("kept") != std::string::npos && status.find("modeA_Change") != std::string::npos, "export: Ctrl+X writes both files and the status names the handlers missing from the user file");
        std::filesystem::remove(udir + "/Demo.h", ufe); ExportForm("");
        Check(std::filesystem::exists(udir + "/Demo.h", ufe) && status.find("created Demo.h") != std::string::npos, "designer: a deleted user file is created again");
        unsetenv("FD_FORM"); std::filesystem::remove_all(udir, ufe); form.Clear(); FixOutDir();
        // ---- radio groups ----
        form.Clear(); FixOutDir(); sel = -1; BuildFields(); form.name = "Radios";
        Place(C_RADIO, 8, 8); Place(C_RADIO, 8, 40); Place(C_RADIO, 8, 72); Place(C_LABEL, 8, 104);
        Check(form.items[0].group == "group1" && form.items[1].group == "group1", "groups: radio buttons start in the default group");
        { auto R = [&](int prop) { for (int i = 0; i < nFields; ++i) if (fieldProp[i] == prop) return i; return -1; };
          Select(3); Check(R(P_GROUP) < 0, "groups: only radio buttons have a Group row");
          Select(2); Check(R(P_GROUP) >= 0, "groups: a radio button has a Group row");
          Click(FIELD_X + 20, FieldY(R(P_GROUP)) + 5); Key(SDLK_a, KMOD_LCTRL); Type("mode s!"); Key(SDLK_RETURN);
          Check(form.items[2].group == "modes", "groups: the group name keeps only identifier characters");
          Click(FIELD_X + 20, FieldY(R(P_GROUP)) + 5); Key(SDLK_a, KMOD_LCTRL); Type("radio1"); Key(SDLK_RETURN);
          Check(form.items[2].group == "modes", "groups: a group cannot be named like a control (both are members of the generated class)");
          Click(FIELD_X + 20, FieldY(R(P_GROUP)) + 5); Key(SDLK_a, KMOD_LCTRL); Type("int"); Key(SDLK_RETURN);
          Check(form.items[2].group == "modes", "groups: a C++ keyword is refused");
          Select(1); Click(FIELD_X + 20, FieldY(R(P_NAME)) + 5); Key(SDLK_a, KMOD_LCTRL); Type("modes"); Key(SDLK_RETURN);
          Check(form.items[1].name == "radio2", "groups: a control cannot be named like a group in use"); }
        std::string rg = GenerateDesigner(form);
        auto rhas = [&](const char* x) { return rg.find(x) != std::string::npos; };
        Check(rhas("SuperRadioGroup group1;") && rhas("SuperRadioGroup modes;") && rhas("group1.Add(&radio1);") && rhas("group1.Add(&radio2);") && rhas("modes.Add(&radio3);"), "groups: the generated class has one SuperRadioGroup per group name, each radio button added to its own");
        Check(rg.find("SuperRadioGroup group1;") < rg.find("SuperRadioGroup modes;") && rg.find("SuperRadioGroup modes;") < rg.find("SuperForm Form;"), "groups: groups are declared in order of first use, before the form");
        Check(GenerateUserFile(form).find("modes.GetSelected()") != std::string::npos, "groups: the user file's hint names the control's own group");
        { std::string e5; DForm g5; Check(g5.FromJson(form.ToJson(), e5) && g5.items[0].group == "group1" && g5.items[2].group == "modes" && form.ToJson().find("\"group\": \"modes\"") != std::string::npos, "model: the radio group survives the JSON round trip");
          Check(g5.FromJson("{\"form\":{},\"controls\":[{\"type\":\"SuperRadioButton\",\"name\":\"a\",\"group\":\"b\"},{\"type\":\"SuperRadioButton\",\"name\":\"b\"},{\"type\":\"SuperRadioButton\",\"name\":\"c\",\"group\":\"while\"}]}", e5)
                && g5.items[0].group != "b" && !g5.ControlNamed(g5.items[0].group) && g5.items[2].group == "group1" && g5.items[1].group == "group1", "model: a group named like a control or a keyword is repaired on load"); }
        form.Clear(); FixOutDir(); sel = -1; BuildFields();
        // ---- zoom ----
        form.Clear(); FixOutDir(); sel = -1; BuildFields(); zoom = 100; scrollX = scrollY = 0; zoomSpin->SetValue(100);
        Check(FormOX() == VX + (VW - 320) / 2 && FormOY() == VY + (VH - 240) / 2, "zoom: a form smaller than the surface is centred");
        { int ox2 = FormOX(), oy2 = FormOY(); Check(ToFX(ox2 + 100) == 100 && ToFY(oy2 + 7) == 7 && ToFX(ox2 - 1) == -1, "zoom: at 100% screen and form pixels agree (and left of the form is negative)"); }
        auto Fit = [&]() { Click(ZFIT_X + 5, ZROW_Y + 3); }; auto One = [&]() { Click(ZONE_X + 5, ZROW_Y + 3); };
        Fit(); Check(zoom == std::min((VW - 2 * PAD) * 100 / 320, (VH - 2 * PAD) * 100 / 240) && zoom == 193 && ZW() + 2 * PAD <= VW && ZH() + 2 * PAD <= VH && zoomSpin->GetValue() == zoom, "zoom: Fit enlarges a small form to the surface (and the spin box shows it)");
        One(); Check(zoom == 100 && zoomSpin->GetValue() == 100, "zoom: the 1:1 button goes back to 100%");
        Click(ZSPIN_X + ZSPIN_W - 6, ZROW_Y + 8); Check(zoom == 110, "zoom: clicking the right third of the spin box zooms in one step");
        Click(ZSPIN_X + 6, ZROW_Y + 8); Click(ZSPIN_X + 6, ZROW_Y + 8); Check(zoom == 90, "zoom: the left third zooms out");
        { int mx2 = ZSPIN_X + ZSPIN_W / 2, my2 = ZROW_Y + 10; Handle(ev(InputEvent::DOWN, mx2, my2)); Handle(ev(InputEvent::MOVE, mx2, my2 - 12)); Handle(ev(InputEvent::MOVE, mx2, my2 - 24)); Check(zoom > 90, "zoom: dragging up on the spin box zooms in live"); Handle(ev(InputEvent::UP, mx2, my2 - 24)); }
        Check(zoom == zoomSpin->GetValue() && !zoomSpin->IsEditing() && zoom > 90, "zoom: lifting accepts the value");
        { InputEvent w2; w2.type = InputEvent::WHEEL; w2.x = ZSPIN_X + 20; w2.y = ZROW_Y + 8; w2.wheel = 1; int z0 = zoom; Handle(w2); Check(zoom == z0 + 10, "zoom: the wheel over the spin box steps the zoom"); }
        One(); for (int i = 0; i < 100; ++i) ZoomCentre(zoom + 100); Check(zoom == ZOOM_MAX, "zoom: limited to 800%");
        for (int i = 0; i < 100; ++i) ZoomCentre(zoom - 100); Check(zoom == ZOOM_MIN, "zoom: limited to 10%");
        One(); Key(SDLK_PLUS); Check(zoom == 110, "zoom: the + key"); Key(SDLK_MINUS); Key(SDLK_MINUS); Check(zoom == 90, "zoom: the - key"); Key(SDLK_0); Check(zoom == 100, "zoom: the 0 key is 1:1"); Key(SDLK_f); Check(zoom == 193, "zoom: the F key is Fit");
        One();
        // editing at 200%
        zoom = 100; ZoomCentre(200);
        { int ox2 = FormOX(), oy2 = FormOY();
          Click(40, TOOL_Y0 + 5); Click(ox2 + 2 * 26, oy2 + 2 * 21);
          Check(form.items.size() == 1 && form.items[0].x == 24 && form.items[0].y == 24, "zoom: placing at 200% maps the click back to form pixels (snapped to the grid)");
          int cy2 = oy2 + Zc(form.items[0].y + form.items[0].h / 2);
          Handle(ev(InputEvent::DOWN, ox2 + 2 * 40, oy2 + 2 * 30)); Handle(ev(InputEvent::MOVE, ox2 + 2 * 40 + 32, oy2 + 2 * 30 + 16)); Handle(ev(InputEvent::UP, 0, 0));
          Check(form.items[0].x == 40 && form.items[0].y == 32, "zoom: dragging 32 screen pixels at 200% moves the control 16 form pixels (snapped)");
          int rx2 = ox2 + Zc(form.items[0].x + form.items[0].w); cy2 = oy2 + Zc(form.items[0].y + form.items[0].h / 2);
          Handle(ev(InputEvent::DOWN, rx2 + 3, cy2)); Handle(ev(InputEvent::MOVE, ox2 + 2 * 200, cy2)); Handle(ev(InputEvent::UP, 0, 0));
          Check(form.items[0].w == 160, "zoom: the resize handle is hit and dragged at 200% (width 160)"); }
        // zoom around the mouse on a form larger than the surface
        form.Clear(); form.w = 1024; form.h = 768; sel = -1; BuildFields(); zoom = 100; scrollX = scrollY = 0; zoomSpin->SetValue(100); ClampScroll();
        { int mx3 = VX + 300, my3 = VY + 200; int fx0 = ToFX(mx3), fy0 = ToFY(my3);
          InputEvent w3; w3.type = InputEvent::WHEEL; w3.x = mx3; w3.y = my3; w3.wheel = 1;
          for (int i = 0; i < 10; ++i) { Uint16 save = SDL_GetModState(); (void)save; SDL_SetModState((SDL_Keymod)KMOD_LCTRL); Handle(w3); SDL_SetModState(KMOD_NONE); }
          Check(zoom == 200 && std::abs(ToFX(mx3) - fx0) <= 1 && std::abs(ToFY(my3) - fy0) <= 1, "zoom: Ctrl/Cmd + wheel zooms around the mouse (the form point under it stays put)");
          Check(scrollX >= 0 && scrollX <= ZW() + 2 * PAD - VW && scrollY >= 0 && scrollY <= ZH() + 2 * PAD - VH, "zoom: the scroll position stays inside the zoomed form"); }
        // the scaled picture itself
        form.Clear(); form.w = 40; form.h = 30; zoom = 100; ZoomCentre(200); RenderFormSprite(); formSpr.fillRect(0, 0, 10, 10, 0xF800); scr.fillRect(VX, VY, VW, VH, 0x2124);
        { int ox4 = FormOX(), oy4 = FormOY(); BlitForm(ox4, oy4);
          Check(scr.readPixel(ox4 + 19, oy4 + 19) == formSpr.readPixel(9, 9) && scr.readPixel(ox4 + 20, oy4 + 20) == formSpr.readPixel(10, 10) && scr.readPixel(ox4 + 19, oy4 + 19) != scr.readPixel(ox4 + 20, oy4 + 20), "zoom: each form pixel becomes a 2x2 block at 200%");
          Check(scr.readPixel(ox4 + ZW() + 6, oy4 + 5) == scr.readPixel(VX + 2, VY + 2) && scr.readPixel(ox4 + 5, oy4 + ZH() + 6) == scr.readPixel(VX + 2, VY + 2), "zoom: nothing is drawn outside the scaled form"); }
        form.Clear(); FixOutDir(); sel = -1; BuildFields(); zoom = 100; scrollX = scrollY = 0; zoomSpin->SetValue(100);
        // ---- names: C++-safe ----
        {
            auto bad = [&](std::initializer_list<const char*> v) { std::string w; for (const char* n : v) if (ValidName(n)) w += std::string(n) + " "; return w; };
            auto good = [&](std::initializer_list<const char*> v) { std::string w; for (const char* n : v) if (!ValidName(n)) w += std::string(n) + " "; return w; };
            std::string w;
            w = bad({ "int", "class", "wchar_t", "static_assert", "bitand", "compl", "consteval", "co_await", "thread_local", "dynamic_cast", "and_eq", "char8_t", "requires", "concept", "final", "override", "import", "nullptr", "xor", "typeid" });
            Check(w.empty(), ("names: C++ keywords are refused (accepted: " + w + ")").c_str());
            w = bad({ "Form", "Show", "Service", "Dst", "FormWidth", "FormHeight", "FormBackColor", "InitializeComponent" });
            Check(w.empty(), ("names: the generated class's own names are refused (accepted: " + w + ")").c_str());
            w = bad({ "_x", "_X", "__x", "a__b", "x__", "okBtn_Click", "okBtn_Change", "okBtn_ClickThunk", "x_ChangeThunk", "SuperLabel", "SuperForm", "SuperEditState", "lgfx", "lgfxFoo", "LGFX", "LGFX_Device", "TFT_RED", "TFT_BLACK", "ESP32", "esp_x", "GPIO5", "gpio_x", "PICO_W", "ARDUINO", "A0", "A21", "D13", "P4", "setup", "loop", "main" });
            Check(w.empty(), ("names: reserved shapes, handler names, SuperGUI / LovyanGFX / board names and pins are refused (accepted: " + w + ")").c_str());
            w = bad({ "HIGH", "LOW", "INPUT", "OUTPUT", "INPUT_PULLUP", "LED_BUILTIN", "PI", "TWO_PI", "min", "max", "abs", "constrain", "round", "byte", "word", "boolean", "String", "Serial", "Wire", "SPI", "delay", "millis", "pinMode", "digitalWrite", "map", "random", "sin", "F", "SDA", "SCL", "SS", "MOSI", "linux", "unix" });
            Check(w.empty(), ("names: Arduino names and macros are refused (accepted: " + w + ")").c_str());
            w = bad({ "NULL", "EOF", "errno", "assert", "stdin", "std", "size_t", "uint8_t", "uint16_t", "int32_t", "intptr_t", "FILE", "time", "exit", "printf", "malloc", "memcpy", "INT_MAX", "RAND_MAX", "BUFSIZ", "CHAR_BIT", "SDL_BUTTON_LEFT" });
            Check(w.empty(), ("names: C library names and macros (stdint types, NULL, limits.h ...) are refused (accepted: " + w + ")").c_str());
            w = good({ "title", "okBtn", "btnOk", "lblStatus", "x1", "statusLabel", "progress1", "wifi", "modeA", "CamelCase", "snake_case", "a", "Form1", "Settings", "OK", "temperature", "pressure", "tab_", "Ab12_", "label_2", "myA0", "D", "Dx3", "P", "Pause" });
            Check(w.empty(), ("names: ordinary names stay valid (refused: " + w + ")").c_str());
            Check(!ValidName("") && !ValidName("1abc") && !ValidName("a b") && !ValidName("a-b") && !ValidName("a.b") && !ValidName("é"), "names: empty, leading digit and non-identifier characters are refused");
            bool macrosOk = true; for (const char* m : kReservedMacros) if (ValidName(m)) macrosOk = false;
            Check(macrosOk && sizeof(kReservedMacros) / sizeof(kReservedMacros[0]) > 1000, "names: every macro the generated code's headers define is refused (ReservedMacros.h)");
            bool sorted = true; for (size_t i = 1; i < sizeof(kReservedMacros) / sizeof(kReservedMacros[0]); ++i) if (strcmp(kReservedMacros[i - 1], kReservedMacros[i]) >= 0) sorted = false;
            Check(sorted, "names: the generated macro table is sorted (binary search is valid)");
            // clashes inside the generated class
            form.Clear(); FixOutDir(); sel = -1; BuildFields(); form.name = "Demo";
            Place(C_LABEL, 8, 8); Place(C_RADIO, 8, 40);
            Check(form.NameTaken("Demo") && form.NameTaken("DemoBase") && !form.NameTaken("Demo2"), "names: a control or group cannot be named like the form's classes (Demo, DemoBase)");
            Select(0); { auto R2 = [&](int prop) { for (int i = 0; i < nFields; ++i) if (fieldProp[i] == prop) return i; return -1; };
              Click(FIELD_X + 20, FieldY(R2(P_NAME)) + 5); Key(SDLK_a, KMOD_LCTRL); Type("DemoBase"); Key(SDLK_RETURN);
              Check(form.items[0].name == "label1", "names: renaming a control to the form's base class name is refused");
              Click(FIELD_X + 20, FieldY(R2(P_NAME)) + 5); Key(SDLK_a, KMOD_LCTRL); Type("max"); Key(SDLK_RETURN);
              Check(form.items[0].name == "label1", "names: renaming a control to an Arduino macro is refused");
              Click(FIELD_X + 20, FieldY(R2(P_NAME)) + 5); Key(SDLK_a, KMOD_LCTRL); Type("btn_Click"); Key(SDLK_RETURN);
              Check(form.items[0].name == "label1", "names: a control cannot be named like a handler (btn_Click)");
              Select(1); Click(FIELD_X + 20, FieldY(R2(P_GROUP)) + 5); Key(SDLK_a, KMOD_LCTRL); Type("Demo"); Key(SDLK_RETURN);
              Check(form.items[1].group == "group1", "names: a radio group cannot be named like the form's class"); }
            Select(-1); { int fr = -1; for (int i = 0; i < nFields; ++i) if (fieldProp[i] == P_FNAME) fr = i;
              Click(FIELD_X + 20, FieldY(fr) + 5); Key(SDLK_a, KMOD_LCTRL); Type("label1"); Key(SDLK_RETURN);
              Check(form.name == "Demo", "names: the form cannot be renamed to a control's name");
              Click(FIELD_X + 20, FieldY(fr) + 5); Key(SDLK_a, KMOD_LCTRL); Type("group1"); Key(SDLK_RETURN);
              Check(form.name == "Demo", "names: the form cannot be renamed to a radio group's name");
              Click(FIELD_X + 20, FieldY(fr) + 5); Key(SDLK_a, KMOD_LCTRL); Type("Serial"); Key(SDLK_RETURN);
              Check(form.name == "Demo", "names: the form cannot be named like a reserved word");
              Click(FIELD_X + 20, FieldY(fr) + 5); Key(SDLK_a, KMOD_LCTRL); Type("Clock"); Key(SDLK_RETURN);
              Check(form.name == "Clock", "names: a good form name is accepted"); }
            { std::string e6; DForm f6; Check(f6.FromJson("{\"form\":{\"name\":\"label1\"},\"controls\":[{\"type\":\"SuperLabel\",\"name\":\"label1\"},{\"type\":\"SuperRadioButton\",\"name\":\"r\",\"group\":\"MyForm\"}]}", e6) && f6.name != "label1" && ValidName(f6.name) && !f6.FormNameClashes(f6.name), "model: a file whose form name equals a control name is repaired on load");
              Check(f6.FromJson("{\"form\":{\"name\":\"MyForm\"},\"controls\":[{\"type\":\"SuperRadioButton\",\"name\":\"r\",\"group\":\"MyFormBase\"},{\"type\":\"SuperLabel\",\"name\":\"max\"},{\"type\":\"SuperLabel\",\"name\":\"x_Click\"}]}", e6)
                    && f6.items[0].group != "MyFormBase" && f6.items[1].name != "max" && f6.items[2].name != "x_Click" && ValidName(f6.items[1].name) && ValidName(f6.items[2].name), "model: reserved control names and groups in a file are repaired on load"); }
            // a new radio button joins the form's first group
            form.Clear(); FixOutDir(); sel = -1; BuildFields(); Place(C_RADIO, 8, 8); form.items[0].group = "modes"; Place(C_RADIO, 8, 40);
            Check(form.items[1].group == "modes", "names: a new radio button joins the form's first radio group");
            form.Clear(); FixOutDir(); sel = -1; BuildFields();
        }
        // ---- chunk 1: spin box and slider, the property schema, the label, the scrolling panel ----
        form.Clear(); FixOutDir(); sel = -1; BuildFields(); form.name = "Ctl";
        Check(C_COUNT == 20 && std::string(Info(C_SPINBOX).key) == "SuperSpinBox" && std::string(Info(C_SLIDER).key) == "SuperSlider" && Info(C_SPINBOX).editable && !Info(C_PUSHBUTTON).editable, "chunk 1: the spin box and slider exist and are editable controls");
        Check(ExtraCount(C_SPINBOX) == 8 + N_LABEL_EXTRAS && ExtraCount(C_SLIDER) == 5 + N_LABEL_EXTRAS && ExtraCount(C_LABEL) == 5 && ExtraCount(C_PROGRESS) == N_LABEL_EXTRAS, "chunk 1: a control has its own extras plus the label group (SuperLabel has only the rich text options: no label group)");
        { DControl d; d.type = C_SPINBOX; const ExtraDef& mn = *FindExtra(C_SPINBOX, "min");
          Check(ExtraSet(d, mn, "-5") && XInt(d, "min") == -5 && d.extra.count("min") && ExtraSet(d, mn, "0") && !d.extra.count("min"), "extras: a value equal to the default is not stored");
          Check(ExtraSet(d, *FindExtra(C_SPINBOX, "step"), "0") && XInt(d, "step") == 1 && !XSet(d, "step"), "extras: numbers are clamped to their limits (step >= 1)");
          Check(!ExtraSet(d, mn, "") && !ExtraSet(d, mn, "-") && !ExtraSet(d, *FindExtra(C_SPINBOX, "wrap"), "maybe") && !ExtraSet(d, *FindExtra(C_SPINBOX, "labelAlign"), "7"), "extras: an invalid value is refused and nothing changes");
          Check(ExtraSet(d, *FindExtra(C_SPINBOX, "labelColor"), "63488") && XInt(d, "labelColor") == 0xF800 && XStr(d, "labelColor") == "0xF800", "extras: colours are kept as 0xRRRR"); }
        Place(C_SPINBOX, 8, 8); Place(C_SLIDER, 8, 80);
        Check(form.items[0].name == "spin1" && form.items[1].name == "slider1" && form.items[0].extra.empty() && form.items[0].h > 8, "chunk 1: placing a spin box / slider (default names, no overrides, the real control's height)");
        Select(0);
        auto XR = [&](const char* key) { for (int i = 0; i < nFields; ++i) if (const ExtraDef* d = ExtraOf(fieldProp[i])) if (std::string(d->key) == key) return i; return -1; };
        auto XClick = [&](const char* key, int dx = 20) { int r = XR(key); EnsureRowVisible(r); Click(FIELD_X + dx, FieldY(r) + 5); };
        Check(XR("value") >= 0 && XR("min") >= 0 && XR("max") >= 0 && XR("step") >= 0 && XR("decimals") >= 0 && XR("wrap") >= 0 && XR("prefix") >= 0 && XR("suffix") >= 0 && XR("label") >= 0 && XR("labelFont") >= 0, "chunk 1: the property panel has a row per extra");
        Check(MaxPropScroll() > 0 && FieldAt(FIELD_X + 5, FieldY(XR("labelColor")) + 5) == -1, "panel: a spin box has more rows than fit (it scrolls) and a row below the panel cannot be clicked");
        { InputEvent w; w.type = InputEvent::WHEEL; w.x = 1200; w.y = 400; w.wheel = -3; Handle(w); Check(propScroll > 0 && propScroll <= MaxPropScroll(), "panel: the wheel over the property panel scrolls it, within its limits");
          w.wheel = 100; Handle(w); Check(propScroll == 0, "panel: scrolling stops at the top"); }
        XClick("min"); Key(SDLK_a, KMOD_LCTRL); Type("-20x"); Key(SDLK_RETURN);
        Check(XInt(form.items[0], "min") == -20 && fields[XR("min")].text == "-20", "chunk 1: a signed number field takes digits and a minus sign");
        XClick("max"); Key(SDLK_a, KMOD_LCTRL); Type("50"); Key(SDLK_RETURN); XClick("step"); Key(SDLK_a, KMOD_LCTRL); Type("5"); Key(SDLK_RETURN);
        XClick("value"); Key(SDLK_a, KMOD_LCTRL); Type("20"); Key(SDLK_RETURN);
        Check(XInt(form.items[0], "max") == 50 && XInt(form.items[0], "step") == 5 && XInt(form.items[0], "value") == 20, "chunk 1: max / step / value are applied");
        XClick("step"); Key(SDLK_a, KMOD_LCTRL); Key(SDLK_BACKSPACE); Key(SDLK_RETURN);
        Check(!XSet(form.items[0], "step") && fields[XR("step")].text == "1", "chunk 1: an empty number goes back to the default");
        XClick("suffix"); Type(" %"); Key(SDLK_RETURN); XClick("decimals"); Key(SDLK_a, KMOD_LCTRL); Type("1"); Key(SDLK_RETURN);
        { auto sp = MakeControl(&scr, form.items[0], 0, 0); Check(static_cast<SuperSpinBox*>(sp.get())->GetText() == "2.0 %", "chunk 1: the real spin box shows the value with its decimal point and suffix (20 -> \"2.0 %\")"); }
        XClick("wrap", FIELD_W - 20); Check(XBool(form.items[0], "wrap") && ToJsonHas(form, "\"wrap\": true"), "chunk 1: clicking Yes on a Boolean row sets it (saved as true)");
        XClick("wrap", 20); Check(!XBool(form.items[0], "wrap") && !ToJsonHas(form, "wrap"), "chunk 1: No is the default and is not saved");
        XClick("label"); Type("Level"); Key(SDLK_RETURN);
        { int h0 = form.items[0].h; XClick("labelPos", FIELD_W - 20); Check(XInt(form.items[0], "labelPos") == 1 && form.items[0].h == h0, "chunk 1: the label position is an enum row (Below)"); }
        XClick("labelAlign", FIELD_W / 2); Check(XInt(form.items[0], "labelAlign") == 1 && ToJsonHas(form, "\"labelAlign\": \"center\""), "chunk 1: the label alignment is saved by name");
        { int hl = form.items[0].h; DControl nolabel = form.items[0]; nolabel.extra.erase("label"); Check(hl > ControlHeight(&scr, nolabel), "chunk 1: a label makes the control taller (the height comes from the real control)"); }
        // label colour and label font go through the colour / font dialogs
        XClick("labelColor"); Check(colorDlg != nullptr && colorDlg->color == 0xFFFF, "chunk 1: the label colour row opens the colour dialog");
        { InputEvent v; v.type = InputEvent::DOWN; v.button = SDL_BUTTON_LEFT; v.clicks = 1; v.x = colorDlg->PalX() + 4; v.y = colorDlg->PalY() + 1 * (ColorDialog::CELL + ColorDialog::GAP) + 4; Handle(v); v.type = InputEvent::UP; Handle(v); }
        Key(SDLK_RETURN); Check(colorDlg == nullptr && XInt(form.items[0], "labelColor") == 0xF800, "chunk 1: OK stores the label colour");
        XClick("labelFont"); Check(fontDlg != nullptr, "chunk 1: the label font row opens the font dialog (on the control's font while unset)");
        Key(SDLK_DOWN); Key(SDLK_DOWN); Key(SDLK_RETURN); Check(fontDlg == nullptr && XSet(form.items[0], "labelFont") && XStr(form.items[0], "labelFont") == Fonts()[FontIndex("Font2") + 2].id, "chunk 1: the chosen label font is stored");
        XClick("labelFont", FIELD_W - 8); Check(!XSet(form.items[0], "labelFont"), "chunk 1: the x clears the label font (back to the control's own)");
        // JSON
        form.items[0].extra["prefix"] = "a\"b\\c"; Select(1);
        { Select(1); XClick("showValue", 20); Check(!XBool(form.items[1], "showValue"), "chunk 1: the slider's Show value switches off"); }
        std::string xj = form.ToJson(), xe; DForm xf;
        Check(xf.FromJson(xj, xe) && xf.items[0].extra == form.items[0].extra && xf.items[1].extra == form.items[1].extra && xf.ToJson() == xj, "model: all extras survive the JSON round trip (numbers, booleans, enums, colours, fonts, text with quotes)");
        Check(xj.find("\"min\": -20") != std::string::npos && xj.find("\"labelPos\": \"below\"") != std::string::npos && xj.find("\"showValue\": false") != std::string::npos && xj.find("\"decimals\": 1") != std::string::npos, "model: extras are written readably (numbers, names, true / false)");
        Check(xf.FromJson("{\"form\":{},\"controls\":[{\"type\":\"SuperSpinBox\",\"min\":\"x\",\"max\":99999999999,\"step\":-4,\"wrap\":7,\"labelPos\":\"sideways\",\"labelAlign\":\"RIGHT\",\"labelFont\":\"NoFont\",\"labelColor\":\"zzz\",\"decimals\":9}]}", xe)
              && XInt(xf.items[0], "min") == 0 && XInt(xf.items[0], "max") == 999999999 && XInt(xf.items[0], "step") == 1 && XBool(xf.items[0], "wrap") && XInt(xf.items[0], "labelPos") == 0
              && XInt(xf.items[0], "labelAlign") == 2 && !XSet(xf.items[0], "labelFont") && XInt(xf.items[0], "decimals") == 6, "model: bad extras in a file are repaired (clamped / ignored), enum names are matched ignoring case");
        // the real controls
        { auto sl = MakeControl(&scr, form.items[1], 0, 0); Check(!static_cast<SuperSlider*>(sl.get())->ShowValue, "chunk 1: the real slider gets Show value = off"); }
        // generated code
        { std::string g = GenerateDesigner(form);
          auto has2 = [&](const char* x) { return g.find(x) != std::string::npos; };
          Check(has2("#include <SuperSpinBox.h>") && has2("#include <SuperSlider.h>") && has2("CtlBase(lgfx::LovyanGFX* dst, SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB)"), "codegen: editable controls bring their headers and the constructor takes both buttons");
          Check(has2("spin1(dst, 8, 8, 100, &lgfx::fonts::Font2, enc, btnA, btnB)") && has2("slider1(dst, 8, 80, 160, &lgfx::fonts::Font2, enc, btnA, btnB)"), "codegen: spin box and slider are built with the encoder and the buttons (and no text argument)");
          Check(has2("spin1.SetRange(-20, 50);") && has2("spin1.SetDecimals(1);") && has2("spin1.SetPrefix(\"a\\\"b\\\\c\");") && has2("spin1.SetSuffix(\" %\");") && has2("spin1.SetValue(20);") && !has2("spin1.SetStep") && !has2("spin1.SetWrap"), "codegen: only the spin box extras that differ from the defaults, escaped for C++");
          Check(g.find("spin1.SetRange") < g.find("spin1.SetValue"), "codegen: the range comes before the value (SetValue clamps)");
          Check(has2("spin1.SetLabel(\"Level\", SuperControl::LABEL_BELOW);") && has2("spin1.SetLabelAlign(SuperControl::ALIGN_CENTER);") && has2("spin1.ColLabel = 0xF800;") && !has2("SetLabelFont"), "codegen: the label (text, position, alignment, colour) and not the unset label font");
          Check(has2("slider1.ShowValue = false;") && !has2("slider1.SetRange") && !has2("slider1.SetValue"), "codegen: a slider with default range / value writes only Show value");
          Check(has2("virtual void spin1_Change() {}") && has2("virtual void spin1_Accept() {}") && has2("virtual void slider1_Change() {}") && has2("virtual void slider1_Accept() {}"), "codegen: spin box and slider have a live Change and an Accept handler");
          Check(has2("spin1.SetOnChange(spin1_ChangeThunk, this);") && has2("spin1.SetOnAccept(spin1_AcceptThunk, this);") && has2("static void spin1_AcceptThunk(void* u)"), "codegen: both events are wired through thunks");
          Check(has2("Form.Add(&spin1);") && has2("Form.Add(&slider1);"), "codegen: spin box and slider take focus");
          std::string u = GenerateUserFile(form);
          Check(u.find("void spin1_Change() override") != std::string::npos && u.find("spin1.GetLiveValue()") != std::string::npos && u.find("spin1.GetValue()") != std::string::npos && u.find("&BtnB") != std::string::npos, "user file: stubs for both events with hints, and the sketch lines pass both buttons"); }
        Check(!ValidName("spin1_Accept") && !ValidName("x_AcceptThunk") && ValidName("accepted"), "names: _Accept handler names are reserved too");
        form.Clear(); FixOutDir(); sel = -1; BuildFields();
        // ---- chunk 2: text box, input box (templates), time box, date box; Fit; refuse bad values ----
        form.Clear(); FixOutDir(); sel = -1; BuildFields(); form.name = "Ch2";
        Check(std::string(Info(C_TEXTBOX).key) == "SuperTextBox" && std::string(Info(C_INPUTBOX).key) == "SuperInputBox" && std::string(Info(C_TIMEBOX).key) == "SuperTimeBox" && std::string(Info(C_DATEBOX).key) == "SuperDateBox", "chunk 2: the four types exist");
        Check(Info(C_INPUTBOX).fitWidth && Info(C_TEXTBOX).fitWidth && !Info(C_SLIDER).fitWidth && !Info(C_TEXTBOX).ctorText && Info(C_LABEL).ctorText && !Info(C_TEXTBOX).editable, "chunk 2: capability flags (fit width, text in the constructor, editable)");
        Place(C_INPUTBOX, 8, 8); Place(C_TIMEBOX, 8, 60); Place(C_DATEBOX, 8, 110); Place(C_TEXTBOX, 8, 160);
        auto X2 = [&](const char* key) { for (int i = 0; i < nFields; ++i) if (const ExtraDef* d = ExtraOf(fieldProp[i])) if (std::string(d->key) == key) return i; return -1; };
        auto R2 = [&](int prop) { for (int i = 0; i < nFields; ++i) if (fieldProp[i] == prop) return i; return -1; };
        auto XT = [&](const char* key, const char* text) { int r = X2(key); EnsureRowVisible(r); Click(FIELD_X + 20, FieldY(r) + 5); Key(SDLK_a, KMOD_LCTRL); Type(text); Key(SDLK_RETURN); };
        auto XS = [&](const char* key, int seg, int n) { int r = X2(key); EnsureRowVisible(r); Click(FIELD_X + seg * (FIELD_W / n) + 6, FieldY(r) + 5); };
        Check(form.items[0].name == "input1" && form.items[1].name == "time1" && form.items[2].name == "date1" && form.items[3].name == "text1" && form.items[0].extra.empty(), "chunk 2: default names, nothing overridden");
        { DControl& ib = form.items[0]; Check(FitWidth(&scr, ib) == ib.w, "fit: a new input box is placed exactly as wide as its text");
          auto ctl = MakeControl(&scr, ib, 0, 0); Check(static_cast<SuperTextBox*>(ctl.get())->GetContentWidth() == ib.w, "fit: the real control agrees (no scroll arrows)"); }
        // ---- input box: template, value ----
        Select(0);
        { int r = R2(P_W); Check(r >= 0 && RowButtons(P_W, (RowBtn[2]){}) == 1, "fit: the Width row of a text-like control has a Fit button"); }
        XT("value", "5551234567"); Check(XStr(form.items[0], "value") == "5551234567", "input: a value that fits the template is accepted");
        XT("value", "55512345678"); Check(XStr(form.items[0], "value") == "5551234567" && !rejectMsg.empty() && fields[X2("value")].text != "5551234567" || focus == -1, "input: a value that is too long is refused (the model keeps the old one)");
        { int r = X2("value"); EnsureRowVisible(r); Click(FIELD_X + 20, FieldY(r) + 5); Key(SDLK_a, KMOD_LCTRL); Type("55x");
          Check(XStr(form.items[0], "value") == "5551234567" && fields[r].text == "55x" && !rejectMsg.empty(), "input: a character a slot cannot hold is refused while typing, the field shows the text red and says why");
          Key(SDLK_RETURN); Check(fields[r].text == "5551234567" && focus == -1, "input: leaving the field puts the model's value back"); }
        XT("value", "42"); Check(XStr(form.items[0], "value") == "0000000042" || XStr(form.items[0], "value") == "42" || XStr(form.items[0], "value").size() <= 10, "input: a short numeric value is accepted");
        XT("value", "5551234567");
        { std::string t0 = XStr(form.items[0], "template"); XT("template", "---"); Check(XStr(form.items[0], "template") == t0 && status.find("slot") != std::string::npos, "input: a template without a slot is refused"); }
        XT("template", "#####"); Check(XStr(form.items[0], "template") == "#####" && !XSet(form.items[0], "value") && status.find("cleared") != std::string::npos, "input: a new template that the value no longer fits clears the value (and says so)");
        XT("value", "12345"); XT("template", "##-###"); Check(XStr(form.items[0], "value") == "12345", "input: a new template that still fits keeps the value");
        XT("template", "AAA-###"); XT("value", "ABC123"); XS("trimRight", 0, 2); Check(!XBool(form.items[0], "trimRight") && XStr(form.items[0], "value") == "ABC123", "input: Trim right can be switched off");
        // Fit
        { int w0 = form.items[0].w; form.items[0].w = 40; Click(FIELD_X + FIELD_W - 20, FieldY(R2(P_W)) + 5); Check(form.items[0].w == w0 || form.items[0].w == FitWidth(&scr, form.items[0]), "fit: the Fit button sets the width that shows the whole text");
          form.items[0].w = 40; SyncFields(); Click(FIELD_X + FIELD_W - 20, FieldY(R2(P_W)) + 5); Check(form.items[0].w == FitWidth(&scr, form.items[0]) && form.items[0].w > 40, "fit: after narrowing it, Fit restores the exact width"); }
        // ---- the presets list ----
        XT("template", "###-###-####"); XT("value", "5551234567");
        { RowBtn rb[2]; RowButtons(X2("template") + P_EXTRA * 0 + 0, rb); int tp = TemplateRowProp(); Check(tp >= P_EXTRA && RowButtons(tp, rb) == 2, "presets: the template row has the Presets and Build buttons"); }
        { int r = X2("template"); EnsureRowVisible(r); Click(FIELD_X + FIELD_W - 150 + 10, FieldY(r) + 5); }
        Check(presetDlg != nullptr && presetDlg->tmpl == "###-###-####", "presets: the Presets button opens the list on the current template");
        Key(SDLK_DOWN); Check(presetDlg->tmpl == "(###) ###-####", "presets: Down moves to the next preset");
        Key(SDLK_ESCAPE); Check(presetDlg == nullptr && XStr(form.items[0], "template") == "###-###-####", "presets: Esc cancels");
        { int r = X2("template"); EnsureRowVisible(r); Click(FIELD_X + FIELD_W - 150 + 10, FieldY(r) + 5); }
        Key(SDLK_DOWN); Key(SDLK_RETURN); Check(presetDlg == nullptr && XStr(form.items[0], "template") == "(###) ###-####" && XStr(form.items[0], "value") == "5551234567", "presets: Enter applies the preset (a value that still fits stays)");
        { int r = X2("template"); EnsureRowVisible(r); Click(FIELD_X + FIELD_W - 150 + 10, FieldY(r) + 5); }
        { SuperListBox* lb = presetDlg->Lb(); Key(SDLK_HOME); int rx = lb->GetX() + lb->GetBorder() + lb->GetPadding() + 8, ry = lb->GetBoxTop() + lb->GetBorder() + lb->GetPadding() + 2 + 2 * 18;
          InputEvent v; v.type = InputEvent::DOWN; v.button = SDL_BUTTON_LEFT; v.clicks = 2; v.x = rx; v.y = ry; Handle(v); v.type = InputEvent::UP; Handle(v); }
        Check(presetDlg == nullptr && XStr(form.items[0], "template") == "#####" && !XSet(form.items[0], "value"), "presets: a double click picks that preset (ZIP code); the value that no longer fits is cleared");
        // ---- the builder ----
        XT("template", "###-###"); XT("value", "123456");
        { int r = X2("template"); EnsureRowVisible(r); Click(FIELD_X + FIELD_W - 70 + 10, FieldY(r) + 5); }
        Check(builderDlg != nullptr && builderDlg->tmpl == "###-###" && builderDlg->Selected() == 0 && builderDlg->Slots() == 6, "builder: the Build button opens the builder on the current template");
        { auto BK = [&](SDL_Keycode k, Uint16 m = 0) { InputEvent v; v.type = InputEvent::KEY_DOWN; v.key = k; v.mod = m; Handle(v); };
          auto BT = [&](const char* tx) { InputEvent v; v.type = InputEvent::TEXT; strncpy(v.text, tx, sizeof v.text - 1); Handle(v); };
          auto BC = [&](int x, int y) { Handle(ev(InputEvent::DOWN, x + 4, y + 3)); Handle(ev(InputEvent::UP, x + 4, y + 3)); };
          BT("A"); Check(builderDlg->tmpl == "A##-###" && builderDlg->Selected() == 1, "builder: typing a slot character sets the cell and moves on");
          BT("a"); BT("B"); BT("*"); Check(builderDlg->tmpl == "AaB*###" || builderDlg->tmpl == "AaB*-###", "builder: a / B / * are slots too");
          BK(SDLK_HOME); BT("("); BT(")"); Check(builderDlg->tmpl.substr(0, 2) == "()" && builderDlg->Slots() == (int)builderDlg->tmpl.size() - 2, "builder: any other character becomes a literal");
          BK(SDLK_END); BT("#"); Check(builderDlg->tmpl.back() == '#', "builder: typing on the + cell appends");
          Check(builderDlg->tmpl.size() == 8 || builderDlg->tmpl.size() == 7 + 1, "builder: the template grows by the appended cell");
          BC(builderDlg->ChipX(0), builderDlg->ChipY(0)); Check(builderDlg->Selected() == 0, "builder: clicking a cell selects it");
          BC(builderDlg->TypeBtnX(1), builderDlg->TypeBtnY()); Check(builderDlg->tmpl[0] == 'A', "builder: the A button makes the cell an upper-case slot");
          BC(builderDlg->TypeBtnX(0), builderDlg->TypeBtnY()); Check(builderDlg->tmpl[0] == '#', "builder: the # button makes it a digit");
          BC(builderDlg->LitX(0), builderDlg->LitY()); Check(builderDlg->tmpl[0] == '-', "builder: a literal button makes it a literal");
          size_t n0 = builderDlg->tmpl.size(); BC(builderDlg->OpX(0), builderDlg->OpY()); Check(builderDlg->tmpl.size() == n0 + 1 && builderDlg->tmpl[0] == '#', "builder: Insert adds a digit cell before the selection");
          BC(builderDlg->OpX(1), builderDlg->OpY()); Check(builderDlg->tmpl.size() == n0, "builder: Delete removes the selected cell");
          std::string before = builderDlg->tmpl; BC(builderDlg->OpX(3), builderDlg->OpY()); Check(builderDlg->tmpl != before && builderDlg->Selected() == 1, "builder: Move right swaps the cell with its neighbour");
          BC(builderDlg->OpX(2), builderDlg->OpY()); Check(builderDlg->tmpl == before && builderDlg->Selected() == 0, "builder: Move left moves it back");
          for (int i = 0; i < 60; ++i) { BK(SDLK_END); BT("#"); } Check((int)builderDlg->tmpl.size() == TemplateBuilder::MAX_CELLS && !builderDlg->state, "builder: a template has at most 40 cells");
          // only literals: OK is refused
          BK(SDLK_HOME); for (int i = 0; i < 45 && builderDlg->Slots() > 0; ++i) { BC(builderDlg->OpX(1), builderDlg->OpY()); } for (int i = 0; i < 3; ++i) BT("-");
          Check(builderDlg->Slots() == 0, "builder: (a template of literals only)");
          BK(SDLK_RETURN); Check(builderDlg != nullptr && builderDlg->state == TemplateBuilder::OPEN, "builder: OK is refused while there is no slot");
          BK(SDLK_HOME); BT("#"); BT("#"); BT("#"); BT("-"); BT("#"); BT("#");
          BC(builderDlg->OkX(), builderDlg->BtnY()); }
        Check(builderDlg == nullptr && XStr(form.items[0], "template").size() >= 5 && XStr(form.items[0], "template").substr(0, 6) == "###-##", "builder: OK applies the template");
        XT("template", "###-###"); { int r = X2("template"); EnsureRowVisible(r); Click(FIELD_X + FIELD_W - 70 + 10, FieldY(r) + 5); }
        Key(SDLK_ESCAPE); Check(builderDlg == nullptr && XStr(form.items[0], "template") == "###-###", "builder: Esc cancels");
        // ---- time box ----
        Select(1);
        XT("value", "7:5"); Check(XStr(form.items[1], "value") == "07:05:00", "time: a value is stored the way the box prints it (7:5 -> 07:05:00)");
        for (const char* bad : { "24:00", "12:61", "abc", "12", "12:30:99", "1:2:3:4", "12-30" }) { XT("value", bad); if (XStr(form.items[1], "value") != "07:05:00") Check(false, (std::string("time: refused: ") + bad).c_str()); }
        Check(XStr(form.items[1], "value") == "07:05:00" && status.find("time") != std::string::npos, "time: impossible times (24:00, 12:61, ...) are refused, with a reason");
        XS("seconds", 0, 2); Check(!XBool(form.items[1], "seconds") && XStr(form.items[1], "value") == "07:05", "time: switching Seconds off stores the value without seconds");
        XS("seconds", 1, 2); Check(XBool(form.items[1], "seconds") && XStr(form.items[1], "value") == "07:05:00", "time: switching it on again");
        XS("wrap", 1, 2); Check(XBool(form.items[1], "wrap"), "time: Wrap around");
        // ---- date box ----
        Select(2);
        XT("value", "2026-2-3"); Check(XStr(form.items[2], "value") == "2026-02-03", "date: a value is stored as YYYY-MM-DD (2026-2-3 -> 2026-02-03)");
        for (const char* bad : { "2026-02-30", "2026-13-01", "1969-01-01", "2100-01-01", "x", "2026/02/03", "2026-02" }) { XT("value", bad); if (XStr(form.items[2], "value") != "2026-02-03") Check(false, (std::string("date: refused: ") + bad).c_str()); }
        Check(XStr(form.items[2], "value") == "2026-02-03" && status.find("date") != std::string::npos, "date: impossible dates and dates outside the year range are refused, with a reason");
        XT("value", "2024-02-29"); Check(XStr(form.items[2], "value") == "2024-02-29", "date: a leap day is accepted in a leap year");
        XT("value", "2025-02-29"); Check(XStr(form.items[2], "value") == "2024-02-29", "date: 29 February is refused in a year that is not a leap year");
        for (const char* bad : { "", "ab", "5", " " }) { XT("separator", bad); if (XStr(form.items[2], "separator") != "-") Check(false, (std::string("date: separator refused: ") + bad).c_str()); }
        XT("separator", "/"); Check(XStr(form.items[2], "separator") == "/", "date: a one-character separator is accepted"); XT("separator", "-");
        XT("yearMax", "2000"); Check(XInt(form.items[2], "yearMax") == 2099, "date: a year range that excludes the value is refused");
        XT("value", "1999-05-05"); XT("yearMax", "2000"); Check(XInt(form.items[2], "yearMax") == 2000 && XStr(form.items[2], "value") == "1999-05-05", "date: the year range is applied when the value fits");
        XT("yearMin", "2001"); Check(XInt(form.items[2], "yearMin") == 1970, "date: Year from after Year to is refused");
        XS("style", 1, 2); Check(XInt(form.items[2], "style") == 1 && ToJsonHas(form, "\"style\": \"m-d-y\""), "date: the style is a choice (saved as m-d-y)");
        // ---- text box ----
        Select(3); Check(R2(P_TEXT) >= 0 && R2(P_ALIGN) >= 0 && X2("label") >= 0, "text box: Text, Align and the label group");
        { int r = R2(P_TEXT); Click(FIELD_X + 20, FieldY(r) + 5); Key(SDLK_a, KMOD_LCTRL); Type("Ready"); Key(SDLK_RETURN); Check(form.items[3].text == "Ready", "text box: the text is applied"); }
        // ---- JSON and generated code ----
        { std::string cj = form.ToJson(), ce; DForm cf; Check(cf.FromJson(cj, ce) && cf.ToJson() == cj && cf.items[2].extra == form.items[2].extra && cf.items[0].extra == form.items[0].extra, "model: chunk 2 extras survive the JSON round trip"); }
        form.items[0].extra.clear(); form.items[1].extra.clear(); form.items[2].extra.clear();
        { DControl& ib = form.items[0]; ExtraSet(ib, *FindExtra(C_INPUTBOX, "template"), "AAA-###"); ExtraSet(ib, *FindExtra(C_INPUTBOX, "value"), "ABC123"); ExtraSet(ib, *FindExtra(C_INPUTBOX, "trimRight"), "false"); ExtraSet(ib, *FindExtra(C_INPUTBOX, "literalColor"), "0x07E0");
          ExtraSet(form.items[1], *FindExtra(C_TIMEBOX, "seconds"), "false"); ExtraSet(form.items[1], *FindExtra(C_TIMEBOX, "value"), "07:30"); ExtraSet(form.items[1], *FindExtra(C_TIMEBOX, "wrap"), "true");
          ExtraSet(form.items[2], *FindExtra(C_DATEBOX, "style"), "1"); ExtraSet(form.items[2], *FindExtra(C_DATEBOX, "separator"), "/"); ExtraSet(form.items[2], *FindExtra(C_DATEBOX, "yearMin"), "2000"); ExtraSet(form.items[2], *FindExtra(C_DATEBOX, "value"), "2026-12-31"); }
        { std::string g = GenerateDesigner(form); auto h3 = [&](const char* x) { return g.find(x) != std::string::npos; };
          Check(h3("#include <SuperTextBox.h>") && h3("#include <SuperInputBox.h>") && h3("#include <SuperTimeBox.h>") && g.find("#include <SuperTimeBox.h>") == g.rfind("#include <SuperTimeBox.h>"), "codegen: the headers of the new types (SuperTimeBox.h once for time and date)");
          Check(h3("input1(dst, 8, 8, ") && h3(", &lgfx::fonts::Font2, \"AAA-###\", enc, btnA, btnB)"), "codegen: the input box template is the argument before the encoder and buttons");
          Check(h3(("time1(dst, 8, " + std::to_string(form.items[1].y) + ", ").c_str()) && h3("enc, btnA, btnB, false)") && h3(("date1(dst, 8, " + std::to_string(form.items[2].y) + ", ").c_str()) && h3("enc, btnA, btnB, SuperDateBox::DATE_MDY)"), "codegen: seconds = false and the M-D-Y style are constructor arguments");
          Check(h3(("text1(dst, 8, " + std::to_string(form.items[3].y) + ", 120, &lgfx::fonts::Font2)").c_str()) && h3("text1.SetText(\"Ready\");") && !h3(", &lgfx::fonts::Font2, \"Ready\")"), "codegen: the text box text is set with SetText, not in the constructor");
          Check(h3("input1.SetValue(\"ABC123\");") && h3("input1.TrimRight = false;") && h3("input1.ColLiteral = 0x07E0;"), "codegen: input box value, Trim right and literal colour");
          Check(h3("time1.SetWrap(true);") && h3("time1.SetValue(\"07:30\");") && h3("date1.SetSeparator('/');") && h3("date1.SetYearRange(2000, 2099);") && h3("date1.SetValue(\"2026-12-31\");"), "codegen: time / date settings; the date value comes after the year range");
          Check(g.find("date1.SetYearRange") < g.find("date1.SetValue(") && !h3("date1.SetWrap"), "codegen: order, and no Wrap when it is off");
          Check(h3("Form.Add(&input1);") && h3("Form.Add(&time1);") && h3("Form.Add(&date1);") && h3("Form.Add(&text1, false);"), "codegen: the text box is display only (not focusable), the others take focus");
          Check(h3("virtual void input1_Change() {}") && h3("virtual void date1_Accept() {}") && !h3("text1_"), "codegen: input / time / date have Change and Accept handlers; the text box none"); }
        { auto dc = MakeControl(&scr, form.items[2], 0, 0); Check(static_cast<SuperTextBox*>(dc.get())->GetText() == "12/31/2026", "chunk 2: the real date box shows M-D-Y with the separator (12/31/2026)");
          auto tc = MakeControl(&scr, form.items[1], 0, 0); Check(static_cast<SuperTextBox*>(tc.get())->GetText() == "07:30", "chunk 2: the real time box without seconds shows 07:30");
          auto ic = MakeControl(&scr, form.items[0], 0, 0); Check(static_cast<SuperTextBox*>(ic.get())->GetText() == "ABC-123", "chunk 2: the real input box shows the template with the value"); }
        form.Clear(); FixOutDir(); sel = -1; BuildFields();
        // ---- check box / radio button: no label group, an initial state ----
        form.Clear(); FixOutDir(); sel = -1; BuildFields(); form.name = "Chk";
        Check(ExtraCount(C_CHECKBOX) == 1 && ExtraCount(C_RADIO) == 1 && ExtraCount(C_PUSHBUTTON) == 0 && !Info(C_CHECKBOX).hasLabel && !Info(C_RADIO).hasLabel && !Info(C_PUSHBUTTON).hasLabel, "controls with a caption of their own (check box, radio button, push button) have no label group");
        Place(C_CHECKBOX, 8, 8); Place(C_RADIO, 8, 40); Place(C_RADIO, 8, 72); Place(C_RADIO, 8, 104); Place(C_PUSHBUTTON, 8, 136);
        auto CX = [&](const char* key) { for (int i = 0; i < nFields; ++i) if (const ExtraDef* d = ExtraOf(fieldProp[i])) if (std::string(d->key) == key) return i; return -1; };
        auto CS = [&](const char* key, int seg) { int r = CX(key); EnsureRowVisible(r); Click(FIELD_X + seg * (FIELD_W / 2) + 6, FieldY(r) + 5); };
        Select(0); Check(CX("checked") >= 0 && CX("label") < 0 && CX("labelFont") < 0 && CX("labelColor") < 0 && PropLabel(fieldProp[CX("checked")]) == "Checked", "check box: a Checked row, no label rows");
        Select(1); Check(CX("checked") >= 0 && PropLabel(fieldProp[CX("checked")]) == "Selected" && CX("label") < 0, "radio button: a Selected row, no label rows");
        Select(4); Check(CX("label") < 0 && CX("checked") < 0 && R2(P_TEXT) >= 0, "push button: no label rows (its Text is its caption)");
        Select(0); Check(!XBool(form.items[0], "checked") && form.items[0].extra.empty(), "check box: unchecked by default (nothing stored)");
        { auto cc = MakeControl(&scr, form.items[0], 0, 0); Check(!static_cast<SuperCheckBox*>(cc.get())->GetChecked(), "check box: the real control starts unchecked"); }
        CS("checked", 1); Check(XBool(form.items[0], "checked") && ToJsonHas(form, "\"checked\": true"), "check box: Yes sets the initial state (saved as true)");
        { auto cc = MakeControl(&scr, form.items[0], 0, 0); Check(static_cast<SuperCheckBox*>(cc.get())->GetChecked(), "check box: the real control is drawn checked"); }
        { std::string g = GenerateDesigner(form); Check(g.find("check1.SetChecked(true);") != std::string::npos && g.find("check1.SetChecked(true);") < g.find("check1.SetOnChange"), "check box: the generated code sets it silently, before the event is wired"); }
        CS("checked", 0); Check(!XBool(form.items[0], "checked") && !ToJsonHas(form, "checked"), "check box: No is the default again (not saved)");
        { std::string g = GenerateDesigner(form); Check(g.find("SetChecked") == std::string::npos, "check box: nothing is generated for an unchecked box"); }
        // radio buttons: one selected per group
        Select(1); CS("checked", 1); Check(XBool(form.items[1], "checked"), "radio: Selected = Yes");
        Select(2); CS("checked", 1); Check(XBool(form.items[2], "checked") && !XBool(form.items[1], "checked"), "radio: selecting another button of the group deselects the first");
        { int n = 0; for (auto& c : form.items) if (c.type == C_RADIO && XBool(c, "checked")) ++n; Check(n == 1, "radio: only one button per group is selected"); }
        { auto rc = MakeControl(&scr, form.items[2], 0, 0); Check(static_cast<SuperCheckBox*>(rc.get())->GetChecked(), "radio: the real radio button is drawn selected"); }
        { std::string g = GenerateDesigner(form); Check(g.find("radio2.SetChecked(true);") != std::string::npos && g.find("radio2.SetChecked(true);") < g.find("group1.Add(&radio2);") && g.find("radio1.SetChecked") == std::string::npos, "radio: only the selected button is generated, before it joins the group"); }
        // another group keeps its own selection; a selected button joining a group with a selection does not take it over
        Select(3); { int r = -1; for (int i = 0; i < nFields; ++i) if (fieldProp[i] == P_GROUP) r = i; Click(FIELD_X + 20, FieldY(r) + 5); Key(SDLK_a, KMOD_LCTRL); Type("modes"); Key(SDLK_RETURN); }
        CS("checked", 1); Check(XBool(form.items[3], "checked") && XBool(form.items[2], "checked"), "radio: a different group has its own selection");
        Select(3); { int r = -1; for (int i = 0; i < nFields; ++i) if (fieldProp[i] == P_GROUP) r = i; Click(FIELD_X + 20, FieldY(r) + 5); Key(SDLK_a, KMOD_LCTRL); Type("group1"); Key(SDLK_RETURN); }
        Check(form.items[3].group == "group1" && !XBool(form.items[3], "checked") && XBool(form.items[2], "checked"), "radio: moving a selected button into a group that already has a selection does not take it over");
        // files
        { std::string cj = form.ToJson(), ce; DForm cf; Check(cf.FromJson(cj, ce) && cf.items[2].extra == form.items[2].extra && XBool(cf.items[2], "checked") && cf.ToJson() == cj, "model: the initial state survives the JSON round trip");
          Check(cf.FromJson("{\"form\":{},\"controls\":[{\"type\":\"SuperRadioButton\",\"name\":\"a\",\"checked\":true},{\"type\":\"SuperRadioButton\",\"name\":\"b\",\"checked\":true},{\"type\":\"SuperRadioButton\",\"name\":\"c\",\"group\":\"other\",\"checked\":true}]}", ce)
                && XBool(cf.items[0], "checked") && !XBool(cf.items[1], "checked") && XBool(cf.items[2], "checked"), "model: two selected radio buttons in one group in a file: the first wins, other groups are untouched");
          Check(cf.FromJson("{\"form\":{},\"controls\":[{\"type\":\"SuperCheckBox\",\"name\":\"k\",\"label\":\"old\",\"labelColor\":\"0xF800\",\"checked\":true}]}", ce) && XBool(cf.items[0], "checked") && !cf.items[0].extra.count("label") && !cf.items[0].extra.count("labelColor"), "model: label settings on a check box in an older file are ignored"); }
        form.Clear(); FixOutDir(); sel = -1; BuildFields();
        // ---- the list controls' run-time API (library additions) ----
        {
            SuperListBox lb(&scr, 0, 0, 100, &lgfx::fonts::Font2, 3, nullptr, nullptr, nullptr); lb.SetVisible(false, false);
            lb.InsertItem(0, "a"); Check(lb.Count() == 1 && lb.GetSelected() == 0 && lb.GetItem(0) == "a", "library ListBox: InsertItem into an empty list selects the new item");
            lb.AddItem("b"); lb.AddItem("c"); lb.SetSelected(1);
            lb.InsertItem(0, "z"); Check(lb.Count() == 4 && lb.GetItem(0) == "z" && lb.GetSelectedText() == "b" && lb.GetSelected() == 2, "library ListBox: inserting before the selection keeps the selection on its item");
            lb.InsertItem(99, "end"); Check(lb.GetItem(4) == "end" && lb.GetSelectedText() == "b", "library ListBox: an index past the end appends");
            lb.InsertItem(-5, "first"); Check(lb.GetItem(0) == "first" && lb.GetSelectedText() == "b", "library ListBox: a negative index inserts at the start");
            lb.RemoveItem(0); Check(lb.GetSelectedText() == "b" && lb.Count() == 5, "library ListBox: removing an item before the selection keeps it");
            lb.SetSelected(2); lb.RemoveItem(2); Check(lb.GetSelected() == 2 && lb.GetSelectedText() == "c", "library ListBox: removing the selected item selects the one that moved into its place");
            lb.SetSelected(lb.Count() - 1); lb.RemoveItem(lb.Count() - 1); Check(lb.GetSelected() == lb.Count() - 1, "library ListBox: removing the selected last item selects the new last one");
            lb.RemoveItem(77); lb.RemoveItem(-1); lb.SetItem(77, "x"); Check(lb.Count() == 3, "library ListBox: bad indexes are ignored");
            lb.SetItem(1, "renamed"); Check(lb.GetItem(1) == "renamed", "library ListBox: SetItem renames");
            while (lb.Count()) lb.RemoveItem(0); Check(lb.GetSelected() == -1 && lb.GetSelectedText() == "" && lb.GetCursor() == -1, "library ListBox: removing every item leaves no selection");
            SuperComboBox cb(&scr, 0, 0, 120, &lgfx::fonts::Font2, nullptr, nullptr, nullptr); cb.SetVisible(false, false);
            cb.SetItems({ "Small", "Medium", "Large" }); cb.SetSelected(1);
            Check(cb.GetItem(2) == "Large" && cb.GetItem(9) == "" && cb.GetText() == "Medium", "library ComboBox: GetItem(i), and the box shows the selected item");
            cb.InsertItem(0, "Tiny"); Check(cb.GetValue() == "Medium" && cb.GetText() == "Medium" && cb.GetSelected() == 2, "library ComboBox: InsertItem keeps the selection (and the shown text)");
            cb.SetItem(2, "Mid"); Check(cb.GetValue() == "Mid" && cb.GetText() == "Mid", "library ComboBox: SetItem on the selected item updates the shown text");
            cb.RemoveItem(2); Check(cb.GetValue() == "Large" && cb.GetText() == "Large" && cb.Count() == 3, "library ComboBox: RemoveItem of the selected item shows the next one");
            SuperTabBar tb(&scr, 0, 0, 200, &lgfx::fonts::Font2, nullptr, nullptr, nullptr); tb.SetVisible(false, false);
            tb.SetItems({ "A", "B", "C" }); tb.SetSelected(1); Check(tb.Count() == 3 && tb.GetItem(1) == "B" && tb.GetTab(1) == "B" && tb.GetSelected() == 1, "library TabBar: SetItems / GetItem (GetTab is the same)");
            tb.InsertItem(0, "Z"); Check(tb.GetSelected() == 2 && tb.GetItem(2) == "B", "library TabBar: InsertItem keeps the selected tab");
            tb.RemoveItem(2); Check(tb.GetSelected() == 2 && tb.GetItem(2) == "C", "library TabBar: removing the selected tab selects the one that took its place");
            tb.SetItem(0, "Zed"); tb.SetItems({ "only" }); Check(tb.Count() == 1 && tb.GetSelected() == 0, "library TabBar: SetItems keeps a valid selection");
            tb.SetItems({}); Check(tb.GetSelected() == -1, "library TabBar: an empty list has no selection");
            SuperMenu mn(&scr, 0, 0, 160, &lgfx::fonts::Font2, 4, nullptr, nullptr, nullptr); mn.SetVisible(false, false);
            auto* sys = mn.AddItem("System", 0); auto* reb = mn.AddItem("Reboot", 7, sys); auto* off = mn.AddItem("Power off", 8, sys); auto* about = mn.AddItem("About", 20);
            Check(mn.Root()->Kids.size() == 2 && mn.Root()->Kids[0] == sys && sys->Kids.size() == 2, "library Menu: Root() gives the top level");
            Check(mn.FindById(8) == off && mn.FindById(20) == about && mn.FindById(99) == nullptr && mn.FindById(7)->Text == "Reboot", "library Menu: FindById searches the whole tree");
            Check(mn.RemoveItem(reb) && sys->Kids.size() == 1 && mn.FindById(7) == nullptr, "library Menu: RemoveItem removes a leaf");
            Check(mn.RemoveItem(sys) && mn.Root()->Kids.size() == 1 && mn.FindById(8) == nullptr && mn.Root()->Kids[0] == about, "library Menu: RemoveItem removes an item with everything below it");
            Check(!mn.RemoveItem(sys) && !mn.RemoveItem(nullptr) && !mn.RemoveItem(const_cast<SuperMenu::Item*>(mn.Root())), "library Menu: removing twice, nothing, or the root is refused");
            auto* again = mn.AddItem("New", 30); Check(mn.FindById(30) == again && (again == sys || again == reb || again == off), "library Menu: AddItem reuses the memory of removed items");
            mn.Clear(); Check(mn.Root()->Kids.empty() && mn.FindById(20) == nullptr, "library Menu: Clear empties it");
        }
        // ---- chunk 3a: list box, combo box, tab bar ----
        form.Clear(); FixOutDir(); sel = -1; BuildFields(); form.name = "Ch3";
        Check(std::string(Info(C_LISTBOX).key) == "SuperListBox" && std::string(Info(C_COMBOBOX).key) == "SuperComboBox" && std::string(Info(C_TABBAR).key) == "SuperTabBar" && HasList(C_LISTBOX) && HasList(C_COMBOBOX) && HasList(C_TABBAR) && !HasList(C_SPINBOX), "chunk 3a: the three list types exist");
        Place(C_LISTBOX, 8, 8); Place(C_COMBOBOX, 8, 120); Place(C_TABBAR, 8, 160);
        Check(form.items[0].list == std::vector<std::string>({ "Item 1", "Item 2", "Item 3" }) && form.items[2].list == std::vector<std::string>({ "Tab 1", "Tab 2", "Tab 3" }), "chunk 3a: new lists start with a few sample items (tabs are Tab 1, 2, 3)");
        auto L3 = [&](int prop) { for (int i = 0; i < nFields; ++i) if (fieldProp[i] == prop) return i; return -1; };
        auto X3 = [&](const char* key) { for (int i = 0; i < nFields; ++i) if (const ExtraDef* d = ExtraOf(fieldProp[i])) if (std::string(d->key) == key) return i; return -1; };
        auto XT3 = [&](const char* key, const char* text) { int r = X3(key); EnsureRowVisible(r); Click(FIELD_X + 20, FieldY(r) + 5); Key(SDLK_a, KMOD_LCTRL); Type(text); Key(SDLK_RETURN); };
        auto OpenItems = [&]() { int r = L3(P_ITEMS); EnsureRowVisible(r); Click(FIELD_X + FIELD_W - 30, FieldY(r) + 5); };
        Select(0); Check(L3(P_ITEMS) >= 0 && X3("selected") >= 0 && X3("rows") >= 0 && X3("wrap") >= 0 && X3("rowPad") >= 0 && PropLabel(P_ITEMS) == "Items" && X3("label") >= 0, "chunk 3a: list box rows: Items, Selected, Visible rows, Wrap around, Row padding (+ label group)");
        Select(1); Check(X3("maxRows") >= 0 && X3("restore") >= 0 && X3("selected") >= 0, "chunk 3a: combo box rows: Selected, Popup rows, Popup restore");
        Select(2); Check(PropLabel(P_ITEMS) == "Tabs" && X3("tabPad") >= 0, "chunk 3a: tab bar rows: Tabs, Selected, Tab padding");
        Select(0);
        { auto lc = MakeControl(&scr, form.items[0], 0, 0); auto* l = static_cast<SuperListBox*>(lc.get()); Check(l->Count() == 3 && l->GetItem(1) == "Item 2" && l->GetRows() == 5, "chunk 3a: the real list box is built from the model (items, rows)"); }
        XT3("selected", "2"); Check(XInt(form.items[0], "selected") == 2, "chunk 3a: Selected is applied");
        XT3("selected", "3"); Check(XInt(form.items[0], "selected") == 2 && status.find("3 items") != std::string::npos, "chunk 3a: a Selected beyond the last item is refused, with the reason");
        XT3("rows", "3"); XT3("rowPad", "2"); { int r = X3("wrap"); EnsureRowVisible(r); Click(FIELD_X + FIELD_W - 20, FieldY(r) + 5); }
        Check(XInt(form.items[0], "rows") == 3 && XInt(form.items[0], "rowPad") == 2 && XBool(form.items[0], "wrap"), "chunk 3a: rows, row padding and wrap");
        // ---- the items dialog ----
        OpenItems(); Check(itemsDlg != nullptr && itemsDlg->items.size() == 3 && itemsDlg->Selected() == 2, "items dialog: the Edit button opens it on the control's items, with the selected item highlighted");
        { int g0 = grid; Key(SDLK_g); Check(itemsDlg != nullptr && grid == g0 && itemsDlg->items.size() == 3, "items dialog: modal (keys do not reach the form)"); }
        auto IK = [&](SDL_Keycode k, Uint16 m = 0) { InputEvent v; v.type = InputEvent::KEY_DOWN; v.key = k; v.mod = m; Handle(v); };
        auto IT = [&](const char* tx) { InputEvent v; v.type = InputEvent::TEXT; strncpy(v.text, tx, sizeof v.text - 1); Handle(v); };
        auto IB = [&](int row, int k) { int x = itemsDlg->BtnX(row, k) + 6, y = (row == 1 ? itemsDlg->Row1Y() : itemsDlg->Row2Y()) + 3; Handle(ev(InputEvent::DOWN, x, y)); Handle(ev(InputEvent::UP, x, y)); };
        { SuperListBox* lb = itemsDlg->Lb(); int rx = lb->GetX() + lb->GetBorder() + lb->GetPadding() + 8, ry = lb->GetBoxTop() + lb->GetBorder() + lb->GetPadding() + 2;
          Handle(ev(InputEvent::DOWN, rx, ry)); Handle(ev(InputEvent::UP, rx, ry)); Check(itemsDlg->Selected() == 0 && itemsDlg->FieldText() == "Item 1", "items dialog: clicking an item selects it and shows its text"); }
        IK(SDLK_TAB); Check(itemsDlg->FieldFocused(), "items dialog: Tab goes to the text field");
        IK(SDLK_a, KMOD_LCTRL); IT("Red"); Check(itemsDlg->items[0] == "Red" && itemsDlg->Lb()->GetItem(0) == "Red", "items dialog: typing changes the item live (and the list)");
        IK(SDLK_a, KMOD_LCTRL); IK(SDLK_BACKSPACE); Check(itemsDlg->items[0] == "Red" && !itemsDlg->Message().empty(), "items dialog: an empty item is refused: the old text stays, with a reason");
        IT("Red!"); Check(itemsDlg->items[0] == "Red!", "items dialog: typing again is accepted"); IK(SDLK_BACKSPACE);
        IK(SDLK_RETURN); Check(itemsDlg->items.size() == 4 && itemsDlg->Selected() == 1 && itemsDlg->FieldFocused() && itemsDlg->items[1].substr(0, 5) == "Item ", "items dialog: Enter in the field adds the next item (fast entry) and selects it");
        IK(SDLK_a, KMOD_LCTRL); IT("Green"); IK(SDLK_RETURN); IK(SDLK_a, KMOD_LCTRL); IT("Blue"); Check(itemsDlg->items.size() == 5 && itemsDlg->items[2] == "Blue", "items dialog: typing a list with Enter between the items");
        IK(SDLK_ESCAPE); Check(!itemsDlg->FieldFocused() && itemsDlg != nullptr, "items dialog: Esc in the field goes back to the list");
        IB(1, 1); Check(itemsDlg->items.size() == 6 && itemsDlg->Selected() == 2, "items dialog: Insert adds before the selected item");
        IB(1, 2); Check(itemsDlg->items.size() == 5 && itemsDlg->items[2] == "Blue", "items dialog: Delete removes the selected item");
        IB(1, 3); Check(itemsDlg->items[1] == "Blue" && itemsDlg->items[2] == "Green" && itemsDlg->Selected() == 1, "items dialog: Move up swaps with the item above");
        IB(1, 4); Check(itemsDlg->items[2] == "Blue" && itemsDlg->Selected() == 2, "items dialog: Move down");
        IB(2, 0); { bool sorted = true; for (size_t i = 1; i < itemsDlg->items.size(); ++i) { std::string a = itemsDlg->items[i - 1], b = itemsDlg->items[i]; for (char& c : a) c = (char)tolower((unsigned char)c); for (char& c : b) c = (char)tolower((unsigned char)c); if (a > b) sorted = false; } Check(sorted && itemsDlg->items[itemsDlg->Selected()] == "Blue", "items dialog: Sort A-Z (ignoring case) keeps the selected item selected"); }
        { char* oldClip = SDL_GetClipboardText(); SDL_SetClipboardText("Cyan\r\n\r\n  Magenta  \nYellow\n\tx\xC3\xA9y\n");
          size_t n0 = itemsDlg->items.size(); IB(2, 1);
          Check(itemsDlg->items.size() == n0 + 4, "items dialog: Paste lines adds every non-empty line of the clipboard");
          bool found = false; for (auto& it : itemsDlg->items) if (it == "Magenta") found = true; bool ascii = true; for (auto& it : itemsDlg->items) for (char c : it) if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7E) ascii = false;
          Check(found && ascii, "items dialog: pasted lines are trimmed and non-ASCII characters are dropped");
          SDL_SetClipboardText(""); size_t n1 = itemsDlg->items.size(); IB(2, 1); Check(itemsDlg->items.size() == n1 && !itemsDlg->Message().empty(), "items dialog: an empty clipboard adds nothing and says so");
          if (oldClip) { SDL_SetClipboardText(oldClip); SDL_free(oldClip); } }
        IB(2, 2); Check(itemsDlg->items.empty() && itemsDlg->Selected() == -1, "items dialog: Clear all");
        for (int i = 0; i < 205; ++i) IB(1, 0);
        Check((int)itemsDlg->items.size() == MAX_LIST_ITEMS && itemsDlg->Message().find("200") != std::string::npos, "items dialog: at most 200 items");
        IB(2, 2); IB(1, 0); IK(SDLK_a, KMOD_LCTRL); IT("A"); IK(SDLK_RETURN); IK(SDLK_a, KMOD_LCTRL); IT("B"); IK(SDLK_ESCAPE); IK(SDLK_ESCAPE);
        Check(itemsDlg == nullptr && form.items[0].list.size() == 3 && form.items[0].list[0] == "Item 1", "items dialog: Esc (in the list) cancels: the control keeps its items");
        OpenItems(); IB(2, 2); IB(1, 0); IK(SDLK_a, KMOD_LCTRL); IT("Red"); IK(SDLK_RETURN); IK(SDLK_a, KMOD_LCTRL); IT("Green"); IK(SDLK_RETURN); IK(SDLK_a, KMOD_LCTRL); IT("Blue"); IK(SDLK_ESCAPE);
        { int x = itemsDlg->OkX() + 5, y = itemsDlg->BtnY() + 3; Handle(ev(InputEvent::DOWN, x, y)); Handle(ev(InputEvent::UP, x, y)); }
        Check(itemsDlg == nullptr && form.items[0].list == std::vector<std::string>({ "Red", "Green", "Blue" }), "items dialog: OK applies the new items");
        Check(XInt(form.items[0], "selected") == 2, "items dialog: (the selection stays on a valid item)");
        OpenItems(); IB(2, 2); IB(1, 0); IK(SDLK_a, KMOD_LCTRL); IT("Only"); IK(SDLK_ESCAPE); IK(SDLK_RETURN);
        Check(form.items[0].list.size() == 1 && !XSet(form.items[0], "selected"), "items dialog: after shrinking the list the selection is pulled back to a valid item (here: the default 0)");
        OpenItems(); IB(2, 2); IK(SDLK_RETURN); Check(form.items[0].list.empty() && !XSet(form.items[0], "selected") && ControlHeight(&scr, form.items[0]) > 0, "items dialog: an empty list is allowed");
        // ---- JSON and generated code ----
        form.items[0].list = { "Red", "Gr\"een", "Bl\\ue" }; ExtraSet(form.items[0], *FindExtra(C_LISTBOX, "selected"), "2"); form.items[1].list = { "Small", "Large" }; form.items[2].list = { "Home", "Settings" };
        ExtraSet(form.items[2], *FindExtra(C_TABBAR, "selected"), "1");
        { std::string lj = form.ToJson(), le; DForm lf; Check(lf.FromJson(lj, le) && lf.items[0].list == form.items[0].list && lf.items[1].list == form.items[1].list && lf.ToJson() == lj && lj.find("\"items\": [\"Red\", \"Gr\\\"een\", \"Bl\\\\ue\"]") != std::string::npos, "model: the items survive the JSON round trip, readable and escaped");
          std::string longItem(80, 'x');
          Check(lf.FromJson("{\"form\":{},\"controls\":[{\"type\":\"SuperListBox\",\"items\":[\"ok\",\"\",\"caf\\u00e9\",\"" + longItem + "\",5,\"last\"],\"selected\":9}]}", le), "model: (a file with odd items)");
          Check(lf.items[0].list == std::vector<std::string>({ "ok", "caf", std::string(MAX_ITEM_LEN, 'x'), "last" }) && XInt(lf.items[0], "selected") == 3, "model: empty and non-string items are dropped, \\u escapes above ASCII are dropped, long items cut, and a Selected past the end is pulled back");
          std::string many = "{\"form\":{},\"controls\":[{\"type\":\"SuperTabBar\",\"items\":["; for (int i = 0; i < 300; ++i) many += (i ? ",\"t" : "\"t") + std::to_string(i) + "\""; many += "]}]}";
          Check(lf.FromJson(many, le) && (int)lf.items[0].list.size() == MAX_LIST_ITEMS, "model: at most 200 items are read"); }
        { std::string g = GenerateDesigner(form); auto h4 = [&](const char* x) { return g.find(x) != std::string::npos; };
          Check(h4("#include <SuperListBox.h>") && h4("#include <SuperComboBox.h>") && h4("#include <SuperTabBar.h>"), "codegen: headers of the list controls");
          Check(h4(("list1(dst, 8, " + std::to_string(form.items[0].y) + ", ").c_str()) && h4(", &lgfx::fonts::Font2, 3, enc, btnA, btnB)"), "codegen: the list box takes its visible rows before the encoder and buttons");
          Check(h4("list1.SetItems({ \"Red\", \"Gr\\\"een\", \"Bl\\\\ue\" });") && h4("list1.SetSelected(2);") && g.find("list1.SetItems") < g.find("list1.SetSelected"), "codegen: the items (escaped for C++), then the selection");
          Check(h4("list1.Wrap = true;") && h4("list1.RowPad = 2;"), "codegen: Wrap and Row padding when they differ from the defaults");
          Check(h4("combo1.SetItems({ \"Small\", \"Large\" });") && h4("combo1.SetRestore(combo1_RestoreThunk, this);") && h4("static void combo1_RestoreThunk(void* u)") && h4("s->Form.Redraw(s->combo1.GetPopupX(), s->combo1.GetPopupY(), s->combo1.GetPopupWidth(), s->combo1.GetPopupHeight(), FormBackColor);"), "codegen: the combo box popup is restored by redrawing the form (default)");
          Check(h4("tabs1.SetItems({ \"Home\", \"Settings\" });") && h4("tabs1.SetSelected(1);"), "codegen: the tab bar");
          Check(h4("virtual void list1_Change() {}") && h4("virtual void combo1_Accept() {}") && h4("virtual void tabs1_Accept() {}") && h4("list1.SetOnChange(list1_ChangeThunk, this);"), "codegen: Change and Accept handlers for all three");
          ExtraSet(form.items[1], *FindExtra(C_COMBOBOX, "restore"), "1"); ExtraSet(form.items[1], *FindExtra(C_COMBOBOX, "maxRows"), "8"); std::string g2 = GenerateDesigner(form);
          Check(g2.find("SetRestore") == std::string::npos && g2.find("combo1_RestoreThunk") == std::string::npos && g2.find("combo1.MaxRows = 8;") != std::string::npos, "codegen: 'Read back' writes no restore code; Popup rows becomes MaxRows");
          std::string u = GenerateUserFile(form);
          Check(u.find("list1.GetSelectedText()") != std::string::npos && u.find("combo1.GetValue()") != std::string::npos && u.find("tabs1.GetSelected()") != std::string::npos, "user file: handler hints for the list controls"); }
        Check(!ValidName("combo1_RestoreThunk") && !ValidName("x_Accept"), "names: _RestoreThunk is reserved too");
        form.Clear(); FixOutDir(); sel = -1; BuildFields();
        // ---- chunk 3b: the menu ----
        {
            Check(std::string(Info(C_MENU).key) == "SuperMenu" && ExtraCount(C_MENU) == 5 + N_LABEL_EXTRAS && !HasList(C_MENU), "menu: the type exists (rows, wrap, back row, row padding, ID prefix + the label group)");
            Check(MenuUpper("Power off") == "POWER_OFF" && MenuUpper("  a--b  ") == "A_B" && MenuUpper("!!!") == "" && MenuUpper("Save as...") == "SAVE_AS", "menu: item text becomes an upper-case name part");
            Check(AutoItemId("MNU_", "Power off", [](const std::string&) { return false; }) == "MNU_POWER_OFF" && AutoItemId("MNU_", "!!!", [](const std::string&) { return false; }) == "MNU_ITEM", "menu: automatic item ids are <prefix><TEXT> (MNU_ITEM when the text has no letters)");
            Check(AutoItemId("MNU_", "A", [](const std::string& n) { return n == "MNU_A" || n == "MNU_A_2"; }) == "MNU_A_3", "menu: a taken automatic name gets _2, _3 ...");
            DForm f; std::string e;
            auto load = [&](const std::string& items, const char* extra = "") { return f.FromJson("{\"form\":{\"name\":\"Mn\"},\"controls\":[{\"type\":\"SuperMenu\",\"name\":\"menu1\"" + std::string(extra) + ",\"items\":" + items + "}]}", e); };
            Check(load("[\"A\", {\"text\":\"B\"}, {\"text\":\"C\",\"items\":[\"D\",\"E\"]}]"), "menu: a minimal hand-written menu loads (strings and objects)");
            { const auto& m = f.items[0].menu; Check(m.size() == 3 && m[0].itemId == "MNU_A" && m[0].id == 1 && m[1].id == 2 && m[2].itemId.empty() && m[2].id == -1 && m[2].kids[0].itemId == "MNU_D" && m[2].kids[0].id == 3 && m[2].kids[1].id == 4, "menu: leaves get an item ID and the next ids (in order), a parent gets none"); }
            Check(load("[{\"text\":\"X\",\"id\":5,\"itemId\":\"MNU_X\"},{\"text\":\"Y\",\"id\":5,\"itemId\":\"MNU_X\"},{\"text\":\"Z\"}]"), "menu: (duplicates in a file)");
            { const auto& m = f.items[0].menu; Check(m[0].id == 5 && m[0].itemId == "MNU_X" && m[1].id == 6 && m[1].itemId == "MNU_Y" && m[2].id == 7 && m[2].itemId == "MNU_Z", "menu: a repeated id or item ID keeps its first owner; later ones get the next free id and an automatic name"); }
            Check(load("[{\"text\":\"a\",\"itemId\":\"int\"},{\"text\":\"b\",\"itemId\":\"1x\"},{\"text\":\"c\",\"itemId\":\"Form\"},{\"text\":\"d\",\"itemId\":\"HIGH\"},{\"text\":\"e\",\"itemId\":\"menu1\"},{\"text\":\"f\",\"itemId\":\"Mn\"},{\"text\":\"g\",\"itemId\":\"MnBase\"},{\"text\":\"h\",\"itemId\":\"x_Click\"}]"), "menu: (unsafe item IDs in a file)");
            { bool ok = true; for (const auto& k : f.items[0].menu) if (!ValidName(k.itemId) || k.itemId.rfind("MNU_", 0) != 0) ok = false; Check(ok, "menu: keywords, bad characters, library names, control / form names and handler names are replaced by automatic ones"); }
            Check(load("[{\"text\":\"a\",\"id\":70000},{\"text\":\"b\",\"id\":-5},{\"text\":\"c\",\"id\":3}]"), "menu: (ids out of range)");
            { const auto& m = f.items[0].menu; Check(m[2].id == 3 && m[0].id == 4 && m[1].id == 5, "menu: an id out of range counts as missing: the next free id above the highest valid one"); }
            Check(load("[{\"text\":\"P\",\"itemId\":\"MNU_P\",\"items\":[\"k\"]},{\"text\":\"Q\",\"id\":9,\"items\":[\"l\"]},{\"text\":\"R\",\"items\":[\"m\"]}]"), "menu: (parents)");
            { const auto& m = f.items[0].menu; Check(m[0].id >= 0 && m[0].itemId == "MNU_P" && m[1].id == 9 && m[1].itemId == "MNU_Q" && m[2].itemId.empty() && m[2].id == -1, "menu: a parent may have both an item ID and an id, or neither; one without the other is completed"); }
            Check(load("[\"\", {\"text\":\"   \"}, {\"text\":\"ok\"}, 5, null]"), "menu: (empty items)");
            Check(f.items[0].menu.size() == 1 && f.items[0].menu[0].text == "ok", "menu: items without text and values that are not items are dropped");
            { std::string deep = "[{\"text\":\"1\",\"items\":[{\"text\":\"2\",\"items\":[{\"text\":\"3\",\"items\":[{\"text\":\"4\",\"items\":[{\"text\":\"5\",\"items\":[{\"text\":\"6\"}]}]}]}]}]}]";
              Check(load(deep) && MenuDepth(f.items[0].menu) == MAX_MENU_DEPTH, "menu: nesting is limited to 5 levels"); }
            { std::string many = "["; for (int i = 0; i < 300; ++i) many += (i ? ",\"i" : "\"i") + std::to_string(i) + "\""; many += "]";
              Check(load(many) && MenuNodeCount(f.items[0].menu) == MAX_MENU_NODES, "menu: at most 200 items are read"); }
            Check(load("[{\"text\":\"Reboot\"}]", ",\"idPrefix\":\"CMD_\"") && f.items[0].menu[0].itemId == "CMD_REBOOT", "menu: the ID prefix is used for automatic names");
            Check(load("[{\"text\":\"Reboot\"}]", ",\"idPrefix\":\"\"") && ValidName(f.items[0].menu[0].itemId), "menu: an empty prefix still gives a valid name");
            Check(load("[{\"text\":\"Settings\",\"items\":[{\"text\":\"Reboot\",\"id\":7,\"itemId\":\"MNU_REBOOT\"},\"About\"]}]"), "menu: (round trip)");
            { std::string j1 = f.ToJson(), e2; DForm g2; Check(g2.FromJson(j1, e2) && g2.ToJson() == j1 && g2.items[0].menu[0].kids[0].id == 7, "menu: the JSON round trip is stable");
              Check(j1.find("\"items\": [\n") != std::string::npos && j1.find("\"itemId\": \"MNU_REBOOT\", \"id\": 7") != std::string::npos && j1.find("{ \"text\": \"Settings\", \"items\": [") != std::string::npos, "menu: the layout is written nested and readable, one item per line"); }
            // the form-level view of item ids
            Check(f.FromJson("{\"form\":{\"name\":\"Mn\"},\"controls\":[{\"type\":\"SuperMenu\",\"name\":\"menu1\",\"items\":[{\"text\":\"Go\",\"itemId\":\"MNU_GO\"}]},{\"type\":\"SuperMenu\",\"name\":\"menu2\",\"items\":[{\"text\":\"Go\",\"itemId\":\"MNU_GO\"}]}]}", e), "menu: (names across the form)");
            Check(f.items[0].menu[0].itemId == "MNU_GO" && f.items[1].menu[0].itemId != "MNU_GO" && ValidName(f.items[1].menu[0].itemId), "menu: item IDs are unique across menus (constants of one class): the first keeps it");
            Check(f.NameTaken("MNU_GO") && !f.NameTaken("MNU_GO", -1, 0) && f.NameTaken("MNU_GO", -1, 1), "menu: a control or group cannot be named like an item ID (except one that belongs to the menu being edited)");
            Check(f.FromJson("{\"form\":{\"name\":\"Mn\"},\"controls\":[{\"type\":\"SuperLabel\",\"name\":\"MNU_GO\"},{\"type\":\"SuperMenu\",\"name\":\"menu1\",\"items\":[{\"text\":\"Go\",\"itemId\":\"MNU_GO\"}]}]}", e) && f.items[1].menu[0].itemId != "MNU_GO" && ValidName(f.items[1].menu[0].itemId), "menu: an item ID equal to a control's name is replaced");
            Check(f.FromJson("{\"form\":{\"name\":\"Mn\"},\"controls\":[{\"type\":\"SuperMenu\",\"name\":\"menu1\",\"items\":[{\"text\":\"Go\",\"itemId\":\"MNU_GO\"}]},{\"type\":\"SuperMenu\",\"name\":\"menu2\",\"items\":[{\"text\":\"Go\",\"itemId\":\"MNU_GO\"}]}]}", e), "menu: (again, for the form-level checks)");
            Check(f.FormNameClashes("MNU_GO") && !f.FormNameClashes("Plain"), "menu: the form cannot be renamed to an item ID");
            { f.items[0].menu[0].id = 3; f.items[0].menu.push_back(f.items[0].menu[0]); Check(f.MenuProblem().find("used twice") != std::string::npos || f.MenuProblem().find("unused") != std::string::npos, "menu: MenuProblem reports a duplicate id or item ID (what the export refuses)"); }
            { DForm bad; bad.name = "Bad"; DControl m; m.type = C_MENU; m.name = "menu1"; MenuNode a, b; a.text = "A"; a.itemId = "MNU_A"; a.id = 1; b.text = "B"; b.itemId = "MNU_B"; b.id = 1; m.menu = { a, b }; bad.items.push_back(m);
              std::error_code bec; std::string bdir = "/tmp/fd_badmenu"; std::filesystem::remove_all(bdir, bec); std::filesystem::create_directories(bdir, bec);
              GenResult br = WriteDesigner(bad, bdir); Check(!br.ok && br.error.find("used twice") != std::string::npos && !std::filesystem::exists(bdir + "/Bad.designer.h", bec), "export: a menu with a duplicate id is refused (nothing is written)");
              bad.items[0].menu[1].id = 2; bad.items[0].menu[1].itemId = ""; br = WriteDesigner(bad, bdir); Check(!br.ok && br.error.find("needs an item ID") != std::string::npos, "export: a command without an item ID is refused");
              bad.items[0].menu[1].itemId = "int"; br = WriteDesigner(bad, bdir); Check(!br.ok && br.error.find("safe") != std::string::npos, "export: an unsafe item ID is refused");
              bad.items[0].menu[1].itemId = "MNU_B"; br = WriteDesigner(bad, bdir); Check(br.ok, "export: a valid menu is written"); std::filesystem::remove_all(bdir, bec); }
        }
        // ---- the menu editor ----
        form.Clear(); FixOutDir(); sel = -1; BuildFields(); form.name = "Mn2"; Place(C_MENU, 8, 8);
        Check(FD::MenuNodeCount(form.items[0].menu) == 4 && form.items[0].menu[0].itemId.empty() && form.items[0].menu[0].kids[0].itemId == "MNU_REBOOT" && form.items[0].menu[0].kids[0].id == 1 && form.items[0].menu[1].itemId == "MNU_ABOUT" && form.items[0].menu[1].id == 3, "menu editor: a new menu has a small sample with automatic ids and names");
        { int r = -1; for (int i = 0; i < nFields; ++i) if (fieldProp[i] == P_ITEMS) r = i; Check(r >= 0 && PropLabel(P_ITEMS) == "Menu items", "menu editor: a Menu items row"); EnsureRowVisible(r); Click(FIELD_X + FIELD_W - 30, FieldY(r) + 5); }
        Check(menuDlg != nullptr && menuDlg->Rows() == 4 && menuDlg->Selected() == 0 && menuDlg->RowText(1).find("MNU_REBOOT = 1") != std::string::npos && menuDlg->RowText(1).substr(0, 4) == "    ", "menu editor: opens on the tree (indented, with each command's constant and id)");
        if (menuDlg) { auto N = [&]() -> const MenuNode& { static MenuNode none; const MenuNode* n = menuDlg ? menuDlg->SelectedNode() : nullptr; return n ? *n : none; };
          auto MK = [&](SDL_Keycode k, Uint16 m = 0) { InputEvent v; v.type = InputEvent::KEY_DOWN; v.key = k; v.mod = m; Handle(v); };
          auto MT = [&](const char* tx) { InputEvent v; v.type = InputEvent::TEXT; strncpy(v.text, tx, sizeof v.text - 1); Handle(v); };
          auto MF = [&](int k) { Handle(ev(InputEvent::DOWN, menuDlg->RX() + 20, menuDlg->FieldY(k) + 5)); Handle(ev(InputEvent::UP, 0, 0)); };
          auto MB = [&](int row, int k) { int x = menuDlg->BtnX(row, k) + 6, y = (row == 1 ? menuDlg->Row1Y() : menuDlg->Row2Y()) + 3; Handle(ev(InputEvent::DOWN, x, y)); Handle(ev(InputEvent::UP, x, y)); };
          auto MSel = [&](int i) { SuperListBox* lb = menuDlg->Lb(); int x = lb->GetX() + lb->GetBorder() + lb->GetPadding() + 8, y = lb->GetBoxTop() + lb->GetBorder() + lb->GetPadding() + 2 + i * 18; Handle(ev(InputEvent::DOWN, x, y)); Handle(ev(InputEvent::UP, x, y)); };
          auto MSet = [&](int k, const char* v) { MF(k); MK(SDLK_a, KMOD_LCTRL); MT(v); };
          auto MClear = [&](int k) { MF(k); MK(SDLK_a, KMOD_LCTRL); MK(SDLK_BACKSPACE); };
          auto Leave = [&]() { if (menuDlg && menuDlg->FocusedField() >= 0) MK(SDLK_ESCAPE); };          // Esc in a field goes back to the tree; Esc in the tree would cancel
          MSel(1); Check(menuDlg->Selected() == 1 && menuDlg->FieldText(0) == "Reboot" && menuDlg->FieldText(1) == "MNU_REBOOT" && menuDlg->FieldText(2) == "1", "menu editor: clicking an item shows its text, item ID and id");
          MSet(0, "Restart"); Check(N().text == "Restart" && N().itemId == "MNU_RESTART", "menu editor: an automatic item ID follows the text");
          MSet(1, "MNU_REBOOT_NOW"); Check(N().itemId == "MNU_REBOOT_NOW" && menuDlg->Message().empty(), "menu editor: the item ID can be edited");
          MSet(0, "Restart now"); Check(N().itemId == "MNU_REBOOT_NOW", "menu editor: an item ID you edited no longer follows the text");
          MSet(1, "int"); Check(N().itemId == "MNU_REBOOT_NOW" && menuDlg->Message().find("not a safe C++ name") != std::string::npos && menuDlg->FieldText(1) == "int", "menu editor: a keyword is refused (red, with the reason, the old one stays)");
          MSet(1, "MNU_ABOUT"); Check(N().itemId == "MNU_REBOOT_NOW" && menuDlg->Message().find("already used") != std::string::npos, "menu editor: an item ID already used by another item is refused");
          MSet(1, "menu1"); Check(N().itemId == "MNU_REBOOT_NOW" && menuDlg->Message().find("already used") != std::string::npos, "menu editor: an item ID equal to a control name is refused");
          MSet(1, "HIGH"); Check(N().itemId == "MNU_REBOOT_NOW" && !menuDlg->Message().empty(), "menu editor: an Arduino macro is refused");
          MClear(1); Check(N().itemId == "MNU_REBOOT_NOW" && menuDlg->Message().find("needs") != std::string::npos, "menu editor: a command cannot lose its item ID");
          MSet(1, "MNU_REBOOT"); Check(N().itemId == "MNU_REBOOT", "menu editor: a valid item ID is accepted again");
          MSet(2, "3"); Check(N().id == 1 && menuDlg->Message().find("already used") != std::string::npos, "menu editor: an id used by another item of the menu is refused");
          MSet(2, "70000"); Check(N().id == 1 && menuDlg->Message().find("65535") != std::string::npos, "menu editor: an id above 65535 is refused");
          MClear(2); Check(N().id == 1 && menuDlg->Message().find("needs an id") != std::string::npos, "menu editor: a command cannot lose its id");
          MSet(2, "9"); Check(N().id == 9 && menuDlg->Message().empty(), "menu editor: a free id is accepted");
          Leave(); MSel(0); Check(menuDlg->FieldText(1).empty() && menuDlg->FieldText(2).empty(), "menu editor: a parent (System) has no item ID and no id");
          MSet(1, "MNU_SYSTEM"); Check(N().itemId == "MNU_SYSTEM" && N().id == 10, "menu editor: giving a parent an item ID gives it the next free id");
          MClear(1); Check(N().itemId.empty() && N().id == -1, "menu editor: clearing a parent's item ID clears its id too");
          Leave();
          // structure: System{Reboot, Power off}, About
          MSel(3); int maxBefore = std::max(0, MenuMaxId(menuDlg->nodes)); MB(1, 0);
          Check(menuDlg->Rows() == 5 && menuDlg->Selected() == 4 && N().itemId.rfind("MNU_ITEM", 0) == 0 && N().id == maxBefore + 1 && menuDlg->FocusedField() == 0, "menu editor: Add puts a new command after the selection with the next id, and focuses its text");
          Leave(); MB(1, 1); Check(menuDlg->Rows() == 6 && menuDlg->RowText(5).substr(0, 4) == "    " && N().id == maxBefore + 2, "menu editor: Add child adds a sub-item (the selected item becomes a parent)");
          Leave(); MB(1, 2); Check(menuDlg->Rows() == 5 && menuDlg->Selected() == 4, "menu editor: Delete removes the selected item (here the sub-item)");
          MB(1, 3); Check(menuDlg->Rows() == 5 && menuDlg->Selected() == 3 && menuDlg->RowText(4).find("About") != std::string::npos, "menu editor: Move up");
          MB(1, 4); Check(menuDlg->Selected() == 4 && menuDlg->RowText(4).find("Item") != std::string::npos, "menu editor: Move down");
          MB(1, 3); MSel(4); MB(2, 0); Check(menuDlg->Rows() == 5 && menuDlg->RowText(4).substr(0, 4) == "    " && menuDlg->RowText(4).find("About") != std::string::npos, "menu editor: Indent makes the item a child of the one above");
          MB(2, 1); Check(menuDlg->RowText(4).substr(0, 4) != "    " && menuDlg->RowText(4).find("About") != std::string::npos, "menu editor: Outdent brings it back");
          { bool refused = false; for (int i = 0; i < 8 && !refused; ++i) { MB(1, 1); Leave(); refused = menuDlg && menuDlg->Message().find("5 levels") != std::string::npos; }
            Check(menuDlg != nullptr && refused && MenuDepth(menuDlg->nodes) <= MAX_MENU_DEPTH, "menu editor: nesting stops at 5 levels"); }
          // paste an outline
          MB(2, 3); Check(menuDlg->Rows() == 0 && menuDlg->nodes.empty(), "menu editor: Clear all");
          { char* oldClip = SDL_GetClipboardText(); SDL_SetClipboardText("File\n  Open\n  Save\n    Save as\n    Save copy\nEdit\n\tCut\n\tCopy\n\nHelp\n");
            MB(2, 2); const auto& n = menuDlg->nodes;
            Check(n.size() == 3 && n[0].text == "File" && n[0].kids.size() == 2 && n[0].kids[1].text == "Save" && n[0].kids[1].kids.size() == 2 && n[0].kids[1].kids[0].text == "Save as" && n[1].kids.size() == 2 && n[1].kids[1].text == "Copy" && n[2].text == "Help", "menu editor: Paste outline builds the tree from the indentation (spaces or tabs, blank lines ignored)");
            Check(n[0].itemId.empty() && n[0].kids[1].itemId.empty() && n[0].kids[0].itemId == "MNU_OPEN" && n[0].kids[1].kids[0].itemId == "MNU_SAVE_AS" && n[2].itemId == "MNU_HELP", "menu editor: pasted leaves become commands with automatic names, parents get none");
            { std::set<int> ids; bool uniq = true; MenuForEach(n, [&](const MenuNode& k) { if (k.id >= 0 && !ids.insert(k.id).second) uniq = false; }); Check(uniq && ids.size() == 6, "menu editor: their ids are unique (next free numbers)"); }
            SDL_SetClipboardText("Open\n  Deep\n    Deeper\n      Deepest\n        TooDeep\n          Way too deep\n"); MB(2, 2);
            Check(MenuDepth(menuDlg->nodes) <= MAX_MENU_DEPTH, "menu editor: a pasted outline is cut at 5 levels");
            SDL_SetClipboardText(""); size_t n0 = (size_t)MenuNodeCount(menuDlg->nodes); MB(2, 2); Check((size_t)MenuNodeCount(menuDlg->nodes) == n0 && !menuDlg->Message().empty(), "menu editor: an empty clipboard adds nothing and says so");
            if (oldClip) { SDL_SetClipboardText(oldClip); SDL_free(oldClip); } }
          menuDlg->Draw(0);                                                    // (the whole dialog draws without trouble)
          MB(2, 3); MB(1, 0); Leave(); MK(SDLK_ESCAPE); Check(menuDlg == nullptr && FD::MenuNodeCount(form.items[0].menu) == 4, "menu editor: Esc cancels (the control keeps its menu)");
          { int r = -1; for (int i = 0; i < nFields; ++i) if (fieldProp[i] == P_ITEMS) r = i; EnsureRowVisible(r); Click(FIELD_X + FIELD_W - 30, FieldY(r) + 5); }
          MB(2, 3); MT("x"); { char* oldClip = SDL_GetClipboardText(); SDL_SetClipboardText("One\nTwo\n  Three"); MB(2, 2); if (oldClip) { SDL_SetClipboardText(oldClip); SDL_free(oldClip); } }
          if (menuDlg) { int x = menuDlg->OkX() + 5, y = menuDlg->BtnY() + 3; Handle(ev(InputEvent::DOWN, x, y)); Handle(ev(InputEvent::UP, x, y)); }
          Check(menuDlg == nullptr && form.items[0].menu.size() == 2 && form.items[0].menu[0].text == "One" && form.items[0].menu[0].itemId == "MNU_ONE" && form.items[0].menu[1].kids.size() == 1 && form.items[0].menu[1].kids[0].itemId == "MNU_THREE", "menu editor: OK stores the menu");
        }
        // ---- generated code ----
        form.Clear(); FixOutDir(); sel = -1; BuildFields(); form.name = "Mg";
        { DControl m; m.type = C_MENU; m.name = "menu1"; m.x = 8; m.y = 8; m.w = 160;
          MenuNode sys, reb, off, about, sub; sys.text = "System"; reb.text = "Reboot"; reb.itemId = "MNU_REBOOT"; reb.id = 7; off.text = "Power \"off\""; off.itemId = "MNU_POWER_OFF"; off.id = 8; about.text = "About"; about.itemId = "MNU_ABOUT"; about.id = 20;
          sub.text = "More"; sub.itemId = "MNU_MORE"; sub.id = 30; MenuNode deep; deep.text = "Deep"; deep.itemId = "MNU_DEEP"; deep.id = 31; sub.kids = { deep }; sys.kids = { reb, off, sub }; m.menu = { sys, about };
          ExtraSet(m, *FindExtra(C_MENU, "rows"), "4"); ExtraSet(m, *FindExtra(C_MENU, "wrap"), "true"); ExtraSet(m, *FindExtra(C_MENU, "showBack"), "false"); ExtraSet(m, *FindExtra(C_MENU, "rowPad"), "3");
          form.items.push_back(m); }
        Check(form.MenuProblem().empty(), "codegen: (the test menu is consistent)");
        { std::string g = GenerateDesigner(form); auto h5 = [&](const char* x) { return g.find(x) != std::string::npos; };
          Check(h5("#include <SuperMenu.h>") && h5("menu1(dst, 8, 8, 160, &lgfx::fonts::Font2, 4, enc, btnA, btnB)"), "codegen: the menu takes its visible rows before the encoder and buttons");
          Check(h5("static constexpr uint16_t MNU_REBOOT = 7;") && h5("static constexpr uint16_t MNU_POWER_OFF = 8;") && h5("static constexpr uint16_t MNU_DEEP = 31;") && h5("// menu1: item ids") && g.find("MNU_REBOOT = 7") < g.find("SuperMenu menu1;"), "codegen: every item ID is a static constexpr uint16_t in the class, before the controls");
          Check(h5("auto* fd_p1 = menu1.AddItem(\"System\", 0);") && h5("menu1.AddItem(\"Reboot\", MNU_REBOOT, fd_p1);") && h5("menu1.AddItem(\"Power \\\"off\\\"\", MNU_POWER_OFF, fd_p1);"), "codegen: AddItem calls use the constants as ids, parents as local variables (text escaped)");
          Check(h5("auto* fd_p2 = menu1.AddItem(\"More\", MNU_MORE, fd_p1);") && h5("menu1.AddItem(\"Deep\", MNU_DEEP, fd_p2);") && h5("menu1.AddItem(\"About\", MNU_ABOUT);"), "codegen: nesting, a parent that has an id, and a top-level item without a parent argument");
          Check(g.find("fd_p1 = menu1.AddItem(\"System\"") < g.find("menu1.AddItem(\"Reboot\"") && g.find("menu1.AddItem(\"Deep\"") < g.find("menu1.AddItem(\"About\""), "codegen: the items come in menu order");
          Check(h5("menu1.Wrap = true;") && h5("menu1.ShowBack = false;") && h5("menu1.RowPad = 3;"), "codegen: menu settings that differ from the defaults");
          Check(h5("virtual void menu1_Change() {}") && h5("virtual void menu1_Choose(uint16_t id) {}") && h5("menu1.SetOnAccept(menu1_ChooseThunk, this);") && h5("s->menu1_Choose((uint16_t)s->menu1.GetResultId());"), "codegen: the Choose handler receives the id of the chosen command");
          std::string u = GenerateUserFile(form);
          Check(u.find("void menu1_Choose(uint16_t id) override {") != std::string::npos && u.find("switch (id) {") != std::string::npos && u.find("case MNU_REBOOT:    // Reboot") != std::string::npos && u.find("case MNU_ABOUT:") != std::string::npos && u.find("case MNU_DEEP:") != std::string::npos && u.find("case MNU_MORE") == std::string::npos && u.find("case MNU_POWER_OFF:    // Power \"off\"") == std::string::npos == false, "user file: a switch with one case per command (parents are not commands)");
          Check(UserFileMissing(form, u).empty() && HandlerNames(form) == std::vector<std::string>({ "menu1_Change", "menu1_Choose" }), "user file: the Choose handler counts as a handler (not reported missing once the stub exists)"); }
        Check(!ValidName("menu1_Choose") && !ValidName("x_ChooseThunk") && !ValidName("fd_p1") && !ValidName("fd_anything") && ValidName("fdx"), "names: _Choose, _ChooseThunk and the generator's fd_ locals are reserved");
        form.Clear(); FixOutDir(); sel = -1; BuildFields();
        // ---- the library: control groups ----
        {
            SuperLabel l1(&scr, 0, 0, 40, &lgfx::fonts::Font2, "a"), l2(&scr, 0, 0, 40, &lgfx::fonts::Font2, "b"), l3(&scr, 0, 0, 40, &lgfx::fonts::Font2, "c"), l4(&scr, 0, 0, 40, &lgfx::fonts::Font2, "d");
            SuperPushButton b1(&scr, 0, 0, 40, &lgfx::fonts::Font2, "x"), b2(&scr, 0, 0, 40, &lgfx::fonts::Font2, "y"), b3(&scr, 0, 0, 40, &lgfx::fonts::Font2, "z");
            SuperForm sf(nullptr, nullptr);
            Check(l1.GetGroup() == 0 && b1.GetGroup() == 0, "library groups: a control starts in group 0 (no group)");
            l1.SetGroup(1); l2.SetGroup(1); l3.SetGroup(2); b1.SetGroup(1); b2.SetGroup(2); b3.SetGroup(3); Check(l1.GetGroup() == 1 && b3.GetGroup() == 3, "library groups: SetGroup / GetGroup");
            sf.Add(&l1, false); sf.Add(&l2, false); sf.Add(&l3, false); sf.Add(&l4, false); sf.Add(&b1); sf.Add(&b2); sf.Add(&b3);
            Check(sf.GroupCount(1) == 3 && sf.GroupCount(2) == 2 && sf.GroupCount(3) == 1 && sf.GroupCount(0) == 0 && sf.GroupCount(9) == 0, "library groups: GroupCount (group 0 is never counted)");
            sf.SetGroupVisible(1, false, false); Check(!l1.IsVisible() && !l2.IsVisible() && !b1.IsVisible() && l3.IsVisible() && l4.IsVisible() && b2.IsVisible(), "library groups: SetGroupVisible hides exactly that group");
            Check(sf.GetFocused() == &b2, "library groups: the focus leaves a control that was hidden (b1 -> b2)");
            sf.SetGroupVisible(1, true, false); Check(l1.IsVisible() && b1.IsVisible(), "library groups: and shows it again");
            sf.SetGroupVisible(0, false, false); Check(l4.IsVisible(), "library groups: group 0 is never touched");
            sf.SetGroupEnabled(2, false); Check(!l3.IsEnabled() && !b2.IsEnabled() && b1.IsEnabled(), "library groups: SetGroupEnabled disables a group");
            sf.SetGroupEnabled(2, true); Check(l3.IsEnabled() && b2.IsEnabled(), "library groups: and enables it");
            const uint16_t pages[] = { 1, 2, 3 };
            sf.ShowPage(pages, 3, 2, false); Check(!l1.IsVisible() && !b1.IsVisible() && l3.IsVisible() && b2.IsVisible() && !b3.IsVisible() && l4.IsVisible(), "library groups: ShowPage shows one page, hides the other listed ones, leaves ungrouped controls alone");
            sf.ShowPage(pages, 3, 1, false); Check(l1.IsVisible() && b1.IsVisible() && !l3.IsVisible() && !b2.IsVisible(), "library groups: ShowPage switches pages");
            Check(sf.GetFocused() == &b1, "library groups: the focus moves to the new page's first focusable control");
            sf.ShowPage(pages, 3, 0, false); Check(!l1.IsVisible() && !l3.IsVisible() && !b3.IsVisible() && l4.IsVisible() && sf.GetFocused() == nullptr, "library groups: show = 0 hides every listed page (nothing left to focus)");
            const uint16_t partial[] = { 1 }; sf.ShowPage(pages, 3, 3, false); sf.ShowPage(partial, 1, 1, false); Check(b3.IsVisible() && b1.IsVisible() && !b2.IsVisible(), "library groups: a group that is not listed stays as it is");
        }
        // ---- chunk 6: Visible / Enabled / control groups / tab pages in the designer ----
        form.Clear(); FixOutDir(); sel = -1; BuildFields(); form.name = "Pg"; viewGroup.clear();
        Place(C_TABBAR, 8, 8); Place(C_LABEL, 8, 48); Place(C_PUSHBUTTON, 8, 80); Place(C_SLIDER, 8, 120);
        { DControl d; Check(d.ctlGroup.empty() && d.visible && d.enabled && d.pages.empty(), "groups: the defaults (no group, visible, enabled)"); }
        auto GR = [&](int prop) { for (int i = 0; i < nFields; ++i) if (fieldProp[i] == prop) return i; return -1; };
        auto GSet = [&](const char* text) { int r = GR(P_CGROUP); EnsureRowVisible(r); Click(FIELD_X + 20, FieldY(r) + 5); Key(SDLK_a, KMOD_LCTRL); if (text[0]) Type(text); else Key(SDLK_BACKSPACE); Key(SDLK_RETURN); };
        auto GSeg = [&](int prop, int seg) { int r = GR(prop); EnsureRowVisible(r); Click(FIELD_X + seg * (FIELD_W / 2) + 6, FieldY(r) + 5); };
        for (int i = 0; i < 4; ++i) { Select(i); Check(GR(P_CGROUP) >= 0 && GR(P_VISIBLE) >= 0 && GR(P_ENABLED) >= 0 && PropLabel(P_CGROUP) == "Control group", ("groups: control " + std::to_string(i) + " has Control group / Visible / Enabled rows").c_str()); }
        Select(1); GSet("GRP_HOME"); Check(form.items[1].ctlGroup == "GRP_HOME", "groups: a control group is typed as a name");
        Select(2); GSet("GRP_HOME"); Check(form.items[2].ctlGroup == "GRP_HOME" && form.UsedGroups().size() == 1, "groups: several controls can share a group name");
        Select(3); GSet("GRP_SETTINGS"); Check(form.items[3].ctlGroup == "GRP_SETTINGS" && form.UsedGroups() == std::vector<std::string>({ "GRP_HOME", "GRP_SETTINGS" }), "groups: groups are listed in order of first use");
        Check(form.GroupId("GRP_HOME") == 1 && form.GroupId("GRP_SETTINGS") == 2 && form.GroupId("nope") == 0, "groups: numbered 1, 2, 3 by first use (0 = none)");
        { bool unchanged = true; for (const char* bad : { "int", "HIGH", "1abc", "label1", "Pg", "PgBase", "x_Click", "fd_x" }) { GSet(bad); if (form.items[3].ctlGroup != "GRP_SETTINGS") unchanged = false; }
          Check(unchanged && status.find("not a safe") != std::string::npos, "groups: a keyword, macro, control name or form class name is refused, the old group stays, with the reason"); }
        GSet("label1"); Check(status.find("already used") != std::string::npos, "groups: (a control's name: already used)");
        GSet(""); Check(form.items[3].ctlGroup.empty() && form.UsedGroups() == std::vector<std::string>({ "GRP_HOME" }) && form.GroupId("GRP_HOME") == 1, "groups: clearing the field means no group (and an unused group disappears)");
        GSet("GRP_SETTINGS"); Check(form.GroupId("GRP_SETTINGS") == 2, "groups: a group that comes back gets the next free number");
        Check(form.NameTaken("GRP_HOME") && form.NameTaken("GRP_SETTINGS") && !form.GroupNameTaken("GRP_HOME") && form.GroupNameTaken("label1") && form.GroupNameTaken("Pg"), "groups: group names count as names (a control cannot be called GRP_HOME) but several controls may share one");
        Select(2); { int r = GR(P_NAME); Click(FIELD_X + 20, FieldY(r) + 5); Key(SDLK_a, KMOD_LCTRL); Type("GRP_HOME"); Key(SDLK_RETURN); } Check(form.items[2].name == "button1", "groups: a control cannot be renamed to a group name");
        Check(form.FormNameClashes("GRP_HOME") && !form.FormNameClashes("Other"), "groups: the form cannot be renamed to a group name");
        // Visible / Enabled
        Select(2); GSeg(P_VISIBLE, 0); Check(!form.items[2].visible && ToJsonHas(form, "\"visible\": false"), "visible: No hides the control initially (saved as false)");
        GSeg(P_VISIBLE, 1); Check(form.items[2].visible && !ToJsonHas(form, "visible"), "visible: Yes is the default (not saved)");
        GSeg(P_ENABLED, 0); Check(!form.items[2].enabled && ToJsonHas(form, "\"enabled\": false"), "enabled: No disables the control initially");
        { auto dc = MakeControl(&scr, form.items[2], 0, 0); Check(!dc->IsEnabled() && dc->IsVisible(), "enabled: the real control is drawn disabled (dimmed) in the designer"); }
        GSeg(P_ENABLED, 1); Check(form.items[2].enabled && !ToJsonHas(form, "enabled"), "enabled: Yes is the default (not saved)");
        // JSON and the numbers
        { std::string gj = form.ToJson(), ge; DForm gf;
          Check(gf.FromJson(gj, ge) && gf.items[1].ctlGroup == "GRP_HOME" && gf.items[3].ctlGroup == "GRP_SETTINGS" && gf.ToJson() == gj, "groups: the control groups and their numbers survive the JSON round trip");
          Check(gj.find("\"controlGroup\": \"GRP_HOME\"") != std::string::npos && gj.find("\"groupIds\": { \"GRP_HOME\": 1, \"GRP_SETTINGS\": 2 }") != std::string::npos, "groups: written readably (controlGroup per control, groupIds in the form)");
          Check(gf.FromJson("{\"form\":{\"name\":\"F\",\"groupIds\":{\"B\":7,\"A\":7,\"Z\":3}},\"controls\":[{\"type\":\"SuperLabel\",\"name\":\"a\",\"controlGroup\":\"A\"},{\"type\":\"SuperLabel\",\"name\":\"b\",\"controlGroup\":\"B\"},{\"type\":\"SuperLabel\",\"name\":\"c\",\"controlGroup\":\"C\"}]}", ge)
                && gf.GroupId("B") == 7 && gf.GroupId("A") == 8 && gf.GroupId("C") == 9, "groups: numbers from a file are kept (the first owner of a repeated number keeps it); the others get the next free numbers");
          Check(gf.GroupId("Z") == 0 && gf.groupIds.size() == 3, "groups: an unused name in groupIds is dropped");
          Check(gf.FromJson("{\"form\":{\"name\":\"F\"},\"controls\":[{\"type\":\"SuperLabel\",\"name\":\"a\",\"controlGroup\":\"int\"},{\"type\":\"SuperLabel\",\"name\":\"b\",\"controlGroup\":\"a\"},{\"type\":\"SuperLabel\",\"name\":\"c\",\"controlGroup\":\"G_OK\",\"visible\":false,\"enabled\":false}]}", ge)
                && gf.items[0].ctlGroup.empty() && gf.items[1].ctlGroup.empty() && gf.items[2].ctlGroup == "G_OK" && !gf.items[2].visible && !gf.items[2].enabled, "groups: unsafe group names or names that are a control's name in a file are dropped; visible / enabled are read"); }
        // the design view
        viewGroup.clear(); Select(3); Check(viewGroup.empty(), "view: starts on All");
        Click(VIEW_X + 10, ZROW_Y + 5); Check(viewGroup == "GRP_HOME" && sel == -1, "view: the View button goes to the first group (and deselects a control that is no longer shown)");
        Check(HitControl(10, 52) == 1 && HitControl(10, 125) == -1, "view: a control of another page cannot be clicked, one of the shown group can");
        Click(VIEW_X + 10, ZROW_Y + 5); Check(viewGroup == "GRP_SETTINGS" && HitControl(10, 52) == -1 && HitControl(10, 125) == 3, "view: ... then the next group");
        Click(VIEW_X + 10, ZROW_Y + 5); Check(viewGroup.empty() && HitControl(10, 52) == 1, "view: ... then back to All");
        viewGroup = "GRP_HOME"; Select(3); Check(viewGroup == "GRP_SETTINGS" && sel == 3, "view: selecting a control of another page (from the tree) switches the view to its group");
        Select(0); Check(viewGroup == "GRP_SETTINGS" && sel == 0, "view: a control without a group is shown in every view");
        viewGroup.clear();
        // tab pages in the items dialog
        Select(0);
        auto OpenTabs = [&]() { int r = GR(P_ITEMS); EnsureRowVisible(r); Click(FIELD_X + FIELD_W - 30, FieldY(r) + 5); };
        auto TK = [&](SDL_Keycode k, Uint16 m = 0) { InputEvent v; v.type = InputEvent::KEY_DOWN; v.key = k; v.mod = m; Handle(v); };
        auto TT = [&](const char* tx) { InputEvent v; v.type = InputEvent::TEXT; strncpy(v.text, tx, sizeof v.text - 1); Handle(v); };
        auto TB = [&](int row, int k) { int x = itemsDlg->BtnX(row, k) + 6, y = (row == 1 ? itemsDlg->Row1Y() : itemsDlg->Row2Y()) + 3; Handle(ev(InputEvent::DOWN, x, y)); Handle(ev(InputEvent::UP, x, y)); };
        auto TSel = [&](int i) { SuperListBox* lb = itemsDlg->Lb(); int x = lb->GetX() + lb->GetBorder() + lb->GetPadding() + 8, y = lb->GetBoxTop() + lb->GetBorder() + lb->GetPadding() + 2 + i * 18; Handle(ev(InputEvent::DOWN, x, y)); Handle(ev(InputEvent::UP, x, y)); };
        auto TPage = [&](const char* g) { Handle(ev(InputEvent::DOWN, itemsDlg->RX() + 20, itemsDlg->PageY() + 5)); Handle(ev(InputEvent::UP, 0, 0)); TK(SDLK_a, KMOD_LCTRL); if (g[0]) TT(g); else TK(SDLK_BACKSPACE); };
        auto TOk = [&]() { int x = itemsDlg->OkX() + 5, y = itemsDlg->BtnY() + 3; Handle(ev(InputEvent::DOWN, x, y)); Handle(ev(InputEvent::UP, x, y)); };
        OpenTabs(); Check(itemsDlg != nullptr && itemsDlg->items.size() == 3 && itemsDlg->pages.size() == 3 && itemsDlg->PageText().empty(), "tab pages: the tabs dialog has a Page group per tab (none yet)");
        TSel(0); TPage("GRP_HOME"); Check(itemsDlg->pages[0] == "GRP_HOME" && itemsDlg->PageFocused(), "tab pages: the page group of a tab is typed in its field");
        TK(SDLK_ESCAPE); TSel(1); TPage("GRP_SETTINGS"); TK(SDLK_ESCAPE); TSel(2); Check(itemsDlg->pages[1] == "GRP_SETTINGS" && itemsDlg->pages[2].empty() && itemsDlg->PageText().empty(), "tab pages: each tab has its own (the third has none)");
        TPage("int"); Check(itemsDlg->pages[2].empty() && itemsDlg->Message().find("not a safe") != std::string::npos, "tab pages: a keyword is refused (red, with the reason)");
        TPage("label1"); Check(itemsDlg->pages[2].empty() && itemsDlg->Message().find("already used") != std::string::npos, "tab pages: a control's name is refused");
        TPage("GRP_HOME"); Check(itemsDlg->pages[2] == "GRP_HOME" && itemsDlg->Message().empty(), "tab pages: a group another tab (or a control) already uses is fine: pages may share a group");
        TPage(""); TK(SDLK_ESCAPE); Check(itemsDlg->pages[2].empty(), "tab pages: clearing it means no page");
        TSel(1); TB(1, 1); Check(itemsDlg->items.size() == 4 && itemsDlg->pages.size() == 4 && itemsDlg->pages[0] == "GRP_HOME" && itemsDlg->pages[1].empty() && itemsDlg->pages[2] == "GRP_SETTINGS", "tab pages: Insert adds a tab without a page; the others keep theirs");
        TK(SDLK_ESCAPE); TB(1, 2); Check(itemsDlg->items.size() == 3 && itemsDlg->pages == std::vector<std::string>({ "GRP_HOME", "GRP_SETTINGS", "" }), "tab pages: Delete removes the tab and its page");
        TSel(0); TB(1, 4); Check(itemsDlg->items[1] == "Tab 1" && itemsDlg->pages[1] == "GRP_HOME" && itemsDlg->pages[0] == "GRP_SETTINGS", "tab pages: Move down moves the page with its tab");
        TB(1, 3); Check(itemsDlg->items[0] == "Tab 1" && itemsDlg->pages[0] == "GRP_HOME", "tab pages: Move up moves it back");
        TSel(2); TPage("GRP_LAST"); TK(SDLK_ESCAPE); TB(1, 3); TB(1, 3); Check(itemsDlg->items[0] == "Tab 3" && itemsDlg->pages[0] == "GRP_LAST", "tab pages: (Tab 3 moved to the top with its page)");
        TB(2, 0); { auto pageOf = [&](const char* t) { for (size_t i = 0; i < itemsDlg->items.size(); ++i) if (itemsDlg->items[i] == t) return itemsDlg->pages[i]; return std::string("?"); };
                    Check(itemsDlg->items[0] == "Tab 1" && pageOf("Tab 1") == "GRP_HOME" && pageOf("Tab 2") == "GRP_SETTINGS" && pageOf("Tab 3") == "GRP_LAST", "tab pages: Sort A-Z keeps every page with its tab"); }
        { char* oldClip = SDL_GetClipboardText(); SDL_SetClipboardText("One\nTwo"); size_t n0 = itemsDlg->items.size(); TB(2, 1); Check(itemsDlg->items.size() == n0 + 2 && itemsDlg->pages.size() == itemsDlg->items.size(), "tab pages: Paste lines adds tabs without pages"); if (oldClip) { SDL_SetClipboardText(oldClip); SDL_free(oldClip); } }
        TB(2, 2); Check(itemsDlg->items.empty() && itemsDlg->pages.empty(), "tab pages: Clear all clears both");
        TB(1, 0); TK(SDLK_a, KMOD_LCTRL); TT("Home"); TK(SDLK_ESCAPE); TK(SDLK_ESCAPE);
        Check(itemsDlg == nullptr && form.items[0].list == std::vector<std::string>({ "Tab 1", "Tab 2", "Tab 3" }) && form.items[0].pages.empty(), "tab pages: Esc cancels (the tab bar keeps its tabs and pages)");
        OpenTabs(); TB(2, 2);
        TB(1, 0); TK(SDLK_a, KMOD_LCTRL); TT("Home"); TK(SDLK_TAB); TT("GRP_HOME"); TK(SDLK_ESCAPE);
        TB(1, 0); TK(SDLK_a, KMOD_LCTRL); TT("Settings"); TK(SDLK_TAB); TT("GRP_SETTINGS"); TK(SDLK_ESCAPE);
        TB(1, 0); TK(SDLK_a, KMOD_LCTRL); TT("About"); TK(SDLK_ESCAPE); TOk();
        Check(itemsDlg == nullptr && form.items[0].list == std::vector<std::string>({ "Home", "Settings", "About" }) && form.items[0].pages == std::vector<std::string>({ "GRP_HOME", "GRP_SETTINGS", "" }), "tab pages: OK stores the tabs with their page groups");
        form.items[1].ctlGroup = "GRP_HOME"; form.items[2].ctlGroup = "GRP_HOME"; form.items[3].ctlGroup = "GRP_SETTINGS"; form.items[2].visible = false; form.items[3].enabled = false;
        { std::string tj = form.ToJson(), te; DForm tf; Check(tf.FromJson(tj, te) && tf.items[0].list == form.items[0].list && tf.items[0].pages == form.items[0].pages && tf.ToJson() == tj, "tab pages: the pages survive the JSON round trip");
          Check(tj.find("\"items\": [{ \"text\": \"Home\", \"group\": \"GRP_HOME\" }, { \"text\": \"Settings\", \"group\": \"GRP_SETTINGS\" }, \"About\"]") != std::string::npos, "tab pages: a tab with a page is written as { text, group }, one without as a plain string");
          Check(tf.FromJson("{\"form\":{},\"controls\":[{\"type\":\"SuperTabBar\",\"name\":\"t\",\"items\":[\"A\",{\"text\":\"B\",\"group\":\"G1\"},{\"text\":\"C\",\"group\":\"int\"},{\"group\":\"G3\"}]}]}", te)
                && tf.items[0].list == std::vector<std::string>({ "A", "B", "C" }) && tf.items[0].pages == std::vector<std::string>({ "", "G1", "" }), "tab pages: a file may mix strings and objects; an unsafe page name is dropped, a tab without text is dropped"); }
        // generated code
        { std::string g = GenerateDesigner(form); auto hg = [&](const char* x) { return g.find(x) != std::string::npos; };
          Check(hg("// control groups") && hg("static constexpr uint16_t GRP_HOME = 1;") && hg("static constexpr uint16_t GRP_SETTINGS = 2;") && g.find("GRP_HOME = 1") < g.find("SuperTabBar tabs1;"), "codegen: every used control group is a static constexpr uint16_t in the class");
          Check(hg("label1.SetGroup(GRP_HOME);") && hg("button1.SetGroup(GRP_HOME);") && hg("slider1.SetGroup(GRP_SETTINGS);") && !hg("tabs1.SetGroup"), "codegen: SetGroup for the controls that have a group");
          Check(hg("button1.SetVisible(false, false);") && !hg("label1.SetVisible") && hg("slider1.SetEnabled(false);") && !hg("button1.SetEnabled"), "codegen: initial Visible = No and Enabled = No");
          Check(g.find("tabs1_ShowPage(tabs1.GetSelected(), false);") < g.find("button1.SetVisible(false, false);") && g.find("button1.SetVisible(false, false);") > g.find("InitializeComponent();"), "codegen: Visible = No is applied after the first page is shown, so it wins at start (the designer shows it faded)");
          Check(hg("void tabs1_ShowPage(int index, bool draw = true) {") && hg("static const uint16_t pages[] = { GRP_HOME, GRP_SETTINGS, 0 };") && hg("Form.ShowPage(pages, 3, (index >= 0 && index < 3) ? pages[index] : 0, draw);"), "codegen: a ShowPage method per tab bar (a tab without a page is 0)");
          Check(hg("tabs1_ShowPage(tabs1.GetSelected(), false);") && g.find("InitializeComponent();\n        tabs1_ShowPage") != std::string::npos, "codegen: the selected tab's page is shown at start, the others hidden");
          Check(hg("static void tabs1_AcceptThunk(void* u) { auto* s = static_cast<PgBase*>(u); s->tabs1_ShowPage(s->tabs1.GetSelected()); s->tabs1_Accept(); }"), "codegen: choosing a tab switches the page, then calls your handler");
          Check(hg("tabs1.SetOnAccept(tabs1_AcceptThunk, this);") && hg("virtual void tabs1_Accept() {}"), "codegen: the handler is still yours");
          std::string u = GenerateUserFile(form); Check(u.find("The page (control group) of that tab is already shown.") != std::string::npos, "user file: the tab Accept hint says the page is already shown");
          ExtraSet(form.items[0], *FindExtra(C_TABBAR, "selected"), "1"); Check(GenerateDesigner(form).find("tabs1.SetSelected(1);") != std::string::npos, "codegen: (a different initial tab)");
          form.items[0].pages = { "", "", "" }; std::string g2 = GenerateDesigner(form);
          Check(g2.find("_ShowPage") == std::string::npos && g2.find("tabs1_AcceptThunk") != std::string::npos && g2.find("s->tabs1_ShowPage") == std::string::npos && g2.find("GRP_SETTINGS = ") != std::string::npos, "codegen: without any page group a tab bar writes no page code (the groups of controls still exist)"); }
        { DForm bad; bad.name = "Bad"; DControl a; a.type = C_LABEL; a.name = "grp"; a.ctlGroup = "grp"; bad.items.push_back(a); std::error_code bec; std::string bdir = "/tmp/fd_badgroup"; std::filesystem::remove_all(bdir, bec); std::filesystem::create_directories(bdir, bec);
          GenResult br = WriteDesigner(bad, bdir); Check(!br.ok && br.error.find("control group") != std::string::npos && !std::filesystem::exists(bdir + "/Bad.designer.h", bec), "export: a control group named like a control is refused (nothing is written)");
          bad.items[0].ctlGroup = "G_OK"; br = WriteDesigner(bad, bdir); Check(br.ok, "export: a valid group exports"); std::filesystem::remove_all(bdir, bec); }
        Check(!ValidName("tabs1_ShowPage") && !ValidName("x_ShowPage"), "names: _ShowPage is reserved too");
        form.Clear(); FixOutDir(); sel = -1; BuildFields(); viewGroup.clear();
        // ---- chunk 4: gauge, graph, user draw; decimal numbers ----
        {
            Check(FloatText(12.5) == "12.5" && FloatText(100.0) == "100" && FloatText(0.1 + 0.2) == "0.3" && FloatText(-20.5) == "-20.5" && FloatText(1.23456789) == "1.2346" && FloatText(2.50) == "2.5", "decimals: numbers are kept as short canonical text (at most 4 decimals)");
            DControl d; d.type = C_GAUGE; const ExtraDef& mn = *FindExtra(C_GAUGE, "min");
            Check(ExtraSet(d, mn, "-20.5") && XFloat(d, "min") == -20.5 && XStr(d, "min") == "-20.5" && ExtraSet(d, mn, "0.0") && !d.extra.count("min"), "decimals: a decimal extra is stored canonically, and the default is not stored");
            Check(!ExtraSet(d, mn, "") && !ExtraSet(d, mn, "-") && !ExtraSet(d, mn, ".") && !ExtraSet(d, mn, "1.2.3") && !ExtraSet(d, mn, "12x") && ExtraSet(d, mn, "9999999") && XFloat(d, "min") == 999999, "decimals: not-a-number text is refused, an out-of-range number is clamped");
            JV v; std::string jsrc = "[1, 2.5, -0.25, 1e2, 7]", je; JsonParser jp(jsrc); Check(jp.Parse(v, je) && v.a.size() == 5 && v.a[1].d == 2.5 && v.a[2].d == -0.25 && v.a[3].d == 100 && v.a[0].n == 1 && v.a[1].n == 2, "decimals: the JSON reader reads decimal numbers (the integer part stays in n)");
            Check(Info(C_GAUGE).extras && ExtraCount(C_GAUGE) == 22 + N_LABEL_EXTRAS && ExtraCount(C_GRAPH) == 10 + N_LABEL_EXTRAS && ExtraCount(C_USERDRAW) == 4 + N_LABEL_EXTRAS && !Info(C_GAUGE).editable && !Info(C_GRAPH).editable && !Info(C_USERDRAW).editable, "chunk 4: three display types with their extras (not editable: no encoder, no buttons)");
        }
        form.Clear(); FixOutDir(); sel = -1; BuildFields(); form.name = "Ch4";
        Place(C_GAUGE, 8, 8); Place(C_GRAPH, 8, 120); Place(C_USERDRAW, 8, 180);
        { DControl& g0 = form.items[0]; Check(g0.name == "gauge1" && g0.zones.size() == 3 && XFloat(g0, "value") == 42 && g0.zones[0].to == 60 && g0.zones[2].color == 0xF800 && g0.h > 90, "gauge: a new gauge has a sample (three bands, value 42) and the height of its body plus frame"); }
        auto F4 = [&](const char* key) { for (int i = 0; i < nFields; ++i) if (const ExtraDef* d = ExtraOf(fieldProp[i])) if (std::string(d->key) == key) return i; return -1; };
        auto T4 = [&](const char* key, const char* text) { int r = F4(key); EnsureRowVisible(r); Click(FIELD_X + 20, FieldY(r) + 5); Key(SDLK_a, KMOD_LCTRL); if (text[0]) Type(text); else Key(SDLK_BACKSPACE); Key(SDLK_RETURN); };
        auto S4 = [&](const char* key, int seg, int n) { int r = F4(key); EnsureRowVisible(r); Click(FIELD_X + seg * (FIELD_W / n) + 6, FieldY(r) + 5); };
        Select(0);
        { int zr = -1; for (int i = 0; i < nFields; ++i) if (fieldProp[i] == P_ZONES) zr = i; Check(zr >= 0 && PropLabel(P_ZONES) == "Zones" && F4("bodyHeight") >= 0 && F4("min") >= 0 && F4("value") >= 0 && F4("sweepStart") >= 0 && F4("minMaxMs") >= 0 && F4("label") >= 0, "gauge: rows for Zones, Body height, Minimum / Maximum / Value, sweep, ticks, format, look, Min/Max and the label group"); }
        T4("value", "55.5"); Check(XFloat(form.items[0], "value") == 55.5 && XStr(form.items[0], "value") == "55.5", "gauge: a decimal value is accepted");
        T4("value", "150"); Check(XFloat(form.items[0], "value") == 55.5 && status.find("between") != std::string::npos, "gauge: a value outside the range is refused, with the reason");
        T4("max", "80"); Check(XFloat(form.items[0], "max") == 80 && XFloat(form.items[0], "value") == 55.5, "gauge: a new Maximum above the value keeps it");
        T4("max", "40"); Check(XFloat(form.items[0], "max") == 40 && XFloat(form.items[0], "value") == 40, "gauge: a new range moves the needle into it (value 55.5 -> 40)");
        T4("min", "40"); Check(XFloat(form.items[0], "min") == 0 && status.find("above") != std::string::npos, "gauge: Minimum must stay below Maximum (refused)");
        T4("max", "100"); T4("min", "-20.5"); Check(XFloat(form.items[0], "min") == -20.5 && XStr(form.items[0], "min") == "-20.5", "gauge: a negative decimal Minimum");
        T4("min", "0");
        T4("sweepEnd", "-90"); Check(XInt(form.items[0], "sweepEnd") == 90 && status.find("sweep") != std::string::npos, "gauge: the sweep must end after it starts (refused)");
        T4("sweepStart", "-135"); T4("sweepEnd", "135"); Check(XInt(form.items[0], "sweepStart") == -135 && XInt(form.items[0], "sweepEnd") == 135, "gauge: a 270 degree dial");
        T4("minMaxMs", "5000"); Check(XInt(form.items[0], "minMaxMs") == 5000, "gauge: Min/Max window");
        T4("unit", " %"); T4("decimals", "1");
        S4("needle", 1, 2); S4("tickLabels", 0, 2); Check(XInt(form.items[0], "needle") == 1 && !XBool(form.items[0], "tickLabels"), "gauge: needle style and tick numbers");
        { auto gc = MakeControl(&scr, form.items[0], 0, 0); auto* g = static_cast<SuperGauge*>(gc.get());
          Check(g->GetValue() == 42 || g->GetValue() == 40, "gauge: the real gauge is built (value at once, no animation)");
          Check(g->IsMinMax() && !g->ShowTickLabels && g->Needle == SuperGauge::NEEDLE_LINE, "gauge: the real gauge gets the settings"); }
        // JSON
        { std::string gj = form.ToJson(), ge; DForm gf; Check(gf.FromJson(gj, ge) && gf.ToJson() == gj && gf.items[0].zones.size() == 3 && gf.items[0].zones[1].from == 60 && XFloat(gf.items[0], "min") == 0, "gauge: the JSON round trip is stable (numbers, zones)");
          Check(gj.find("\"zones\": [{ \"from\": 0, \"to\": 60, \"color\": \"0x07E0\" }, { \"from\": 60, \"to\": 85, \"color\": \"0xFFE0\" }, { \"from\": 85, \"to\": 100, \"color\": \"0xF800\" }]") != std::string::npos && gj.find("\"sweepStart\": -135") != std::string::npos, "gauge: zones are written readably, decimals as numbers");
          Check(gf.FromJson("{\"form\":{},\"controls\":[{\"type\":\"SuperGauge\",\"name\":\"g\",\"value\":2.5,\"min\":-1.25,\"zones\":[{\"from\":5,\"to\":5,\"color\":\"0x07E0\"},{\"from\":3,\"to\":2},{\"from\":0,\"to\":1.5,\"color\":63488},{\"from\":0,\"to\":1,\"color\":\"zzz\"},\"x\",{\"to\":4}]}]}", ge)
                && gf.items[0].zones.size() == 2 && gf.items[0].zones[0].to == 1.5 && gf.items[0].zones[0].color == 0xF800 && gf.items[0].zones[1].color == 0, "gauge: zones without from < to are dropped, a colour may be a number, a bad colour is 0");
          std::string manyz = "{\"form\":{},\"controls\":[{\"type\":\"SuperGauge\",\"name\":\"g\",\"zones\":["; for (int i = 0; i < 30; ++i) manyz += (i ? ",{\"from\":" : "{\"from\":") + std::to_string(i) + ",\"to\":" + std::to_string(i + 1) + "}"; manyz += "]}]}";
          Check(gf.FromJson(manyz, ge) && (int)gf.items[0].zones.size() == MAX_GAUGE_ZONES, "gauge: at most 20 zones are read"); }
        // ---- the zones dialog ----
        { int zr = -1; for (int i = 0; i < nFields; ++i) if (fieldProp[i] == P_ZONES) zr = i; EnsureRowVisible(zr); Click(FIELD_X + FIELD_W - 30, FieldY(zr) + 5); }
        Check(zonesDlg != nullptr && zonesDlg->zones.size() == 3 && zonesDlg->Selected() == 0 && zonesDlg->FieldText(0) == "0" && zonesDlg->FieldText(1) == "60" && zonesDlg->FieldText(2) == "07E0", "zones dialog: opens on the gauge's zones with the first selected");
        { auto ZK = [&](SDL_Keycode k, Uint16 m = 0) { InputEvent v; v.type = InputEvent::KEY_DOWN; v.key = k; v.mod = m; Handle(v); };
          auto ZT = [&](const char* tx) { InputEvent v; v.type = InputEvent::TEXT; strncpy(v.text, tx, sizeof v.text - 1); Handle(v); };
          auto ZF = [&](int k) { Handle(ev(InputEvent::DOWN, zonesDlg->FieldX(k) + 20, zonesDlg->FieldY() + 5)); Handle(ev(InputEvent::UP, 0, 0)); };
          auto ZB = [&](int k) { Handle(ev(InputEvent::DOWN, zonesDlg->BtnX(k) + 6, zonesDlg->BtnY(k) + 3)); Handle(ev(InputEvent::UP, 0, 0)); };
          auto ZSet = [&](int k, const char* v) { ZF(k); ZK(SDLK_a, KMOD_LCTRL); if (v[0]) ZT(v); else ZK(SDLK_BACKSPACE); };
          auto ZSel = [&](int i) { SuperListBox* lb = zonesDlg->Lb(); int x = lb->GetX() + lb->GetBorder() + lb->GetPadding() + 8, y = lb->GetBoxTop() + lb->GetBorder() + lb->GetPadding() + 2 + i * 18; Handle(ev(InputEvent::DOWN, x, y)); Handle(ev(InputEvent::UP, x, y)); };
          { int g0 = grid; ZK(SDLK_g); Check(zonesDlg != nullptr && grid == g0, "zones dialog: modal"); }
          ZSet(1, "55.5"); Check(zonesDlg->zones[0].to == 55.5 && zonesDlg->Message().empty(), "zones dialog: a decimal To is applied");
          ZSet(1, "0"); Check(zonesDlg->zones[0].to == 55.5 && zonesDlg->Message().find("end above") != std::string::npos, "zones dialog: a zone must end above where it starts (refused, red, with the reason)");
          ZSet(1, "-"); Check(zonesDlg->zones[0].to == 55.5 && zonesDlg->Message().find("number") != std::string::npos, "zones dialog: text that is not a number is refused");
          ZSet(0, "60"); Check(zonesDlg->zones[0].from == 0, "zones dialog: From above To is refused too");
          ZSet(1, "60"); ZSet(2, "07"); Check(zonesDlg->zones[0].color == 0x07E0, "zones dialog: a partial colour is ignored until it has 4 hex digits");
          ZSet(2, "F81f"); Check(zonesDlg->zones[0].color == 0xF81F && zonesDlg->FieldText(2) == "F81F", "zones dialog: a 4-digit hex colour is applied (digits only, upper case)");
          ZK(SDLK_ESCAPE);
          { int px = zonesDlg->PalX() + 2, py = zonesDlg->PalY() + 2; Handle(ev(InputEvent::DOWN, px, py)); Handle(ev(InputEvent::UP, 0, 0)); Check(zonesDlg->zones[0].color == ZonesDialog::PaletteColor(0), "zones dialog: clicking a palette colour sets it"); }
          ZSel(2); Check(zonesDlg->Selected() == 2 && zonesDlg->FieldText(0) == "85", "zones dialog: clicking a zone selects it");
          ZB(0); Check(zonesDlg->zones.size() == 4 && zonesDlg->Selected() == 3 && zonesDlg->zones[3].from < zonesDlg->zones[3].to && zonesDlg->FocusedField() == 0, "zones dialog: Add puts a new valid zone after the selection and focuses From");
          ZK(SDLK_ESCAPE); ZB(1); Check(zonesDlg->zones.size() == 3, "zones dialog: Delete");
          ZSel(0); ZB(3); Check(zonesDlg->zones[1].to == 60 && zonesDlg->Selected() == 1 && zonesDlg->zones[0].from == 60, "zones dialog: Move down swaps neighbours");
          ZB(2); Check(zonesDlg->zones[0].from == 0 && zonesDlg->Selected() == 0, "zones dialog: Move up");
          ZB(5); Check(zonesDlg->zones.empty() && zonesDlg->Selected() == -1, "zones dialog: Clear all");
          ZB(4); Check(zonesDlg->zones.size() == 3 && zonesDlg->zones[0].from == 0 && zonesDlg->zones[0].to == 60 && zonesDlg->zones[1].to == 85 && zonesDlg->zones[2].to == 100 && zonesDlg->zones[0].color == 0x07E0 && zonesDlg->zones[1].color == 0xFFE0 && zonesDlg->zones[2].color == 0xF800, "zones dialog: Fill 3 bands makes green / yellow / red over 60 / 85 / 100 % of the range");
          for (int i = 0; i < 25 && zonesDlg; ++i) { ZB(0); if (zonesDlg && zonesDlg->FocusedField() >= 0) ZK(SDLK_ESCAPE); }          // (Esc in the list would cancel the dialog)
          Check((int)zonesDlg->zones.size() == MAX_GAUGE_ZONES && zonesDlg->Message().find("20") != std::string::npos, "zones dialog: at most 20 zones");
          if (zonesDlg) { ZB(4); ZK(SDLK_ESCAPE); } Check(zonesDlg == nullptr && form.items[0].zones.size() == 3 && form.items[0].zones[1].to == 85, "zones dialog: Esc cancels (the gauge keeps its zones)");
          { int zr = -1; for (int i = 0; i < nFields; ++i) if (fieldProp[i] == P_ZONES) zr = i; EnsureRowVisible(zr); Click(FIELD_X + FIELD_W - 30, FieldY(zr) + 5); }
          if (zonesDlg) { ZB(5); ZB(0); if (zonesDlg->FocusedField() >= 0) ZK(SDLK_ESCAPE); int x = zonesDlg->OkX() + 5, y = zonesDlg->OkY() + 3; Handle(ev(InputEvent::DOWN, x, y)); Handle(ev(InputEvent::UP, x, y)); }
          Check(zonesDlg == nullptr && form.items[0].zones.size() == 1, "zones dialog: OK stores the zones"); }
        // ---- graph ----
        Select(1);
        Check(F4("bodyHeight") >= 0 && F4("capacity") >= 0 && F4("autoRange") >= 0 && F4("rangeMin") >= 0 && F4("showGrid") >= 0 && F4("lineColor") >= 0 && F4("preview") >= 0, "graph: rows for body height, samples, range, grid, colours and the preview data");
        { auto gc = MakeControl(&scr, form.items[1], 0, 0); auto* g = static_cast<SuperGraph*>(gc.get()); Check(g->GetCount() == g->GetWidth() - 2 * (g->GetBorder() + g->GetPadding()) && g->GetCount() > 100, "graph: the preview fills the real graph (one sample per pixel of its inner width, sine by default)"); }
        S4("preview", 3, 4); { auto gc = MakeControl(&scr, form.items[1], 0, 0); Check(static_cast<SuperGraph*>(gc.get())->GetCount() == 0 && XInt(form.items[1], "preview") == 3, "graph: Preview data = None leaves the graph empty"); }
        S4("preview", 1, 4); T4("capacity", "50"); { auto gc = MakeControl(&scr, form.items[1], 0, 0); Check(static_cast<SuperGraph*>(gc.get())->GetCount() == 50, "graph: Samples sets the capacity (the preview fills it)"); }
        S4("autoRange", 0, 2); T4("rangeMax", "0"); Check(XFloat(form.items[1], "rangeMax") == 100 && status.find("above") != std::string::npos, "graph: a fixed range must end above where it starts (refused)");
        T4("rangeMin", "-5.5"); T4("rangeMax", "5.5"); Check(XFloat(form.items[1], "rangeMin") == -5.5 && XFloat(form.items[1], "rangeMax") == 5.5, "graph: a decimal range");
        S4("autoRange", 1, 2); S4("showGrid", 0, 2); S4("showRange", 1, 2);
        // ---- user draw ----
        Select(2);
        Check(F4("bodyHeight") >= 0 && F4("persistent") >= 0 && F4("autoRedraw") >= 0 && F4("touch") >= 0, "user draw: rows for body height, persistent canvas, auto redraw, touch input");
        { auto uc = MakeControl(&scr, form.items[2], 0, 0); uc->Push(); Check(uc->GetHeight() >= 100, "user draw: the placeholder control draws (the designer cannot run your code)"); }
        T4("bodyHeight", "60"); Check(XInt(form.items[2], "bodyHeight") == 60 && form.items[2].h < 100, "user draw: the body height sets the control's height");
        T4("bodyHeight", "100");
        // ---- generated code ----
        form.items[0].zones = { { 0, 60, 0x07E0 }, { 60, 85.5, 0xFFE0 } };
        { DControl& g0 = form.items[0]; g0.extra.clear(); ExtraSet(g0, *FindExtra(C_GAUGE, "min"), "-20"); ExtraSet(g0, *FindExtra(C_GAUGE, "max"), "120.5"); ExtraSet(g0, *FindExtra(C_GAUGE, "value"), "55.5"); ExtraSet(g0, *FindExtra(C_GAUGE, "sweepStart"), "-135"); ExtraSet(g0, *FindExtra(C_GAUGE, "sweepEnd"), "135");
          ExtraSet(g0, *FindExtra(C_GAUGE, "majorTicks"), "6"); ExtraSet(g0, *FindExtra(C_GAUGE, "decimals"), "1"); ExtraSet(g0, *FindExtra(C_GAUGE, "unit"), " C"); ExtraSet(g0, *FindExtra(C_GAUGE, "tickLabels"), "false"); ExtraSet(g0, *FindExtra(C_GAUGE, "showValue"), "false");
          ExtraSet(g0, *FindExtra(C_GAUGE, "needle"), "1"); ExtraSet(g0, *FindExtra(C_GAUGE, "needleByZone"), "false"); ExtraSet(g0, *FindExtra(C_GAUGE, "smoothing"), "false"); ExtraSet(g0, *FindExtra(C_GAUGE, "trackColor"), "0x1082"); ExtraSet(g0, *FindExtra(C_GAUGE, "minMaxMs"), "3000"); ExtraSet(g0, *FindExtra(C_GAUGE, "minMaxDeg"), "5"); ExtraSet(g0, *FindExtra(C_GAUGE, "bodyHeight"), "120"); }
        { DControl& g1 = form.items[1]; g1.extra.clear(); ExtraSet(g1, *FindExtra(C_GRAPH, "capacity"), "100"); ExtraSet(g1, *FindExtra(C_GRAPH, "autoRange"), "false"); ExtraSet(g1, *FindExtra(C_GRAPH, "rangeMin"), "-5.5"); ExtraSet(g1, *FindExtra(C_GRAPH, "rangeMax"), "5.5"); ExtraSet(g1, *FindExtra(C_GRAPH, "showGrid"), "false"); ExtraSet(g1, *FindExtra(C_GRAPH, "showRange"), "true"); ExtraSet(g1, *FindExtra(C_GRAPH, "lineColor"), "0xF800"); ExtraSet(g1, *FindExtra(C_GRAPH, "preview"), "2"); ExtraSet(g1, *FindExtra(C_GRAPH, "bodyHeight"), "60"); }
        { DControl& g2 = form.items[2]; g2.extra.clear(); ExtraSet(g2, *FindExtra(C_USERDRAW, "persistent"), "true"); ExtraSet(g2, *FindExtra(C_USERDRAW, "autoRedraw"), "50"); ExtraSet(g2, *FindExtra(C_USERDRAW, "touch"), "true"); }
        { std::string g = GenerateDesigner(form); auto h6 = [&](const char* x) { return g.find(x) != std::string::npos; };
          Check(h6("#include <SuperGauge.h>") && h6("#include <SuperGraph.h>") && h6("#include <SuperUserDraw.h>"), "codegen: headers of the display controls");
          Check(h6(("gauge1(dst, 8, " + std::to_string(form.items[0].y) + ", ").c_str()) && h6(", 120, &lgfx::fonts::Font2)") && h6(", &lgfx::fonts::Font2, 60)") && h6(", 100, &lgfx::fonts::Font2)"), "codegen: constructors: gauge (.., bodyHeight, font), graph (.., font, bodyHeight), user draw (.., bodyHeight, font)");
          Check(h6("gauge1.SetRange(-20.0f, 120.5f);") && h6("gauge1.SetSweep(-135.0f, 135.0f);") && h6("gauge1.SetTicks(6, 1);") && h6("gauge1.SetFormat(1, \" C\");") && h6("gauge1.ShowTickLabels = false;") && h6("gauge1.ShowValue = false;") && h6("gauge1.Needle = SuperGauge::NEEDLE_LINE;") && h6("gauge1.NeedleByZone = false;") && h6("gauge1.ColTrack = 0x1082;") && h6("gauge1.SetMinMax(3000, 5.0f, 2);") && h6("gauge1.SetSmoothing(false);"), "codegen: gauge settings (floats as 12.5f, only what differs from the defaults)");
          Check(h6("gauge1.AddZone(0.0f, 60.0f, 0x07E0);") && h6("gauge1.AddZone(60.0f, 85.5f, 0xFFE0);") && g.find("gauge1.AddZone(0.0f") < g.find("gauge1.SetValue(55.5f);") && g.find("gauge1.SetRange") < g.find("gauge1.AddZone"), "codegen: the zones, with the range before them and the value last");
          Check(h6("graph1.SetCapacity(100);") && h6("graph1.SetRange(-5.5f, 5.5f);") && h6("graph1.ShowGrid = false;") && h6("graph1.ShowRange = true;") && h6("graph1.ColLine = 0xF800;") && !h6("preview") && !h6("graph1.ColGrid"), "codegen: graph settings; the preview data is designer only and is not generated");
          Check(h6("draw1.SetPersistent(true);") && h6("draw1.SetAutoRedraw(50);"), "codegen: user draw settings");
          Check(h6("virtual void draw1_Draw(lgfx::LGFX_Sprite* spr, const SuperRect& body) {}") && h6("virtual void draw1_Pointer(const SuperUserPointer& e) {}") && h6("virtual void draw1_Accept() {}"), "codegen: user draw handlers: Draw, and Pointer / Accept because Touch input is on");
          Check(h6("static void draw1_DrawThunk(SuperUserDraw&, lgfx::LGFX_Sprite* s, const SuperRect& r, void* u) { static_cast<Ch4Base*>(u)->draw1_Draw(s, r); }") && h6("static void draw1_PointerThunk(SuperUserDraw&, const SuperUserPointer& e, void* u) { static_cast<Ch4Base*>(u)->draw1_Pointer(e); }"), "codegen: the library callbacks pass their arguments on to the handlers");
          Check(h6("draw1.SetOnDraw(draw1_DrawThunk, this);") && h6("draw1.SetOnPointer(draw1_PointerThunk, this);") && h6("draw1.SetOnAccept(draw1_AcceptThunk, this);"), "codegen: the callbacks are connected");
          Check(h6("Form.Add(&gauge1, false);") && h6("Form.Add(&graph1, false);") && h6("Form.Add(&draw1);") && !h6("gauge1_") && !h6("graph1_"), "codegen: gauge and graph are display only (no handlers, not focusable); user draw with touch takes focus");
          std::string u = GenerateUserFile(form);
          Check(u.find("void draw1_Draw(lgfx::LGFX_Sprite* spr, const SuperRect& body) override {") != std::string::npos && u.find("spr->drawCircle(body.x + body.w / 2") != std::string::npos && u.find("void draw1_Pointer(const SuperUserPointer& e) override {") != std::string::npos, "user file: stubs with the signatures and a drawing example");
          ExtraSet(form.items[2], *FindExtra(C_USERDRAW, "touch"), "false"); std::string g2 = GenerateDesigner(form);
          Check(g2.find("draw1_Draw") != std::string::npos && g2.find("draw1_Pointer") == std::string::npos && g2.find("draw1_Accept") == std::string::npos && g2.find("Form.Add(&draw1, false);") != std::string::npos, "codegen: without touch a user draw control has only the Draw handler and is not focusable");
          Check(UserFileMissing(form, GenerateUserFile(form)).empty() && HandlerNames(form) == std::vector<std::string>({ "draw1_Draw" }), "user file: the Draw handler counts as a handler"); }
        Check(!ValidName("x_Draw") && !ValidName("x_DrawThunk") && !ValidName("x_Pointer") && !ValidName("x_PointerThunk") && ValidName("drawing"), "names: _Draw and _Pointer handler names are reserved");
        // ---- chunk 5: colour picker, message boxes ----
        {
            form.Clear(); Place(C_COLORPICKER, 8, 8); Check(form.items.size() == 1 && form.items[0].type == C_COLORPICKER && ControlHeight(&scr, form.items[0]) > 0, "chunk 5: a colour picker is placed and drawn");
            ExtraSet(form.items[0], *FindExtra(C_COLORPICKER, "value"), "0x07E0");
            { auto rc = MakeControl(&scr, form.items[0], 0, 0); Check(static_cast<SuperColorPicker*>(rc.get())->GetLive565() == 0x07E0, "chunk 5: the real colour picker shows the Value"); }
            { std::string g = GenerateDesigner(form); Check(g.find("#include <SuperColorPicker.h>") != std::string::npos && g.find("SetColor565(0x07E0);") != std::string::npos, "codegen: colour picker header and initial colour"); }
            form.Clear(); FD::DForm lf; std::string le;
            const std::string js = "{\"form\":{\"name\":\"Ch5\"},\"controls\":[{\"type\":\"SuperPushButton\",\"name\":\"btnGo\",\"text\":\"Go\"}],\"messageBoxes\":[{\"name\":\"msgDelete\",\"title\":\"Careful\",\"message\":[\"Delete all?\",\"Cannot be undone.\"],\"buttons\":[{\"text\":\"Yes\",\"itemId\":\"MSG_DELETE_YES\"},\"No\"],\"default\":1},{\"name\":\"btnGo\",\"message\":\"x\\ny\",\"buttons\":[\"A\",{\"text\":\"B\",\"itemId\":\"MSG_DELETE_YES\"},\"\",\"C\",\"D\"]}]}";
            Check(lf.FromJson(js, le), "message boxes: a file with boxes loads");
            Check(lf.boxes.size() == 2 && lf.boxes[0].name == "msgDelete" && lf.boxes[0].lines.size() == 2 && lf.boxes[0].buttons.size() == 2 && lf.boxes[0].buttons[0].itemId == "MSG_DELETE_YES" && lf.boxes[0].defaultBtn == 1, "message boxes: name, lines, buttons, optional button names, default");
            Check(lf.boxes[1].name != "btnGo" && ValidName(lf.boxes[1].name) && lf.boxes[1].lines == std::vector<std::string>({ "x", "y" }) && lf.boxes[1].buttons.size() == 3 && lf.boxes[1].buttons[1].itemId.empty(), "message boxes: a clashing name is repaired, a message string splits at \\n, at most 3 non-empty buttons, a repeated button name is cleared");
            Check(lf.BoxProblem().empty(), "message boxes: after repair nothing is wrong");
            FD::DForm rt; Check(rt.FromJson(lf.ToJson(), le) && rt.boxes.size() == 2 && rt.boxes[0].lines == lf.boxes[0].lines && rt.boxes[0].buttons[0].itemId == "MSG_DELETE_YES" && rt.ToJson() == lf.ToJson(), "message boxes: save and load round-trip");
            Check(!ValidName("msgDelete_Ask") && !ValidName("x_Open") && !ValidName("x_Poll") && !ValidName("MSG_CANCELLED") && !ValidName("MSG_PENDING"), "names: the message box helper names are reserved");
            std::string g = GenerateDesigner(lf); auto h = [&](const char* x) { return g.find(x) != std::string::npos; };
            Check(h("#include <SuperMessageBox.h>") && h("static constexpr int MSG_CANCELLED = -1, MSG_PENDING = -2;") && h("static constexpr int MSG_DELETE_YES = 0;") && h("SuperMessageBox msgDelete;") && h("msgDelete(dst, 220, &lgfx::fonts::Font2, enc, btnA, btnB)"), "codegen: message box header, constants, member, constructor");
            Check(h("msgDelete.SetMessage(\"Delete all?\\n\"") && h("\"Cannot be undone.\");") && h("msgDelete.AddButton(\"Yes\");") && h("msgDelete.SetDefault(1);") && h("msgDelete.SetLabel(\"Careful\");") && h("msgDelete.SetRestore(msgDelete_RestoreThunk, this);"), "codegen: message box set-up");
            Check(h("int msgDelete_Ask()") && h("void msgDelete_Open()") && h("int msgDelete_Poll()") && h("static void msgDelete_RestoreThunk(void* u)") && h("s->Form.Redraw(s->msgDelete.GetX()"), "codegen: Ask / Open / Poll helpers and the restore thunk that redraws the form");
            FD::DForm none; none.name = "Plain"; Check(GenerateDesigner(none).find("MSG_") == std::string::npos && GenerateDesigner(none).find("SuperMessageBox") == std::string::npos, "codegen: a form without boxes has no message box code");
            lf.boxes[0].buttons[0].itemId = "class"; Check(!lf.BoxProblem().empty(), "export: an unsafe button name is refused (BoxProblem)");
            lf.boxes[0].buttons[0].itemId.clear();
            { auto mb = MakeMessageBox(&scr, lf.boxes[0]); Check(mb && mb->GetHeight() > 0, "designer: a message box can be built for the preview"); }
        }
        // ---- the message box editor ----
        {
            form.Clear(); Place(C_PUSHBUTTON, 8, 8); form.items[0].name = "btnGo"; form.boxes.clear(); sel = -1; BuildFields();
            auto Key5 = [&](SDL_Keycode k, Uint16 m = 0) { InputEvent v; v.type = InputEvent::KEY_DOWN; v.key = k; v.mod = m; Handle(v); };
            auto Txt5 = [&](const char* tx) { InputEvent v; v.type = InputEvent::TEXT; strncpy(v.text, tx, sizeof v.text - 1); Handle(v); };
            auto Click5 = [&](int x, int y) { Handle(ev(InputEvent::DOWN, x, y)); Handle(ev(InputEvent::UP, x, y)); };
            int br = -1; for (int i = 0; i < nFields; ++i) if (fieldProp[i] == P_MSGBOXES) br = i;
            Check(br >= 0 && PropLabel(P_MSGBOXES) == "Message boxes", "message box editor: the form has a Message boxes row");
            EnsureRowVisible(br); Click5(FIELD_X + FIELD_W - 30, FieldY(br) + 5);
            Check(boxDlg != nullptr && boxDlg->Boxes().empty() && boxDlg->Selected() == -1, "message box editor: Edit opens it (empty list)");
            { int g0 = grid; Key5(SDLK_g); Check(boxDlg != nullptr && grid == g0, "message box editor: modal"); }
            Key5(SDLK_INSERT); Check(boxDlg->Boxes().size() == 1 && boxDlg->Boxes()[0].name == "msg1" && boxDlg->FocusedField() == MessageBoxEditor::F_NAME && boxDlg->FieldText(MessageBoxEditor::F_NAME) == "msg1", "message box editor: Insert adds a valid box and focuses its name");
            auto SetF = [&](int k, const char* v) { Click5(boxDlg->FX(k) + 20, boxDlg->FY(k) + 5); Key5(SDLK_a, KMOD_LCTRL); if (*v) Txt5(v); else Key5(SDLK_BACKSPACE); };
            SetF(MessageBoxEditor::F_NAME, "msgDelete"); Check(boxDlg->Boxes()[0].name == "msgDelete" && boxDlg->Message().empty(), "message box editor: the name is applied as typed");
            SetF(MessageBoxEditor::F_NAME, "class");
            Check(boxDlg->Boxes()[0].name != "class" && boxDlg->BadField() == MessageBoxEditor::F_NAME && boxDlg->Message().find("safe C++ name") != std::string::npos, "message box editor: a keyword name is refused (red, with the reason)");
            SetF(MessageBoxEditor::F_NAME, "btnGo"); Check(boxDlg->Boxes()[0].name != "btnGo" && boxDlg->Message().find("already used") != std::string::npos, "message box editor: a control's name is refused");
            SetF(MessageBoxEditor::F_NAME, "msgDelete");
            SetF(MessageBoxEditor::F_TITLE, "Careful"); Check(boxDlg->Boxes()[0].title == "Careful", "message box editor: title");
            SetF(MessageBoxEditor::F_W, "10"); Check(boxDlg->Boxes()[0].w == 220 && boxDlg->Message().find("60 to 1000") != std::string::npos, "message box editor: a width below 60 is refused");
            SetF(MessageBoxEditor::F_W, "260"); Check(boxDlg->Boxes()[0].w == 260, "message box editor: width");
            SetF(MessageBoxEditor::F_T0, "Yes"); SetF(MessageBoxEditor::F_T1, "No"); Check(boxDlg->Boxes()[0].buttons.size() == 2 && boxDlg->Boxes()[0].buttons[1].text == "No", "message box editor: typing a text in the next slot adds a button");
            SetF(MessageBoxEditor::F_T2, "Maybe"); SetF(MessageBoxEditor::F_T2, ""); Check(boxDlg->Boxes()[0].buttons.size() == 2, "message box editor: emptying a text removes the button");
            SetF(MessageBoxEditor::F_N0, "MSG_DELETE_YES"); Check(boxDlg->Boxes()[0].buttons[0].itemId == "MSG_DELETE_YES", "message box editor: an optional button name");
            SetF(MessageBoxEditor::F_N1, "MSG_DELETE_YES"); Check(boxDlg->Boxes()[0].buttons[1].itemId.empty() && boxDlg->Message().find("already used") != std::string::npos, "message box editor: a repeated button name is refused");
            SetF(MessageBoxEditor::F_N1, "int"); Check(boxDlg->Boxes()[0].buttons[1].itemId.empty() && boxDlg->BadField() == MessageBoxEditor::F_N1, "message box editor: a keyword as button name is refused");
            SetF(MessageBoxEditor::F_DEF, "5"); Check(boxDlg->Boxes()[0].defaultBtn == 0 && boxDlg->Message().find("0 to 1") != std::string::npos, "message box editor: a default button past the last is refused");
            SetF(MessageBoxEditor::F_DEF, "1"); Check(boxDlg->Boxes()[0].defaultBtn == 1, "message box editor: default button");
            SetF(MessageBoxEditor::F_N2, "X"); Check(boxDlg->Message().find("text first") != std::string::npos, "message box editor: a name for a button that does not exist is refused");
            Key5(SDLK_ESCAPE);
            { int x = boxDlg->AutoX() + 5, y = boxDlg->AutoY() + 36 * 3 - 4 + 3; Click5(x, y); Check(boxDlg->Boxes()[0].buttons[1].itemId == "MSG_DELETE_NO", "message box editor: Auto names fills the empty button names"); }
            Click5(boxDlg->CenterX() + 5, boxDlg->CenterY() + 3); Check(!boxDlg->Boxes()[0].centered, "message box editor: Centered toggles");
            Click5(boxDlg->MsgBtnX() + 5, boxDlg->MsgBtnY() + 3); Check(boxDlg->Sub() != nullptr && boxDlg->Sub()->items == std::vector<std::string>({ "Message" }), "message box editor: Edit message opens the lines dialog on the box's lines");
            Key5(SDLK_INSERT); Key5(SDLK_a, KMOD_LCTRL); Txt5("Second line"); Key5(SDLK_ESCAPE); Key5(SDLK_RETURN);
            Check(boxDlg->Sub() == nullptr && boxDlg->Boxes()[0].lines.size() == 2 && boxDlg->Boxes()[0].lines[1] == "Second line", "message box editor: OK in the lines dialog stores the lines");
            Key5(SDLK_INSERT); Check(boxDlg->Boxes().size() == 2 && boxDlg->Boxes()[1].name == "msg1" && boxDlg->Selected() == 1, "message box editor: a second box");
            Key5(SDLK_ESCAPE); Key5(SDLK_UP); Check(boxDlg->Selected() == 0 && boxDlg->FieldText(MessageBoxEditor::F_NAME) == "msgDelete", "message box editor: the list selects a box and the fields follow");
            { int x = boxDlg->LBX(3) + 5, y = boxDlg->LBY() + 3; Click5(x, y); Check(boxDlg->Boxes()[1].name == "msgDelete" && boxDlg->Selected() == 1, "message box editor: Down moves the box"); }
            { int x = boxDlg->LBX(1) + 5, y = boxDlg->LBY() + 3; Click5(x, y); Check(boxDlg->Boxes().size() == 1, "message box editor: Delete"); }
            { int x = boxDlg->YesNoX() + 5, y = boxDlg->YesNoY() + 3; Click5(x, y); const DMessageBox& yn = boxDlg->Boxes()[boxDlg->Selected()];
              Check(yn.name == "msgYesNo" && yn.lines == std::vector<std::string>({ "Confirm?" }) && yn.buttons.size() == 2 && yn.buttons[0].text == "Yes" && yn.buttons[0].itemId == "MSG_YESNO_YES" && yn.buttons[1].itemId == "MSG_YESNO_NO" && yn.defaultBtn == 1, "message box editor: Add Yes/No makes the ready-made Confirm? box with named buttons");
              Click5(x, y); Check(boxDlg->Boxes()[boxDlg->Selected()].name == "msgYesNo2" && boxDlg->Boxes()[boxDlg->Selected()].buttons[0].itemId != "MSG_YESNO_YES", "message box editor: a second Yes/No gets its own names"); Key5(SDLK_ESCAPE); Key5(SDLK_DELETE); Key5(SDLK_DELETE); }
            // the look rows: font, colours, rich text, style fonts
            { const DMessageBox& cb = boxDlg->Boxes()[boxDlg->Selected()];
              Click5(boxDlg->RichX(1) + 5, boxDlg->LookY() + 5); Check(cb.rich, "message box editor: Rich text Yes"); Click5(boxDlg->RichX(0) + 5, boxDlg->LookY() + 5); Check(!cb.rich, "message box editor: Rich text No");
              Click5(boxDlg->FontBtnX() + 10, boxDlg->LookY() + 5); Check(boxDlg->FontDlg() != nullptr, "message box editor: the Font button opens the font dialog");
              { int g0 = grid; Key5(SDLK_g); Check(grid == g0 && boxDlg->FontDlg() != nullptr, "message box editor: (the font dialog is modal)"); }
              std::string before = cb.font; Key5(SDLK_DOWN); Key5(SDLK_ESCAPE); Check(boxDlg != nullptr && boxDlg->FontDlg() == nullptr && cb.font == before, "message box editor: Esc in the font dialog changes nothing (and keeps the editor open)");
              Click5(boxDlg->FontBtnX() + 10, boxDlg->LookY() + 5); Key5(SDLK_DOWN); Key5(SDLK_RETURN); Check(boxDlg->FontDlg() == nullptr && cb.font != before, "message box editor: choosing a font in the dialog sets the box font");
              Click5(boxDlg->ColBtnX(1) + 5, boxDlg->LookY() + 5); Check(boxDlg->ColorDlg() != nullptr, "message box editor: a colour swatch opens the colour dialog"); Key5(SDLK_ESCAPE); Check(boxDlg->ColorDlg() == nullptr, "message box editor: Esc closes it");
              Click5(boxDlg->ColBtnX(0) + 5, boxDlg->LookY() + 5); boxDlg->ColorDlg()->color = 0x07E0; Key5(SDLK_RETURN); Check(boxDlg->ColorDlg() == nullptr && cb.fg == 0x07E0, "message box editor: OK in the colour dialog sets the text colour");
              Click5(boxDlg->StyleX(0) + 10, boxDlg->StyleY() + 5); Check(boxDlg->FontDlg() != nullptr, "message box editor: a style font button opens the font dialog"); Key5(SDLK_DOWN); Key5(SDLK_RETURN); Check(!cb.styleFont[0].empty() && cb.styleFont[1].empty(), "message box editor: the chosen font becomes the bold style font");
              Click5(boxDlg->StyleX(0) + 170, boxDlg->StyleY() + 5); Check(cb.styleFont[0].empty(), "message box editor: clicking the right end of a set style font clears it"); }
            Key5(SDLK_ESCAPE); Check(boxDlg == nullptr && form.boxes.empty(), "message box editor: Esc cancels (the form keeps its boxes)");
            Click5(FIELD_X + FIELD_W - 30, FieldY(br) + 5); Key5(SDLK_INSERT); SetF(MessageBoxEditor::F_NAME, "msgAsk"); SetF(MessageBoxEditor::F_T0, "Go"); Key5(SDLK_ESCAPE);
            { int x = boxDlg->OkX() + 5, y = boxDlg->BtnY() + 3; Click5(x, y); }
            Check(boxDlg == nullptr && form.boxes.size() == 1 && form.boxes[0].name == "msgAsk" && form.boxes[0].buttons[0].text == "Go", "message box editor: OK stores the boxes in the form");
            { std::string g = GenerateDesigner(form); Check(g.find("int msgAsk_Ask()") != std::string::npos, "message box editor: the generated code has the box"); }
            form.boxes.clear(); sel = -1; BuildFields();
        }
        // ---- start-up: the last form, else template.json, else the default ----
        {
            namespace fs = std::filesystem; std::error_code se; std::string dir = fs::temp_directory_path(se).string() + "/fd_startup"; fs::remove_all(dir, se); fs::create_directories(dir + "/app", se);
            auto put = [&](const std::string& p, const std::string& t) { std::ofstream o(p, std::ios::binary); o << t; };
            setenv("FD_SETTINGS", (dir + "/settings.json").c_str(), 1); settings = AppSettings(); std::string savedFD = getenv("FD_FORM") ? getenv("FD_FORM") : ""; unsetenv("FD_FORM"); appDirOverride = dir + "/app"; currentFile.clear();
            put(dir + "/mine.json", "{\"form\":{\"name\":\"Mine\"},\"controls\":[{\"type\":\"SuperLabel\",\"name\":\"hello\",\"text\":\"Hi\"}]}");
            put(dir + "/app/template.json", "{\"form\":{\"name\":\"Tmpl\"},\"controls\":[{\"type\":\"SuperPushButton\",\"name\":\"go\",\"text\":\"Go\"}]}");
            form.Clear(); FixOutDir(); settings.lastForm = dir + "/mine.json";
            Check(StartupForm() == "last" && form.name == "Mine" && form.items.size() == 1 && form.items[0].name == "hello" && currentFile == dir + "/mine.json", "start-up: the last form file is reopened (and is the current file)");
            currentFile.clear(); form.Clear(); settings.lastForm = dir + "/gone.json";
            Check(StartupForm() == "template" && form.name == "NewForm" && form.items.size() == 1 && form.items[0].name == "go" && currentFile.empty(), "start-up: a last form that is not accessible falls back to template.json, named NewForm, with no file yet");
            form.Clear(); settings.lastForm = ""; Check(StartupForm() == "template" && form.name == "NewForm", "start-up: no last form: template.json too");
            put(dir + "/broken.json", "not json"); form.Clear(); settings.lastForm = dir + "/broken.json"; currentFile.clear();
            Check(StartupForm() == "template" && form.name == "NewForm" && currentFile.empty(), "start-up: a last form that cannot be read falls back to the template");
            put(dir + "/app/template.json", "{\"form\":{\"name\":\"T\"},\"controls\":[{\"type\":\"SuperLabel\",\"name\":\"NewForm\"}]}"); form.Clear(); settings.lastForm = "";
            Check(StartupForm() == "template" && form.name == "NewForm2", "start-up: the form name NewForm gives way to a control with that name");
            fs::remove(dir + "/app/template.json", se); form.Clear(); Place(C_LABEL, 8, 8); form.name = "Keep"; settings.lastForm = dir + "/gone.json"; currentFile.clear();
            Check(StartupForm() == "" && form.name == "Keep" && form.items.size() == 1 && currentFile.empty(), "start-up: neither exists: the form is left as it is (the regular default new form)");
            put(dir + "/app/template.json", "not json"); Check(StartupForm() == "", "start-up: a template that cannot be read is ignored too");
            setenv("FD_FORM", (dir + "/mine.json").c_str(), 1); settings.lastForm = dir + "/mine.json"; form.Clear(); Check(StartupForm() == "" && form.items.empty(), "start-up: an explicit FD_FORM wins (the old behaviour: FD_OPEN loads it)");
            // the setting is saved and loaded
            unsetenv("FD_FORM"); settings.lastForm = dir + "/mine.json"; settings.Save(); { AppSettings back; Check(back.Load() && back.lastForm == dir + "/mine.json", "start-up: lastForm is kept in the application settings json"); }
            { std::ifstream in(dir + "/settings.json"); std::stringstream ss; ss << in.rdbuf(); Check(ss.str().find("\"lastForm\"") != std::string::npos, "start-up: ... as a readable key"); }
            // opening and saving through the dialogs remembers the file
            settings.lastForm = ""; form.Clear(); currentFile.clear(); OpenFileDialog(FP_OPEN); fileDlg->path = dir + "/mine.json"; fileDlg->state = FD::FileDialog::OK; CloseFileDialog();
            Check(settings.lastForm == dir + "/mine.json" && form.name == "Mine", "start-up: opening a file through the dialog remembers it as the last form");
            settings.lastForm = ""; currentFile.clear(); OpenFileDialog(FP_SAVE); fileDlg->path = dir + "/saved.json"; fileDlg->state = FD::FileDialog::OK; CloseFileDialog();
            Check(settings.lastForm == dir + "/saved.json" && fs::exists(dir + "/saved.json", se), "start-up: saving through the dialog remembers it as well");
            appDirOverride.clear(); currentFile.clear(); fs::remove_all(dir, se); unsetenv("FD_SETTINGS"); settings = AppSettings(); if (!savedFD.empty()) setenv("FD_FORM", savedFD.c_str(), 1);
            form.Clear(); FixOutDir(); sel = -1; BuildFields();
        }
        // ---- control order: move up / down / front / back ----
        {
            form.Clear(); for (int i = 0; i < 4; ++i) { Place(C_LABEL, 8, 8); form.items.back().name = "c" + std::to_string(i + 1); form.items.back().x = 8; form.items.back().y = 8; form.items.back().w = 60; } sel = -1; treeFirst = 0; BuildFields();
            auto names = [&]() { std::string r; for (auto& c : form.items) r += c.name + ","; return r; };
            auto Ob = [&](int k) { Handle(ev(InputEvent::DOWN, OrdBtnX(k) + 5, OrdBtnY() + 5)); Handle(ev(InputEvent::UP, 0, 0)); };
            Ob(1); Check(names() == "c1,c2,c3,c4," && sel == -1, "order: with nothing selected the order buttons do nothing");
            Select(1); Ob(0); Check(names() == "c2,c1,c3,c4," && sel == 0 && Sel()->name == "c2", "order: Up moves the selected control one place earlier and keeps it selected");
            Ob(0); Check(names() == "c2,c1,c3,c4," && sel == 0 && status.find("already") != std::string::npos, "order: Up on the first control does nothing and says why");
            Ob(1); Ob(1); Check(names() == "c1,c3,c2,c4," && sel == 2, "order: Down moves it one place later");
            Ob(2); Check(names() == "c1,c3,c4,c2," && sel == 3 && Sel()->name == "c2", "order: Front moves it to the end (drawn last, on top)");
            Ob(1); Check(names() == "c1,c3,c4,c2," && status.find("already") != std::string::npos, "order: Down on the last control does nothing");
            Ob(3); Check(names() == "c2,c1,c3,c4," && sel == 0, "order: Back moves it to the start (drawn first, behind)");
            // draw order decides what a click hits where controls overlap
            Select(-1); Check(HitControl(10, 10) == 3, "order: the last control is the one a click hits where all four overlap");
            Select(0); Ob(2); Check(HitControl(10, 10) == 3 && Sel()->name == "c2", "order: after Front the control is the one a click hits");
            { std::string j = form.ToJson(); DForm lf; std::string le; lf.FromJson(j, le); Check(lf.items[3].name == "c2" && lf.items[0].name == "c1", "order: the order is saved (and so is the generated Form.Add order)"); }
            { std::string g = GenerateDesigner(form); Check(g.find("Form.Add(&c1") < g.find("Form.Add(&c3") && g.find("Form.Add(&c4") < g.find("Form.Add(&c2"), "order: the code adds the controls in that order (the last is in front)"); }
            for (int i = 0; i < 12; ++i) { Place(C_LABEL, 8, 8); } Select(0); Ob(2); Check(sel == (int)form.items.size() - 1 && treeFirst + TreeRows() > sel && treeFirst <= sel, "order: the list scrolls to show the moved control");
            form.Clear(); sel = -1; treeFirst = 0; BuildFields();
        }
        { form.Clear(); form.w = 5000; form.h = 5000; Check(MAX_FORM_W == 1920 && MAX_FORM_H == 1080, "size: the largest form is 1920 x 1080"); form.w = MAX_FORM_W; form.h = MAX_FORM_H; DForm lf; std::string le; Check(lf.FromJson(form.ToJson(), le) && lf.w == 1920 && lf.h == 1080, "size: a 1920 x 1080 form is saved and loaded");
          lf.FromJson("{\"form\":{\"width\":99999,\"height\":99999},\"controls\":[]}", le); Check(lf.w == 1920 && lf.h == 1080, "size: a file asking for more is cut to 1920 x 1080");
          zoom = 100; scrollX = scrollY = 0; ZoomCentre(100); ClampScroll(); SyncBars(); Check(surfV.Needed() && surfH.Needed() && surfV.Total() == 1080 + 2 * PAD && surfH.Total() == 1920 + 2 * PAD, "size: the big form scrolls on both axes");
          Fit(); Check(zoom == std::min((VW - 2 * PAD) * 100 / 1920, (VH - 2 * PAD) * 100 / 1080) && ZW() + 2 * PAD <= VW && ZH() + 2 * PAD <= VH, "size: Fit shrinks it into the surface");
          RenderFormSprite(); Check(formSpr.width() == 1920 && formSpr.height() == 1080, "size: the design sprite is the full size");
          form.Clear(); sel = -1; zoom = 100; ZoomCentre(100); scrollX = scrollY = 0; BuildFields(); }
        // ---- tab (focus) order: set once, independent of the list order ----
        {
            form.Clear(); for (int i = 0; i < 4; ++i) { Place(C_PUSHBUTTON, 8, 8 + i * 30); form.items.back().name = "b" + std::to_string(i + 1); } sel = -1; treeFirst = 0; BuildFields();
            auto tabs = [&]() { std::string r; for (size_t i = 0; i < form.items.size(); ++i) r += form.items[i].name + "=" + std::to_string(form.EffTab(i)) + ","; return r; };
            auto Ob = [&](int k) { Handle(ev(InputEvent::DOWN, OrdBtnX(k) + 5, OrdBtnY() + 5)); Handle(ev(InputEvent::UP, 0, 0)); };
            Check(tabs() == "b1=1,b2=2,b3=3,b4=4," && !form.TabOrderCustom() && !NeedsExplicitTabs(form), "tab order: new controls get the next number as they are added");
            Check(form.ToJson().find("tabOrder") == std::string::npos && GenerateDesigner(form).find("SetTabOrder") == std::string::npos, "tab order: while it equals the list order nothing is saved or generated");
            Select(0); Ob(2); Check(tabs() == "b2=2,b3=3,b4=4,b1=1,", "tab order: moving a control to the front changes its draw order but NOT its tab number");
            Check(form.TabOrderCustom() && NeedsExplicitTabs(form), "tab order: now the focus order differs from the list order");
            { std::string j = form.ToJson(); DForm lf; std::string le; Check(j.find("\"tabOrder\": 1") != std::string::npos && lf.FromJson(j, le) && lf.TabSequence() == std::vector<int>({ 3, 0, 1, 2 }) && lf.ToJson() == j, "tab order: the numbers are saved when they differ, and loaded back"); }
            { std::string g = GenerateDesigner(form); Check(g.find("b1.SetTabOrder(1);") != std::string::npos && g.find("b2.SetTabOrder(2);") != std::string::npos && g.find("b4.SetTabOrder(4);") != std::string::npos && g.find("Form.Add(&b2") < g.find("Form.Add(&b1"), "tab order: the code sets SetTabOrder on every focusable control (and adds them in list order)"); }
            // the user edits the number
            { int tr = -1; for (int i = 0; i < nFields; ++i) if (fieldProp[i] == P_TABORDER) tr = i; Select(3); tr = -1; for (int i = 0; i < nFields; ++i) if (fieldProp[i] == P_TABORDER) tr = i; Check(tr >= 0 && PropLabel(P_TABORDER) == "Tab order" && fields[tr].text == "1", "tab order: a Tab order row shows the control's number");
              EnsureRowVisible(tr); Click(FIELD_X + 20, FieldY(tr) + 5); Key(SDLK_a, KMOD_LCTRL); Type("9"); Key(SDLK_RETURN); Check(form.items[3].tabOrder == 9 && form.TabSequence() == std::vector<int>({ 0, 1, 2, 3 }), "tab order: typing a number changes the focus order (here b1 goes last)");
              Click(FIELD_X + 20, FieldY(tr) + 5); Key(SDLK_a, KMOD_LCTRL); Key(SDLK_BACKSPACE); Check(form.items[3].tabOrder == 9, "tab order: an empty number is refused (the old one stays)");
              Ob(3); Check(form.items[0].name == "b1" && form.items[0].tabOrder == 9 && form.items[1].tabOrder == 2, "tab order: moving it to the back again changes the draw order only"); }
            // a form without numbers keeps its list positions, and they stick
            { DForm lf; std::string le; lf.FromJson("{\"form\":{},\"controls\":[{\"type\":\"SuperPushButton\",\"name\":\"x\"},{\"type\":\"SuperPushButton\",\"name\":\"y\"},{\"type\":\"SuperPushButton\",\"name\":\"z\"}]}", le);
              Check(lf.items[0].tabOrder == 1 && lf.items[1].tabOrder == 2 && lf.items[2].tabOrder == 3, "tab order: a file without numbers gets the list positions, fixed from then on");
              lf.FromJson("{\"form\":{},\"controls\":[{\"type\":\"SuperPushButton\",\"name\":\"x\",\"tabOrder\":5},{\"type\":\"SuperPushButton\",\"name\":\"y\"}]}", le); Check(lf.items[0].tabOrder == 5 && lf.items[1].tabOrder == 2, "tab order: a mixed file keeps the given number, the other gets its position"); }
            { DForm lf; DControl a, b, c; a.type = C_PUSHBUTTON; a.name = "a"; b = a; b.name = "b"; c = a; c.name = "c"; a.tabOrder = 2; b.tabOrder = 2; c.tabOrder = 1; lf.items = { a, b, c }; Check(lf.TabSequence() == std::vector<int>({ 2, 0, 1 }), "tab order: equal numbers keep the list order (as the library does)"); }
            // display-only controls have no tab number and are not given SetTabOrder
            { form.Clear(); Place(C_LABEL, 8, 8); Place(C_PUSHBUTTON, 8, 40); Place(C_PUSHBUTTON, 8, 80); form.items[2].tabOrder = 1; Select(0); bool has = false; for (int i = 0; i < nFields; ++i) if (fieldProp[i] == P_TABORDER) has = true; Check(!has, "tab order: a label has no Tab order row");
              std::string g = GenerateDesigner(form); Check(g.find("button1.SetTabOrder(2);") != std::string::npos && g.find("button2.SetTabOrder(1);") != std::string::npos && g.find("label1.SetTabOrder") == std::string::npos, "tab order: only focusable controls get SetTabOrder"); }
            form.Clear(); sel = -1; treeFirst = 0; BuildFields();
        }
        // ---- Keep hidden controls hidden (form property) ----
        {
            form.Clear(); Place(C_LABEL, 8, 8); Place(C_LABEL, 8, 40); form.items[0].ctlGroup = "GRP_A"; form.items[1].ctlGroup = "GRP_A"; form.items[1].visible = false; sel = -1; BuildFields();
            int kr = -1; for (int i = 0; i < nFields; ++i) if (fieldProp[i] == P_KEEPHIDDEN) kr = i;
            Check(kr >= 0 && PropLabel(P_KEEPHIDDEN) == "Keep hidden" && !form.keepHidden && SegCurrent(P_KEEPHIDDEN) == 0, "keep hidden: a form row, off by default");
            { std::string g = GenerateDesigner(form); Check(g.find("SetKeepHidden") == std::string::npos && g.find("label2.SetVisible(false, false);") != std::string::npos, "keep hidden: off: nothing extra is generated"); }
            EnsureRowVisible(kr); Click(FIELD_X + FIELD_W - 10, FieldY(kr) + 5); Check(form.keepHidden && SegCurrent(P_KEEPHIDDEN) == 1, "keep hidden: clicking Yes turns it on");
            { std::string g = GenerateDesigner(form); Check(g.find("label2.SetKeepHidden(true);") != std::string::npos && g.find("label1.SetKeepHidden") == std::string::npos && g.find("label2.SetVisible(false, false);") < g.find("label2.SetKeepHidden(true);"), "keep hidden: on: only the Visible = No control gets SetKeepHidden(true), after SetVisible(false, false)"); }
            { std::string j = form.ToJson(); DForm lf; std::string le; Check(j.find("\"keepHidden\": true") != std::string::npos && lf.FromJson(j, le) && lf.keepHidden && lf.ToJson() == j, "keep hidden: saved in the form and loaded back"); form.keepHidden = false; Check(form.ToJson().find("keepHidden") == std::string::npos, "keep hidden: off is not saved"); }
            form.keepHidden = true; form.Clear(); Check(!form.keepHidden, "keep hidden: Clear() resets it");
            form.Clear(); sel = -1; BuildFields();
        }
        // ---- rich text options: Label, Rich text control, message boxes; style fonts ----
        {
            form.Clear(); Place(C_LABEL, 8, 8); Place(C_RICHTEXT, 8, 40); form.items[0].name = "lbl"; form.items[1].name = "rt"; form.items[0].text = "<color=red>Hot</color> <b>bold</b>";
            Check(FindExtra(C_LABEL, "rich") && FindExtra(C_LABEL, "boldFont") && FindExtra(C_RICHTEXT, "headingFont") && !XBool(form.items[0], "rich"), "rich options: the label has Rich text (off) and the four style fonts, the rich text control the style fonts");
            { std::string g = GenerateDesigner(form); Check(g.find("SetRich") == std::string::npos && g.find("SetStyleFont") == std::string::npos, "rich options: a plain label generates nothing extra"); }
            { auto lc = MakeControl(&scr, form.items[0], 0, 0); lc->Push(); int h0 = lc->GetHeight(); (void)h0; Select(0); SyncFields();
              int rr = -1; for (int i = 0; i < nFields; ++i) if (const ExtraDef* d = ExtraOf(fieldProp[i])) if (std::string(d->key) == "rich") rr = i; Check(rr >= 0 && PropLabel(fieldProp[rr]) == "Rich text", "rich options: the label has a Rich text row");
              EnsureRowVisible(rr); Click(FIELD_X + FIELD_W - 10, FieldY(rr) + 5); Check(XBool(form.items[0], "rich"), "rich options: clicking Yes turns it on"); }
            ExtraSet(form.items[0], *FindExtra(C_LABEL, "boldFont"), "FreeSansBold12pt7b");
            { std::string g = GenerateDesigner(form); Check(g.find("lbl.SetRich(true);") != std::string::npos && g.find("lbl.SetStyleFont(SuperLabel::FONT_BOLD, &lgfx::fonts::FreeSansBold12pt7b);") != std::string::npos, "rich options: a rich label gets SetRich(true) and its style fonts"); }
            { DControl plain = form.items[0]; plain.extra["rich"] = "false"; plain.extra["boldFont"] = "FreeSansBold12pt7b"; DForm f2; f2.items.push_back(plain); f2.items[0].name = "l2"; Check(GenerateDesigner(f2).find("SetStyleFont") == std::string::npos, "rich options: a style font on a label that is not rich is not generated"); }
            { auto a = MakeControl(&scr, form.items[0], 0, 0); DControl rich = form.items[0]; ExtraSet(rich, *FindExtra(C_LABEL, "boldFont"), ""); auto b = MakeControl(&scr, rich, 0, 0); Check(a->GetHeight() > b->GetHeight(), "rich options: the preview uses the bold font (a taller line)"); }
            ExtraSet(form.items[1], *FindExtra(C_RICHTEXT, "italicFont"), "FreeSansOblique12pt7b"); ExtraSet(form.items[1], *FindExtra(C_RICHTEXT, "headingFont"), "FreeSansBold24pt7b");
            { std::string g = GenerateDesigner(form); Check(g.find("rt.SetStyleFont(SuperRichText::FONT_ITALIC, &lgfx::fonts::FreeSansOblique12pt7b);") != std::string::npos && g.find("rt.SetStyleFont(SuperRichText::FONT_HEADING, &lgfx::fonts::FreeSansBold24pt7b);") != std::string::npos, "rich options: the rich text control generates its style fonts"); }
            { std::string j = form.ToJson(); DForm lf; std::string le; Check(lf.FromJson(j, le) && XBool(lf.items[0], "rich") && XStr(lf.items[0], "boldFont") == "FreeSansBold12pt7b" && XStr(lf.items[1], "headingFont") == "FreeSansBold24pt7b" && lf.ToJson() == j, "rich options: saved and loaded back"); }
            // message boxes
            DMessageBox mb; mb.name = "msgR"; mb.lines = { "<color=red>Danger</color>" }; mb.buttons = { { "OK", "" } }; mb.rich = true; mb.styleFont[0] = "FreeSansBold12pt7b"; mb.font = "Font4"; mb.fg = 0xFFE0; mb.bg = 0x0010; mb.border = 0xF800;
            form.boxes.push_back(mb);
            { std::string g = GenerateDesigner(form); Check(g.find("msgR.SetRich(true);") != std::string::npos && g.find("msgR.SetStyleFont(SuperMessageBox::FONT_BOLD, &lgfx::fonts::FreeSansBold12pt7b);") != std::string::npos && g.find("msgR.ColFg = 0xFFE0;") != std::string::npos && g.find("msgR.ColBg = 0x0010;") != std::string::npos, "rich options: a message box generates SetRich, style fonts and its colours"); }
            { std::string j = form.ToJson(); DForm lf; std::string le; Check(j.find("\"rich\": true") != std::string::npos && lf.FromJson(j, le) && lf.boxes.size() == 1 && lf.boxes[0].rich && lf.boxes[0].styleFont[0] == "FreeSansBold12pt7b" && lf.boxes[0].font == "Font4" && lf.ToJson() == j, "rich options: message box rich flag and fonts saved and loaded back"); }
            { DForm lf; std::string le; lf.FromJson("{\"form\":{},\"controls\":[],\"messageBoxes\":[{\"name\":\"m\",\"message\":\"x\",\"buttons\":[\"OK\"],\"rich\":true,\"boldFont\":\"NoSuchFont\"}]}", le); Check(lf.boxes.size() == 1 && lf.boxes[0].rich && lf.boxes[0].styleFont[0].empty(), "rich options: an unknown style font in a file is dropped"); }
            { DMessageBox hb = form.boxes[0]; hb.lines = { "<h1>Big</h1>" }; hb.styleFont[3] = "FreeSansBold24pt7b"; hb.font = "Font2"; auto pm = MakeMessageBox(&scr, hb); DMessageBox plain = hb; plain.rich = false; auto pp = MakeMessageBox(&scr, plain); Check(pm->GetHeight() > 0 && pm->GetHeight() != pp->GetHeight(), "rich options: the message box preview is rich when asked"); }
            form.Clear(); sel = -1; BuildFields();
        }
        // ---- the ? help window ----
        {
            form.Clear(); Place(C_LABEL, 8, 8); sel = -1; BuildFields();
            auto K6 = [&](SDL_Keycode k, Uint16 m = 0) { InputEvent v; v.type = InputEvent::KEY_DOWN; v.key = k; v.mod = m; Handle(v); };
            K6(SDLK_SLASH, KMOD_LSHIFT); Check(helpDlg != nullptr && helpDlg->open, "help: ? opens the keyboard help");
            Check(helpDlg->BodyCtl()->GetLineCount() > 20 && helpDlg->BodyCtl()->NeedsScroll() && helpDlg->HeadCtl()->GetText().find("SuperGUI Form Designer") != std::string::npos && helpDlg->HeadCtl()->GetText().find("With Assistance from Claude") != std::string::npos, "help: the header and a scrolling key list");
            { int g0 = grid; K6(SDLK_g); Check(helpDlg != nullptr && grid == g0, "help: modal (keys do not reach the form)"); }
            K6(SDLK_DOWN); K6(SDLK_DOWN); Check(helpDlg->BodyCtl()->GetFirstLine() == 2, "help: Down scrolls the text");
            K6(SDLK_END); int endLine = helpDlg->BodyCtl()->GetFirstLine(); Check(endLine > 2 && endLine < helpDlg->BodyCtl()->GetLineCount(), "help: End scrolls to the last page");
            K6(SDLK_HOME); K6(SDLK_PAGEDOWN); Check(helpDlg->BodyCtl()->GetFirstLine() == std::min(12, endLine), "help: Page Down"); K6(SDLK_HOME); Check(helpDlg->BodyCtl()->GetFirstLine() == 0, "help: Home");
            { InputEvent v; v.type = InputEvent::WHEEL; v.x = 700; v.y = 400; v.wheel = -3; Handle(v); Check(helpDlg->BodyCtl()->GetFirstLine() == std::min(6, endLine), "help: the wheel scrolls"); }
            Check(!helpDlg->Source(), "help: starts rendered");
            Handle(ev(InputEvent::DOWN, helpDlg->ToggleX() + 5, helpDlg->BtnY() + 5)); Handle(ev(InputEvent::UP, 0, 0));
            Check(helpDlg->Source() && helpDlg->BodyCtl()->GetShowSource(), "help: the Show source check box switches to the markup"); Handle(ev(InputEvent::DOWN, helpDlg->ToggleX() + 5, helpDlg->BtnY() + 5)); Handle(ev(InputEvent::UP, 0, 0));
            K6(SDLK_ESCAPE); Check(helpDlg == nullptr, "help: Esc closes it");
            K6(SDLK_F1); Check(helpDlg != nullptr, "help: F1 opens it too"); K6(SDLK_RETURN); Check(helpDlg == nullptr, "help: Enter closes it");
            K6(SDLK_SLASH, KMOD_LSHIFT); { int x = helpDlg->OkX() + 5, y = helpDlg->BtnY() + 3; Handle(ev(InputEvent::DOWN, x, y)); Handle(ev(InputEvent::UP, x, y)); } Check(helpDlg == nullptr, "help: the OK button closes it");
            Select(0); { int r = -1; for (int i = 0; i < nFields; ++i) if (fieldProp[i] == P_TEXT) r = i; EnsureRowVisible(r); Click(FIELD_X + 20, FieldY(r) + 5); }
            { InputEvent v; v.type = InputEvent::TEXT; strncpy(v.text, "?", sizeof v.text - 1); Handle(v); } K6(SDLK_SLASH, KMOD_LSHIFT);
            Check(helpDlg == nullptr, "help: ? typed into a text field does not open the help");
            K6(SDLK_ESCAPE); form.Clear(); sel = -1; BuildFields();
        }
        // ---- the multi-line field, the rich text editor and the Rich text control ----
        {
            RichTextField f; f.SetText("one\ntwo words\n\nfour");
            Check(f.LineCount() == 4 && f.LineText(1) == "two words" && f.LineText(2).empty() && f.LineStart(3) == 15 && f.LineOf(15) == 3 && f.Lines().size() == 4, "multi-line field: lines");
            f.caret = f.anchor = 6; f.MoveLine(1, false); Check(f.LineOf(f.caret) == 2 && f.caret == f.LineStart(2), "multi-line field: Down onto a shorter (empty) line goes to its end");
            f.MoveLine(1, false); Check(f.LineOf(f.caret) == 3 && f.caret == 17, "multi-line field: Down again returns to the remembered column");
            f.caret = f.anchor = 4 + 6; f.MoveLine(-1, false); Check(f.caret == 3, "multi-line field: Up onto a shorter line goes to its end");
            f.SetText("abcdef\nxy\nabcdef"); f.caret = f.anchor = 5; f.MoveLine(1, false); Check(f.caret == 9, "multi-line field: Down onto a shorter line stops at its end");
            f.MoveLine(1, false); Check(f.LineOf(f.caret) == 2 && f.ColOf(f.caret) == 5, "multi-line field: the column is remembered across the short line");
            f.MoveLine(1, false); Check(f.caret == (int)f.text.size(), "multi-line field: Down on the last line goes to its end");
            f.SetText("ab"); f.caret = f.anchor = 1; f.Insert("\n"); Check(f.text == "a\nb" && f.LineCount() == 2 && f.caret == 2, "multi-line field: Enter / a pasted new line splits the line");
            f.Insert("x\ty\r\nz\xC3\xA9"); Check(f.text == "a\nx y\nzb", "multi-line field: tab becomes a space, \\r is dropped, non-ASCII is dropped");
            f.SetText("a\nb"); f.SelectAll(); f.Insert("q"); Check(f.text == "q", "multi-line field: typing over a selection across lines");
            f.Undo(); Check(f.text == "a\nb", "multi-line field: undo");
            TextField single; single.SetText(""); single.Insert("a\nb"); Check(single.text == "a b", "single-line field: still turns a new line into a space");
            f.SetText("l1\nl2\nl3"); f.caret = f.anchor = 4; InputEvent v; v.type = InputEvent::KEY_DOWN; v.key = SDLK_HOME; f.Key(v); Check(f.caret == 3, "multi-line field: Home goes to the start of the line");
            v.key = SDLK_END; f.Key(v); Check(f.caret == 5, "multi-line field: End goes to the end of the line");
            v.key = SDLK_DOWN; v.mod = KMOD_LSHIFT; f.Key(v); Check(f.HasSel() && f.LineOf(f.caret) == 2, "multi-line field: Shift + Down selects");
            { RichTextField g; g.SetText("a\nb\nc\nd\ne\nf"); g.caret = g.anchor = 11; g.EnsureVisibleXY([](const std::string& t) { return (int)t.size() * 6; }, 100, 3); Check(g.firstLine == 3, "multi-line field: scrolls to keep the caret in view"); }
        }
        {
            form.Clear(); Place(C_RICHTEXT, 8, 8); form.items[0].name = "rich1"; sel = 0; BuildFields();
            Check(form.items[0].type == C_RICHTEXT && !form.items[0].list.empty() && ControlHeight(&scr, form.items[0]) > 100, "rich text: placed with sample text, as tall as its body");
            int rr = -1; for (int i = 0; i < nFields; ++i) if (fieldProp[i] == P_RICH) rr = i; Check(rr >= 0 && PropLabel(P_RICH) == "Text", "rich text: a Text row");
            auto Key7 = [&](SDL_Keycode k, Uint16 m = 0) { InputEvent v; v.type = InputEvent::KEY_DOWN; v.key = k; v.mod = m; Handle(v); };
            auto Txt7 = [&](const char* tx) { InputEvent v; v.type = InputEvent::TEXT; strncpy(v.text, tx, sizeof v.text - 1); Handle(v); };
            EnsureRowVisible(rr); Click(FIELD_X + FIELD_W - 30, FieldY(rr) + 5);
            Check(richDlg != nullptr && richDlg->Lines() == form.items[0].list && !richDlg->Rendered(), "rich text editor: Edit opens it on the control's lines, in source mode");
            { int g0 = grid; Key7(SDLK_g, 0); Check(richDlg != nullptr && grid == g0, "rich text editor: typing goes to the text, not the form"); }
            Key7(SDLK_a, KMOD_LCTRL); Key7(SDLK_BACKSPACE); Txt7("<color=red>Hi</color>"); Key7(SDLK_RETURN); Txt7("second &lt;line&gt;");
            Check(richDlg->Lines() == std::vector<std::string>({ "<color=red>Hi</color>", "second &lt;line&gt;" }), "rich text editor: typing and Enter make lines");
            Key7(SDLK_UP); Key7(SDLK_END); Txt7("!"); Check(richDlg->Lines()[0] == "<color=red>Hi</color>!", "rich text editor: Up, End and typing");
            Key7(SDLK_r, KMOD_LCTRL); Check(richDlg->Rendered(), "rich text editor: Ctrl+R switches to the rendered view"); Key7(SDLK_r, KMOD_LCTRL); Check(!richDlg->Rendered(), "rich text editor: and back to source");
            Handle(ev(InputEvent::DOWN, richDlg->RenderedX() + 10, richDlg->ModeY() + 5)); Handle(ev(InputEvent::UP, 0, 0)); Check(richDlg->Rendered(), "rich text editor: the Rendered button");
            Handle(ev(InputEvent::DOWN, richDlg->SourceX() + 10, richDlg->ModeY() + 5)); Handle(ev(InputEvent::UP, 0, 0)); Check(!richDlg->Rendered(), "rich text editor: the Source button");
            Key7(SDLK_ESCAPE); Check(richDlg == nullptr && form.items[0].list.size() == 3, "rich text editor: Esc cancels (the control keeps its text)");
            Click(FIELD_X + FIELD_W - 30, FieldY(rr) + 5); Key7(SDLK_a, KMOD_LCTRL); Txt7("<big>Big</big>"); Key7(SDLK_RETURN); Txt7("x"); Key7(SDLK_RETURN, KMOD_LCTRL);
            Check(richDlg == nullptr && form.items[0].list == std::vector<std::string>({ "<big>Big</big>", "x" }), "rich text editor: Ctrl+Enter stores the lines");
            { std::string g = GenerateDesigner(form); auto h = [&](const char* x) { return g.find(x) != std::string::npos; };
              Check(h("#include <SuperRichText.h>") && h("rich1(dst, 8, 8, 240, 120, &lgfx::fonts::Font2, enc, btnA, btnB)") && h("rich1.SetTextStatic(\"<big>Big</big>\\n\"") && h("\"x\");") && h("virtual void rich1_Accept()") && !h("SetShowSource") && !h("SetTabChars"), "codegen: rich text: header, constructor, the text as literals, an Accept handler, defaults not written");
              ExtraSet(form.items[0], *FindExtra(C_RICHTEXT, "tabChars"), "12"); ExtraSet(form.items[0], *FindExtra(C_RICHTEXT, "showSource"), "true"); ExtraSet(form.items[0], *FindExtra(C_RICHTEXT, "lineGap"), "2");
              std::string g2 = GenerateDesigner(form); Check(g2.find("rich1.SetTabChars(12);") != std::string::npos && g2.find("rich1.SetShowSource(true);") != std::string::npos && g2.find("rich1.SetLineGap(2);") != std::string::npos, "codegen: rich text settings that differ from the defaults"); }
            { std::string j = form.ToJson(); DForm lf; std::string le; Check(j.find("\"richText\": [\"<big>Big</big>\", \"x\"]") != std::string::npos && lf.FromJson(j, le) && lf.items[0].list == form.items[0].list && lf.ToJson() == j, "rich text: saved as a readable array of lines and loaded back"); }
            { DForm lf; std::string le; std::string many = "["; for (int i = 0; i < 500; ++i) many += (i ? ",\"l\"" : "\"l\""); many += "]";
              Check(lf.FromJson("{\"form\":{},\"controls\":[{\"type\":\"SuperRichText\",\"name\":\"r\",\"richText\":" + many + "}]}", le) && (int)lf.items[0].list.size() == MAX_RICH_LINES, "rich text: a file with too many lines is cut");
              Check(lf.FromJson("{\"form\":{},\"controls\":[{\"type\":\"SuperRichText\",\"name\":\"r\",\"richText\":\"a\\nb\\u00e9\"}]}", le) && lf.items[0].list == std::vector<std::string>({ "a", "b" }), "rich text: a single string with \\n is split, non-ASCII dropped"); }
            { DForm lf; lf.name = "T"; DControl c; c.type = C_RICHTEXT; c.name = "r"; c.list = { "x" }; lf.items.push_back(c); std::string u = GenerateUserFile(lf); Check(u.find("r_Accept() override") != std::string::npos, "user file: a stub for the rich text's Accept"); }
            form.Clear(); sel = -1; BuildFields();
        }
        // ---- scroll bars ----
        {
            ScrollBar v(ScrollBar::VERTICAL, 12), h(ScrollBar::HORIZONTAL, 10);
            v.SetGeometry(100, 20, 200); v.SetRange(1000, 250); v.SetPos(0);
            Check(v.Needed() && v.MaxPos() == 750 && v.ThumbLen() == 50 && v.ThumbStart() == 0 && v.TrackW() == 12 && v.TrackH() == 200, "scroll bar: thumb size follows page / total");
            v.SetPos(750); Check(v.ThumbStart() == 150 && v.ThumbStart() + v.ThumbLen() == 200, "scroll bar: at the end the thumb touches the end of the track");
            v.SetPos(5000); Check(v.Pos() == 750, "scroll bar: Pos is clamped to MaxPos"); v.SetPos(-5); Check(v.Pos() == 0, "scroll bar: ... and to 0");
            Check(v.Down(105, 150) && !v.Dragging() && v.Pos() == 250, "scroll bar: a click below the thumb moves one page down");
            Check(v.Down(105, 20 + 5) && v.Pos() == 0, "scroll bar: a click above the thumb moves one page up");
            Check(v.Down(105, 20 + 10) && v.Dragging(), "scroll bar: a press on the thumb grabs it");
            Check(v.Move(105, 20 + 10 + 75) && v.Pos() == 375, "scroll bar: dragging half the room moves half the way (grab offset kept)");
            v.Move(105, 5000); Check(v.Pos() == 750, "scroll bar: dragging past the end stops at the end"); v.Move(105, -5000); Check(v.Pos() == 0, "scroll bar: ... and at the start");
            v.Up(); Check(!v.Dragging() && !v.Move(105, 100), "scroll bar: after the button is up the mouse does nothing");
            Check(!v.Contains(99, 100) && !v.Contains(112, 100) && !v.Contains(105, 19) && !v.Contains(105, 220) && v.Contains(105, 100), "scroll bar: hit area is the track only");
            v.SetRange(100, 250); Check(!v.Needed() && !v.Contains(105, 100) && !v.Down(105, 100) && v.Pos() == 0, "scroll bar: nothing to scroll = no bar, no clicks");
            h.SetGeometry(10, 300, 400); h.SetRange(800, 400); h.SetPos(100);
            Check(h.TrackW() == 400 && h.TrackH() == 10 && h.ThumbLen() == 200 && h.ThumbStart() == 50 && h.Down(10 + 60, 305) && h.Dragging() && h.Move(10 + 60 + 100, 999) && h.Pos() == 300, "scroll bar: horizontal works the same along x");
            h.Up(); h.SetRange(1000000, 10); Check(h.ThumbLen() == 24, "scroll bar: the thumb never gets smaller than its minimum");
        }
        {
            form.Clear(); form.w = 320; form.h = 240; sel = -1; BuildFields(); zoom = 100; scrollX = scrollY = 0; zoomSpin->SetValue(100); SyncBars();
            Check(!surfV.Needed() && !surfH.Needed(), "surface bars: a form that fits has no scroll bars");
            ZoomCentre(400); scrollX = scrollY = 0; ClampScroll(); SyncBars();
            Check(surfV.Needed() && surfH.Needed() && surfV.PageSize() == VH && surfV.Total() == ZH() + 2 * PAD && surfH.Total() == ZW() + 2 * PAD, "surface bars: zoomed beyond the viewport both bars appear");
            Check(surfV.TrackX() == VX + VW - SB && surfH.TrackY() == VY + VH - SB && surfV.TrackH() == VH - SB && surfH.TrackW() == VW - SB, "surface bars: they sit on the viewport's right and bottom edges and leave the corner free");
            auto Dn = [&](int x, int y) { Handle(ev(InputEvent::DOWN, x, y)); }; auto Mv = [&](int x, int y) { Handle(ev(InputEvent::MOVE, x, y)); }; auto Up = [&]() { Handle(ev(InputEvent::UP, 0, 0)); };
            int before = scrollY; Dn(surfV.TrackX() + 4, surfV.TrackY() + surfV.TrackH() - 5); Up();
            Check(scrollY == std::min(before + VH, ZH() + 2 * PAD - VH) && form.items.empty(), "surface bars: a click on the track pages the view (and does not touch the form)");
            { int ty = surfV.TrackY() + surfV.ThumbStart() + 3; Dn(surfV.TrackX() + 4, ty); Mv(surfV.TrackX() + 4, 5000); Check(scrollY == ZH() + 2 * PAD - VH, "surface bars: dragging the thumb to the bottom scrolls to the end"); Mv(surfV.TrackX() + 4, -5000); Check(scrollY == 0, "surface bars: and back to the top"); Up(); }
            { int tx = surfH.TrackX() + surfH.ThumbStart() + 3; Dn(tx, surfH.TrackY() + 4); Mv(5000, surfH.TrackY() + 4); Check(scrollX == ZW() + 2 * PAD - VW, "surface bars: the horizontal bar scrolls sideways"); Up(); Mv(0, surfH.TrackY()); Check(scrollX == ZW() + 2 * PAD - VW, "surface bars: after the button is released the mouse does not scroll"); }
            { tool = C_LABEL; int n0 = (int)form.items.size(); Dn(surfV.TrackX() + 4, surfV.TrackY() + 40); Up(); Check((int)form.items.size() == n0 && tool == C_LABEL, "surface bars: a click on a bar does not place a control"); tool = -1; }
            zoom = 100; ZoomCentre(100); scrollX = scrollY = 0; ClampScroll(); SyncBars(); Check(!surfV.Needed() && !surfH.Needed(), "surface bars: back at 1:1 they are gone");
            // the property panel's bar
            form.Clear(); Place(C_GAUGE, 8, 8); Select(0); SyncBars();
            Check(MaxPropScroll() > 0 && propBar.Needed() && propBar.Total() == (PANEL_BOT - PANEL_TOP) + MaxPropScroll(), "property bar: a control with many rows has a scroll bar");
            { int ty = propBar.TrackY() + propBar.ThumbStart() + 3; Dn(propBar.TrackX() + 3, ty); Mv(propBar.TrackX() + 3, 5000); Check(propScroll == MaxPropScroll(), "property bar: dragging the thumb scrolls the rows to the end"); Up(); }
            Dn(propBar.TrackX() + 3, propBar.TrackY() + 5); Up(); Check(propScroll < MaxPropScroll() && propScroll >= 0, "property bar: a click above the thumb pages up");
            form.Clear(); sel = -1; BuildFields(); SyncBars(); Check(!propBar.Needed(), "property bar: the form's own few rows need none");
            // the controls-on-form list
            form.Clear(); for (int i = 0; i < 12; ++i) Place(C_LABEL, 8, 8 + i * 4); sel = -1; treeFirst = 0; BuildFields(); SyncBars();
            Check(TreeRows() >= 3 && TreeRows() < 12 && treeBar.Needed() && treeBar.PageSize() == TreeRows() && treeBar.Total() == 12, "list bar: more controls than rows gives the controls list a scroll bar");
            { InputEvent w; w.type = InputEvent::WHEEL; w.x = 100; w.y = TREE_Y0 + 10; w.wheel = -3; Handle(w); Check(treeFirst == 3, "list bar: the wheel scrolls the list"); }
            { Handle(ev(InputEvent::DOWN, 100, TREE_Y0 + 5)); Handle(ev(InputEvent::UP, 0, 0)); Check(sel == 3, "list bar: a click selects the row under it (not the first one)"); }
            { int ty = treeBar.TrackY() + treeBar.ThumbStart() + 3; Handle(ev(InputEvent::DOWN, treeBar.TrackX() + 3, ty)); Handle(ev(InputEvent::MOVE, treeBar.TrackX() + 3, 5000)); Handle(ev(InputEvent::UP, 0, 0)); Check(treeFirst == 12 - TreeRows() && sel == 3, "list bar: dragging the thumb scrolls to the end without selecting anything"); }
            { int below = TREE_Y0 + TreeRows() * TREE_H + 2; if (below < TREE_BOT + 12) { int sb = sel; treeFirst = 0; Handle(ev(InputEvent::DOWN, 100, below)); Handle(ev(InputEvent::UP, 0, 0)); Check(sel == sb, "list: a click below the last drawn row selects nothing"); } }
            Select(0); Check(treeFirst == 0, "list bar: selecting a control scrolls it into view"); Select(11); Check(treeFirst == 12 - TreeRows(), "list bar: ... also at the end");
            form.Clear(); sel = -1; treeFirst = 0; BuildFields();
            { int worst = 0; for (int t = 0; t < C_COUNT; ++t) { form.Clear(); Place(t, 8, 8); Select(0); worst = std::max(worst, nFields); } Check(worst > 0 && worst < MAX_FIELDS, "property panel: no control type needs more rows than the field table holds (a gauge has about 42)"); form.Clear(); sel = -1; BuildFields(); }
        }
        form.Clear(); FixOutDir(); sel = -1; BuildFields();
        // delete
        form.Clear(); Place(C_LABEL, 8, 8); Place(C_CHECKBOX, 8, 40); sel = -1; Key(SDLK_TAB); Key(SDLK_DELETE); Check(form.items.size() == 1 && sel == -1, "designer: Delete removes the selected control");
        form.Clear(); sel = -1; BuildFields(); unsetenv("FD_FORM");
    }
    printf("%s (%d failed)\n", fails ? "SELFTEST FAILED" : "SELFTEST PASSED", fails);
    fflush(stdout);
    exit(fails ? 1 : 0);
}

// At start (no FD_FORM given): the form file used last (an application setting); if that is not accessible, template.json in the program's folder, named NewForm;
// else nothing: the regular default new form stays. Returns what was used: "last", "template" or "".
static std::string StartupForm() {
    if (getenv("FD_FORM") && *getenv("FD_FORM")) return "";
    namespace fs = std::filesystem; std::error_code ec;
    if (!settings.lastForm.empty() && fs::is_regular_file(settings.lastForm, ec)) {
        currentFile = settings.lastForm;
        if (OpenForm()) return "last";
        currentFile.clear();
    }
    fs::path t = fs::path(AppDir()) / "template.json";
    if (fs::is_regular_file(t, ec)) {
        currentFile = t.string(); bool ok = OpenForm(); currentFile.clear();                       // the template is a starting point: saving asks for a new name
        if (ok) {
            std::string nm = "NewForm"; for (int k = 2; form.FormNameClashes(nm); ++k) nm = "NewForm" + std::to_string(k);
            form.name = nm; SyncFields(); Status("started from template.json as %s", nm.c_str());
            return "template";
        }
    }
    return "";
}

void setup() {
    // LovyanGFX's SDL panel turns plain 1-6 into "rescale the window" and r / l into "rotate the screen". The designer has text fields, so
    // require a modifier combination nobody presses: the shortcuts are then effectively off.
    lgfx::Panel_sdl::setShortcutKeymod((SDL_Keymod)(KMOD_LSHIFT | KMOD_LCTRL | KMOD_LALT | KMOD_LGUI));
    lcd = new LGFX(FD::WIN_W, FD::WIN_H, 1);
    lcd->init();
    scr.setColorDepth(16); scr.createSprite(FD::WIN_W, FD::WIN_H);
    zoomSpin = new SuperSpinBox(&scr, ZSPIN_X, ZROW_Y, ZSPIN_W, &lgfx::fonts::Font2, nullptr, nullptr, nullptr);
    zoomSpin->SetRange(ZOOM_MIN, ZOOM_MAX); zoomSpin->SetStep(ZOOM_STEP); zoomSpin->SetSuffix("%"); zoomSpin->SetValue(100);
    Input::Begin(FD::WIN_W, FD::WIN_H);
    if (SDL_Window* w = Input::Window()) SDL_GetWindowSize(w, &winW, &winH);
    settings.Load(); FixOutDir();
    if (!getenv("FD_SELFTEST")) form.boxes.push_back(form.MakeYesNoBox());     // a new form starts with the ready-made Yes/No box (a form opened from a file has what the file has)
    BuildFields();
    if (getenv("FD_OPEN")) OpenForm();                    // FD_OPEN=1: load the form file at start
    else if (!getenv("FD_SELFTEST") && !getenv("FD_EXPORT")) { std::string how = StartupForm(); (void)how; }          // else the last form, or template.json, or the default new form
    if (getenv("FD_EXPORT")) {                            // FD_EXPORT=1: export <Form>.designer.h from the form file, print the result and exit (0 / 1)
        FD::GenResult r = FD::WriteDesigner(form, form.outDir);
        printf("%s %s\n", r.ok ? (r.unchanged ? "up to date:" : "generated:") : "FAILED:", r.ok ? r.path.c_str() : r.error.c_str());
        FD::UserResult u = r.ok ? FD::WriteUserFile(form, form.outDir) : FD::UserResult();
        if (r.ok) { printf("%s %s\n", u.ok ? (u.created ? "created:  " : "kept:     ") : "FAILED:   ", u.ok ? u.path.c_str() : u.error.c_str()); for (auto& h : u.missing) printf("  not in %s yet: %s\n", FD::UserFileName(form).c_str(), h.c_str()); SaveForm(); }
        fflush(stdout); exit(r.ok && u.ok ? 0 : 1);
    }
    if (getenv("FD_ZOOM")) { if (getenv("FD_ZOOM")[0] == 'f') ZoomFit(); else ZoomCentre(atoi(getenv("FD_ZOOM"))); }   // FD_ZOOM=fit | percent
    if (getenv("FD_SELECT") && form.items.size() > (size_t)atoi(getenv("FD_SELECT"))) Select(atoi(getenv("FD_SELECT")));   // FD_SELECT=n: select control n at start
    if (getenv("FD_SHOW") && getenv("FD_SHOW")[0] == 'p') OpenPresetDialog();            // FD_SHOW=presets | builder: the template dialogs (select an input box with FD_SELECT)
    else if (getenv("FD_SHOW") && getenv("FD_SHOW")[0] == 'b') OpenBuilderDialog();
    else if (getenv("FD_SHOW") && getenv("FD_SHOW")[0] == 'o') OpenFileDialog(FP_OPEN);          // FD_SHOW=open: the Open dialog
    else if (getenv("FD_SHOW") && getenv("FD_SHOW")[0] == 'h') helpDlg = new FD::HelpDialog(&scr, HELP_SOURCE_TOGGLE);        // FD_SHOW=help
    else if (getenv("FD_SHOW") && getenv("FD_SHOW")[0] == 'i') OpenItemsDialog();                // (also the menu editor for a selected menu)                // FD_SHOW=items
    else if (getenv("FD_SHOW")) { if (!form.items.empty()) { sel = 0; BuildFields(); } for (int i = 0; i < nFields; ++i) if (fieldProp[i] == (getenv("FD_SHOW")[0] == 'f' ? P_FONT : Sel() ? P_FG : P_FBG)) { if (fieldProp[i] == P_FONT) OpenFontDialog(P_FONT); else OpenColorDialog(fieldProp[i]); break; } }   // FD_SHOW=color | font: open a dialog at start (for screenshots)
    if (getenv("FD_SELFTEST")) SelfTest();
}

void loop() {
    InputEvent e;
    while (Input::Poll(e)) Handle(e);
    static Uint32 blink = 0;
    if (SDL_GetTicks() / 500 != blink) { blink = SDL_GetTicks() / 500; dirty = true; }     // the caret blinks
    if (dirty) Render();
    static bool snapped = false;                          // FD_SNAPSHOT=file.raw: write the first frame (raw big-endian RGB565) and quit
    if (!snapped && getenv("FD_SNAPSHOT")) {
        snapped = true;
        if (FILE* f = fopen(getenv("FD_SNAPSHOT"), "wb")) { fwrite(scr.getBuffer(), 2, (size_t)FD::WIN_W * FD::WIN_H, f); fclose(f); }
        exit(0);
    }
    SDL_Delay(8);
}
