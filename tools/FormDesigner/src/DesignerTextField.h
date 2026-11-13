#pragma once
// A single-line text editing model for the Form Designer (US-ASCII only: one byte = one character, no drawing). The designer's property grid and dialogs will reuse it.
//
// Input outside 0x20..0x7E (accented / non-Latin characters, control codes) is dropped when typed or pasted.
//
// Keys handled by Key():
//   Left / Right            caret one character (collapses a selection);  Up / Home = start,  Down / End = end
//   Alt or Ctrl + Left/Right  one word;   Cmd + Left/Right  start / end of the line (macOS)
//   Shift + any of the above  extends the selection
//   Backspace / Delete      one character, or the selection;  with Alt / Ctrl: one word;  Cmd+Backspace: to the line start
//   Ctrl|Cmd + A / C / X / V  select all / copy / cut / paste (system clipboard)
//   Ctrl|Cmd + Z, Shift+Ctrl|Cmd+Z, Ctrl+Y   undo / redo
// Mouse (the owner supplies the text-origin-relative x and a width measuring function): SetCaretFromX, SelectWord, SelectAll.
#include <SDL.h>
#include <algorithm>
#include <functional>
#include <string>
#include <vector>
#include "DesignerInput.h"

namespace FD {
class TextField {
public:
    using Measure = std::function<int(const std::string&)>;      // pixel width of a string in the field's font
    virtual ~TextField() = default;

    std::string text;
    int caret = 0, anchor = 0;       // character positions (0..text.size()); selection = [min, max)
    int scrollX = 0;                 // horizontal scroll in pixels
    size_t maxLen = 256;             // characters
    enum Filter { ANY, DIGITS, SIGNED, DECIMAL, IDENT, HEX } filter = ANY;   // what may be typed / pasted: any printable ASCII, digits only, a C++ identifier, hex digits

    bool HasSel() const { return caret != anchor; }
    int SelBegin() const { return std::min(caret, anchor); }
    int SelEnd() const { return std::max(caret, anchor); }
    std::string Selected() const { return text.substr(SelBegin(), SelEnd() - SelBegin()); }

    void SetText(const std::string& s) { text = s; if (text.size() > maxLen) text.resize(maxLen); caret = anchor = (int)text.size(); scrollX = 0; undo.clear(); redo.clear(); last = NONE; }

    // ---- editing ----
    void Insert(const std::string& s0, bool paste = false) {
        std::string s = Sanitize(s0);
        if (s.empty()) return;
        Remember(paste ? OTHER : TYPING);
        EraseSel();
        size_t room = maxLen > text.size() ? maxLen - text.size() : 0;
        if (s.size() > room) s.resize(room);
        text.insert(caret, s); caret += (int)s.size(); anchor = caret;
    }
    void Backspace(bool word, bool toStart) {
        if (!HasSel() && caret == 0) return;
        Remember(OTHER);
        if (!HasSel()) anchor = toStart ? 0 : word ? WordLeft(caret) : Prev(caret);
        EraseSel();
    }
    void DeleteFwd(bool word) {
        if (!HasSel() && caret >= (int)text.size()) return;
        Remember(OTHER);
        if (!HasSel()) anchor = word ? WordRight(caret) : Next(caret);
        EraseSel();
    }
    void Cut()  { if (!HasSel()) return; SDL_SetClipboardText(Selected().c_str()); Remember(OTHER); EraseSel(); }
    void Copy() { if (HasSel()) SDL_SetClipboardText(Selected().c_str()); }
    void Paste() { if (char* c = SDL_GetClipboardText()) { Insert(c, true); SDL_free(c); } }
    void Undo() { Swap(undo, redo); }
    void Redo() { Swap(redo, undo); }

    // ---- caret ----
    void SelectAll() { anchor = 0; caret = (int)text.size(); last = NONE; }
    void MoveTo(int pos, bool extend) { caret = pos; if (!extend) anchor = pos; last = NONE; }
    void Left(bool extend, bool word)  { if (HasSel() && !extend) MoveTo(SelBegin(), false); else MoveTo(word ? WordLeft(caret) : Prev(caret), extend); }
    void Right(bool extend, bool word) { if (HasSel() && !extend) MoveTo(SelEnd(), false);   else MoveTo(word ? WordRight(caret) : Next(caret), extend); }
    void SelectWord(int pos) { anchor = WordStart(pos); caret = WordEnd(pos); last = NONE; }
    // x = pixels from the left edge of the text area (scroll is added here); the caret goes to the nearest character boundary
    void SetCaretFromX(int x, bool extend, const Measure& m) {
        x += scrollX;
        int best = 0, bestD = 1 << 30;
        for (int i = 0; i <= (int)text.size(); ++i) {
            int d = std::abs(m(text.substr(0, i)) - x);
            if (d < bestD) { bestD = d; best = i; }
        }
        MoveTo(best, extend);
    }
    // keep the caret inside the visible innerW pixels
    void EnsureVisible(const Measure& m, int innerW) {
        int total = m(text), cx = m(text.substr(0, caret));
        if (cx - scrollX < 0) scrollX = cx;
        if (cx - scrollX > innerW - 2) scrollX = cx - innerW + 2;
        if (total - scrollX < innerW - 2) scrollX = std::max(0, total - innerW + 2);    // no empty gap after the end of the text
        if (scrollX < 0) scrollX = 0;
    }

