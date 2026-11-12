#pragma once
// SuperControl - common frame for the Super* controls. Depends only on LovyanGFX.
//
//  * Fixed pixel width; height = body height (set by the subclass, usually font based)
//    + 2 * (border + padding) [+ label height + label gap when a label is set].
//  * Border styles: BORDER_STANDARD (one colour, ColBorder) or BORDER_THREED (ColBorder on the top and
//    left, ColBezel on the bottom and right, joined by a 45 degree bevel line of ColBezelEdge that is
//    BorderSize/2 pixels thick, drawn from each control corner to the matching input-area corner,
//    on all four corners).
//  * Optional label above or below the box, with its own font, colour and alignment. When the label is
//    ABOVE, the control's y is the top of the label and the box sits below it; when BELOW it is appended.
//  * All colours are public variables. After changing one directly, call Invalidate() (or Refresh()).
//  * The control renders into its own LGFX_Sprite and pushes itself to a destination at a position:
//    SetDisplayPos(x, y). Refresh() redraws only if something changed; Push() always redraws.
//  * DrawPreview(highlight) redraws the control without any input handling and (optionally) draws a
//    highlight box around it, so a GUI can walk the user through a set of controls.
#include <LovyanGFX.hpp>
#include <algorithm>
#include <string>
#include <vector>

enum SuperEditState : uint8_t { EDIT_IDLE = 0, EDIT_ACTIVE, EDIT_ACCEPTED, EDIT_CANCELLED };

// A pointer (touch) event in screen coordinates. SuperForm converts SuperTouch events into these, so the
// controls themselves don't depend on SuperTouch.
struct SuperPointerEvent {
    enum Kind : uint8_t { TAP, LONG_PRESS, DRAG_START, DRAG_MOVE, DRAG_END, PRESS } Type = TAP;   // PRESS = finger just touched (before any gesture is known)
    int16_t X = 0, Y = 0;                   // current position (TAP / LONG_PRESS: where the touch started)
    int16_t StartX = 0, StartY = 0;         // touch-down point
};

// One set of colours / border settings for a whole UI. Apply with ctl.ApplyTheme(SuperTheme::Dark()).
struct SuperTheme {
    uint16_t Bg = 0x0000, Fg = 0xFFFF, Arrow = 0xC618, Border = 0xFFFF, Bezel = 0x4208, BezelEdge = 0x8410;
    uint16_t Label = 0xFFFF, Highlight = 0xFFE0, Cursor = 0xFFE0, CursorFg = 0x0000, Sel = 0x3186, SelFg = 0xFFFF;
    uint16_t Backdrop = 0x0000;             // the screen behind the controls (erases highlight boxes / hidden controls)
    int      BorderSize = 1;
    uint8_t  BorderStyle = 0;               // 0 = standard, 1 = ThreeD
    int      Padding = 2;
    int      PressMs = 50;                  // push button: how long the inverted face flashes

    static SuperTheme Dark() { return SuperTheme(); }
    static SuperTheme Light() {
        SuperTheme t;
        t.Bg = 0xFFFF; t.Fg = 0x0000; t.Arrow = 0x7BEF; t.Border = 0x0000; t.Bezel = 0x8410; t.BezelEdge = 0xC618;
        t.Label = 0x0000; t.Highlight = 0xF800; t.Cursor = 0x001F; t.CursorFg = 0xFFFF; t.Sel = 0xC618; t.SelFg = 0x0000;
        t.Backdrop = 0xFFFF;
        return t;
    }
};

class SuperControl {
public:
    enum Align       : uint8_t { ALIGN_LEFT = 0, ALIGN_CENTER, ALIGN_RIGHT };
    enum BorderStyle : uint8_t { BORDER_STANDARD = 0, BORDER_THREED };
    enum LabelPos    : uint8_t { LABEL_ABOVE = 0, LABEL_BELOW };

