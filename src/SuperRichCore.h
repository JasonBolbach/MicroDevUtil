#pragma once
// SuperRichCore - the lenient "basic HTML" text engine shared by SuperRichText (and, with SetRich(true), SuperLabel and SuperMessageBox).
// Header only, depends on LovyanGFX only. Nothing here allocates unless you lay text out.
//
// Text with a few HTML-like tags, drawn with ONE font (the control's). Everything is a single pass over the text; there is no parse tree and no
// syntax checking: a tag that is not understood is skipped, a '<' that does not start a tag is shown as it is, an unclosed tag simply stays in
// effect to the end. Bad text shows as bad text.
//
//   <br>                       end the line (a plain new line character does the same)
//   <tab>  (or a tab char)     jump to the next tab stop (SetTabChars)
//   <hr>                       a horizontal rule across the whole width
//   <color=red> ... </color>   text colour: #RGB, #RRGGBB or a name (black white red green blue yellow cyan magenta orange gray ...)
//   <bg=#003366> ... </bg>     colour behind the text
//   <font color=red bg=blue>   both at once;  </font> returns to the control's colours
//   <u> </u>   <s> </s>        underline / strike through (drawn as lines)
//   <big> </big>               text at double size (LovyanGFX text scaling)
//   <left> <center> <right>    alignment of the line (taken where its first text starts);  </center> etc. return to the control's alignment
//   &lt; &gt; &amp; &quot; &apos; &nbsp; &#65; &#x41;     entities (ASCII range)
//   Anything else (<b>, <i>, <p>, <img>, ...) is ignored. Closing tags (</color>, </u>, ...) reset to the control's default: there is no nesting stack,
//   so  <color=red>a<color=blue>b</color>c  shows c in the default colour.
// Words wrap at the width; a word wider than the width is broken inside. Spaces are kept as written (no HTML whitespace collapsing).
// "Show source" mode (Interpret = false) draws the same text with every tag and entity as plain characters (new lines and tabs still work).
#include <LovyanGFX.hpp>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace SuperRich {

enum : uint8_t { F_UNDER = 1, F_STRIKE = 2, F_BIG = 4, F_HASBG = 8 };            // style flags
enum : uint8_t { A_LEFT = 0, A_CENTER = 1, A_RIGHT = 2 };

struct Style { uint16_t Fg = 0xFFFF, Bg = 0; uint8_t Flags = 0, Align = A_LEFT; };

struct Options {
    Style Def;                       // the control's colours and alignment (what the closing tags go back to)
    bool  Interpret = true;          // false: show the source (no tags, no entities)
    bool  Wrap = true;               // false: a line never wraps (it is as wide as its text; the control clips it)
    int   TabPx = 32;                // tab stop distance in pixels
};

// One visual line of the layout: enough to draw it (and to start drawing from any line) without storing the text again.
struct Line {
    uint32_t Start = 0;              // offset into the text
    Style    St;                     // the style in effect at Start
    uint16_t H = 0;                  // height in pixels
    bool     Rule = false;           // an <hr> line
    bool     Cont = false;           // follows a soft wrap (leading spaces are dropped)
};

struct Atom { std::string T; Style St; int X = 0, W = 0; bool Space = false; };
struct LineOut { std::vector<Atom> Atoms; int Width = 0, H = 0; uint8_t Align = A_LEFT; bool Rule = false, SoftWrap = false, Content = false; };

// ---- colours ----
inline uint16_t Rgb565(int r, int g, int b) { return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xFF) >> 3)); }
inline bool ParseColor(const std::string& v0, uint16_t& out) {
    std::string v; for (char c : v0) { if (c == '"' || c == '\'') continue; v += (char)tolower((unsigned char)c); }
    if (v.empty()) return false;
    auto hex = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
    if (v[0] == '#' || (v.size() > 2 && v[0] == '0' && v[1] == 'x')) {
        std::string h = v.substr(v[0] == '#' ? 1 : 2);
        for (char c : h) if (hex(c) < 0) return false;
        if (h.size() == 3) { out = Rgb565(hex(h[0]) * 17, hex(h[1]) * 17, hex(h[2]) * 17); return true; }
        if (h.size() == 6) { out = Rgb565(hex(h[0]) * 16 + hex(h[1]), hex(h[2]) * 16 + hex(h[3]), hex(h[4]) * 16 + hex(h[5])); return true; }
        return false;
    }
    struct N { const char* n; uint8_t r, g, b; };
    static const N names[] = { { "black", 0, 0, 0 }, { "white", 255, 255, 255 }, { "red", 255, 0, 0 }, { "green", 0, 200, 0 }, { "lime", 0, 255, 0 }, { "blue", 0, 0, 255 },
        { "yellow", 255, 255, 0 }, { "cyan", 0, 255, 255 }, { "magenta", 255, 0, 255 }, { "orange", 255, 160, 0 }, { "gray", 128, 128, 128 }, { "grey", 128, 128, 128 },
        { "silver", 192, 192, 192 }, { "darkgray", 64, 64, 64 }, { "navy", 0, 0, 128 }, { "teal", 0, 128, 128 }, { "maroon", 128, 0, 0 }, { "purple", 128, 0, 128 },
        { "olive", 128, 128, 0 }, { "pink", 255, 128, 192 } };
    for (const N& n : names) if (v == n.n) { out = Rgb565(n.r, n.g, n.b); return true; }
    return false;
}

