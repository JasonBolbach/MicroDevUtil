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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include "DesignerConfig.h"
#include "FontTable.h"
#include "ReservedNames.h"

namespace FD {

enum CType { C_LABEL, C_PUSHBUTTON, C_CHECKBOX, C_RADIO, C_PROGRESS, C_COUNT };

struct TypeInfo {
    const char* key;       // the SuperGUI class name (also the "type" in the JSON)
    const char* title;     // shown in the toolbox
    const char* prefix;    // default names: label1, label2 ...
    int         defW;      // default width
    const char* defText;   // default caption / text
    bool        hasText;
    bool        hasAlign;           // the control has a text alignment (SetAlign)
};
inline const TypeInfo& Info(int t) {
    static const TypeInfo T[C_COUNT] = {
        { "SuperLabel",       "Label",       "label",    100, "Label",  true,  true  },
        { "SuperPushButton",  "Push button", "button",   100, "Button", true,  true  },
        { "SuperCheckBox",    "Check box",   "check",    120, "Check",  true,  false },
        { "SuperRadioButton", "Radio button","radio",    120, "Option", true,  false },
        { "SuperProgressBar", "Progress bar","progress", 150, "",       false, false },
    };
    return T[t < 0 || t >= C_COUNT ? 0 : t];
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
};

// a valid name for a control, a radio group or the form: a plain identifier that is safe in the generated C++ (see ReservedNames.h)
inline bool ValidName(const std::string& s) {
    if (s.empty() || isdigit((unsigned char)s[0]) || IsReserved(s)) return false;
    for (unsigned char c : s) if (!isalnum(c) && c != '_') return false;
    return true;
}

// ---- a tiny JSON reader (objects, arrays, strings, integers; enough for the designer's own files) ----
struct JV {
    enum T { NUL, NUM, STR, ARR, OBJ } t = NUL;
    long n = 0; std::string s;
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
                if (e == 'n') c = '\n'; else if (e == 't') c = '\t'; else if (e == '"' || e == '\\' || e == '/') c = e; else return Fail("unsupported escape");
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
        if (c == '-' || isdigit((unsigned char)c)) {
            char* end = nullptr; v.n = strtol(S.c_str() + P, &end, 10); v.t = JV::NUM;
            P = (size_t)(end - S.c_str()); return true;
        }
        return Fail("value expected");
    }
};

struct DForm {
    std::string name = "Form1";
    int w = DEFAULT_FORM_W, h = DEFAULT_FORM_H;
    uint16_t bg = DEF_FORM_BG;       // the form's back colour
    std::string outDir;              // where the generated code goes ("" until the designer fills in a folder that exists)
    std::vector<DControl> items;

    void Clear() { name = "Form1"; w = DEFAULT_FORM_W; h = DEFAULT_FORM_H; bg = DEF_FORM_BG; items.clear(); }   // outDir is kept: it belongs to the project folder
    // A name is taken when a control (other than `except`) or a radio group already has it: both become C++ members of the generated class.
    // The form's own class names count too: the generated classes are <name>Base and <name>, and no member may be called like its class.
    bool NameTaken(const std::string& n, int except = -1) const {
        if (n == name || n == name + "Base") return true;
        for (int i = 0; i < (int)items.size(); ++i) {
            if (i != except && items[i].name == n) return true;
            if (items[i].type == C_RADIO && items[i].group == n) return true;
        }
        return false;
    }
    // Would renaming the form to f collide with a control or a radio group?
    bool FormNameClashes(const std::string& f, bool withGroups = true) const {
        for (const DControl& c : items) {
            if (c.name == f || c.name == f + "Base") return true;
            if (withGroups && c.type == C_RADIO && (c.group == f || c.group == f + "Base")) return true;
        }
        return false;
    }
    bool ControlNamed(const std::string& n) const { for (const DControl& c : items) if (c.name == n) return true; return false; }
    std::string UniqueGroupName() const {
        for (int n = 1;; ++n) { std::string s = "group" + std::to_string(n); if (!NameTaken(s)) return s; }
    }
    std::string UniqueName(const char* prefix) const {
        for (int n = 1;; ++n) { std::string s = prefix + std::to_string(n); if (!NameTaken(s)) return s; }
    }