    // ---- colours (RGB565) ----
    uint16_t ColBg           = 0x0000;
    uint16_t ColFg           = 0xFFFF;
    uint16_t ColArrow        = 0xC618;      // light gray scroll arrows
    uint16_t ColBorder       = 0xFFFF;      // standard border; top + left of ThreeD
    uint16_t ColBezel        = 0x4208;      // ThreeD only: bottom + right
    uint16_t ColBezelEdge    = 0x8410;      // ThreeD only: the 45 degree corner lines
    uint16_t ColLabel        = 0xFFFF;
    uint16_t ColHighlight    = 0xFFE0;      // DrawPreview(true)
    uint16_t ColHighlightClear = 0x0000;    // the screen colour behind the control: erases highlight boxes / hidden controls
    uint16_t ColCursor       = 0xFFE0;      // edit cursor / active element
    uint16_t ColCursorFg     = 0x0000;      // text on the cursor
    uint16_t ColSel          = 0x3186;      // selected item background
    uint16_t ColSelFg        = 0xFFFF;
    int      DisabledBrightness = 50;       // percent of full brightness while disabled

    // Touch: hit areas are the control plus this many pixels around it (a finger is fatter than a mouse).
    // One global setting for the whole GUI.
    inline static int TouchPad = 4;
    // Dragging a finger this many pixels = one encoder step, for value controls (spin box, input box, ...).
    // Smaller = more sensitive. (Lists, menus and tabs use their own row size.)
    inline static int TouchDragStepPx = 4;
    // Set by SuperForm when it owns a touch panel: called while a blocking BeginEdit() waits, so touch keeps working.
    inline static void (*BlockingHook)() = nullptr;
    // The control currently inside a blocking BeginEdit() (nullptr otherwise).
    inline static SuperControl* EditingNow = nullptr;
    // Memory: every control draws into its own 16-bit sprite (width x height x 2 bytes). true = allocate those in PSRAM
    // (ESP32 boards that have it: SuperControl::UsePsram = psramFound();). If the preferred memory is full the other one is tried.
    inline static bool UsePsram = false;
    inline static void* SharedBuf = nullptr;
    inline static size_t SharedCap = 0;
    // Set by SuperForm::SetTouch: touch-down (PRESS) events are being delivered, so a modal control can ignore a lone stale TAP.
    inline static bool PressEventsOn = false;
    // True only while SuperForm delivers the PRESS that started an edit (StartOnPress controls can tell "opened by this touch").
    inline static bool PressStartedEdit = false;
    // Create a 16-bit sprite in the preferred memory (UsePsram), falling back to the other kind. Returns false if neither worked.
    // Used for every buffer the controls make; use it for your own sprites too so they follow the same setting.
    static bool AllocSprite(lgfx::LGFX_Sprite& s, int w, int h) {
        s.deleteSprite();
        s.setColorDepth(16);
        s.setPsram(UsePsram);
        if (s.createSprite(w, h)) return true;
        s.setPsram(!UsePsram);
        return s.createSprite(w, h) != nullptr;
    }
    // Shared render buffer (trades a little speed for a lot of RAM). false (default): every control keeps its own sprite buffer
    // (width x height x 2 bytes each while visible). true: ONE buffer, as big as the largest control, is lent to each control just
    // for the moment it renders and pushes itself to the screen, so RAM use is the biggest control instead of the sum of the
    // visible ones. Speed is nearly unchanged (a control already redraws completely on every draw). The buffer lives in internal
    // RAM when it fits (fast), else in PSRAM. Set it once at startup, or build with -DSUPERGUI_SHARED_BUFFER=1.
#ifdef SUPERGUI_SHARED_BUFFER
    inline static bool SharedBuffer = (SUPERGUI_SHARED_BUFFER) != 0;
#else
    inline static bool SharedBuffer = false;
#endif
    // Make sure the shared buffer holds at least 'bytes' (never shrinks). SuperForm::Draw() does this for the visible controls, so
    // call it yourself only to pre-size for popups. Returns false if neither internal RAM nor PSRAM could supply it.
    static bool ReserveShared(size_t bytes) {
        if (bytes <= SharedCap) return true;
        void* b = lgfx::heap_alloc(bytes);
        if (!b) b = lgfx::heap_alloc_psram(bytes);
        if (!b && SharedBuf) {                                        // low on memory: give the old one back and retry
            lgfx::heap_free(SharedBuf); SharedBuf = nullptr; SharedCap = 0;
            b = lgfx::heap_alloc(bytes);
            if (!b) b = lgfx::heap_alloc_psram(bytes);
        }
        if (!b) return false;
        if (SharedBuf) lgfx::heap_free(SharedBuf);
        SharedBuf = b; SharedCap = bytes;
        return true;
    }
    static void   FreeShared()  { if (SharedBuf) lgfx::heap_free(SharedBuf); SharedBuf = nullptr; SharedCap = 0; }
    static size_t SharedBytes() { return SharedCap; }                // current size of the shared buffer
    // True if the last draw could not allocate the sprite (out of memory). The control then shows a red outline instead of its look.
    bool AllocFailed = false;