namespace detail {
inline bool IsAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
// A tag starts at '<' + letter or '/', and ends at the next '>' (within 96 characters, with no other '<' before it). Returns the index of '>' or 0.
inline size_t TagEnd(const char* t, size_t n, size_t pos) {
    if (pos + 1 >= n || !(IsAlpha(t[pos + 1]) || t[pos + 1] == '/')) return 0;
    for (size_t j = pos + 1; j < n && j < pos + 96; ++j) { if (t[j] == '<') return 0; if (t[j] == '>') return j; }
    return 0;
}
struct Tag { std::string Name; bool Close = false; std::vector<std::pair<std::string, std::string>> Attr; };
inline Tag ParseTag(const char* t, size_t from, size_t to) {          // t[from..to) is between '<' and '>'
    Tag g; size_t i = from;
    if (i < to && t[i] == '/') { g.Close = true; ++i; }
    while (i < to && t[i] != ' ' && t[i] != '=' && t[i] != '/' && t[i] != '\t') g.Name += (char)tolower((unsigned char)t[i++]);
    auto value = [&](std::string& v) {
        v.clear(); if (i < to && (t[i] == '"' || t[i] == '\'')) { char q = t[i++]; while (i < to && t[i] != q) v += t[i++]; if (i < to) ++i; }
        else while (i < to && t[i] != ' ' && t[i] != '\t' && t[i] != '/') v += t[i++]; };
    if (i < to && t[i] == '=') { ++i; std::string v; value(v); g.Attr.push_back({ g.Name, v }); }          // <color=red>: the tag's own value
    while (i < to) {
        while (i < to && (t[i] == ' ' || t[i] == '\t' || t[i] == '/')) ++i;
        if (i >= to) break;
        std::string k; while (i < to && t[i] != '=' && t[i] != ' ' && t[i] != '\t' && t[i] != '/') k += (char)tolower((unsigned char)t[i++]);
        std::string v; if (i < to && t[i] == '=') { ++i; value(v); }
        g.Attr.push_back({ k, v });
    }
    return g;
}
inline const std::string* Find(const Tag& g, const char* key) { for (auto& a : g.Attr) if (a.first == key) return &a.second; return nullptr; }
inline int Entity(const char* t, size_t n, size_t pos, char& out, size_t& len) {   // &name; at pos -> 1 and the character, else 0
    size_t j = pos + 1; if (j >= n) return 0;
    std::string e; while (j < n && j < pos + 9 && t[j] != ';' && t[j] != ' ' && t[j] != '&' && t[j] != '<') e += t[j++];
    if (j >= n || t[j] != ';' || e.empty()) return 0;
    len = j - pos + 1;
    if (e == "lt") out = '<'; else if (e == "gt") out = '>'; else if (e == "amp") out = '&'; else if (e == "quot") out = '"'; else if (e == "apos") out = '\'';
    else if (e == "nbsp") out = ' ';
    else if (e[0] == '#') { long v = e.size() > 1 && (e[1] == 'x' || e[1] == 'X') ? strtol(e.c_str() + 2, nullptr, 16) : strtol(e.c_str() + 1, nullptr, 10); if (v < 32 || v > 126) return 0; out = (char)v; }
    else return 0;
    return 1;
}
inline void Apply(const Tag& g, Style& st, const Options& o) {                 // style tags; the others (br, hr, tab) are handled by the scanner
    const Style& d = o.Def; uint16_t c;
    if (g.Close) {
        if (g.Name == "color") st.Fg = d.Fg;
        else if (g.Name == "bg") { st.Flags &= ~F_HASBG; st.Bg = d.Bg; }
        else if (g.Name == "font") { st.Fg = d.Fg; st.Flags &= ~F_HASBG; st.Bg = d.Bg; }
        else if (g.Name == "u") st.Flags &= ~F_UNDER;
        else if (g.Name == "s" || g.Name == "strike" || g.Name == "del") st.Flags &= ~F_STRIKE;
        else if (g.Name == "big") st.Flags &= ~F_BIG;
        else if (g.Name == "left" || g.Name == "center" || g.Name == "right") st.Align = d.Align;
        return;
    }
    if (g.Name == "color" || g.Name == "font") { const std::string* v = Find(g, "color"); if (!v) v = Find(g, "fg"); if (v && ParseColor(*v, c)) st.Fg = c; }
    if (g.Name == "bg" || g.Name == "font") { const std::string* v = Find(g, "bg"); if (!v) v = Find(g, "bgcolor"); if (v && ParseColor(*v, c)) { st.Bg = c; st.Flags |= F_HASBG; } }
    if (g.Name == "u") st.Flags |= F_UNDER;
    else if (g.Name == "s" || g.Name == "strike" || g.Name == "del") st.Flags |= F_STRIKE;
    else if (g.Name == "big") st.Flags |= F_BIG;
    else if (g.Name == "left") st.Align = A_LEFT;
    else if (g.Name == "center") st.Align = A_CENTER;
    else if (g.Name == "right") st.Align = A_RIGHT;
}
}   // namespace detail

