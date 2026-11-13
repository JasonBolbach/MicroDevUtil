#pragma once
// The fonts the Form Designer offers: the LovyanGFX fonts that suit US-ASCII user interfaces (no CJK sets). `id` is the exact name in
// lgfx::fonts (saved in the JSON, and what the generated code will write as &lgfx::fonts::<id>).
#include <string>
#include <vector>
#include <LovyanGFX.hpp>

namespace FD {
struct FontInfo {
    const char* id;                 // lgfx::fonts name
    const char* label;              // shown in the font list
    const lgfx::IFont* font;
    bool digitsOnly;                // Font6 / 7 / 8 only contain digits and a few clock characters
};
#define FD_FONT(id, label, digits) { #id, label, &lgfx::fonts::id, digits }
#define FD_FAMILY(name, label) \
    FD_FONT(name##9pt7b,  label " 9pt",  false), FD_FONT(name##12pt7b, label " 12pt", false), \
    FD_FONT(name##18pt7b, label " 18pt", false), FD_FONT(name##24pt7b, label " 24pt", false)

inline const std::vector<FontInfo>& Fonts() {
    static const std::vector<FontInfo> T = {
        FD_FONT(Font0, "Font0   6x8 bitmap", false), FD_FONT(Font2, "Font2   16 px", false), FD_FONT(Font4, "Font4   26 px", false),
        FD_FONT(Font6, "Font6   48 px (digits)", true), FD_FONT(Font7, "Font7   7-segment (digits)", true), FD_FONT(Font8, "Font8   75 px (digits)", true),
        FD_FONT(Font8x8C64, "Font8x8C64   C64 8x8", false), FD_FONT(AsciiFont8x16, "AsciiFont8x16", false), FD_FONT(AsciiFont24x48, "AsciiFont24x48", false),
        FD_FONT(TomThumb, "TomThumb   tiny", false),
        FD_FAMILY(FreeSans, "FreeSans"), FD_FAMILY(FreeSansBold, "FreeSans Bold"), FD_FAMILY(FreeSansOblique, "FreeSans Oblique"), FD_FAMILY(FreeSansBoldOblique, "FreeSans Bold Oblique"),
        FD_FAMILY(FreeSerif, "FreeSerif"), FD_FAMILY(FreeSerifBold, "FreeSerif Bold"), FD_FAMILY(FreeSerifItalic, "FreeSerif Italic"), FD_FAMILY(FreeSerifBoldItalic, "FreeSerif Bold Italic"),
        FD_FAMILY(FreeMono, "FreeMono"), FD_FAMILY(FreeMonoBold, "FreeMono Bold"), FD_FAMILY(FreeMonoOblique, "FreeMono Oblique"), FD_FAMILY(FreeMonoBoldOblique, "FreeMono Bold Oblique"),
        FD_FONT(DejaVu9, "DejaVu 9", false), FD_FONT(DejaVu12, "DejaVu 12", false), FD_FONT(DejaVu18, "DejaVu 18", false), FD_FONT(DejaVu24, "DejaVu 24", false),
        FD_FONT(DejaVu40, "DejaVu 40", false), FD_FONT(DejaVu56, "DejaVu 56", false), FD_FONT(DejaVu72, "DejaVu 72", false),
        FD_FONT(Orbitron_Light_24, "Orbitron Light 24", false), FD_FONT(Orbitron_Light_32, "Orbitron Light 32", false),
        FD_FONT(Roboto_Thin_24, "Roboto Thin 24", false), FD_FONT(Satisfy_24, "Satisfy 24", false), FD_FONT(Yellowtail_32, "Yellowtail 32", false),
    };
    return T;
}
#undef FD_FAMILY
#undef FD_FONT

inline constexpr const char* DEFAULT_FONT = "Font2";                       // what the designer used so far; the controls' usual body font
inline int FontIndex(const std::string& id) {
    const auto& t = Fonts();
    for (int i = 0; i < (int)t.size(); ++i) if (id == t[i].id) return i;
    return -1;
}
inline const FontInfo& FindFont(const std::string& id) { int i = FontIndex(id); return Fonts()[i >= 0 ? i : FontIndex(DEFAULT_FONT)]; }
}