    int HighlightGap  = 1;                  // pixels between the control and its highlight box
    int HighlightSize = 2;                  // thickness of the highlight box
    int PreviewBrightness = 85;             // DrawPreview(false): percent of full brightness (100 = no dimming)

    SuperControl(lgfx::LovyanGFX* dst, int x, int y, int width, const lgfx::IFont* font)
        : Spr(dst), Dst(dst), PosX(x), PosY(y), W(width < 1 ? 1 : width), Font(font), LabelFont(font) {}
    virtual ~SuperControl() = default;
    SuperControl(const SuperControl&) = delete;
    SuperControl& operator=(const SuperControl&) = delete;

    // ---- configuration ----
    void SetDisplayPos(int x, int y)        { PosX = x; PosY = y; Dirty = true; }
    void SetWidth(int w)                    { W = w < 1 ? 1 : w; Dirty = true; }
    void SetFont(const lgfx::IFont* f)      { Font = f; OnFontChanged(); Dirty = true; }   // height follows the font
    void SetBorder(int size, uint16_t color){ BorderReq = size < 0 ? 0 : size; ColBorder = color; Dirty = true; }
    void SetBorderStyle(BorderStyle s)      { Style = s; Dirty = true; }
    void SetBezelColors(uint16_t bezel, uint16_t edge) { ColBezel = bezel; ColBezelEdge = edge; Dirty = true; }
    void SetPadding(int pixels)             { PadReq = pixels < 0 ? 0 : pixels; Dirty = true; }
    void SetAlign(Align a)                  { Alignment = a; Dirty = true; }
    void SetColors(uint16_t fg, uint16_t bg){ ColFg = fg; ColBg = bg; Dirty = true; }
    void SetDestination(lgfx::LovyanGFX* d) { Dst = d; Dirty = true; }
    virtual void ApplyTheme(const SuperTheme& t) {
        ColBg = t.Bg; ColFg = t.Fg; ColArrow = t.Arrow; ColBorder = t.Border; ColBezel = t.Bezel; ColBezelEdge = t.BezelEdge;
        ColLabel = t.Label; ColHighlight = t.Highlight; ColHighlightClear = t.Backdrop;
        ColCursor = t.Cursor; ColCursorFg = t.CursorFg; ColSel = t.Sel; ColSelFg = t.SelFg;
        BorderReq = t.BorderSize; Style = t.BorderStyle ? BORDER_THREED : BORDER_STANDARD; PadReq = t.Padding;
        Dirty = true;
    }

    // ---- enabled / visible ----
    // Disabled: drawn dimmed (DisabledBrightness), BeginEdit() returns false, a SuperForm skips it.
    // Hidden: erased with ColHighlightClear and not drawn until shown again.
    bool IsEnabled() const { return EnabledFlag; }
    bool IsVisible() const { return VisibleFlag; }
    void SetEnabled(bool e) { if (e == EnabledFlag) return; EnabledFlag = e; Dirty = true; Refresh(); }
    void SetVisible(bool v) {
        if (v == VisibleFlag) return;
        VisibleFlag = v;
        if (v) { Dirty = true; Refresh(); return; }
        if (Dst) {
            int m = HighlightGap + HighlightSize + 1;
            Dst->fillRect(PosX - m, PosY - m, W + 2 * m, GetHeight() + 2 * m, ColHighlightClear);
        }
        ReleaseBuffer();                                           // a hidden control needs no memory; it is re-created when shown
    }