// Reads ONE visual line of t[0..n) starting at `pos` with the style `st` (updated). `cont` = this line follows a soft wrap. The caller has set g's font.
// Used both to lay the text out (Layout) and to draw a single line later (DrawLine), so the two always agree. Returns the new position.
inline size_t ScanLine(lgfx::LGFXBase& g, const char* t, size_t n, size_t pos, Style& st, bool cont, const Options& o, int width, LineOut& out) {
    using namespace detail;
    out = LineOut(); int pen = 0, maxScale = 1; bool first = true;
    const int ww = o.Wrap ? width : 0x3FFFFFFF;                       // the width words wrap at
    auto scaleOf = [](const Style& s) { return (s.Flags & F_BIG) ? 2 : 1; };
    auto measure = [&](const std::string& s, const Style& sty) { g.setTextSize((float)scaleOf(sty)); return (int)g.textWidth(s.c_str()); };
    auto setAlign = [&]() { if (first) { out.Align = st.Align; first = false; } };
    auto addAtom = [&](const std::string& txt, bool space, int w) {
        setAlign();
        if (space && !out.Atoms.empty() && out.Atoms.back().Space && out.Atoms.back().St.Fg == st.Fg && out.Atoms.back().St.Flags == st.Flags && out.Atoms.back().St.Bg == st.Bg) { out.Atoms.back().T += txt; out.Atoms.back().W += w; }
        else { Atom a; a.T = txt; a.St = st; a.X = pen; a.W = w; a.Space = space; out.Atoms.push_back(a); }
        pen += w; maxScale = std::max(maxScale, scaleOf(st)); if (!space) out.Content = true;
    };
    while (pos < n) {
        char c = t[pos];
        if (c == '\r') { ++pos; continue; }
        if (c == '\n') { ++pos; goto done; }
        if (c == '<' && o.Interpret) {
            size_t e = TagEnd(t, n, pos);
            if (e) {
                Tag tag = ParseTag(t, pos + 1, e);
                if (!tag.Close && tag.Name == "br") { pos = e + 1; goto done; }
                if (!tag.Close && tag.Name == "hr") {
                    if (!out.Atoms.empty()) goto done;                         // the rule gets its own line: end this one, the tag is read again
                    pos = e + 1; if (pos < n && t[pos] == '\r') ++pos; if (pos < n && t[pos] == '\n') ++pos;
                    out.Rule = true; goto done;
                }
                if (!tag.Close && tag.Name == "tab") {
                    int stop = ((pen / o.TabPx) + 1) * o.TabPx; if (stop > ww) stop = ww;
                    if (stop > pen) { std::string sp; addAtom(sp, true, stop - pen); }
                    pos = e + 1; continue;
                }
                Apply(tag, st, o); pos = e + 1; continue;
            }
        }
        if (c == '\t') { int stop = ((pen / o.TabPx) + 1) * o.TabPx; if (stop > ww) stop = ww; if (stop > pen) { std::string sp; addAtom(sp, true, stop - pen); } ++pos; continue; }
        if (c == ' ') {
            if (cont && out.Atoms.empty() && first) { ++pos; continue; }          // no leading spaces after a soft wrap
            int w = measure(" ", st);
            if (pen + w > ww && out.Content) { out.SoftWrap = true; goto done; }
            addAtom(" ", true, w); ++pos; continue;
        }
        {   // a word: up to a space, tab, new line or tag
            std::string word; std::vector<size_t> src; size_t p = pos;
            while (p < n) {
                char d = t[p];
                if (d == ' ' || d == '\t' || d == '\n' || d == '\r') break;
                if (d == '<' && o.Interpret && TagEnd(t, n, p)) break;
                if (d == '&' && o.Interpret) { char ch; size_t len; if (Entity(t, n, p, ch, len)) { word += ch; src.push_back(p); p += len; continue; } }
                word += d; src.push_back(p); ++p;
            }
            int w = measure(word, st);
            if (pen + w <= ww) { addAtom(word, false, w); pos = p; continue; }
            if (out.Content || w <= ww) { out.SoftWrap = true; goto done; }     // wrap before the word (it fits on a line of its own)
            // the line is empty and the word alone is wider than the width: break inside it
            size_t k = word.size(); while (k > 1 && pen + measure(word.substr(0, k), st) > ww) --k;
            std::string part = word.substr(0, k); addAtom(part, false, measure(part, st));
            pos = k < word.size() ? src[k] : p; out.SoftWrap = k < word.size();
            goto done;
        }
    }
done:
    while (!out.Atoms.empty() && out.Atoms.back().Space) { out.Atoms.pop_back(); }      // trailing spaces are invisible: drop them (and do not count them)
    out.Width = 0; for (const Atom& a : out.Atoms) out.Width = std::max(out.Width, a.X + a.W);
    if (first) out.Align = st.Align;
    g.setTextSize(1.0f);
    int fh = (int)g.fontHeight();
    int sc = out.Atoms.empty() ? scaleOf(st) : 1;
    for (const Atom& a : out.Atoms) sc = std::max(sc, scaleOf(a.St));
    out.H = out.Rule ? std::max(4, fh / 2) : fh * sc;
    return pos;
}

