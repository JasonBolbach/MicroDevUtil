// Desktop test of SuperRichCore / SuperRichText (and the rich modes of SuperLabel and SuperMessageBox). Not part of the library build.
// Build it like examples/SDLDesktopDemo (same CMake: LovyanGFX + SDL2, include paths src, extras/desktop, extras/desktop/compat) with this file as the only .cpp
// next to sdl_main.cpp. Run with  E2E_OUT=/tmp/rich.raw ./RichTextTest  : prints "RICHTEXT PASSED" and writes two RGB565 snapshots (rendered / source).
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include "SuperRichText.h"
#include "SuperLabel.h"
#include "SuperMessageBox.h"
static LGFX* lcd; static int fails = 0, checks = 0;
#define CHECK(c, msg) do { ++checks; if (!(c)) { ++fails; printf("FAIL  %s\n", msg); } } while (0)
using namespace SuperRich;
static std::string Flat(const LineOut& lo) { std::string s; for (const Atom& a : lo.Atoms) s += a.T; return s; }
static std::vector<std::string> Lines(lgfx::LGFXBase& g, const std::string& t, int w, bool interpret = true) {
    Options o; o.Interpret = interpret; o.TabPx = 32; std::vector<Line> L; Layout(g, t.c_str(), t.size(), w, o, L);
    std::vector<std::string> r; for (const Line& l : L) { Style st = l.St; LineOut lo; ScanLine(g, t.c_str(), t.size(), l.Start, st, l.Cont, o, w, lo); r.push_back(Flat(lo)); } return r; }
