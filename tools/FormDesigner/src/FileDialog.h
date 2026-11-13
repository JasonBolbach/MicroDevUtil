#pragma once
// A modal file / folder dialog for the Form Designer, drawn inside the designer window (no OS dialogs, so it looks and behaves the same on
// Windows, macOS and Linux). Only C++17 std::filesystem is used.
//
//   +--------------------------------------------------------------------+
//   | Open form                                                           |
//   | [ /Users/me/project                                              ]  |   path: type a folder (or file) and press Enter
//   | [Up] [Home] [New folder] ([Drives] on Windows)                      |
//   | +----------------------------------------------------------------+ |
//   | | ..                                                             | |   the real SuperListBox: folders first ("name/"),
//   | | src/                                                           | |   then the files that match the extension
//   | | form.json                                                      | |
//   | +----------------------------------------------------------------+ |
//   | Files: *.json                                                       |
//   | File name: [ form.json                                           ]  |
//   |                                              [Cancel]  [ Open ]     |
//   +--------------------------------------------------------------------+
//
// Kinds:  K_OPEN   pick an existing file          K_SAVE   type a name (the extension is added; an existing file asks to replace it)
//         K_FOLDER choose the folder that is shown (New folder creates one first).
// Mouse:  click a row (a file puts its name in the name field), double-click: enter a folder / accept a file. Wheel scrolls.
// Keys:   Up / Down / PageUp / PageDown / Home / End move in the list, Enter = enter / accept, Backspace = up one folder, Tab = next field,
//         Esc = cancel (or leave "New folder"). Names starting with '.' are hidden.
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <LovyanGFX.hpp>
#include <SuperListBox.h>
#include <SuperPushButton.h>
#include "DesignerInput.h"
#include "DesignerTextField.h"
#include "DesignerWidgets.h"

namespace FD {
class FileDialog {
public:
    enum Kind { K_OPEN, K_SAVE, K_FOLDER };
    enum State { OPEN, OK, CANCEL } state = OPEN;
    std::string path;                                          // the result (a file, or for K_FOLDER the folder) when state == OK

    // start: a folder, or a file (its folder is shown and its name filled in); ext: ".json" (matched case-insensitively; "" = every file)
    FileDialog(lgfx::LGFX_Sprite* g, Kind kind, const std::string& title, const std::string& start, const std::string& ext = ".json")
        : G(g), K(kind), Title(title), Ext(ext) {
        namespace fs = std::filesystem; std::error_code ec;
        fs::path st = start.empty() ? fs::path() : fs::path(start), dir;
        if (!st.empty() && fs::is_directory(st, ec)) dir = st;
        else if (!st.empty() && st.has_parent_path() && fs::is_directory(st.parent_path(), ec)) { dir = st.parent_path(); if (K != K_FOLDER) Name.SetText(st.filename().string()); }
        else { dir = fs::current_path(ec); }
        Name.maxLen = 200; Path.maxLen = 400; NewName.maxLen = 100;
        List.reset(new SuperListBox(G, X + 16, Y + 112, W - 32, &lgfx::fonts::Font2, ROWS, nullptr, nullptr, nullptr));
        SuperPushButton probe(G, 0, 0, 90, &lgfx::fonts::Font2, "OK"); BtnH = probe.GetHeight();
        Load(dir);
        if (K != K_FOLDER && !Name.text.empty())                 // starting on a file: highlight it
            for (int i = 0; i < (int)Rows.size(); ++i) if (Rows[i].type == T_FILE && Rows[i].name == Name.text) { List->SetSelected(i); List->StartEdit(); LastCursor = i; break; }
        Focus = K == K_SAVE ? F_NAME : F_LIST;
        if (Focus == F_NAME) Name.SelectAll();
    }

