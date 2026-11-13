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
static const int TOOL_Y0 = 84, TOOL_H = 28;                              // toolbox rows
static const int TREE_Y0 = TOOL_Y0 + FD::C_COUNT * TOOL_H + 40, TREE_H = 20;   // "controls on form" list
static const int VX = 320, VY = 84, VW = 660, VH = 716;                  // the design surface's viewport
static const int PAD = 20;                                               // space around the form inside the viewport
static const int FIELD_X = 1105, FIELD_Y0 = 96, FIELD_W = 276, FIELD_H = 26, FIELD_GAP = 34, FIELD_PAD = 6;

// ---- editor state ----
static FD::DForm form;
static int sel = -1;                                 // selected control (index), -1 = the form itself
static int tool = -1;                                // toolbox item waiting to be placed, -1 = none
static int grid = 8;                                 // snap step in pixels (1 = off)
static int scrollX = 0, scrollY = 0;                 // design surface scroll (in screen pixels, at the current zoom)
static int zoom = 100;                               // design surface zoom in percent (10 .. 800): the form is shown at zoom / 100
static std::string status; static Uint32 statusAt = 0;
enum DragMode { D_NONE, D_MOVE, D_RESIZE_L, D_RESIZE_R, D_TEXT };
static DragMode drag = D_NONE; static int dragDX = 0, dragDY = 0, dragRight = 0;

// ---- property fields ----
enum Prop { P_NAME, P_TEXT, P_X, P_Y, P_W, P_BSIZE, P_PAD, P_GROUP, P_FNAME, P_FW, P_FH, P_FG, P_BG, P_BORDER, P_FBG, P_FONT, P_BSTYLE, P_ALIGN };
static const char* PropLabel(int p) { static const char* L[] = { "Name", "Text", "X", "Y", "Width", "Border size", "Padding", "Group", "Name", "Width", "Height", "Text color", "Back color", "Border color", "Back color", "Font", "Border style", "Align" }; return L[p]; }
static const int MAX_FIELDS = 16;
static bool IsColorProp(int p) { return p >= P_FG && p <= P_FBG; }   // colour rows show a swatch and open the colour dialog ...
static bool IsPickRow(int p) { return p >= P_FG; }                    // ... the font row opens the font dialog: neither has a text field
static FD::TextField fields[MAX_FIELDS];
static int fieldProp[MAX_FIELDS], nFields = 0;
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
static void ClampScroll() {
    int mx = std::max(0, ZW() + 2 * PAD - VW), my = std::max(0, ZH() + 2 * PAD - VH);
    scrollX = std::min(std::max(scrollX, 0), mx); scrollY = std::min(std::max(scrollY, 0), my);
}
static void RefreshGeometry() { for (auto& c : form.items) FD::Measure(&scr, c); }
static void ClampPos(DControl& c) {
    c.w = std::min(std::max(c.w, FD::MIN_CONTROL_W), std::max(FD::MIN_CONTROL_W, form.w));
    c.x = std::min(std::max(c.x, 0), std::max(0, form.w - c.w));
    c.y = std::min(std::max(c.y, 0), std::max(0, form.h - c.h));
}
static DControl* Sel() { return sel >= 0 && sel < (int)form.items.size() ? &form.items[sel] : nullptr; }

// ---- the last successful save folder of the generated code (kept in the form's JSON as outputPath, not shown in the designer) ----
static bool DirExists(const std::string& p) { std::error_code ec; return !p.empty() && std::filesystem::is_directory(p, ec); }
static std::string WorkDir() { std::error_code ec; return std::filesystem::current_path(ec).string(); }     // the executable's working folder
// A quick check after loading (and at start-up): a missing folder falls back to the working folder. Returns true when it had to change.
// The code generator will set form.outDir after every successful save of the generated files.
static bool FixOutDir() {
    if (DirExists(form.outDir)) return false;
    form.outDir = WorkDir(); return true;
}

static uint16_t* ColorRef(int p) {                    // the model colour a colour property edits
    DControl* c = Sel();
    switch (p) { case P_FG: return c ? &c->fg : nullptr; case P_BG: return c ? &c->bg : nullptr; case P_BORDER: return c ? &c->border : nullptr; case P_FBG: return &form.bg; }
    return nullptr;
}
static uint16_t PartnerColor(int p) { DControl* c = Sel(); return p == P_FG && c ? c->bg : p == P_BG && c ? c->fg : p == P_BORDER && c ? c->bg : form.bg; }