    // ---- editing (controls that can be edited override StartEdit / Service) ----
    // Blocking: returns true if the edit was accepted. Non-blocking: StartEdit() then Service() often.
    virtual bool IsEditing() const { return false; }
    virtual bool StartEdit() { return false; }
    virtual SuperEditState Service() { return EDIT_IDLE; }
    bool BeginEdit() {
        if (!EnabledFlag || !VisibleFlag || !StartEdit()) return false;
        SuperControl* prev = EditingNow;
        EditingNow = this;
        SuperEditState s;
        do {
            if (BlockingHook) BlockingHook();
            s = Service();
            if (s == EDIT_ACTIVE) lgfx::delay(1);
        } while (s == EDIT_ACTIVE);
        EditingNow = prev;
        return s == EDIT_ACCEPTED;
    }

    // ---- touch ----
    // Is the point on this control (label included)? 'pad' widens the area on every side.
    virtual bool HitTest(int x, int y, int pad = 0) {
        return x >= PosX - pad && x < PosX + W + pad && y >= PosY - pad && y < PosY + GetHeight() + pad;
    }
    // Pointer input while this control is being edited. Return true if it was used.
    // Default (editable controls): tap = ButtonA short, long press = ButtonA long, tap outside = ButtonB short,
    // dragging up / down = encoder steps. Lists, tabs, menus and sliders add precise picking.
    virtual bool OnPointer(const SuperPointerEvent&) { return false; }
    // Touch behaviour hints for SuperForm:
    //  StartOnPress : an idle control starts editing the moment a finger lands on it at (x, y) (and gets the PRESS event),
    //                 so it can react at once (e.g. highlight the character under the finger).
    //  IdleTapActs  : a tap on an idle control starts the edit AND is delivered to it, so one tap does the whole job
    //                 (tab bar, menu). Otherwise the first tap only focuses + starts the edit.
    virtual bool StartOnPress(int /*x*/, int /*y*/) { return false; }
    virtual bool IdleTapActs() const { return false; }

    // ---- label ----
    void SetLabel(const std::string& text, LabelPos pos = LABEL_ABOVE) { Label = text; LabelPosition = pos; Dirty = true; }
    void SetLabelFont(const lgfx::IFont* f) { LabelFont = f; Dirty = true; }     // default: the control's font at construction
    void SetLabelColor(uint16_t c)          { ColLabel = c; Dirty = true; }
    void SetLabelAlign(Align a)             { LabelAlignment = a; Dirty = true; }
    void SetLabelGap(int pixels)            { LabelGap = pixels < 0 ? 0 : pixels; Dirty = true; }   // default 2
    const std::string& GetLabel() const     { return Label; }

    // ---- geometry ----
    int GetX() const { return PosX; }
    int GetY() const { return PosY; }
    int GetWidth() const { return W; }
    int GetHeight()      { return BoxHeight() + LabelBlock(); }    // whole control, label included
    int GetBoxHeight()   { return BoxHeight(); }                   // the bordered box only
    int GetBoxTop()      { return PosY + BoxOffset(); }            // screen y of the box
    int GetBorder()      { return EffBorder(); }                   // after clamping
    int GetPadding()     { return EffPad(); }
    lgfx::LovyanGFX* GetDestination() const { return Dst; }

    // ---- drawing ----
    void Invalidate() { Dirty = true; }
    void ReleaseBuffer() { Spr.deleteSprite(); Dirty = true; }     // free the sprite memory (re-created on the next draw)
    void Refresh()    { if (Dirty && VisibleFlag) Push(); }                       // redraw + push only if something changed
    void Push() {                                                  // always redraw + push
        if (!VisibleFlag) return;
        if (!Render()) { ShowAllocFailure(); return; }
        if (!EnabledFlag) Dim(DisabledBrightness);
        Present();
    }
    // Redraw (no input handling) and draw / erase a highlight box just outside the control.
    // highlight=true  -> full brightness + box (the focused control).
    // highlight=false -> dimmed to PreviewBrightness percent (label and body), box erased.
    // The control's value and state are not touched. A later Push()/Refresh() draws the normal look.
    void DrawPreview(bool highlight)                { DrawPreview(highlight, ColHighlight); }
    void DrawPreview(bool highlight, uint16_t color) {
        if (!VisibleFlag) return;
        if (!Render()) ShowAllocFailure();
        else {
        if (!EnabledFlag) Dim(DisabledBrightness);
        if (!highlight) Dim(PreviewBrightness);
        Present();
        }
        Dirty = true;                                              // the sprite now holds the preview look
        if (!Dst) return;
        int h = GetHeight();
        uint16_t c = highlight ? color : ColHighlightClear;
        for (int i = 0; i < HighlightSize; ++i) {
            int m = HighlightGap + 1 + i;
            Dst->drawRect(PosX - m, PosY - m, W + 2 * m, h + 2 * m, c);
        }
    }

protected:
    static constexpr uint16_t KeyColor = 0x0001;                   // transparent key used while a label is shown

