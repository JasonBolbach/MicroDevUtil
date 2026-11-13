#pragma once
// A multi-line text field for the Form Designer (US-ASCII), built on TextField: the text is one string with '\n' between the lines and the caret is
// one position in it, so typing, selection, word movement, clipboard and undo / redo are all inherited. What is added: lines (Up / Down keep the
// column, Home / End go to the line ends, Enter starts a new line) and vertical scrolling. No drawing here: the owner draws and measures.
//
//   Up / Down                 previous / next line (the column is kept);  Shift extends the selection
//   Home / End                start / end of the line;  Cmd+Left / Cmd+Right the same (macOS)
//   Ctrl+Home / Ctrl+End      start / end of the text;  Cmd+Up / Cmd+Down the same (macOS)
//   Enter                     a new line;  PageUp / PageDown are left to the owner (it knows how many rows fit)
#include "DesignerTextField.h"

namespace FD {
class RichTextField : public TextField {
public:
    int firstLine = 0;                        // vertical scroll: the first visible line

    RichTextField() { maxLen = 4000; }

    // ---- lines (public: the owner and the tests use them) ----
    int LineCount() const { int n = 1; for (char c : text) if (c == '\n') ++n; return n; }
    int LineStart(int line) const { int i = 0; for (; line > 0 && i < (int)text.size(); ++i) if (text[i] == '\n') --line; return i; }
    int LineEnd(int line) const { int i = LineStart(line); while (i < (int)text.size() && text[i] != '\n') ++i; return i; }
    int LineOf(int pos) const { int l = 0; for (int i = 0; i < pos && i < (int)text.size(); ++i) if (text[i] == '\n') ++l; return l; }
    int ColOf(int pos) const { return pos - LineStart(LineOf(pos)); }
    std::string LineText(int line) const { return text.substr(LineStart(line), LineEnd(line) - LineStart(line)); }
    std::vector<std::string> Lines() const { std::vector<std::string> v; for (int i = 0; i < LineCount(); ++i) v.push_back(LineText(i)); return v; }
    void SetLines(const std::vector<std::string>& v) { std::string t; for (size_t i = 0; i < v.size(); ++i) t += (i ? "\n" : "") + v[i]; SetText(t); firstLine = 0; goal = -1; caret = anchor = 0; }

    // Up / Down by `d` lines, keeping the column (also where a shorter line was passed). Stops at the first / last line.
    void MoveLine(int d, bool extend) {
        int line = LineOf(caret), target = std::max(0, std::min(line + d, LineCount() - 1));
        if (goal < 0 || caret != goalCaret) goal = ColOf(caret);
        int pos = std::min(LineStart(target) + goal, LineEnd(target));
        if (target == line) pos = d < 0 ? LineStart(line) : LineEnd(line);                      // already on the first / last line: go to its start / end
        MoveTo(pos, extend); goalCaret = caret;
    }
    void LineHome(bool extend) { MoveTo(LineStart(LineOf(caret)), extend); }
    void LineEndKey(bool extend) { MoveTo(LineEnd(LineOf(caret)), extend); }
    void SelectLine(int line) { anchor = LineStart(line); caret = LineEnd(line); }

    bool Key(const InputEvent& e) override {
        bool shift = e.mod & KMOD_SHIFT, gui = e.mod & KMOD_GUI, ctrl = e.mod & KMOD_CTRL;
        switch (e.key) {
        case SDLK_RETURN: case SDLK_KP_ENTER: Insert("\n"); return true;
        case SDLK_UP:   if (gui) MoveTo(0, shift); else MoveLine(-1, shift); return true;
        case SDLK_DOWN: if (gui) MoveTo((int)text.size(), shift); else MoveLine(1, shift); return true;
        case SDLK_HOME: if (ctrl) MoveTo(0, shift); else LineHome(shift); return true;
        case SDLK_END:  if (ctrl) MoveTo((int)text.size(), shift); else LineEndKey(shift); return true;
        case SDLK_LEFT:  if (gui) { LineHome(shift); return true; } break;
        case SDLK_RIGHT: if (gui) { LineEndKey(shift); return true; } break;
        default: break;
        }
        return TextField::Key(e);
    }

    // x, y = pixels from the top-left of the text area (the scroll is added here); lineH = the height of a line.
    void SetCaretFromXY(int x, int y, bool extend, const Measure& m, int lineH) {
        int line = std::max(0, std::min(firstLine + (lineH > 0 ? y / lineH : 0), LineCount() - 1));
        std::string t = LineText(line); x += scrollX;
        int best = 0, bestD = 1 << 30;
        for (int i = 0; i <= (int)t.size(); ++i) { int d = std::abs(m(t.substr(0, i)) - x); if (d < bestD) { bestD = d; best = i; } }
        MoveTo(LineStart(line) + best, extend);
    }
    // keep the caret inside the visible rows and innerW pixels
    void EnsureVisibleXY(const Measure& m, int innerW, int rows) {
        int line = LineOf(caret);
        if (line < firstLine) firstLine = line;
        if (line >= firstLine + rows) firstLine = line - rows + 1;
        firstLine = std::max(0, std::min(firstLine, std::max(0, LineCount() - 1)));
        int cx = m(text.substr(LineStart(line), caret - LineStart(line)));
        if (cx - scrollX < 0) scrollX = cx;
        if (cx - scrollX > innerW - 4) scrollX = cx - innerW + 4;
        if (scrollX < 0) scrollX = 0;
    }

protected:
    std::string Sanitize(const std::string& s) const override {          // printable ASCII; a new line stays, \r is dropped, a tab becomes a space
        std::string r; for (unsigned char c : s) { if (c == '\n') r += '\n'; else if (c == '\t') r += ' '; else if (c >= 0x20 && c < 0x7F) r += (char)c; } return r;
    }

private:
    int goal = -1, goalCaret = -1;
};
}