    static constexpr int W = 860, H = 510, X = (FD::WIN_W - W) / 2, Y = (FD::WIN_H - H) / 2, ROWS = 15;
    // ---- inspection (the self-test uses these) ----
    // Saving over this very path (the form's current file) does not ask "Replace it?".
    void SetNoConfirmPath(const std::string& p) { NoConfirm = p; }
    const std::string& Dir() const { return CurStr; }
    const std::string& NameText() const { return Name.text; }
    const std::string& PathText() const { return Path.text; }
    const std::string& ErrorText() const { return Err; }
    bool Confirming() const { return Confirm; }
    bool Naming() const { return Naming_; }
    int  Count() const { return (int)Rows.size(); }
    std::string RowText(int i) const { return i >= 0 && i < (int)Rows.size() ? Rows[i].label : ""; }
    SuperListBox* Lb() { return List.get(); }
    int PathY() const { return Y + 40; }
    int BtnRowY() const { return Y + 76; }
    int NavX(int i) const { static const int off[4] = { 0, 80, 160, 300 }; return X + 16 + off[i]; }          // Up, Home, New folder, Drives
    int NavW(int i) const { static const int w[4] = { 70, 70, 130, 90 }; return w[i]; }
    int InfoY() { return List->GetY() + List->GetHeight() + 6; }
    int NameY() { return InfoY() + 24; }
    int NameX() const { return X + 120; }
    int NameW() const { return W - 32 - 104; }
    int BtnY() const { return Y + H - 16 - BtnH; }
    int OkX() const { return X + W - 16 - 100; }
    int CancelX() const { return OkX() - 10 - 100; }

    void Event(const InputEvent& e) {
        switch (e.type) {
        case InputEvent::DOWN: if (e.button == SDL_BUTTON_LEFT) Down(e); break;
        case InputEvent::MOVE:
            if (Drag == D_TEXT && Cur_()) { Cur_()->SetCaretFromX(e.x - CurFieldX() - 6, true, MeasureFn()); Reveal(); }
            else if (Drag == D_LIST) { Pointer(Started ? SuperPointerEvent::DRAG_MOVE : SuperPointerEvent::DRAG_START, e.x, e.y); Started = true; }
            break;
        case InputEvent::UP: if (Drag == D_LIST) Pointer(SuperPointerEvent::DRAG_END, e.x, e.y); Drag = D_NONE; break;
        case InputEvent::WHEEL: if (e.x >= X && e.x < X + W && e.y >= Y && e.y < Y + H) { List->EditIO()->InjectDelta(-e.wheel * 3); Pump(); } break;
        case InputEvent::KEY_DOWN: Key(e); break;
        case InputEvent::TEXT: if (!Confirm && Cur_()) { Err.clear(); Cur_()->Insert(e.text); Reveal(); } break;
        default: break;
        }
    }

    void Draw(uint32_t ticks) {
        lgfx::LGFX_Sprite& g = *G;
        g.fillRectAlpha(0, 0, FD::WIN_W, FD::WIN_H, 150, 0x0000);
        g.fillRect(X, Y, W, H, COL_PANEL); g.drawRect(X, Y, W, H, COL_ACCENT); g.drawRect(X + 1, Y + 1, W - 2, H - 2, COL_ACCENT);
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left);
        g.setTextColor(COL_ACCENT); g.drawString(Title.c_str(), X + 16, Y + 12);
        DrawField(g, Path, X + 16, PathY(), W - 32, 26, Focus == F_PATH, !Err.empty() && Focus == F_PATH, ticks);
        static const char* nav[4] = { "Up", "Home", "New folder", "Drives" };
        for (int i = 0; i < NavCount(); ++i) { SuperPushButton b(G, NavX(i), BtnRowY(), NavW(i), &lgfx::fonts::Font2, nav[i]); b.Push(); }
        List->Push();
        g.setFont(&lgfx::fonts::Font2); g.setTextDatum(lgfx::textdatum_t::top_left);
        if (!Err.empty()) { g.setTextColor(COL_ERR); g.drawString(Err.c_str(), X + 16, InfoY()); }
        else { g.setTextColor(COL_DIM); g.drawString(K == K_FOLDER ? "Choose the folder that is shown above (open folders with a double click)." : (Ext.empty() ? "All files" : ("Files: *" + Ext).c_str()), X + 16, InfoY()); }
        if (K != K_FOLDER || Naming_) {
            g.setTextColor(0xFFFF); g.drawString(Naming_ ? "New folder:" : "File name:", X + 16, NameY() + 6);
            DrawField(g, Naming_ ? NewName : Name, NameX(), NameY(), NameW(), 26, Focus == F_NAME || Focus == F_NEW, false, ticks);
        }
        if (Confirm) {
            g.setTextColor(0xFFE0); g.drawString((Name.text + " already exists. Replace it?   (Y / N)").c_str(), X + 16, BtnY() + 4);
            SuperPushButton no(G, CancelX(), BtnY(), 100, &lgfx::fonts::Font2, "No"); SuperPushButton yes(G, OkX(), BtnY(), 100, &lgfx::fonts::Font2, "Replace");
            yes.ColBorder = COL_ACCENT; no.Push(); yes.Push();
        } else {
            SuperPushButton cancel(G, CancelX(), BtnY(), 100, &lgfx::fonts::Font2, "Cancel");
            SuperPushButton ok(G, OkX(), BtnY(), 100, &lgfx::fonts::Font2, Naming_ ? "Create" : K == K_OPEN ? "Open" : K == K_SAVE ? "Save" : "Choose");
            ok.ColBorder = COL_ACCENT; cancel.Push(); ok.Push();
        }
    }

private:
    enum Type { T_UP, T_DIR, T_FILE, T_NONE };
    struct Row { std::string name, label; Type type; };
    enum FocusId { F_LIST, F_NAME, F_PATH, F_NEW } Focus = F_LIST;
    enum Drag { D_NONE, D_TEXT, D_LIST } Drag = D_NONE;
    lgfx::LGFX_Sprite* G; Kind K; std::string Title, Ext;
    std::unique_ptr<SuperListBox> List;
    std::vector<Row> Rows;
    std::filesystem::path Cur; std::string CurStr;
    TextField Path, Name, NewName;
    std::string Err, NoConfirm; bool Confirm = false, Naming_ = false, Started = false; int BtnH = 20, LastCursor = -1, PressX = 0, PressY = 0;