    // ---- subclass hooks ----
    virtual int  BodyHeight() { return FontH(); }                  // inner (content) height
    virtual void RenderBody(int x, int y, int w, int h) = 0;       // draw inside the inner area (sprite coords)
    virtual void OnFontChanged() {}

    int FontH()     { Spr.setFont(Font); return Spr.fontHeight(); }
    int EffBorder() { return std::max(0, std::min(BorderReq, std::min((W - 1) / 2, FontH() / 2))); }
    int EffPad()    { return std::max(0, std::min(PadReq, (W - 1) / 2 - EffBorder())); }
    int Inset()     { return EffBorder() + EffPad(); }
    int InnerW()    { return std::max(1, W - 2 * Inset()); }
    int BoxHeight() { return BodyHeight() + 2 * Inset(); }

    lgfx::LGFX_Sprite Spr;
    lgfx::LovyanGFX*   Dst;
    int                PosX, PosY, W;
    const lgfx::IFont* Font;
    bool               Dirty = true;
    bool               EnabledFlag = true, VisibleFlag = true;
    Align              Alignment = ALIGN_LEFT;

private:
    int                BorderReq = 0, PadReq = 2;

    void Present() {
        if (Dst) {
            if (!Label.empty()) Spr.pushSprite(Dst, PosX, PosY, lgfx::rgb565_t(KeyColor));   // label area is transparent
            else                Spr.pushSprite(Dst, PosX, PosY);
            if (SharedBuffer) Dst->waitDMA();                      // the next control reuses the buffer: the transfer must be finished
        }
        if (SharedBuffer) Spr.deleteSprite();                      // hand the shared buffer back (not freed: it is not ours)
        Dirty = false;
    }

    // Scale every pixel's brightness to pct percent (the transparent key is left alone).
    // 16-bit sprite buffers hold byte-swapped RGB565.
    void Dim(int pct) {
        if (pct >= 100) return;
        if (pct < 0) pct = 0;
        uint16_t* p = (uint16_t*)Spr.getBuffer();
        if (!p) return;
        bool keyed = !Label.empty();
        for (size_t n = (size_t)Spr.width() * Spr.height(); n; --n, ++p) {
            uint16_t v = (uint16_t)((*p >> 8) | (*p << 8));
            if (keyed && v == KeyColor) continue;
            uint16_t r = ((v >> 11) & 31) * pct / 100, g = ((v >> 5) & 63) * pct / 100, b = (v & 31) * pct / 100;
            v = (uint16_t)((r << 11) | (g << 5) | b);
            if (keyed && v == KeyColor) v = 0;
            *p = (uint16_t)((v >> 8) | (v << 8));
        }
    }
    BorderStyle        Style = BORDER_STANDARD;
    std::string        Label;
    LabelPos           LabelPosition = LABEL_ABOVE;
    const lgfx::IFont* LabelFont;
    Align              LabelAlignment = ALIGN_LEFT;
    int                LabelGap = 2;

    int LabelH() {
        if (Label.empty()) return 0;
        Spr.setFont(LabelFont);
        return Spr.fontHeight();
    }
    int LabelBlock()  { return Label.empty() ? 0 : LabelH() + LabelGap; }
    int BoxOffset()   { return (!Label.empty() && LabelPosition == LABEL_ABOVE) ? LabelBlock() : 0; }