    static std::string Layout(const DControl& c) {              // only the overrides, e.g. , "borderStyle": "sleek", "padding": 4
        std::string s;
        if (c.type == C_RADIO) s += ", \"group\": \"" + c.group + "\"";
        if (c.borderStyle >= 0) s += std::string(", \"borderStyle\": \"") + StyleName(c.borderStyle) + "\"";
        if (c.borderSize >= 0)  s += ", \"borderSize\": " + std::to_string(c.borderSize);
        if (c.padding >= 0)     s += ", \"padding\": " + std::to_string(c.padding);
        if (c.align >= 0)       s += std::string(", \"align\": \"") + AlignName(c.align) + "\"";
        return s;
    }
    static std::string Hex(uint16_t c) { char b[8]; snprintf(b, sizeof b, "0x%04X", c); return b; }

    std::string ToJson() const {
        auto esc = [](const std::string& s) { std::string r; for (char c : s) { if (c == '"' || c == '\\') r += '\\'; r += c; } return r; };
        std::string j = "{\n  \"form\": { \"name\": \"" + esc(name) + "\", \"width\": " + std::to_string(w) + ", \"height\": " + std::to_string(h) + ", \"backColor\": \"" + Hex(bg) + "\", \"outputPath\": \"" + esc(outDir) + "\" },\n  \"controls\": [";
        for (size_t i = 0; i < items.size(); ++i) {
            const DControl& c = items[i];
            j += i ? ",\n    " : "\n    ";
            j += "{ \"type\": \"" + std::string(Info(c.type).key) + "\", \"name\": \"" + esc(c.name) + "\", \"text\": \"" + esc(c.text) +
                 "\", \"font\": \"" + esc(c.font) + "\", \"textColor\": \"" + Hex(c.fg) + "\", \"backColor\": \"" + Hex(c.bg) + "\", \"borderColor\": \"" + Hex(c.border) +
                 "\"" + Layout(c) + ", \"x\": " + std::to_string(c.x) + ", \"y\": " + std::to_string(c.y) + ", \"width\": " + std::to_string(c.w) + " }";
        }
        j += items.empty() ? "]\n}\n" : "\n  ]\n}\n";
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
        for (const JV& c : cs->a) {
            const JV* ty = c.Get("type");
            int t = -1;
            if (ty && ty->t == JV::STR) for (int i = 0; i < C_COUNT; ++i) if (ty->s == Info(i).key) t = i;
            if (t < 0) { err = "unknown control type \"" + (ty ? ty->s : std::string("?")) + "\""; return false; }
            DControl d; d.type = t;
            const JV* nm = c.Get("name"); const JV* tx = c.Get("text");
            d.name = nm && nm->t == JV::STR && ValidName(nm->s) && !n.ControlNamed(nm->s) ? nm->s : n.UniqueName(Info(t).prefix);   // groups are repaired after the loop
            d.text = tx && tx->t == JV::STR ? tx->s : Info(t).defText;
            if (const JV* fv = c.Get("font")) if (fv->t == JV::STR && FontIndex(fv->s) >= 0) d.font = fv->s;       // unknown fonts fall back to the default
            d.fg = col(c.Get("textColor"), DEF_FG); d.bg = col(c.Get("backColor"), DEF_BG); d.border = col(c.Get("borderColor"), DEF_BORDER);
            if (const JV* v = c.Get("borderStyle")) if (v->t == JV::STR) d.borderStyle = NameIndex(v->s, StyleName);
            if (const JV* v = c.Get("align")) if (v->t == JV::STR && Info(t).hasAlign) d.align = NameIndex(v->s, AlignName);
            if (const JV* v = c.Get("borderSize")) if (v->t == JV::NUM) d.borderSize = num(v, 0, 0, MAX_BORDER);
            if (const JV* v = c.Get("padding")) if (v->t == JV::NUM) d.padding = num(v, 0, 0, MAX_PADDING);
            if (const JV* gv = c.Get("group")) if (gv->t == JV::STR && ValidName(gv->s) && !n.ControlNamed(gv->s)) d.group = gv->s;
            d.x = num(c.Get("x"), 0, 0, MAX_FORM_W); d.y = num(c.Get("y"), 0, 0, MAX_FORM_H);
            d.w = num(c.Get("width"), Info(t).defW, MIN_CONTROL_W, MAX_FORM_W);
            n.items.push_back(d);
        }
        for (int k = 1; n.FormNameClashes(n.name, false); ++k) n.name = "Form" + std::to_string(k);   // the form's class names must not clash with a control (a group gives way to the form, below)
        for (DControl& c : n.items)                                   // a group name must not be a control name or the form's class (all are C++ names)
            if (c.type == C_RADIO && (n.ControlNamed(c.group) || c.group == n.name || c.group == n.name + "Base")) c.group = n.UniqueGroupName();
        *this = n; err.clear(); return true;
    }
};
}