static void snap(lgfx::LGFX_Sprite& s, const char* path) { FILE* f = fopen(path, "wb"); fwrite(s.getBuffer(), 2, s.width() * s.height(), f); fclose(f); }
void setup() {
    lcd = new LGFX(320, 240, 1); lcd->init();
    lgfx::LGFX_Sprite g(lcd); g.setColorDepth(16); g.createSprite(320, 240); g.setFont(&lgfx::fonts::Font2);
    typedef std::vector<std::string> V;
    CHECK(Lines(g, "hello", 200) == V({ "hello" }), "plain word");
    CHECK(Lines(g, "a\nb<br>c", 200) == V({ "a", "b", "c" }), "newline and <br> break lines");
    CHECK(Lines(g, "a\n", 200) == V({ "a" }), "a trailing newline adds no empty line");
    CHECK(Lines(g, "a\n\nb", 200) == V({ "a", "", "b" }), "a blank line");
    CHECK(Lines(g, "", 200) == V({ "" }), "empty text is one empty line");
    CHECK(Lines(g, "&lt;b&gt; &amp; &quot;x&quot; &#65;&#x42;&nbsp;", 300) == V({ "<b> & \"x\" AB " }), "entities");
    CHECK(Lines(g, "a<foo>b</foo>c", 200) == V({ "abc" }), "unknown tags are ignored");
    CHECK(Lines(g, "a < b > c", 200) == V({ "a < b > c" }), "a lone < and > are literal");
    CHECK(Lines(g, "a <b", 200) == V({ "a <b" }), "an unterminated tag is shown as text");
    CHECK(Lines(g, "x <color=red>red</color> y", 200) == V({ "x red y" }), "colour tags take no space");
    CHECK(Lines(g, "<COLOR=#F00>R</Color>", 200) == V({ "R" }), "tags are case-insensitive");
    CHECK(Lines(g, "<color=red>a", 200, false) == V({ "<color=red>a" }), "source mode shows tags");
    { V l = Lines(g, "one two three four five six seven eight nine ten", 100); bool ok = l.size() > 2; for (auto& s : l) { ok = ok && g.textWidth(s.c_str()) <= 100 && (s.empty() || s[0] != ' ' && s.back() != ' '); } CHECK(ok, "words wrap inside the width, no leading / trailing spaces"); }
    { V l = Lines(g, "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz", 80); std::string j; for (auto& s : l) j += s; CHECK(l.size() > 1 && j == "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz", "a word wider than the width is broken inside, nothing lost"); }
    { Options o; Style st = o.Def; LineOut lo; std::string t = "a<tab>b"; ScanLine(g, t.c_str(), t.size(), 0, st, false, o, 300, lo); CHECK(lo.Atoms.size() >= 2 && lo.Atoms.back().X == 32, "tab jumps to the next tab stop"); }
    { Options o; std::vector<Line> L; std::string t = "a<hr>b"; Layout(g, t.c_str(), t.size(), 100, o, L); CHECK(L.size() == 3 && L[1].Rule && L[1].H >= 4, "<hr> is a line of its own"); }
    { Options o; std::vector<Line> L; std::string t = "<big>X</big>y"; Layout(g, t.c_str(), t.size(), 200, o, L); g.setTextSize(1.0f); CHECK(L.size() == 1 && L[0].H == 2 * (int)g.fontHeight(), "<big> doubles the line height"); }
    uint16_t c;
    CHECK(ParseColor("#F00", c) && c == 0xF800 && ParseColor("red", c) && c == 0xF800 && ParseColor("#00FF00", c) && c == 0x07E0 && ParseColor("\"blue\"", c) && c == 0x001F && !ParseColor("nope", c) && !ParseColor("#12", c), "colours");
    // drawing: colour, background, alignment
    auto count = [&](lgfx::LGFX_Sprite& s, uint16_t col, int x0, int x1) { int n = 0; for (int y = 0; y < s.height(); ++y) for (int x = x0; x < x1; ++x) if (s.readPixel(x, y) == col) ++n; return n; };
    auto firstX = [&](lgfx::LGFX_Sprite& s, uint16_t col) { for (int x = 0; x < s.width(); ++x) for (int y = 0; y < s.height(); ++y) if (s.readPixel(x, y) == col) return x; return -1; };
    auto lastX = [&](lgfx::LGFX_Sprite& s, uint16_t col) { for (int x = s.width() - 1; x >= 0; --x) for (int y = 0; y < s.height(); ++y) if (s.readPixel(x, y) == col) return x; return -1; };
    {
        lgfx::LGFX_Sprite s(lcd); s.setColorDepth(16); s.createSprite(200, 40); s.setFont(&lgfx::fonts::Font2);
        auto draw = [&](const std::string& t, int w = 200) { s.fillSprite(0); Options o; o.Def.Fg = 0xFFFF; std::vector<Line> L; Layout(s, t.c_str(), t.size(), w, o, L); DrawLine(s, t.c_str(), t.size(), L[0], o, 0, 0, w); };
        draw("<color=red>XX</color>"); CHECK(count(s, 0xF800, 0, 200) > 20 && count(s, 0xFFFF, 0, 200) == 0, "red text is drawn red");
        draw("XX"); int lw = firstX(s, 0xFFFF); CHECK(lw >= 0 && lw < 3, "left aligned text starts at the left");
        draw("<center>XX</center>"); int cx = firstX(s, 0xFFFF), cr = lastX(s, 0xFFFF); CHECK(cx > 60 && cr < 140 && abs((cx + cr) / 2 - 100) < 4, "centered text is centered");
        draw("<right>XX"); CHECK(lastX(s, 0xFFFF) >= 196, "right aligned text ends at the right");
        draw("<bg=blue>XX</bg>"); CHECK(count(s, 0x001F, 0, 200) > 100, "background colour behind the text");
        draw("<u>XX</u>"); { int under = 0; for (int x = 0; x < 200; ++x) if (s.readPixel(x, (int)s.fontHeight() - 1) == 0xFFFF) ++under; CHECK(under >= 8, "underline is a line under the text"); }
        { auto glyphH = [&]() { int top = 99, bot = -1; for (int y = 0; y < 40; ++y) for (int x = 0; x < 200; ++x) if (s.readPixel(x, y) == 0xFFFF) { top = std::min(top, y); bot = std::max(bot, y); } return bot - top + 1; };
          draw("X"); int h1 = glyphH(); draw("<big>X</big>"); int h2 = glyphH(); CHECK(h1 > 4 && h2 >= 2 * h1 - 1 && h2 <= 2 * h1 + 1, "big text is twice as tall as normal text"); }
    }
    // the control: scrolling, source mode, memory
    {
        SuperEncoder enc(2, 3); SuperButton a(4, true), b(5, true);
        SuperRichText rt(&g, 0, 0, 200, 80, &lgfx::fonts::Font2, &enc, &a, &b);
        std::string t; for (int i = 1; i <= 30; ++i) t += "Line " + std::to_string(i) + "\n";
        rt.SetText(t); CHECK(rt.GetLineCount() == 30 && rt.NeedsScroll() && rt.GetFirstLine() == 0, "30 lines, needs scrolling");
        rt.ScrollBy(5); CHECK(rt.GetFirstLine() == 5, "scroll by 5"); rt.ScrollTo(1000); int last = rt.GetFirstLine(); CHECK(last > 20 && last < 30, "the last page is full");
        rt.ScrollTo(-3); CHECK(rt.GetFirstLine() == 0, "scroll clamps at the top");
        rt.StartEdit(); rt.EditIO()->InjectDelta(3); CHECK(rt.Service() == EDIT_ACTIVE && rt.GetFirstLine() == 3, "the encoder scrolls");
        rt.EditIO()->Inject(SuperEditIO::B_LONG); rt.Service(); CHECK(rt.GetFirstLine() == 0, "button B long = back to the top");
        rt.EditIO()->Inject(SuperEditIO::A_SHORT); CHECK(rt.Service() == EDIT_ACCEPTED, "button A closes");
        rt.SetTextStatic("short"); CHECK(!rt.NeedsScroll() && rt.GetText() == "short", "static text, no scrolling needed");
        rt.SetText("<color=red>x</color>"); rt.SetShowSource(true); CHECK(rt.GetShowSource() && rt.GetLineCount() == 1, "show source");
        // a demo picture
        rt.SetShowSource(false); rt.SetBodyHeight(150);
        rt.SetTextStatic("<center><color=yellow><big>Rich</big> text</color></center>\n<hr>\nNormal, <color=#0F0>green</color>, <bg=#0000A0><color=white> inverse </color></bg>, <u>underlined</u>, <s>struck</s>.\n"
                         "&lt;b&gt; is ignored: <b>bold?</b> and so is <foo>this</foo>.\nName<tab>Value\nWidth<tab>320\n<right>right aligned</right>\n"
                         "A long line that has to wrap around because it is wider than the control, with <color=cyan>some colour in the middle</color> of it.\nlast line");
        rt.SetBorder(1, 0x7BEF); rt.SetDisplayPos(10, 10); lgfx::LGFX_Sprite scr(lcd); scr.setColorDepth(16); scr.createSprite(240, 180); scr.fillSprite(0x2104);
        SuperRichText rd(&scr, 10, 10, 200, 150, &lgfx::fonts::Font2, &enc, &a, &b); rd.SetBorder(1, 0x7BEF);
        rd.SetTextStatic("<center><color=yellow><big>Rich</big> text</color></center>\n<hr>\nNormal, <color=#0F0>green</color>, <bg=#0000A0><color=white> inverse </color></bg>, <u>underlined</u>, <s>struck</s>.\n"
                         "&lt;b&gt; is ignored: <b>bold?</b> and so is <foo>this</foo>.\nName<tab>Value\nWidth<tab>320\n<right>right aligned</right>\n"
                         "A long line that has to wrap around because it is wider than the control, with <color=cyan>some colour in the middle</color> of it.\nlast line");
        rd.Push(); snap(scr, getenv("E2E_OUT"));
        lgfx::LGFX_Sprite scr2(lcd); scr2.setColorDepth(16); scr2.createSprite(240, 180); scr2.fillSprite(0x2104);
        SuperRichText rs(&scr2, 10, 10, 200, 150, &lgfx::fonts::Font2, &enc, &a, &b); rs.SetBorder(1, 0x7BEF); rs.SetShowSource(true); rs.SetText(rd.GetText()); rs.Push(); snap(scr2, (std::string(getenv("E2E_OUT")) + "2").c_str());
    }
    // rich mode of the label and the message box: opt-in, plain by default
    {
        lgfx::LGFX_Sprite s(lcd); s.setColorDepth(16); s.createSprite(200, 60);
        auto cnt = [&](uint16_t col) { int n = 0; for (int y = 0; y < s.height(); ++y) for (int x = 0; x < s.width(); ++x) if (s.readPixel(x, y) == col) ++n; return n; };
        SuperLabel lb(&s, 0, 0, 200, &lgfx::fonts::Font2, "<color=red>red</color> <center>x"); lb.SetPadding(0); lb.SetBorder(0, 0);
        s.fillSprite(0); lb.Push(); CHECK(cnt(0xF800) == 0 && lb.GetLineCount() == 1 && !lb.GetRich(), "label: plain by default, tags are just characters");
        int plainH = lb.GetHeight();
        lb.SetText("<color=red>red</color> plain<br>second line"); lb.SetRich(true); s.fillSprite(0); lb.Push();
        CHECK(lb.GetRich() && cnt(0xF800) > 10 && lb.GetLineCount() == 2 && lb.GetHeight() == 2 * plainH, "label: SetRich(true) understands tags and <br>");
        lb.SetText("<center>mid</center>"); s.fillSprite(0); lb.Push(); { int minx = 999, maxx = -1; for (int y = 0; y < s.height(); ++y) for (int x = 0; x < s.width(); ++x) if (s.readPixel(x, y) == 0xFFFF) { minx = std::min(minx, x); maxx = std::max(maxx, x); } CHECK(minx > 60 && maxx < 140, "label: rich centre alignment"); }
        lb.SetRich(false); CHECK(lb.GetLineCount() == 1, "label: back to plain");
        SuperMessageBox mb(&s, 160, &lgfx::fonts::Font2, nullptr, nullptr, nullptr); mb.SetMessage("<color=red>Danger</color>\n<u>Sure?</u>"); mb.AddButton("Yes"); mb.SetCentered(false); mb.SetDisplayPos(0, 0);
        int h0 = mb.GetHeight(); mb.SetRich(true); mb.SetRich(false); CHECK(mb.GetHeight() == h0, "message box: plain unless SetRich(true)");
        s.fillSprite(0); mb.StartEdit(); CHECK(cnt(0xF800) == 0, "message box: plain shows the tags as text");
        SuperMessageBox mr(&s, 160, &lgfx::fonts::Font2, nullptr, nullptr, nullptr); mr.SetMessage("<color=red>Danger</color>\n<u>Sure?</u>"); mr.AddButton("Yes"); mr.SetCentered(false); mr.SetDisplayPos(0, 0); mr.SetRich(true);
        s.fillSprite(0); mr.StartEdit(); CHECK(cnt(0xF800) > 10, "message box: SetRich(true) draws the colour");
        CHECK(mr.GetHeight() == h0 || mr.GetHeight() < h0, "message box: the rich message is at most as tall as the plain one that shows the tags");
    }
    printf("%s: %d checks, %d failed\n", fails ? "RICHTEXT FAILED" : "RICHTEXT PASSED", checks, fails); fflush(stdout); exit(fails ? 1 : 0);
}
void loop() {}