    bool WindowsDrives() const {
#ifdef _WIN32
        return true;
#else
        return false;
#endif
    }
    int NavCount() const { return WindowsDrives() ? 4 : 3; }
    TextField* Cur_() { return Focus == F_PATH ? &Path : Focus == F_NAME ? &Name : Focus == F_NEW ? &NewName : nullptr; }
    int CurFieldX() { return Focus == F_PATH ? X + 16 : NameX(); }
    std::function<int(const std::string&)> MeasureFn() { return [this](const std::string& s) { return MeasureText(*G, s); }; }
    void Reveal() { if (TextField* f = Cur_()) f->EnsureVisible(MeasureFn(), (Focus == F_PATH ? W - 32 : NameW()) - 12); }

    static std::string Lower(std::string s) { for (char& c : s) c = (char)tolower((unsigned char)c); return s; }
    bool MatchesExt(const std::string& name) const { return Ext.empty() || Lower(std::filesystem::path(name).extension().string()) == Lower(Ext); }

    // Shows a folder (an empty path = the list of drives, Windows only). A folder that cannot be read leaves the dialog where it was.
    void Load(std::filesystem::path dir) {
        namespace fs = std::filesystem; std::error_code ec;
        std::vector<Row> rows;
        if (!dir.empty()) {
            dir = fs::absolute(dir, ec).lexically_normal();
            if (!dir.has_filename() && dir != dir.root_path()) dir = dir.parent_path();         // "/a/b/" -> "/a/b"
            std::vector<Row> dirs, files;
            for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
                std::string n = it->path().filename().string();
                if (n.empty() || n[0] == '.') continue;
                std::error_code e2; bool isDir = it->is_directory(e2);
                if (isDir) dirs.push_back({ n, n + "/", T_DIR });
                else if (K != K_FOLDER && MatchesExt(n) && it->is_regular_file(e2)) files.push_back({ n, n, T_FILE });
            }
            if (ec) { Err = "cannot read this folder"; return; }
            auto less = [](const Row& a, const Row& b) { return Lower(a.name) < Lower(b.name); };
            std::sort(dirs.begin(), dirs.end(), less); std::sort(files.begin(), files.end(), less);
            if (dir != dir.root_path() || WindowsDrives()) rows.push_back({ "..", "..", T_UP });
            rows.insert(rows.end(), dirs.begin(), dirs.end()); rows.insert(rows.end(), files.begin(), files.end());
        } else {
#ifdef _WIN32
            for (char c = 'A'; c <= 'Z'; ++c) { std::string d = std::string(1, c) + ":\\"; if (fs::exists(d, ec)) rows.push_back({ d, d, T_DIR }); }
#endif
        }
        if (rows.empty()) rows.push_back({ "", "(empty folder)", T_NONE });
        Cur = dir; CurStr = dir.string(); Rows = rows; Err.clear(); Confirm = false;
        Path.SetText(CurStr.empty() ? "(drives)" : CurStr);
        std::vector<std::string> items; for (const Row& r : Rows) items.push_back(r.label);
        List->SetItems(items); List->SetSelected(0); List->StartEdit(); LastCursor = 0;
    }
    void GoUp() {
        if (Cur.empty()) return;
        std::filesystem::path p = Cur.parent_path();
        if (p == Cur) { if (WindowsDrives()) Load(std::filesystem::path()); return; }          // at a root: Windows shows the drives, elsewhere stay
        Load(p);
    }
    void GoHome() {
        const char* h = getenv("HOME"); if (!h || !*h) h = getenv("USERPROFILE");
        std::error_code ec; if (h && std::filesystem::is_directory(h, ec)) Load(h);
    }

    void Pump() {                                             // run the list box (a lift "accepts" a row, so keep editing) and follow its cursor
        SuperEditState s = List->Service();
        if (s == EDIT_ACCEPTED || s == EDIT_CANCELLED) List->StartEdit();
        int c = List->GetCursor();
        if (c != LastCursor) {
            LastCursor = c; Err.clear();
            if (c >= 0 && c < (int)Rows.size() && Rows[c].type == T_FILE && K != K_FOLDER) { Name.SetText(Rows[c].name); Name.SelectAll(); }
        }
    }
    void Pointer(SuperPointerEvent::Kind k, int x, int y) {
        SuperPointerEvent p; p.Type = k; p.X = (int16_t)x; p.Y = (int16_t)y; p.StartX = (int16_t)PressX; p.StartY = (int16_t)PressY;
        List->OnPointer(p); Pump();
    }
    void Activate(int c) {
        if (c < 0 || c >= (int)Rows.size()) return;
        const Row& r = Rows[c];
        switch (r.type) {
        case T_UP: GoUp(); break;
        case T_DIR: Load(Cur / r.name); break;
        case T_FILE: if (K != K_FOLDER) { Name.SetText(r.name); Accept(); } break;
        default: break;
        }
    }
    static bool BadName(const std::string& n) {
        if (n.empty() || n == "." || n == "..") return true;
        for (char c : n) if (strchr("/\\:*?\"<>|", c) || (unsigned char)c < 0x20) return true;
        return false;
    }
    void Accept() {                                           // OK pressed (or Enter in the name field)
        namespace fs = std::filesystem; std::error_code ec;
        if (K == K_FOLDER) { if (Cur.empty() || !fs::is_directory(Cur, ec)) { Err = "choose a folder (not the drive list)"; return; } path = CurStr; state = OK; return; }
        if (Cur.empty()) { Err = "open a folder first"; return; }
        std::string n = Name.text;
        if (BadName(n)) { Err = n.empty() ? "type a file name" : "that is not a valid file name"; return; }
        if (K == K_SAVE && !Ext.empty() && Lower(fs::path(n).extension().string()) != Lower(Ext)) n += Ext;
        fs::path p = Cur / n;
        if (K == K_OPEN) { if (!fs::is_regular_file(p, ec)) { Err = "no such file: " + n; return; } path = p.string(); state = OK; return; }
        Name.SetText(n);
        if (fs::exists(p, ec)) { if (fs::is_directory(p, ec)) { Err = n + " is a folder"; return; } if (p.string() == NoConfirm) { path = p.string(); state = OK; return; } Confirm = true; return; }
        path = p.string(); state = OK;
    }
    void GoToTyped() {                                        // Enter in the path field: a folder opens, a file path shows its folder and name
        namespace fs = std::filesystem; std::error_code ec;
        fs::path p = Path.text;
        if (fs::is_directory(p, ec)) { Load(p); Focus = F_LIST; return; }
        if (K != K_FOLDER && p.has_parent_path() && fs::is_directory(p.parent_path(), ec)) { Load(p.parent_path()); Name.SetText(p.filename().string()); Focus = F_NAME; return; }
        Err = "no such folder: " + Path.text;
    }
    void MakeFolder() {
        namespace fs = std::filesystem; std::error_code ec;
        std::string n = NewName.text;
        if (BadName(n)) { Err = n.empty() ? "type a folder name" : "that is not a valid folder name"; return; }
        fs::path p = Cur / n;
        if (fs::exists(p, ec)) { Err = n + " already exists"; return; }
        if (!fs::create_directory(p, ec)) { Err = "cannot create the folder"; return; }
        Naming_ = false; Focus = F_LIST; Load(p);
    }
    void StartNaming() { if (Cur.empty()) return; Naming_ = true; NewName.SetText(""); Focus = F_NEW; Err.clear(); }

    void Down(const InputEvent& e) {
        if (Confirm) {
            if (e.y >= BtnY() && e.y < BtnY() + BtnH) { if (e.x >= OkX() && e.x < OkX() + 100) { path = (Cur / Name.text).string(); state = OK; } else if (e.x >= CancelX() && e.x < CancelX() + 100) Confirm = false; }
            return;
        }
        auto in = [&](int x, int y, int w, int h) { return e.x >= x && e.x < x + w && e.y >= y && e.y < y + h; };
        TextField* hit = nullptr; FocusId hf = F_LIST;
        if (in(X + 16, PathY(), W - 32, 26)) { hit = &Path; hf = F_PATH; }
        else if (K != K_FOLDER && !Naming_ && in(NameX(), NameY(), NameW(), 26)) { hit = &Name; hf = F_NAME; }
        else if (Naming_ && in(NameX(), NameY(), NameW(), 26)) { hit = &NewName; hf = F_NEW; }
        if (hit) {
            Focus = hf; Drag = D_TEXT; Err.clear();
            hit->SetCaretFromX(e.x - CurFieldX() - 6, (SDL_GetModState() & KMOD_SHIFT) != 0, MeasureFn());
            if (e.clicks == 2) hit->SelectWord(hit->caret); else if (e.clicks >= 3) hit->SelectAll();
            Reveal(); return;
        }
        for (int i = 0; i < NavCount(); ++i) if (in(NavX(i), BtnRowY(), NavW(i), BtnH)) {
            Err.clear();
            if (i == 0) GoUp(); else if (i == 1) GoHome(); else if (i == 2) StartNaming(); else Load(std::filesystem::path());
            return;
        }
        if (List->RowAt(e.x, e.y) >= 0) {
            Focus = F_LIST; Drag = D_LIST; Started = false; PressX = e.x; PressY = e.y;
            Pointer(SuperPointerEvent::PRESS, e.x, e.y);
            if (e.clicks >= 2) { Drag = D_NONE; Activate(List->GetCursor()); }
            return;
        }
        if (in(CancelX(), BtnY(), 100, BtnH)) { if (Naming_) { Naming_ = false; Focus = F_LIST; } else state = CANCEL; return; }
        if (in(OkX(), BtnY(), 100, BtnH)) { if (Naming_) MakeFolder(); else Accept(); return; }
    }
    void Key(const InputEvent& e) {
        if (Confirm) {
            if (e.key == SDLK_y || e.key == SDLK_RETURN || e.key == SDLK_KP_ENTER) { path = (Cur / Name.text).string(); state = OK; }
            else if (e.key == SDLK_n || e.key == SDLK_ESCAPE) Confirm = false;
            return;
        }
        bool shift = e.mod & KMOD_SHIFT;
        if (e.key == SDLK_ESCAPE) { if (Naming_) { Naming_ = false; Focus = F_LIST; Err.clear(); } else state = CANCEL; return; }
        if (e.key == SDLK_TAB) {                              // list -> name (or new folder) -> path -> list
            FocusId order[3] = { F_LIST, Naming_ ? F_NEW : F_NAME, F_PATH };
            int n = Focus == F_LIST ? 0 : (Focus == F_PATH ? 2 : 1), count = (K == K_FOLDER && !Naming_) ? 2 : 3;
            if (K == K_FOLDER && !Naming_) { order[1] = F_PATH; n = Focus == F_LIST ? 0 : 1; }
            n = (n + (shift ? count - 1 : 1)) % count; Focus = order[n]; Err.clear();
            if (TextField* f = Cur_()) f->SelectAll();
            return;
        }
        if (Focus == F_LIST) {
            SuperEditIO* io = List->EditIO();
            switch (e.key) {
            case SDLK_UP:       io->InjectDelta(-1); Pump(); break;
            case SDLK_DOWN:     io->InjectDelta(1); Pump(); break;
            case SDLK_PAGEUP:   io->InjectDelta(-(ROWS - 1)); Pump(); break;
            case SDLK_PAGEDOWN: io->InjectDelta(ROWS - 1); Pump(); break;
            case SDLK_HOME:     io->InjectDelta(-100000); Pump(); break;
            case SDLK_END:      io->InjectDelta(100000); Pump(); break;
            case SDLK_BACKSPACE: GoUp(); break;
            case SDLK_RETURN: case SDLK_KP_ENTER: Activate(List->GetCursor()); break;
            default: break;
            }
            return;
        }
        TextField* f = Cur_();
        if (f && f->Key(e)) { Err.clear(); Reveal(); return; }
        if (e.key == SDLK_RETURN || e.key == SDLK_KP_ENTER) {
            if (Focus == F_PATH) GoToTyped(); else if (Focus == F_NEW) MakeFolder(); else Accept();
        }
    }
};
}