    // Keyboard. Returns true when the key was used (Tab / Return / Escape are left to the owner).
    virtual bool Key(const InputEvent& e) {
        bool shift = e.mod & KMOD_SHIFT, cmd = e.mod & (KMOD_CTRL | KMOD_GUI), word = e.mod & (KMOD_CTRL | KMOD_ALT), gui = e.mod & KMOD_GUI;
        switch (e.key) {
        case SDLK_LEFT:  if (gui) MoveTo(0, shift); else Left(shift, word); return true;
        case SDLK_RIGHT: if (gui) MoveTo((int)text.size(), shift); else Right(shift, word); return true;
        case SDLK_UP:    case SDLK_HOME: MoveTo(0, shift); return true;
        case SDLK_DOWN:  case SDLK_END:  MoveTo((int)text.size(), shift); return true;
        case SDLK_BACKSPACE: Backspace(word, gui); return true;
        case SDLK_DELETE:    DeleteFwd(word); return true;
        case SDLK_a: if (!cmd) return false; SelectAll(); return true;
        case SDLK_c: if (!cmd) return false; Copy(); return true;
        case SDLK_x: if (!cmd) return false; Cut(); return true;
        case SDLK_v: if (!cmd) return false; Paste(); return true;
        case SDLK_z: if (!cmd) return false; if (shift) Redo(); else Undo(); return true;
        case SDLK_y: if (!(e.mod & KMOD_CTRL)) return false; Redo(); return true;
        default: return false;
        }
    }

    // ---- character / word positions (public: the tests use them) ----
    int Prev(int i) const { return i > 0 ? i - 1 : 0; }
    int Next(int i) const { return i < (int)text.size() ? i + 1 : (int)text.size(); }
    int WordLeft(int i) const {            // skip spaces to the left, then the run of letters (or of punctuation)
        while (i > 0 && Cls(Prev(i)) == 0) i = Prev(i);
        if (i == 0) return 0;
        int c = Cls(Prev(i)); while (i > 0 && Cls(Prev(i)) == c) i = Prev(i);
        return i;
    }
    int WordRight(int i) const {
        int n = (int)text.size();
        while (i < n && Cls(i) == 0) i = Next(i);
        if (i >= n) return n;
        int c = Cls(i); while (i < n && Cls(i) == c) i = Next(i);
        return i;
    }
    int WordStart(int i) const { int c = i < (int)text.size() ? Cls(i) : (i > 0 ? Cls(Prev(i)) : 0); while (i > 0 && Cls(Prev(i)) == c) i = Prev(i); return i; }
    int WordEnd(int i) const { int n = (int)text.size(); if (i >= n) return n; int c = Cls(i); while (i < n && Cls(i) == c) i = Next(i); return i; }

    // what Insert() lets through (a multi-line field overrides this to keep new lines)
    virtual std::string Sanitize(const std::string& s) const { return Clean(s, filter); }

protected:
    struct Snap { std::string t; int c, a; };
    enum Kind { NONE, TYPING, OTHER } last = NONE;
    std::vector<Snap> undo, redo;

    int Cls(int i) const {                 // 0 space, 1 letter / digit / underscore, 2 punctuation
        unsigned char c = (unsigned char)text[i];
        if (c == ' ' || c == '\t' || c == '\n') return 0;
        if (isalnum(c) || c == '_') return 1;
        return 2;
    }
    void EraseSel() { if (!HasSel()) return; text.erase(SelBegin(), SelEnd() - SelBegin()); caret = anchor = std::min(caret, anchor); }
    // one undo step per run of typing, one per other edit
    void Remember(Kind k) {
        if (!(k == TYPING && last == TYPING && !HasSel())) { undo.push_back({text, caret, anchor}); if (undo.size() > 200) undo.erase(undo.begin()); }
        redo.clear(); last = k;
    }
    void Swap(std::vector<Snap>& from, std::vector<Snap>& to) {
        if (from.empty()) return;
        to.push_back({text, caret, anchor});
        Snap s = from.back(); from.pop_back();
        text = s.t; caret = s.c; anchor = s.a; last = NONE;
    }
    static std::string Clean(const std::string& s, Filter flt) {       // ASCII only: tab / newline become a space, everything else outside 0x20..0x7E is dropped
        std::string r;
        for (unsigned char c : s) {
            if (flt == DIGITS) { if (isdigit(c)) r += (char)c; }
            else if (flt == SIGNED) { if (isdigit(c) || c == '-') r += (char)c; }
            else if (flt == DECIMAL) { if (isdigit(c) || c == '-' || c == '.') r += (char)c; }
            else if (flt == HEX) { if (isxdigit(c)) r += (char)toupper(c); }
            else if (flt == IDENT) { if (isalnum(c) || c == '_') r += (char)c; }
            else if (c == '\t' || c == '\n') r += ' ';
            else if (c >= 0x20 && c < 0x7F) r += (char)c;
        }
        return r;
    }
};
}