// Lays the whole text out: one Line per visual line.
inline void Layout(lgfx::LGFXBase& g, const char* t, size_t n, int width, const Options& o, std::vector<Line>& lines) {
    lines.clear(); Style st = o.Def; size_t pos = 0; bool cont = false;
    do {
        Line L; L.Start = (uint32_t)pos; L.St = st; L.Cont = cont;
        LineOut lo; size_t np = ScanLine(g, t, n, pos, st, cont, o, width, lo);
        L.H = (uint16_t)lo.H; L.Rule = lo.Rule; lines.push_back(L);
        cont = lo.SoftWrap;
        if (np <= pos) { ++np; }                                               // safety: always make progress
        pos = np;
    } while (pos < n);
}

// Draws one line with its top-left corner at (x, y) inside `width` pixels. The caller has set g's font; g's text size is left at 1.
inline void DrawLine(lgfx::LGFXBase& g, const char* t, size_t n, const Line& L, const Options& o, int x, int y, int width) {
    Style st = L.St; LineOut lo; ScanLine(g, t, n, L.Start, st, L.Cont, o, width, lo);
    g.setTextDatum(lgfx::textdatum_t::top_left);
    if (lo.Rule) { g.drawFastHLine(x, y + L.H / 2, width, st.Fg); return; }
    int ox = lo.Align == A_CENTER ? (width - lo.Width) / 2 : lo.Align == A_RIGHT ? width - lo.Width : 0;
    if (ox < 0) ox = 0;
    int fh1 = (int)g.fontHeight();
    for (const Atom& a : lo.Atoms) {
        int sc = (a.St.Flags & F_BIG) ? 2 : 1, ah = fh1 * sc, ay = y + lo.H - ah, ax = x + ox + a.X;
        if (a.St.Flags & F_HASBG) g.fillRect(ax, y, a.W, lo.H, a.St.Bg);
        if (!a.Space) { g.setTextSize((float)sc); g.setTextColor(a.St.Fg); g.drawString(a.T.c_str(), ax, ay); g.setTextSize(1.0f); }
        if (a.St.Flags & F_UNDER) g.drawFastHLine(ax, ay + ah - 1, a.W, a.St.Fg);
        if (a.St.Flags & F_STRIKE) g.drawFastHLine(ax, ay + ah / 2, a.W, a.St.Fg);
    }
}

inline int ContentHeight(const std::vector<Line>& lines, int gap = 0) { int h = 0; for (const Line& l : lines) h += l.H + gap; return h; }
}   // namespace SuperRich