static std::string PropValue(int p) {
    DControl* c = Sel();
    switch (p) {
    case P_NAME: return c ? c->name : ""; case P_TEXT: return c ? c->text : "";
    case P_BSIZE: return c ? std::to_string(c->borderSize >= 0 ? c->borderSize : c->defSize) : "";
    case P_PAD: return c ? std::to_string(c->padding >= 0 ? c->padding : c->defPad) : "";
    case P_GROUP: return c ? c->group : "";
    case P_X: return c ? std::to_string(c->x) : ""; case P_Y: return c ? std::to_string(c->y) : ""; case P_W: return c ? std::to_string(c->w) : "";
    case P_FNAME: return form.name; case P_FW: return std::to_string(form.w); case P_FH: return std::to_string(form.h);
    }
    return "";
}
// Applies a typed value to the model (live, on every change). Returns false when the value is not (yet) valid; the model is then unchanged.
static bool ApplyProp(int p, const std::string& v) {
    DControl* c = Sel();
    if ((p <= P_GROUP) != (c != nullptr)) return false;
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
    case P_GROUP: if (!FD::ValidName(v) || form.ControlNamed(v) || v == form.name || v == form.name + "Base") return false; c->group = v; return true;     // an identifier that is not a control's name
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
    auto add = [](int p) { FD::TextField& f = fields[nFields]; fieldProp[nFields++] = p; f.filter = FD::TextField::ANY; f.maxLen = 64;
                           if (p == P_NAME || p == P_FNAME || p == P_GROUP) f.filter = FD::TextField::IDENT;
                           if (p == P_X || p == P_Y || p == P_W || p == P_FW || p == P_FH) { f.filter = FD::TextField::DIGITS; f.maxLen = 4; }
                           if (p == P_BSIZE || p == P_PAD) { f.filter = FD::TextField::DIGITS; f.maxLen = 2; } };
    if (Sel()) { add(P_NAME); if (FD::Info(Sel()->type).hasText) add(P_TEXT); if (Sel()->type == FD::C_RADIO) add(P_GROUP); add(P_X); add(P_Y); add(P_W); add(P_FONT); add(P_FG); add(P_BG); add(P_BORDER); add(P_BSTYLE); add(P_BSIZE); add(P_PAD); if (FD::Info(Sel()->type).hasAlign) add(P_ALIGN); }
    else       { add(P_FNAME); add(P_FW); add(P_FH); add(P_FBG); }
    focus = -1; drag = drag == D_TEXT ? D_NONE : drag;
    for (int i = 0; i < nFields; ++i) if (!IsPickRow(fieldProp[i])) fields[i].SetText(PropValue(fieldProp[i]));
    dirty = true;
}
static void SyncFields() {                           // model -> fields (the field being typed in keeps its text)
    for (int i = 0; i < nFields; ++i) if (i != focus && !IsPickRow(fieldProp[i])) fields[i].SetText(PropValue(fieldProp[i]));
    dirty = true;
}
static void Select(int i) { if (i != sel) { sel = i; BuildFields(); } dirty = true; }
static void SetFocus(int i) {
    int old = focus; focus = i;
    if (old >= 0 && old != i) SyncFields();          // leaving a field: show what the model really holds (clamped, or the old valid name)
    dirty = true;
}
static void FieldEdited(int i) { if (IsPickRow(fieldProp[i])) return; ApplyProp(fieldProp[i], fields[i].text); RefreshGeometry(); SyncFields(); }

// ---- the colour dialog (modal) ----
static FD::ColorDialog* colorDlg = nullptr; static int colorDlgProp = -1;
static void OpenColorDialog(int prop) {
    uint16_t* ref = ColorRef(prop); if (!ref || colorDlg) return;
    std::string what = std::string(PropLabel(prop)) + (Sel() ? " of " + Sel()->name : " of the form");
    colorDlg = new FD::ColorDialog(&scr, what, *ref, PartnerColor(prop), prop == P_FG);
    colorDlgProp = prop; focus = -1; drag = D_NONE; dirty = true;
}
static void CloseColorDialog() {
    if (!colorDlg) return;
    if (colorDlg->state == FD::ColorDialog::OK) if (uint16_t* ref = ColorRef(colorDlgProp)) { *ref = colorDlg->color; Status("%s = 0x%04X", PropLabel(colorDlgProp), colorDlg->color); }
    delete colorDlg; colorDlg = nullptr; drag = D_NONE; dirty = true;
}

