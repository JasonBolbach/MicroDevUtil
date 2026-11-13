#pragma once
// Application-level settings of the Form Designer (not part of any form): kept in the user's configuration folder, so they survive between sessions
// and between projects.
//   macOS    ~/Library/Application Support/SuperGUI FormDesigner/settings.json
//   Linux    $XDG_CONFIG_HOME or ~/.config /SuperGUI-FormDesigner/settings.json
//   Windows  %APPDATA%\SuperGUI FormDesigner\settings.json
// FD_SETTINGS=/path/file.json overrides the location (the self-test uses it so it never touches the real file).
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include "FormModel.h"

namespace FD {
struct AppSettings {
    std::string lastOpenDir;        // the folder the Open dialog was showing when it was closed last
    std::string lastSaveDir;        // the same for Save (used for a form that has no file yet)

    static std::string File() {
        if (const char* o = getenv("FD_SETTINGS")) if (*o) return o;
        namespace fs = std::filesystem;
#ifdef _WIN32
        const char* b = getenv("APPDATA"); fs::path d = fs::path(b ? b : ".") / "SuperGUI FormDesigner";
#elif defined(__APPLE__)
        const char* h = getenv("HOME"); fs::path d = fs::path(h ? h : ".") / "Library" / "Application Support" / "SuperGUI FormDesigner";
#else
        const char* x = getenv("XDG_CONFIG_HOME"); const char* h = getenv("HOME");
        fs::path d = (x && *x) ? fs::path(x) / "SuperGUI-FormDesigner" : fs::path(h ? h : ".") / ".config" / "SuperGUI-FormDesigner";
#endif
        return (d / "settings.json").string();
    }
    bool Load() {                                              // a missing or damaged file just means the defaults
        *this = AppSettings();
        std::ifstream in(File(), std::ios::binary); if (!in) return false;
        std::stringstream ss; ss << in.rdbuf(); std::string src = ss.str(), err; JV root; JsonParser p(src);
        if (!p.Parse(root, err) || root.t != JV::OBJ) return false;
        if (const JV* v = root.Get("lastOpenFolder")) if (v->t == JV::STR) lastOpenDir = v->s;
        if (const JV* v = root.Get("lastSaveFolder")) if (v->t == JV::STR) lastSaveDir = v->s;
        return true;
    }
    bool Save() const {
        namespace fs = std::filesystem; std::error_code ec;
        fs::path p = File(); if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
        auto esc = [](const std::string& s) { std::string r; for (char c : s) { if (c == '"' || c == '\\') r += '\\'; r += c; } return r; };
        std::ofstream out(p, std::ios::binary | std::ios::trunc); if (!out) return false;
        out << "{\n  \"lastOpenFolder\": \"" << esc(lastOpenDir) << "\",\n  \"lastSaveFolder\": \"" << esc(lastSaveDir) << "\"\n}\n";
        return (bool)out;
    }
};
}
