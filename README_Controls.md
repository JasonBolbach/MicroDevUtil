# Super controls

Encoder- and button-driven UI controls for ESP32 / RP2040, built on [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
and [SuperGUI](https://github.com/JasonBolbach/SuperGUI) (`SuperEncoder`, `SuperButton`).
Each control renders into its **own `LGFX_Sprite`** and pushes itself to a destination at a position.

| Control | Use |
|---|---|
| `SuperTextBox` | single-line text, border, padding, alignment, scrolling (base for several others) |
| `SuperInputBox` | template-driven input: `#` digit, `A` upper, `a` lower, `B` both, `*` any key; other characters are literals |
| `SuperTimeBox` / `SuperDateBox` | `SuperInputBox` with `##:##:##` / `####-##-##` and built-in validation (valid time, real date) |
| `SuperSpinBox` | integer spinner: range, step, wrap, decimals, prefix / suffix |
| `SuperSlider` | horizontal slider with a knob, range, step, optional value text |
| `SuperListBox` | scrolling list with a fixed number of visible rows |
| `SuperComboBox` | drop-down list (popup list box that restores what it covered) |
| `SuperMenu` | nested menu: items with children open sub-menus, leaves return your own id |
| `SuperTabBar` | row of tabs (scrolls when they don't fit); you draw the page |
| `SuperCheckBox` | check box with a caption (instant toggle) |
| `SuperRadioButton` + `SuperRadioGroup` | pick-one buttons; the group keeps them exclusive |
| `SuperPushButton` | captioned button, instant click |
| `SuperColorPicker` | HSV picker with a swatch; get RGB565 / RGB888 |
| `SuperMessageBox` | modal message + 1-3 buttons, restores what it covered |
| `SuperProgressBar` | display-only progress bar with optional percent text |
| `SuperGraph` | display-only scrolling line graph |
| `SuperForm` | not a control: focus handling for a set of controls (wheel moves the highlight, A edits) |

Common to all (`SuperControl.h`): border (standard or ThreeD bevel), padding, optional label above / below with its own
font / color / alignment, public colour variables, `SetDisplayPos`, `DrawPreview(highlight)`,
`SetEnabled(bool)` (drawn dimmed, can't be edited, skipped by `SuperForm`), `SetVisible(bool)` (erased with
`ColHighlightClear`, not drawn), and `ApplyTheme(SuperTheme::Dark() / Light() / your own)`, which sets all colours,
border size / style and padding in one call.

## Input
Encoder = move / change. ButtonA short = start edit / select. ButtonA long = accept. ButtonB short = cancel.
ButtonB long = revert and keep editing. Medium clicks are ignored.

Editable controls offer a **blocking** `BeginEdit()` (returns true if accepted) and a **non-blocking**
`StartEdit()` + `Service()` (returns `EDIT_ACTIVE` until `EDIT_ACCEPTED` / `EDIT_CANCELLED`). The stored value only
changes when an edit is accepted. Check boxes and radio buttons are instant: `BeginEdit()` flips / selects and returns true.

## SuperForm
`SuperForm form(&Enc, &BtnA); form.Add(&a); form.Add(&b); form.Draw();` then call `form.Service()` from `loop()`.
It moves the focus box with the encoder, starts the focused control's edit with a ButtonA short click, runs that
edit without blocking, and calls `SetOnEdit(fn)` when it ends. `form.Add(&bar, false)` shows a control without
ever focusing it. Every control also has the virtual `StartEdit()` / `Service()` / `IsEditing()` that the form uses,
so you can mix controls freely. Hide a page with `SetVisible(false)` on its controls and `form.Draw()` after switching.

## Touch (optional, works together with the encoder)
`SuperTouch touch(readFn); touch.ClickD_MAX = 0; form.SetTouch(&touch);` - `readFn` can simply wrap LovyanGFX's
`lcd.getTouch()` (see `examples/TouchFormExample`). Calling `form.Service()` polls it.

| Gesture | Idle (nothing being edited) | While a control is being edited |
|---|---|---|
| Tap | focus the control under the finger and start its edit (one tap) | same as ButtonA short; **tap outside = cancel** |
| Long press | - | same as ButtonA long (accept) |
| Drag up / down | - | encoder steps: `SuperControl::TouchDragStepPx` pixels per step (default 4, smaller = more sensitive; rows / tabs use their own size) |

Precise picking: list / combo popup / menu rows (tap = choose), tab bar (tap = select), message box buttons (tap = choose),
slider (tap = jump + accept, drag = move live, lift = accept), color picker (tap or drag on the H / S / V bar sets that
component by position; tap the swatch or long press = accept). Input box and spin box use the generic gestures for
now (tap toggles / accepts, drag = encoder). A menu uses long press = back and tap outside = leave.
Hit areas are the control plus `SuperControl::TouchPad` pixels (global, default 4). Blocking `BeginEdit()` calls
(e.g. a message box you open yourself) keep receiving touch while the form has a touch panel.
On the desktop, the left mouse button is the touch panel: LovyanGFX's SDL panel emulates it, so `lcd.getTouch()` works
exactly as on hardware (see `user_code.cpp`).

## Callbacks
Every control has two separate callbacks (`void (*)(void* user)`; the radio group passes the new index):
* `SetOnChange` - the value being edited changed (live, while editing; for list / combo: the cursor moved).
* `SetOnAccept` - an edit was accepted; the stored value is already the new one.
Check box / radio button changes fire both (OnChange first). Slider, tab bar, color picker, menu and message box
follow the same two callbacks; the push button has OnAccept only.

## DrawPreview (for a GUI that moves focus)
`DrawPreview(true)` redraws the control and draws a highlight box just outside it. `DrawPreview(false)` redraws it
dimmed to `PreviewBrightness` percent (default 85, label included) and erases the box. No cursor is drawn: the cursor
only appears while a control is really being edited.

## Combo box and read-back
The popup covers other pixels, and they must be put back when it closes. Options:
1. Destination is a sprite (works as is - sprites can be read back).
2. `SetBackdrop(&sprite)`: pixels are read from a sprite that holds the background, and put back on the real display.
   Most SPI panels cannot be read back, so this is the usual choice (see `examples/ComboBoxExample`).
3. `SetRestore(fn, user)`: you redraw the covered area yourself.

## RAM
Each control is a small object **plus one 16-bit sprite**. The sprite dominates:

    sprite bytes = width x height x 2          height = font height + 2 x (border + padding) [+ label height + 2]

| Example | Size | Sprite |
|---|---|---|
| Input box, Font4, 220 px, border 3, label above | 220 x 54 | ~23 KB |
| Input box, Font2, 216 px, border 1, label above | 216 x 40 | ~17 KB |
| Spin box, Font4, 100 px, border 2, label above | 100 x 50 | ~10 KB |
| Check box / radio button, Font2, 120 px | 120 x 20 | ~4.7 KB |
| List box, Font2, 166 px, 4 rows, label below | 166 x 98 | ~32 KB |
| Combo box (closed), Font2, 150 px, label above | 150 x 40 | ~12 KB |
| Slider, Font2, 220 px, border 1, label above | 220 x 40 | ~17 KB |
| Push button, Font2, 140 px, border 2 | 140 x 24 | ~6.7 KB |
| Tab bar, Font2, 456 px, border 1 | 456 x 22 | ~20 KB |
| Time box, Font4, 130 px, border 1, label above | 130 x 48 | ~12 KB |
| Color picker, Font2, 300 px, border 1, label above | 300 x 70 | ~41 KB |
| Menu, Font2, 200 px, 5 rows, label above | 200 x 112 | ~44 KB |
| Message box, Font2, 260 px, 3 text lines, 2 buttons | 260 x ~100 | ~52 KB + same saved background |
| Graph, Font2, 236 px, body 110, label above | 236 x 132 | ~62 KB + 4 bytes per sample (236 samples ~1 KB) |
| Combo popup (5 rows) while open | 150 x 100 | ~29 KB list sprite + ~29 KB saved background |

Object sizes (the class itself, not the sprite), measured on a 64-bit desktop build; 32-bit ESP32 / RP2040 builds are
smaller (roughly 60-70% of these):

| Class | sizeof (64-bit) |
|---|---|
| SuperTextBox / base | ~630 B |
| SuperInputBox | ~900 B |
| SuperSpinBox | ~870 B |
| SuperListBox | ~760 B |
| SuperComboBox | ~2 KB (includes its own list box) |
| SuperCheckBox / SuperRadioButton | ~710 B |
| the other controls | roughly 700-1000 B each (menu / message box / graph a little more: strings, items and samples) |
| SuperRadioGroup | ~56 B + 4 B per button |

Big sprites are the cost: a menu, graph, color picker or message box is easily 40-60 KB. On an RP2040 pick narrow
widths and small fonts for those, keep only the controls of the visible page alive (`ReleaseBuffer()` on the others),
or make the message box narrower (it frees its sprite when it closes).

Ways to save RAM: `ReleaseBuffer()` frees a control's sprite (it is re-created on the next draw); the combo popup
frees its sprites when it closes; narrower controls and smaller fonts shrink the sprite quadratically.
Measured firmware totals for the examples: RP2040 ~10-11 KB static RAM before sprites; the sprites above come on top
(RP2040 has 264 KB, ESP32-S3 512 KB + optional PSRAM).

## Examples
`examples/<Name>/<Name>.ino` - one tiny sketch per control (Arduino / PlatformIO). Edit the HARDWARE block at the top
(ST7789 SPI panel on a Pico by default) and the encoder / button pins.

## Desktop emulation
`compat/Arduino.h` + `desktop_input.h` run the controls in the LovyanGFX SDL emulator: mouse wheel = encoder, wheel click =
ButtonA, right click = ButtonB. See `user_code.cpp` for the demo.
