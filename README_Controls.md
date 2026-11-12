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
slider (touch = the value jumps to the finger at once, even when idle; drag = keeps moving live; lift = accept), color picker (works directly on the H / S / V bands, no
activation: touching a band sets that component at once, dragging adjusts live, lifting commits; tap the swatch or long press
= start / accept an edit). Input / time / date box: touch a character = it highlights at once (even on an idle box) and the box waits for a drag; drag up /
down = change it; lift = accept (`TouchAcceptOnLift = false` gives the older style: lift only selects, tap the slot again to
finish, long press to accept). A time / date the validator refuses keeps the edit open. Tap `<` / `>` = scroll, tap outside = cancel. Spin box: like the text boxes. Touch it = it activates at once (even when idle), drag up / down = changes the value live, lift =
accepts (`TouchAcceptOnLift = false` turns lift-accept off). A plain tap: left third = one step down, right third = one step up (and
accepts), middle = accepts unchanged; `-` / `+` hints are drawn while editing (`ShowTouchHints = false` hides them). List box: the highlighted row is always the one under the finger. Touch a row = it highlights at once (even when idle), slide up /
down = the highlight follows the finger, lift = accept it (a plain tap therefore chooses the row). Holding the finger above the
first / below the last visible row keeps scrolling the list (`AutoScrollMs`, default 120). Lifting off to the side, or tapping outside,
cancels; long press accepts the highlighted row. Combo box: touching the closed box opens the drop list at once; slide down into
the list (the highlight follows the finger) and lift to accept, or lift without sliding and the list stays open for a second touch
(touch a row, lift = accept). Tapping the box again, or outside, closes it.
Tab bar: one tap selects a tab. Menu: the row under the finger highlights on touch, slide = the highlight follows (edge hold scrolls), lift =
open / choose / go back, a tap is therefore a one-touch choice; lifting over blank space does nothing, lifting off to the side or tapping outside leaves the menu. Touches are ignored for `TouchGuardMs` (250) right
after the level changes, so a quick second touch cannot hit the `< Back` row that just appeared under the finger. A touch that starts an edit
draws the touched item highlighted straight away (no flash of the first row). Sub-menus start
with a `< Back` row, `ShowBack = false` hides it). A menu uses long press = back and tap outside = leave.
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

## Sprite memory
Every control draws into its own 16-bit sprite, so a control costs about width x height x 2 bytes while it is visible.
- **Shared render buffer (big RAM saver):** `SuperControl::SharedBuffer = true;` (or build with `-DSUPERGUI_SHARED_BUFFER=1`) replaces all the
  per-control buffers with ONE buffer, as big as the largest control, that is lent to each control just while it renders and pushes
  itself to the screen. RAM use becomes the biggest control instead of the sum of the visible ones (the 480x320 demo: 72 KB instead of
  194 KB on its busiest page). Speed is nearly unchanged because a control already redraws completely on every draw. The buffer lives in
  internal RAM when it fits, else in PSRAM. Set it once, before the UI is created. `SuperControl::SharedBytes()` reports its size;
  `SuperForm::Draw()` sizes it for the visible controls. The rendered pixels are identical in both modes.
- A control that is hidden (`SetVisible(false)`, e.g. an inactive tab page) frees its sprite and re-creates it when shown, so only the
  visible page counts. A combo box's popup buffer is freed when it closes.
- `SuperControl::UsePsram = true;` puts the sprites in PSRAM (ESP32 boards that have it: `SuperControl::UsePsram = psramFound();`). If the
  preferred memory is full, the other kind is tried.
- If a sprite still cannot be allocated, the control draws a red outline with a diagonal line where it should be (`AllocFailed` is set)
  instead of failing silently. On a 480x320 screen a busy page can need 250 KB or more, which is more than the internal RAM of most ESP32
  boards leaves free: use PSRAM or make the page lighter.

## Message box and touch
A message box usually opens right under the finger that triggered it, and some panels report a stray touch as a finger lifts. So the
box ignores touch for `TouchGuardMs` (default 300) after it opens, and, when the form delivers touch-down events, a tap only counts if
its finger-down happened after the box opened. A long press (about 0.8 s) on an idle push button / check box / list ... acts like a tap.

## DMA, opaque labels and the redraw
- `SuperControl::UseDMA = true;` (with `SharedBuffer`): two shared buffers are alternated, so the CPU renders the next control while the
  previous one is still being sent to the display by DMA. Typical gain: the render and dim time hides behind the transfer, and a
  DMA transfer also runs closer to the bus speed than CPU-fed writes. The buffers are DMA-capable internal RAM when it fits (else PSRAM
  on ESP32-S3, which LovyanGFX can DMA from; else a single buffer and no overlap). Your LovyanGFX SPI bus needs a DMA channel
  (`cfg.dma_channel`, on by default). Build option: `-DSUPERGUI_DMA=1`.
- `SuperControl::LabelOpaque = true;`: the area around a control's label is filled with `ColHighlightClear` (set it to your screen background
  colour) instead of being transparent. Transparency is a per-pixel CPU copy, so a labelled control can only use DMA when its label is
  opaque. It needs a solid colour behind the controls. Build option: `-DSUPERGUI_LABEL_OPAQUE=1`.
- `SuperForm::Draw()` returns with the display idle (`SuperControl::WaitDma()`); call `WaitDma()` yourself before touching a buffer or the
  display from other code if you push controls individually.
- `SetVisible(v, false)` changes only the flag (nothing is drawn or erased). Hide the old page, show the new one this way, clear the area
  once and call `SuperForm::Draw()`: controls then appear once in their final look instead of first at full brightness.
- **Popups leave no remains:** a message box or combo list restores what it covered through your `SetRestore()` callback (SPI panels can't be read
  back). Call `SuperForm::Redraw(x, y, w, h, backgroundColour)` from it (or set a flag and call it from your loop): it clears that
  rectangle to your screen colour and redraws every visible control that touches it. `SuperComboBox::GetPopupX/Y/Width/Height()` and the
  message box's `GetX/GetY/GetWidth/GetHeight()` give the rectangle. See `DemoUI.h` in the Pico / S3 test projects.