// ---- the font dialog (modal) ----
static FD::FontDialog* fontDlg = nullptr;
static void OpenFontDialog() {
    DControl* c = Sel(); if (!c || fontDlg || colorDlg) return;
    fontDlg = new FD::FontDialog(&scr, "Font of " + c->name, *c, form.bg);
    focus = -1; drag = D_NONE; dirty = true;
}
static void CloseFontDialog() {
    if (!fontDlg) return;
    if (fontDlg->state == FD::FontDialog::OK) if (DControl* c = Sel()) {
        c->font = fontDlg->font; RefreshGeometry(); ClampPos(*c); SyncFields(); Status("%s font = %s", c->name.c_str(), c->font.c_str());
    }
    delete fontDlg; fontDlg = nullptr; drag = D_NONE; dirty = true;
}

// ---- the file dialog (modal): Open, Save As, and the folder the code is generated into ----
static bool SaveForm(); static bool OpenForm(); static void ExportForm(const std::string& dir);
enum FilePurpose { FP_OPEN, FP_SAVE, FP_EXPORT };
static FD::FileDialog* fileDlg = nullptr; static int filePurpose = FP_OPEN;
static bool DirOk(const std::string& d) { std::error_code ec; return !d.empty() && std::filesystem::is_directory(d, ec); }
static void OpenFileDialog(int purpose) {
    if (fileDlg || colorDlg || fontDlg) return;
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
    if (filePurpose == FP_OPEN) { currentFile = path; if (!OpenForm()) currentFile = old; }
    else if (filePurpose == FP_SAVE) { currentFile = path; if (!SaveForm()) currentFile = old; }      // a changed name becomes the form's file name
    else ExportForm(path);
}

// a segmented row (border style / alignment): a value equal to the control's own default is stored as "no override"
static void SetEnumProp(int p, int v) {
    DControl* c = Sel(); if (!c) return;
    if (p == P_BSTYLE) c->borderStyle = v == c->defStyle ? -1 : v;
    if (p == P_ALIGN)  c->align = v == c->defAlign ? -1 : v;
    RefreshGeometry(); ClampPos(*c); SyncFields();
}