    // ThreeD mitre: the b x b corner square at (x0,y0) is split along its 45 degree diagonal
    // (outer corner to inner corner). 'anti' = the anti-diagonal (top-right / bottom-left corners).
    // One side of the diagonal is c1, the other c2, and the band around it is ColBezelEdge.
    void BevelCorner(int x0, int y0, int b, bool anti, uint16_t c1, uint16_t c2) {
        float thr = std::max(1, b / 2) * 0.7071f;
        for (int v = 0; v < b; ++v)
            for (int u = 0; u < b; ++u) {
                int s = anti ? u + v - (b - 1) : u - v;
                Spr.drawPixel(x0 + u, y0 + v, (float)std::abs(s) <= thr ? ColBezelEdge : s < 0 ? c1 : c2);
            }
    }

    void DrawBorder(int b, int oy, int H) {
        if (b <= 0) return;
        if (Style == BORDER_STANDARD) {
            for (int i = 0; i < b; ++i) Spr.drawRect(i, oy + i, W - 2 * i, H - 2 * i, ColBorder);
            return;
        }
        Spr.fillRect(0, oy, W - b, b, ColBorder);                  // top (and top-left corner)
        Spr.fillRect(0, oy + b, b, H - 2 * b, ColBorder);          // left
        Spr.fillRect(W - b, oy + b, b, H - 2 * b, ColBezel);       // right
        Spr.fillRect(b, oy + H - b, W - b, b, ColBezel);           // bottom (and bottom-right corner)
        BevelCorner(0,     oy,         b, false, ColBorder, ColBorder);   // top-left
        BevelCorner(W - b, oy,         b, true,  ColBorder, ColBezel);    // top-right
        BevelCorner(0,     oy + H - b, b, true,  ColBorder, ColBezel);    // bottom-left
        BevelCorner(W - b, oy + H - b, b, false, ColBezel,  ColBezel);    // bottom-right
    }

    void DrawLabel(int y, int lh) {
        Spr.setFont(LabelFont);
        Spr.setTextColor(ColLabel);
        Spr.setTextDatum(LabelAlignment == ALIGN_CENTER ? lgfx::textdatum_t::top_center
                       : LabelAlignment == ALIGN_RIGHT  ? lgfx::textdatum_t::top_right
                                                        : lgfx::textdatum_t::top_left);
        int x = LabelAlignment == ALIGN_CENTER ? W / 2 : LabelAlignment == ALIGN_RIGHT ? W - 1 : 0;
        Spr.drawString(Label.c_str(), x, y);
        (void)lh;
    }

    // Draw a red outline where the control should be: the sprite buffer could not be allocated (out of memory).
    void ShowAllocFailure() {
        if (!Dst) return;
        Dst->drawRect(PosX, PosY, W, std::max(8, GetHeight()), 0xF800);
        Dst->drawLine(PosX, PosY, PosX + W - 1, PosY + std::max(8, GetHeight()) - 1, 0xF800);
    }

    bool Render() {
        int boxH = BoxHeight(), lb = LabelBlock(), H = boxH + lb, oy = BoxOffset();
        if (SharedBuffer) {                                           // borrow the shared buffer for this one draw
            if (!ReserveShared((size_t)W * H * 2)) { Spr.deleteSprite(); AllocFailed = true; return false; }
            Spr.setBuffer(SharedBuf, W, H, 16);
            AllocFailed = false;
        } else if (Spr.width() != W || Spr.height() != H || !Spr.getBuffer()) {
            AllocFailed = !AllocSprite(Spr, W, H);
            if (AllocFailed) return false;
        } else AllocFailed = false;
        Spr.setFont(Font);
        Spr.fillSprite(Label.empty() ? ColBg : KeyColor);
        Spr.fillRect(0, oy, W, boxH, ColBg);
        DrawBorder(EffBorder(), oy, boxH);
        if (!Label.empty()) {
            int lh = LabelH();
            DrawLabel(LabelPosition == LABEL_ABOVE ? 0 : boxH + LabelGap, lh);
        }
        Spr.setFont(Font);
        int ins = Inset();
        RenderBody(ins, oy + ins, InnerW(), BodyHeight());
        return true;
    }
};
