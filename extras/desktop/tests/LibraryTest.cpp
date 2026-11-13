// Desktop tests of the newer library pieces: SuperRichCore / SuperRichText (and the rich modes + style fonts of SuperLabel and SuperMessageBox), and the
// KeepHidden flag of SuperControl / SuperForm groups. Not part of the library build.
// Build it like examples/SDLDesktopDemo (same CMake: LovyanGFX + SDL2, include paths src, extras/desktop, extras/desktop/compat) with this file as the only .cpp
// next to sdl_main.cpp. Run with  E2E_OUT=/tmp/rich.raw ./LibraryTest  : prints "LIBRARY TESTS PASSED" and writes two RGB565 snapshots (rendered / source).
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include "SuperRichText.h"
#include "SuperLabel.h"
#include "SuperMessageBox.h"
#include "SuperForm.h"
#include "SuperProgressBar.h"
#include "SuperListBox.h"
#include "SuperComboBox.h"
#include "SuperMenu.h"
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
    // unclickable rows ("~" line, "~Text" heading, "~~Text" escape) in the list box, combo box and menu
    {
        lgfx::LGFX_Sprite s(lcd); s.setColorDepth(16); s.createSprite(200, 200);
        SuperEncoder enc(2, 3); SuperButton ba(4, true), bb(5, true);
        SuperListBox lb(&s, 0, 0, 160, &lgfx::fonts::Font2, 8, &enc, &ba, &bb);
        lb.SetItems({ "~Fruit", "Apple", "~", "Pear", "~~Tilde", "~Veg", "Leek" });
        CHECK(lb.GetSelected() == 1 && lb.GetSelectedText() == "Apple", "separators: the selection is the first row that can be chosen (not a leading heading)");
        lb.SetSelected(0); CHECK(lb.GetSelected() == 1, "separators: SetSelected on a heading moves to the next real item");
        lb.SetSelected(2); CHECK(lb.GetSelected() == 3, "separators: ... also on a separator line");
        lb.SetSelected(5); CHECK(lb.GetSelected() == 6, "separators: ... and on the last heading before an item");
        CHECK(lb.IsSelectable(1) && !lb.IsSelectable(0) && !lb.IsSelectable(2) && lb.IsSelectable(4) && !lb.IsSelectable(5), "separators: ~Text and ~ are not selectable, ~~Text is");
        lb.SetSelected(1); lb.StartEdit(); lb.EditIO()->InjectDelta(1); lb.Service(); CHECK(lb.GetCursor() == 3, "separators: the encoder skips the separator line");
        lb.EditIO()->InjectDelta(1); lb.Service(); CHECK(lb.GetCursor() == 4, "separators: ... then the escaped ~~Tilde item");
        lb.EditIO()->InjectDelta(1); lb.Service(); CHECK(lb.GetCursor() == 6, "separators: ... and the heading before Leek");
        lb.EditIO()->InjectDelta(1); lb.Service(); CHECK(lb.GetCursor() == 6, "separators: the last item stops the cursor (no wrap)");
        lb.EditIO()->InjectDelta(-1); lb.EditIO()->InjectDelta(-1); lb.Service(); lb.Service(); CHECK(lb.GetCursor() <= 4, "separators: moving back skips them too");
        lb.EditIO()->Inject(SuperEditIO::A_SHORT); SuperEditState st = lb.Service(); CHECK(st == EDIT_ACCEPTED && lb.GetSelectedText() != "" && lb.IsSelectable(lb.GetSelected()), "separators: accepting always gives a real item");
        lb.Wrap = true; lb.SetSelected(6); lb.StartEdit(); lb.EditIO()->InjectDelta(1); lb.Service(); CHECK(lb.GetCursor() == 1, "separators: with Wrap the cursor wraps past the leading heading to the first item"); lb.EditIO()->Inject(SuperEditIO::B_SHORT); lb.Service(); lb.Wrap = false;
        // touch: a tap on a separator row does nothing
        lb.SetSelected(1); lb.StartEdit(); s.setFont(&lgfx::fonts::Font2); { int rh = (int)s.fontHeight() + 2, y0 = lb.GetBoxTop() + lb.GetBorder() + lb.GetPadding(); SuperPointerEvent e; e.Type = SuperPointerEvent::TAP; e.X = 20; e.Y = (int16_t)(y0 + 2 * rh + rh / 2); lb.OnPointer(e); st = lb.Service(); CHECK(st == EDIT_ACTIVE && lb.GetCursor() == 1, "separators: a tap on a separator row does not choose it or move the cursor");
          e.Y = (int16_t)(y0 + 3 * rh + rh / 2); lb.OnPointer(e); st = lb.Service(); CHECK(st == EDIT_ACCEPTED && lb.GetSelected() == 3, "separators: a tap on a real row still chooses it"); }
        // drawing: a line is drawn for "~", the heading is dim (ColArrow), nothing is highlighted on them
        lb.SetItems({ "A", "~", "B" }); s.fillSprite(0); lb.Push(); s.setFont(&lgfx::fonts::Font2); { int rh = (int)s.fontHeight() + 2, y0 = lb.GetBoxTop() + lb.GetBorder() + lb.GetPadding(); int ly = y0 + rh + rh / 2; int n = 0; for (int x = 0; x < 160; ++x) if (s.readPixel(x, ly) == lb.ColArrow) ++n; CHECK(n > 100, "separators: a separator line is drawn across its row"); }
        SuperListBox only(&s, 0, 0, 100, &lgfx::fonts::Font2, 3, &enc, &ba, &bb); only.SetItems({ "~A", "~" }); CHECK(only.GetSelected() == -1 && !only.StartEdit(), "separators: a list with nothing but separators and headings cannot be edited");
        // headings are centred in a line: ---- Text ----, too long ones are cut with room for a character plus 3 pixels of line on each side
        { SuperListBox hl(&s, 0, 0, 160, &lgfx::fonts::Font2, 4, &enc, &ba, &bb); hl.SetBorder(0, 0); hl.SetPadding(0); hl.SetItems({ "~Weapon", "Arm", "~This heading is far too long to fit in the row", "Fire" });
          s.fillSprite(0); hl.Push(); s.setFont(&lgfx::fonts::Font2); int rh = (int)s.fontHeight() + 2, my = rh / 2; uint16_t col = hl.ColArrow;
          int firstLine = -1, lastLine = -1; for (int x = 0; x < 160; ++x) if (s.readPixel(x, my) == col) { if (firstLine < 0) firstLine = x; lastLine = x; }
          int tl = 999, tr = -1; for (int y = 0; y < rh; ++y) for (int x = 8; x < 152; ++x) if (s.readPixel(x, y) == col && y != my) { tl = std::min(tl, x); tr = std::max(tr, x); }
          CHECK(firstLine >= 0 && firstLine < 6 && lastLine > 153, "separators: a heading has line on both sides, out to the edges");
          int tw = (int)s.textWidth("Weapon"); CHECK(std::abs((tl + tr) / 2 - 80) <= 3 && tr - tl + 1 <= tw + 2, "separators: the heading text is centred in the row");
          int gapL = 0; for (int x = tl - 1; x >= 0 && s.readPixel(x, my) != col; --x) ++gapL; int lineL = 0; for (int x = tl - 1 - gapL; x >= 0 && s.readPixel(x, my) == col; --x) ++lineL; CHECK(lineL >= 8, "separators: the line left of the text is at least a character wide");
          CHECK(SuperSep::Fit(s, "~Weapon", 160) == "Weapon" && SuperSep::Fit(s, "~~Tilde", 160) == "~Tilde", "separators: Fit keeps a short heading (and the escape text)");
          std::string cut = SuperSep::Fit(s, "~This heading is far too long to fit in the row", 160); int cw = (int)s.textWidth("0");
          CHECK(!cut.empty() && cut.size() < 40 && (int)s.textWidth(cut.c_str()) <= 160 - 2 * (cw + 3) && cut == std::string("This heading is far too long to fit in the row").substr(0, cut.size()), "separators: a too long heading is truncated so that a character + 3 pixels of line remain on each side");
          int ly = 2 * rh + rh / 2; int ln = 0; for (int x = 0; x < 160; ++x) if (s.readPixel(x, ly) == col) ++ln; CHECK(ln >= 2 * (cw + 3 - 3), "separators: the line is still drawn on both sides of a cut heading");
        }
        // combo box: same rules through its popup list
        SuperComboBox cb(&s, 0, 100, 120, &lgfx::fonts::Font2, &enc, &ba, &bb); cb.SetItems({ "~Pick one", "One", "~", "Two" }); CHECK(cb.GetSelected() == 1 && cb.GetValue() == "One", "separators: a combo box starts on the first real item");
        // menu
        SuperMenu m(&s, 0, 0, 160, &lgfx::fonts::Font2, 6, &enc, &ba, &bb);
        m.AddItem("~Main", 0); m.AddItem("Alpha", 1); m.AddItem("~", 0); auto* sub = m.AddItem("More", 0); m.AddItem("~Inside", 0, sub); m.AddItem("Beta", 2, sub); m.AddItem("~~Tilde", 3);
        m.StartEdit(); CHECK(m.IsEditing(), "separators: a menu that starts with a heading can be opened");
        m.EditIO()->Inject(SuperEditIO::A_SHORT); st = m.Service(); CHECK(st == EDIT_ACCEPTED && m.GetResultId() == 1, "separators: the cursor starts on Alpha, not on the heading (A chooses it)");
        m.StartEdit(); m.EditIO()->InjectDelta(1); m.Service(); m.EditIO()->Inject(SuperEditIO::A_SHORT); st = m.Service(); CHECK(st == EDIT_ACTIVE && m.GetCurrentLevel() == sub, "separators: the encoder skips the separator line and enters the sub-menu");
        m.EditIO()->InjectDelta(1); m.Service(); m.EditIO()->Inject(SuperEditIO::A_SHORT); st = m.Service(); CHECK(st == EDIT_ACCEPTED && m.GetResultId() == 2, "separators: inside the sub-menu the cursor skips the heading and reaches Beta");
        m.StartEdit(); m.EditIO()->InjectDelta(1); m.Service(); m.EditIO()->InjectDelta(1); m.Service(); m.EditIO()->Inject(SuperEditIO::A_SHORT); st = m.Service(); CHECK(st == EDIT_ACCEPTED && m.GetResultId() == 3, "separators: ~~Tilde is an ordinary item");
        { m.ShowLevel(sub); CHECK(m.GetCurrentLevel() == sub, "menu level preview: ShowLevel shows a sub-menu while idle"); s.fillSprite(0); m.Push(); int px = 0; for (int y = 0; y < 40; ++y) for (int x = 0; x < 160; ++x) if (s.readPixel(x, y)) ++px; CHECK(px > 100, "menu level preview: and draws it");
          m.ShowLevel(nullptr); CHECK(m.GetCurrentLevel() == nullptr, "menu level preview: nullptr is the top level again"); m.ShowLevel(sub); m.StartEdit(); CHECK(m.GetCurrentLevel() == nullptr, "menu level preview: StartEdit always begins at the top level"); m.EditIO()->Inject(SuperEditIO::B_LONG); m.Service(); }
        SuperMenu mo(&s, 0, 0, 100, &lgfx::fonts::Font2, 3, &enc, &ba, &bb); mo.AddItem("~Head", 0); mo.AddItem("~", 0); CHECK(!mo.StartEdit(), "separators: a menu with nothing to choose cannot be opened");
    }
    // SuperProgressBar: the percentage follows the text colour, can be aligned and hidden
    {
        lgfx::LGFX_Sprite s(lcd); s.setColorDepth(16); s.createSprite(200, 30);
        SuperProgressBar pb(&s, 0, 0, 200, &lgfx::fonts::Font2); pb.SetBorder(0, 0); pb.SetPadding(0); pb.ColBar = 0xF800; pb.ColFg = 0x001F; pb.SetValue(60);
        auto cnt = [&](uint16_t col, int x0, int x1) { int n = 0; for (int y = 0; y < 30; ++y) for (int x = x0; x < x1; ++x) if (s.readPixel(x, y) == col) ++n; return n; };
        s.fillSprite(0); pb.Push(); CHECK(cnt(0x001F, 0, 120) > 5, "progress bar: the percentage on the bar is drawn in the text colour (the right-aligned test below covers the part off the bar)");
        pb.SetPercentAlign(SuperControl::ALIGN_LEFT); s.fillSprite(0); pb.Push(); int xl = 999; for (int y = 0; y < 30; ++y) for (int x = 0; x < 200; ++x) if (s.readPixel(x, y) == 0x001F) xl = std::min(xl, x); CHECK(xl < 8, "progress bar: SetPercentAlign(ALIGN_LEFT)");
        pb.SetPercentAlign(SuperControl::ALIGN_RIGHT); s.fillSprite(0); pb.Push(); int xr = -1; for (int y = 0; y < 30; ++y) for (int x = 0; x < 200; ++x) if (s.readPixel(x, y) == 0x001F) xr = std::max(xr, x); CHECK(xr > 190, "progress bar: SetPercentAlign(ALIGN_RIGHT)");
        pb.ShowPercent = false; s.fillSprite(0); pb.Push(); CHECK(cnt(0x001F, 0, 200) == 0 && cnt(0xF800, 0, 200) > 300, "progress bar: ShowPercent = false keeps only the bar");
    }
    // KeepHidden: a group show leaves a marked control hidden
    {
        lgfx::LGFX_Sprite s(lcd); s.setColorDepth(16); s.createSprite(200, 100);
        SuperEncoder enc(2, 3); SuperButton ba(4, true);
        SuperLabel a(&s, 0, 0, 80, &lgfx::fonts::Font2, "a"), b(&s, 0, 20, 80, &lgfx::fonts::Font2, "b"), c(&s, 0, 40, 80, &lgfx::fonts::Font2, "c");
        a.SetGroup(1); b.SetGroup(1); c.SetGroup(2); SuperForm form(&enc, &ba); form.Add(&a, false); form.Add(&b, false); form.Add(&c, false);
        b.SetVisible(false, false); b.SetKeepHidden(true);
        form.SetGroupVisible(1, false, false); form.SetGroupVisible(1, true, false);
        CHECK(a.IsVisible() && !b.IsVisible() && b.GetKeepHidden(), "keep hidden: showing the group shows its controls except the marked one");
        const uint16_t pages[2] = { 1, 2 }; form.ShowPage(pages, 2, 2, false); form.ShowPage(pages, 2, 1, false);
        CHECK(a.IsVisible() && !b.IsVisible() && !c.IsVisible(), "keep hidden: ShowPage leaves the marked control hidden too");
        b.SetVisible(true, false); CHECK(b.IsVisible() && !b.GetKeepHidden(), "keep hidden: a direct SetVisible(true) shows the control and clears the flag");
        form.SetGroupVisible(1, false, false); form.SetGroupVisible(1, true, false); CHECK(b.IsVisible(), "keep hidden: afterwards the control is an ordinary group member again");
        SuperLabel d(&s, 0, 60, 80, &lgfx::fonts::Font2, "d"); d.SetGroup(3); d.SetVisible(false, false); form.Add(&d, false); form.SetGroupVisible(3, true, false);
        CHECK(d.IsVisible(), "keep hidden: off by default - a hidden control without the flag is shown by its group (as before)");
    }
    // style fonts: <b> <i> <h1> use registered fonts, are ignored otherwise
    {
        Options o; o.Base = &lgfx::fonts::Font2; Fonts fs; o.StyleFonts = &fs;
        auto firstLine = [&](const std::string& t, LineOut& lo) { g.setFont(&lgfx::fonts::Font2); Style st = o.Def; ScanLine(g, t.c_str(), t.size(), 0, st, false, o, 300, lo); };
        LineOut plain, none; firstLine("ab", plain); firstLine("<b>ab</b>", none);
        CHECK(none.Width == plain.Width && none.H == plain.H, "style fonts: <b> without a registered font changes nothing");
        fs.Slot[FS_BOLD] = &lgfx::fonts::FreeSansBold12pt7b; LineOut bold; firstLine("<b>ab</b>", bold);
        CHECK(bold.Width != plain.Width && bold.H > plain.H && bold.Atoms.size() == 1 && bold.Atoms[0].H == bold.H, "style fonts: <b> uses the bold slot (wider, taller line)");
        LineOut mixed; firstLine("a<b>b</b>c", mixed); CHECK(mixed.Atoms.size() == 3 && mixed.H == bold.H && mixed.Atoms[0].H == plain.H && mixed.Atoms[1].H == bold.H, "style fonts: the line is as tall as its tallest font, each text keeps its own");
        LineOut ital; firstLine("<i>ab</i>", ital); CHECK(ital.Width == plain.Width, "style fonts: <i> with an empty italic slot is ignored");
        LineOut bi; firstLine("<b><i>ab</i></b>", bi); CHECK(bi.Width == bold.Width, "style fonts: bold + italic falls back to the bold slot when there is no bold-italic font");
        fs.Slot[FS_BOLDITALIC] = &lgfx::fonts::FreeSansBoldOblique18pt7b; LineOut bi2; firstLine("<b><i>ab</i></b>", bi2); CHECK(bi2.Width != bold.Width && bi2.H > bold.H, "style fonts: bold + italic uses the bold-italic slot when registered");
        LineOut after; firstLine("<b>ab</b>cd", after); CHECK(after.Atoms.size() == 2 && after.Atoms[1].H == plain.H, "style fonts: </b> returns to the control's font");
        fs.Slot[FS_HEADING] = &lgfx::fonts::FreeSansBold24pt7b; LineOut head; firstLine("<h1>ab</h1>", head); CHECK(head.H > bold.H, "style fonts: <h1> uses the heading slot");
        LineOut big; firstLine("<big><b>a</b></big>", big); CHECK(big.H == 2 * bold.Atoms[0].H, "style fonts: <big> scales whatever font is in use");
        lgfx::LGFX_Sprite s(lcd); s.setColorDepth(16); s.createSprite(200, 60); s.setFont(&lgfx::fonts::Font2);
        std::string t = "x<b>BOLD</b>"; std::vector<Line> L; Layout(s, t.c_str(), t.size(), 200, o, L); CHECK(L.size() == 1 && L[0].H == bold.H, "style fonts: layout and the line height agree");
        s.fillSprite(0); s.setFont(&lgfx::fonts::Font2); DrawLine(s, t.c_str(), t.size(), L[0], o, 0, 0, 200); int lastx = 0; for (int x = 0; x < 200; ++x) for (int y = 0; y < 60; ++y) if (s.readPixel(x, y)) lastx = x; CHECK(lastx > 20, "style fonts: drawing with style fonts puts text on the sprite");
        { Options ob = o; ob.Base = &lgfx::fonts::FreeSans9pt7b; Fonts fb; fb.Slot[FS_BOLD] = &lgfx::fonts::FreeSansBold24pt7b; ob.StyleFonts = &fb; s.setFont(ob.Base); s.fillSprite(0);
          std::string t2 = "x<b>x</b>"; std::vector<Line> L2; Layout(s, t2.c_str(), t2.size(), 200, ob, L2); DrawLine(s, t2.c_str(), t2.size(), L2[0], ob, 0, 0, 200);
          auto bottom = [&](int x0, int x1) { int b = -1; for (int y = 0; y < 60; ++y) for (int x = x0; x < x1; ++x) if (s.readPixel(x, y)) b = std::max(b, y); return b; };
          s.setFont(ob.Base); int w1 = (int)s.textWidth("x"); int b1 = bottom(0, w1), b2 = bottom(w1 + 2, 200); CHECK(b1 > 0 && b2 > 0 && std::abs(b1 - b2) <= 1, "style fonts: glyphs of different fonts on one line sit on the same baseline"); }
        SuperRichText rt(&s, 0, 0, 200, 40, &lgfx::fonts::Font2, nullptr, nullptr, nullptr); rt.SetText("<b>Bold</b> text"); int h0 = rt.GetContentHeight(); rt.SetStyleFont(SuperRichText::FONT_BOLD, &lgfx::fonts::FreeSansBold18pt7b); CHECK(rt.GetContentHeight() > h0, "style fonts: SuperRichText::SetStyleFont changes the layout");
        SuperLabel lb(&s, 0, 0, 200, &lgfx::fonts::Font2, "<b>x</b>"); lb.SetPadding(0); lb.SetBorder(0, 0); lb.SetRich(true); int lh = lb.GetHeight(); lb.SetStyleFont(SuperLabel::FONT_BOLD, &lgfx::fonts::FreeSansBold18pt7b); CHECK(lb.GetHeight() > lh, "style fonts: SuperLabel::SetStyleFont");
        SuperMessageBox mb(&s, 160, &lgfx::fonts::Font2, nullptr, nullptr, nullptr); mb.SetMessage("<h1>T</h1>"); mb.AddButton("OK"); mb.SetRich(true); int mh = mb.GetHeight(); mb.SetStyleFont(SuperMessageBox::FONT_HEADING, &lgfx::fonts::FreeSansBold24pt7b); CHECK(mb.GetHeight() > mh, "style fonts: SuperMessageBox::SetStyleFont");
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
    printf("%s: %d checks, %d failed\n", fails ? "LIBRARY TESTS FAILED" : "LIBRARY TESTS PASSED", checks, fails); fflush(stdout); exit(fails ? 1 : 0);
}
void loop() {}