// ---- model operations ----
static void Place(int type, int fx, int fy) {        // fx, fy in form coordinates
    DControl c; c.type = type; c.name = form.UniqueName(FD::Info(type).prefix); c.text = FD::Info(type).defText;
    c.w = FD::Info(type).defW; c.x = Snap(fx); c.y = Snap(fy);
    if (type == FD::C_RADIO) { c.group = form.UniqueGroupName(); for (const DControl& o : form.items) if (o.type == FD::C_RADIO) { c.group = o.group; break; } }   // joins the form's first group
    FD::Measure(&scr, c); ClampPos(c);
    form.items.push_back(c); sel = (int)form.items.size() - 1; BuildFields();
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
    std::string note = !u.ok ? "  (" + u.error + ")" : u.created ? "  + created " + FD::UserFileName(form) : "  (" + FD::UserFileName(form) + " kept)";
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
static void Reveal(int i) { fields[i].EnsureVisible(MeasureText, FIELD_W - 2 * FIELD_PAD); dirty = true; }
static int FieldY(int i) { return FIELD_Y0 + i * FIELD_GAP; }
static int FieldAt(int x, int y) {
    for (int i = 0; i < nFields; ++i) if (x >= FIELD_X && x < FIELD_X + FIELD_W && y >= FieldY(i) && y < FieldY(i) + FIELD_H) return i;
    return -1;
}

// ---- zoom controls: a real SuperSpinBox (drag, click the left / right third, wheel) with Fit and 1:1 buttons ----
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
    for (auto& c : form.items) FD::MakeControl(&formSpr, c, 0, 0)->Push();
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
    scr.setTextColor(C_ACCENT); scr.drawString("Controls on form", 18, TREE_Y0 - 22);
    for (int i = 0; i < (int)form.items.size() && TREE_Y0 + (i + 1) * TREE_H < 806; ++i) {
        int y = TREE_Y0 + i * TREE_H;
        if (i == sel) scr.fillRect(18, y, 274, TREE_H - 2, C_SEL);
        scr.setTextColor(0xFFFF); scr.drawString(form.items[i].name.c_str(), 24, y + 2);
        scr.setTextColor(0x8410); scr.drawString(FD::Info(form.items[i].type).title, 170, y + 2);
    }

    // design surface
    Frame(310, 56, 680, 754, "Design surface");
    char b[160];
    snprintf(b, sizeof b, "%s  %d x %d   grid %d", form.name.c_str(), form.w, form.h, grid);
    scr.setTextColor(0x8410); scr.drawString(b, 430, 62);
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

    // property panel
    Frame(1000, 56, 390, 754, Sel() ? ("Properties: " + Sel()->name).c_str() : ("Properties: form " + form.name).c_str());
    for (int i = 0; i < nFields; ++i) {
        int fy = FieldY(i);
        scr.setFont(&lgfx::fonts::Font2); scr.setTextDatum(lgfx::textdatum_t::top_left);
        scr.setTextColor(0xFFFF); scr.drawString(PropLabel(fieldProp[i]), 1010, fy + 6);
        if (fieldProp[i] == P_BSTYLE || fieldProp[i] == P_ALIGN) {                           // three segments: click one
            static const char* S[] = { "Standard", "3D", "Sleek" }; static const char* A[] = { "Left", "Center", "Right" };
            bool st = fieldProp[i] == P_BSTYLE; const DControl* c = Sel();
            int cur = st ? (c->borderStyle >= 0 ? c->borderStyle : c->defStyle) : (c->align >= 0 ? c->align : c->defAlign), segw = FIELD_W / 3;
            scr.setTextDatum(lgfx::textdatum_t::middle_center);
            for (int k = 0; k < 3; ++k) {
                int sx = FIELD_X + k * segw;
                scr.fillRect(sx, fy, segw, FIELD_H, k == cur ? C_SEL : 0x0000); scr.drawRect(sx, fy, segw, FIELD_H, k == cur ? C_ACCENT : 0x7BEF);
                scr.setTextColor(k == cur ? 0xFFFF : 0x8410); scr.drawString(st ? S[k] : A[k], sx + segw / 2, fy + FIELD_H / 2);
            }
            scr.setTextDatum(lgfx::textdatum_t::top_left);
            continue;
        }
        if (fieldProp[i] == P_FONT) {                                                       // the font name in its own font: click opens the font dialog
            const FD::FontInfo& fi = FD::FindFont(Sel()->font);
            scr.fillRect(FIELD_X, fy, FIELD_W, FIELD_H, 0x0000); scr.drawRect(FIELD_X, fy, FIELD_W, FIELD_H, 0x7BEF);
            scr.setTextColor(0xFFFF); scr.setFont(&lgfx::fonts::Font2); scr.drawString(fi.label, FIELD_X + 6, fy + 6);
            scr.setTextColor(0x8410); scr.drawString("click to change", FIELD_X + FIELD_W - 100, fy + 6);
            continue;
        }
        if (IsColorProp(fieldProp[i])) {                                                    // a swatch: click opens the colour dialog
            uint16_t col = *ColorRef(fieldProp[i]);
            scr.fillRect(FIELD_X, fy, 70, FIELD_H, col); scr.drawRect(FIELD_X, fy, 70, FIELD_H, 0xFFFF);
            snprintf(b, sizeof b, "0x%04X   (click to change)", col);
            scr.setTextColor(0xC618); scr.drawString(b, FIELD_X + 80, fy + 6);
            continue;
        }
        bool bad = focus == i && (fieldProp[i] == P_NAME || fieldProp[i] == P_FNAME || fieldProp[i] == P_GROUP) && fields[i].text != PropValue(fieldProp[i]);
        FD::DrawField(scr, fields[i], FIELD_X, fy, FIELD_W, FIELD_H, focus == i, bad, SDL_GetTicks());
    }
    scr.setTextColor(0x8410);
    if (Sel()) { snprintf(b, sizeof b, "Height %d (follows the font)", Sel()->h); scr.drawString(b, 1010, FieldY(nFields) + 6); }
    else scr.drawString("Select a control to edit it.", 1010, FieldY(nFields) + 6);
    if (focus >= 0) scr.drawString("Enter / Esc: done    Tab: next field", 1010, FieldY(nFields) + 30);
    if (colorDlg) colorDlg->Draw(SDL_GetTicks());
    if (fontDlg) fontDlg->Draw();
    if (fileDlg) fileDlg->Draw(SDL_GetTicks());
    scr.pushSprite(lcd, 0, 0);
    dirty = false;
}

// ---- input ----
static void Handle(const InputEvent& e) {
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
        if (InRect(e.x, e.y, ZSPIN_X, ZROW_Y, ZSPIN_W, ZRowH())) {          // the zoom spin box: click the left / right third, or drag up / down
            SetFocus(-1); spinPX = e.x; spinPY = e.y; spinDrag = true; spinStarted = false;
            zoomSpin->StartEdit(); SpinPointer(SuperPointerEvent::PRESS, e.x, e.y); break;
        }
        if (InRect(e.x, e.y, ZFIT_X, ZROW_Y, ZFIT_W, ZRowH())) { SetFocus(-1); ZoomFit(); break; }
        if (InRect(e.x, e.y, ZONE_X, ZROW_Y, ZONE_W, ZRowH())) { SetFocus(-1); ZoomCentre(100); break; }
        int fi = FieldAt(e.x, e.y);
        if (fi >= 0 && (fieldProp[fi] == P_BSTYLE || fieldProp[fi] == P_ALIGN)) { SetFocus(-1); SetEnumProp(fieldProp[fi], std::min(2, (e.x - FIELD_X) * 3 / FIELD_W)); break; }
        if (fi >= 0 && fieldProp[fi] == P_FONT) { SetFocus(-1); OpenFontDialog(); break; }
        if (fi >= 0 && IsColorProp(fieldProp[fi])) { SetFocus(-1); OpenColorDialog(fieldProp[fi]); break; }
        if (fi >= 0) {                                                       // a property field: caret / selection
            SetFocus(fi); drag = D_TEXT;
            FD::TextField& f = fields[fi];
            f.SetCaretFromX(e.x - FIELD_X - FIELD_PAD, (SDL_GetModState() & KMOD_SHIFT) != 0, MeasureText);
            if (e.clicks == 2) f.SelectWord(f.caret); else if (e.clicks >= 3) f.SelectAll();
            Reveal(fi); break;
        }
        SetFocus(-1);
        if (e.x >= 18 && e.x < 292 && e.y >= TOOL_Y0 && e.y < TOOL_Y0 + FD::C_COUNT * TOOL_H) {   // toolbox
            int t = (e.y - TOOL_Y0) / TOOL_H; tool = tool == t ? -1 : t; dirty = true; break;
        }
        if (e.x >= 18 && e.x < 292 && e.y >= TREE_Y0) {                                           // controls-on-form list
            int i = (e.y - TREE_Y0) / TREE_H; if (i < (int)form.items.size()) Select(i); break;
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
        drag = D_NONE; break;
    case InputEvent::WHEEL:
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
        if (cmd && e.key == SDLK_s) { OpenFileDialog(FP_SAVE); break; }          // Save always shows the dialog (a new name becomes the form's file)
        if (cmd && e.key == SDLK_o) { OpenFileDialog(FP_OPEN); break; }          // Open always shows the dialog, where it was last looking
        if (cmd && e.key == SDLK_x) { if (shift) OpenFileDialog(FP_EXPORT); else ExportForm(""); break; }   // Export (Shift: choose the folder first)
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
static void Check(bool ok, const char* what) { printf("%s  %s\n", ok ? "ok  " : "FAIL", what); if (!ok) ++fails; }
static bool Next(InputEvent& e) { return Input::Poll(e); }
static void SelfTest() {
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
        Check(nFields == 4 && fieldProp[1] == P_FW && fieldProp[3] == P_FBG, "designer: with nothing selected the property panel edits the form");
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
        Check(DirExists(form.outDir) && form.outDir == WorkDir(), "designer: a new form's output folder is the working folder");
        { std::string e2; DForm h; h.outDir = "/some \"dir\""; Check(g.FromJson(h.ToJson(), e2) && g.outDir == h.outDir, "model: outputPath survives a JSON round trip (quotes escaped)"); }
        { FILE* f = fopen("/tmp/fd_selftest_form.json", "wb"); fputs("{\"form\":{\"outputPath\":\"/no/such/folder/anywhere\"},\"controls\":[]}", f); fclose(f); }
        OpenForm(); Check(form.outDir == WorkDir(), "designer: a saved output folder that no longer exists is replaced by the working folder");
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
        { std::ifstream in(gdir + "/demo.json"); std::stringstream ss; ss << in.rdbuf();
          Check(std::filesystem::exists(gdir + "/Demo.designer.h") && ss.str().find("\"outputPath\": \"" + gdir + "\"") != std::string::npos && grid == 8, "export: Ctrl+X writes the code and saves the form with outputPath"); }
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
        Check(uhas("#include \"Demo.designer.h\"") && uhas("class Demo : public DemoBase {") && uhas("Demo(lgfx::LovyanGFX* dst, SuperEncoder* enc, SuperButton* btnA) : DemoBase(dst, enc, btnA)"), "user file: includes the designer header and derives from the Base with the same constructor");
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
        // delete
        form.Clear(); Place(C_LABEL, 8, 8); Place(C_CHECKBOX, 8, 40); sel = -1; Key(SDLK_TAB); Key(SDLK_DELETE); Check(form.items.size() == 1 && sel == -1, "designer: Delete removes the selected control");
        form.Clear(); sel = -1; BuildFields(); unsetenv("FD_FORM");
    }
    printf("%s (%d failed)\n", fails ? "SELFTEST FAILED" : "SELFTEST PASSED", fails);
    fflush(stdout);
    exit(fails ? 1 : 0);
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
    settings.Load(); FixOutDir(); BuildFields();
    if (getenv("FD_OPEN")) OpenForm();                    // FD_OPEN=1: load the form file at start
    if (getenv("FD_EXPORT")) {                            // FD_EXPORT=1: export <Form>.designer.h from the form file, print the result and exit (0 / 1)
        FD::GenResult r = FD::WriteDesigner(form, form.outDir);
        printf("%s %s\n", r.ok ? (r.unchanged ? "up to date:" : "generated:") : "FAILED:", r.ok ? r.path.c_str() : r.error.c_str());
        FD::UserResult u = r.ok ? FD::WriteUserFile(form, form.outDir) : FD::UserResult();
        if (r.ok) { printf("%s %s\n", u.ok ? (u.created ? "created:  " : "kept:     ") : "FAILED:   ", u.ok ? u.path.c_str() : u.error.c_str()); for (auto& h : u.missing) printf("  not in %s yet: %s\n", FD::UserFileName(form).c_str(), h.c_str()); SaveForm(); }
        fflush(stdout); exit(r.ok && u.ok ? 0 : 1);
    }
    if (getenv("FD_ZOOM")) { if (getenv("FD_ZOOM")[0] == 'f') ZoomFit(); else ZoomCentre(atoi(getenv("FD_ZOOM"))); }   // FD_ZOOM=fit | percent
    if (getenv("FD_SELECT") && form.items.size() > (size_t)atoi(getenv("FD_SELECT"))) Select(atoi(getenv("FD_SELECT")));   // FD_SELECT=n: select control n at start
    if (getenv("FD_SHOW") && getenv("FD_SHOW")[0] == 'o') OpenFileDialog(FP_OPEN);          // FD_SHOW=open: the Open dialog
    else if (getenv("FD_SHOW")) { if (!form.items.empty()) { sel = 0; BuildFields(); } for (int i = 0; i < nFields; ++i) if (fieldProp[i] == (getenv("FD_SHOW")[0] == 'f' ? P_FONT : Sel() ? P_FG : P_FBG)) { if (fieldProp[i] == P_FONT) OpenFontDialog(); else OpenColorDialog(fieldProp[i]); break; } }   // FD_SHOW=color | font: open a dialog at start (for screenshots)
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
