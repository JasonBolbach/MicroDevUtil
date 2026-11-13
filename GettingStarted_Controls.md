# Getting started with the Super* controls

A step-by-step guide: start from a blank project, put each kind of control on the screen, and react to what the user does.
Reference tables (RAM, every method) are in [README_Controls.md](README_Controls.md). Complete sketches, one per control,
are in `examples/*Example/`.

Contents
1. [Blank project](#1-blank-project)
2. [How every control reports results](#2-how-every-control-reports-results)
3. [Add controls, one by one](#3-add-controls-one-by-one)
4. [Run them together with SuperForm](#4-run-them-together-with-superform)
5. [Add touch](#5-add-touch)
6. [Modal and blocking use](#6-modal-and-blocking-use)
7. [Checklist and common mistakes](#7-checklist-and-common-mistakes)

---

## 1. Blank project

**Step 1.1 - Libraries.** You need LovyanGFX and SuperGUI (this library). PlatformIO:

```ini
[env:pico]                      ; or your ESP32 board
platform  = ...
framework = arduino
lib_deps  = lovyan03/LovyanGFX, https://github.com/JasonBolbach/SuperGUI.git
```

**Step 1.2 - Hardware block.** The controls draw through LovyanGFX and read a `SuperEncoder` and two `SuperButton`s.
Copy the `HARDWARE` block from any example (display class `LGFX`, `lcd`, `Enc`, `BtnA`, `BtnB`) and change the pins to your wiring.

```cpp
#include <Arduino.h>
#include <LovyanGFX.hpp>
// ... class LGFX : public lgfx::LGFX_Device { ... };   (from an example)
LGFX         lcd;
SuperEncoder Enc(2, 3);                    // A / B pins
SuperButton  BtnA(4, true), BtnB(5, true); // active low
```

**Step 1.3 - setup().**

```cpp
void setup() {
  lcd.init();
  lcd.fillScreen(TFT_BLACK);               // your screen background
  Enc.Begin(); BtnA.Begin(INPUT_PULLUP); BtnB.Begin(INPUT_PULLUP);
}
```

**Step 1.4 - Learn the input scheme once** (the same for every control):

| Input | Meaning |
|---|---|
| Encoder | move / change value |
| ButtonA short | select / next / choose |
| ButtonA long | accept |
| ButtonB short | cancel |
| ButtonB long | revert to the starting value and keep editing |

**Step 1.5 - Rules that apply to all controls**
- A control is created with the destination (`&lcd`), position `x, y`, width, and a font. Height follows the font.
- It draws into its own sprite and pushes itself to the destination.
- Style it after construction: `SetLabel("Name")`, `SetBorder(1, TFT_WHITE)`, `SetBorderStyle(SuperControl::BORDER_SLEEK)` (label on the frame line, `SetLabelAlign` left / centre / right, see `examples/SleekExample`), `SetPadding(2)`, `SetColors(fg, bg)`, `ApplyTheme(SuperTheme::Dark())`.
- `SetEnabled(false)` greys it out; `SetVisible(false)` hides it (set `ColHighlightClear` to your screen background first so hiding erases cleanly).
- Controls are global objects (they must outlive their use); don't copy them.

---

## 2. How every control reports results

Read this once; every control below follows it.

**Two kinds of events**

| Callback | Fires | Use it for |
|---|---|---|
| `SetOnChange(fn, user)` | live, while the user is still editing (list/combo/tabs/menu: when the cursor moves) | live preview, e.g. a slider driving a bar |
| `SetOnAccept(fn, user)` | once, when the user accepts (the stored value is already the new one) | commit the setting |

Both take a plain function pointer `void fn(void* user)` and an optional `user` pointer (use it to pass a struct or `this`).
Cancelling fires neither; the old value stays.

```cpp
void onAccept(void* user) {
  Serial.println(myBox.GetValue().c_str());   // already the new value
}
myBox.SetOnAccept(onAccept);
```

**Stored values change only on accept.** While editing, `GetValue()` still returns the old value (slider and spin box add `GetLiveValue()` for the value being edited).

**Polling instead of callbacks.** Controls are non-blocking:

```cpp
myBox.StartEdit();                  // false if it cannot edit (empty list, display-only ...)
// then call often:
SuperEditState s = myBox.Service(); // EDIT_ACTIVE while editing,
                                    // then EDIT_ACCEPTED or EDIT_CANCELLED exactly once, then EDIT_IDLE
```

Or blocking (the call returns when the user is done): `if (myBox.BeginEdit()) { /* accepted */ }`.

**Exceptions to the pattern:** `SuperCheckBox`, `SuperRadioButton` and `SuperPushButton` act immediately in `StartEdit()` and
report `EDIT_ACCEPTED` on the next `Service()`. `SuperProgressBar` and `SuperGraph` are display only. `SuperRadioGroup`'s
callbacks receive the selected index instead of just `user`.

You will normally not call `StartEdit()/Service()` yourself: `SuperForm` does (section 4). Sections 3.x show each control
alone so you can see its events. For testing one control in isolation, see 3.1.

---

## 3. Add controls, one by one

Add these above `setup()`, configure in `setup()`, and (for a single control with no form) run it from `loop()` as in 3.1.

### 3.1 Input box (template text) - first control, with the full loop

```cpp
#include <SuperInputBox.h>
SuperInputBox phone(&lcd, 10, 10, 220, &lgfx::fonts::Font2, "(###) ###-####", &Enc, &BtnA, &BtnB);
```
Template characters: `#` digit, `A` upper-case, `a` lower-case, `B` both, `*` any printable; everything else is a literal.

Setup:
```cpp
phone.SetLabel("Phone");
phone.SetBorder(1, TFT_WHITE);
phone.SetValue("5551234567");        // value without literals
phone.Push();                        // first draw
```
Using it alone (no form):
```cpp
void onPhone(void*) { Serial.println(phone.GetValue().c_str()); }   // "5551234567"
// setup: phone.SetOnAccept(onPhone);
void loop() {
  static bool started = false;
  if (!started) { started = phone.StartEdit(); }
  if (phone.Service() != EDIT_ACTIVE) started = false;   // edit finished; restart whenever you want
}
```
Events: `OnChange` on every character change, `OnAccept` on accept. Optional `SetValidator(fn)` can refuse an accept
(`bool fn(const std::string& value, void* user)`; return false and the edit continues).
Edit: encoder moves between slots, A short edits a slot, encoder scrolls characters, A short returns to slot picking, A long accepts.

### 3.2 Spin box (number)

```cpp
#include <SuperSpinBox.h>
SuperSpinBox volume(&lcd, 10, 50, 100, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
// setup
volume.SetRange(0, 100); volume.SetStep(5); volume.SetValue(30); volume.SetSuffix(" %");
// SetWrap(true), SetDecimals(1) (value 125 shows 12.5), SetPrefix("$") are optional
volume.SetOnChange([](void*){ showPreview(volume.GetLiveValue()); });
volume.SetOnAccept([](void*){ analogWrite(PIN_BL, volume.GetValue()); });
```
Value: `GetValue()` returns the integer (decimals only affect display).
Edit: encoder changes, A short or long accepts.

### 3.3 List box

```cpp
#include <SuperListBox.h>
SuperListBox modes(&lcd, 10, 90, 150, &lgfx::fonts::Font2, 4 /*visible rows*/, &Enc, &BtnA, &BtnB);
// setup
modes.AddItem("Off"); modes.AddItem("Low"); modes.AddItem("High");   // or SetItems(std::vector<std::string>)
modes.SetSelected(0);
modes.SetOnChange([](void*){ /* cursor moved */ });
modes.SetOnAccept([](void*){ Serial.println(modes.GetSelectedText().c_str()); /* modes.GetSelected() = index */ });
```
Edit: encoder moves the cursor, A short chooses, B short cancels.

### 3.4 Combo box (drop-down)

```cpp
#include <SuperComboBox.h>
SuperComboBox unit(&lcd, 10, 130, 120, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
// setup
unit.AddItem("Metric"); unit.AddItem("Imperial");
unit.SetSelected(0);
unit.SetBackdrop(&screenSprite);   // IMPORTANT, see below
unit.SetOnAccept([](void*){ useMetric = (unit.GetSelected() == 0); });
```
The popup opens over other controls and must restore what is below it. Most SPI panels cannot be read back, so give it either
`SetBackdrop(&sprite)` (a sprite that holds your screen), draw into a sprite `Dst`, or `SetRestore(fn, user)` to redraw
the area yourself. On an SDL desktop build the read-back works without any of these.
Same results API as the list box (`GetSelected()`, `GetValue()` = text).

### 3.5 Check box

```cpp
#include <SuperCheckBox.h>
SuperCheckBox beep(&lcd, 10, 170, 150, &lgfx::fonts::Font2, "Beep");
// setup
beep.SetChecked(true);                          // silent, no callbacks
beep.SetOnAccept([](void*){ buzzerOn = beep.GetChecked(); });
```
Instant: a form click (A short, or a tap) toggles it. `OnChange` and `OnAccept` both fire at the toggle. `Toggle()` does it from code.

### 3.6 Radio buttons + group

```cpp
#include <SuperRadioButton.h>
SuperRadioButton r[3] = {
  { &lcd, 10, 200, 100, &lgfx::fonts::Font2, "Slow" },
  { &lcd, 10, 220, 100, &lgfx::fonts::Font2, "Med"  },
  { &lcd, 10, 240, 100, &lgfx::fonts::Font2, "Fast" } };
SuperRadioGroup speed;
// setup
for (auto& b : r) speed.Add(&b);
speed.SetSelected(1);                           // silent
speed.SetOnAccept([](int idx, void*) { setSpeed(idx); });   // idx = order of Add()
```
Each button is its own stop for the form. `speed.GetSelected()` returns the index, or -1. `speed.Select(i)` selects with callbacks.
Selecting the already-selected button does nothing.

### 3.7 Slider

```cpp
#include <SuperSlider.h>
SuperSlider level(&lcd, 10, 260, 220, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
// setup
level.SetRange(0, 255); level.SetStep(5); level.SetValue(128);
level.SetOnChange([](void*){ bar.SetValue(level.GetLiveValue()); });   // live, while turning
level.SetOnAccept([](void*){ commit(level.GetValue()); });             // committed value
```
`GetValue()` is the stored value and only changes on accept. Inside `OnChange` use `GetLiveValue()` (the value being edited;
the spin box has it too). Outside an edit both return the same number.

### 3.8 Progress bar (display only)

```cpp
#include <SuperProgressBar.h>
SuperProgressBar bar(&lcd, 10, 290, 220, &lgfx::fonts::Font2);
// setup
bar.SetRange(100); bar.ShowPercent = true;
// anywhere
bar.SetValue(42);                // redraws only if changed
```
No events: you push values into it. It is never focusable: `form.Add(&bar, false)`.

### 3.9 Push button

```cpp
#include <SuperPushButton.h>
SuperPushButton reset(&lcd, 130, 90, 90, &lgfx::fonts::Font2, "Reset");
// setup
reset.SetOnAccept([](void*){ doReset(); });
```
A single click, tap or A press flashes the face inverted for `PressMs` (default 50 ms, blocking; set per theme with `SuperTheme::PressMs` or per button), then fires `OnAccept`. No second press or accept step.

### 3.10 Time box and date box

```cpp
#include <SuperTimeBox.h>
SuperTimeBox alarm(&lcd, 10, 10, 120, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB, /*seconds*/ false);
SuperDateBox day  (&lcd, 10, 50, 140, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);          // add , SuperDateBox::DATE_MDY for 12-31-2026
// setup
alarm.SetTime(7, 30);  day.SetDate(2026, 11, 11);
alarm.SetOnAccept([](void*){ setAlarm(alarm.GetHour(), alarm.GetMinute()); });
day.SetOnAccept  ([](void*){ Serial.println(day.GetValue().c_str()); });                // always "YYYY-MM-DD"
```
Both are made of numeric fields. **Encoder:** turning changes the whole current field (like a spin box); A short moves to the next field;
A long accepts, B short cancels, B long reverts. **Touch:** touch a field to select it, drag up / down to change it, lift to accept.

- **Time** is always 24 hour: `HH` 0-23, `MM` and `SS` 0-59. `GetValue()` is `"HH:MM:SS"` (`"HH:MM"` without seconds), `GetSeconds()` is seconds since midnight.
- **Date**: every field stays in range, and changing the month (or year) pulls the day back to the last day of that month: December 31 -> February
  gives 28 (29 in a leap year). `SetStyle(DATE_YMD / DATE_MDY)` only changes how it is shown (`2026-12-31` or `12-31-2026`); `SetValue()` and
  `GetValue()` always use `YYYY-MM-DD`. Other settings: `SetSeparator('/')`, `SetYearRange(min, max)` (default 1970-2099).
- `SetWrap(true)` makes a field go round (59 -> 00, December -> January) instead of stopping at its limit.
- `SetTime()` / `SetDate()` return false and change nothing for a value that does not exist.

### 3.11 Tab bar

```cpp
#include <SuperTabBar.h>
SuperTabBar tabs(&lcd, 0, 0, 240, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
// setup
tabs.AddTab("Main"); tabs.AddTab("Setup"); tabs.AddTab("Log");
tabs.SetOnAccept([](void*){ showPage(tabs.GetSelected()); });
```
In `showPage`, hide the other pages' controls with `SetVisible(false)` and show these, then `form.Draw()`. Page switching is the
usual way to use a tab bar (see the demo in `user_code.cpp` of the development project).
`OnChange` fires as the cursor moves over tabs (`GetCursor()`), `OnAccept` when you choose (`GetSelected()`).

### 3.12 Color picker

```cpp
#include <SuperColorPicker.h>
SuperColorPicker color(&lcd, 10, 10, 220, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
// setup
color.SetRGB(255, 128, 0);
color.SetOnAccept([](void*){ uint8_t r,g,b; color.GetRGB(r,g,b); led.set(r,g,b); });   // or GetHSV(h,s,v)
```
Edit: A short moves H -> S -> V, encoder changes the selected component, A long accepts.

### 3.13 Menu (nested)

```cpp
#include <SuperMenu.h>
SuperMenu menu(&lcd, 10, 10, 200, &lgfx::fonts::Font2, 5, &Enc, &BtnA, &BtnB);
// setup
auto* wifi = menu.AddItem("WiFi", 0);                 // has sub-items
menu.AddItem("Scan",    10, wifi);
menu.AddItem("Connect", 11, wifi);
menu.AddItem("About",   20);
menu.SetOnAccept([](void*){
  switch (menu.GetResultId()) {                       // the id you gave AddItem()
    case 10: scan(); break;
    case 11: connect(); break;
    case 20: about(); break;
  }
});
```
A short enters a sub-menu or chooses a leaf (accepting it). B short goes back one level; B long leaves the menu.
`OnChange` fires as the cursor moves.

### 3.14 Graph (display only)

```cpp
#include <SuperGraph.h>
SuperGraph trace(&lcd, 10, 150, 220, &lgfx::fonts::Font2, 60 /*body height*/);
// setup
trace.SetAutoRange();            // or SetRange(lo, hi)
trace.ShowRange = true;
// whenever you have a sample
trace.AddSample(sensorValue);    // AddSample(v, false) several times then trace.Refresh()
```
No events: push samples in.

### 3.15 Message box (modal popup)

```cpp
#include <SuperMessageBox.h>
SuperMessageBox box(&lcd, 200, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
// setup
box.SetLabel("Confirm");
box.SetMessage("Erase all settings?");
box.AddButton("Yes"); box.AddButton("No");          // up to 3
box.SetDefault(1);
box.SetBackdrop(&screenSprite);                    // or SetRestore(fn): see the combo box
```
Open it from your code, usually inside another control's event:
```cpp
if (box.BeginEdit()) {                              // true = a button was chosen; false = cancelled
  if (box.GetResult() == 0) eraseAll();             // index of the button
}
form.Draw();                                        // redraw the screen after it closes
```

### 3.16 UserDraw (draw it yourself)

```cpp
#include <SuperUserDraw.h>
SuperUserDraw gauge(&lcd, 10, 10, 150, /*body height*/ 90);
// setup
gauge.SetBorder(1, TFT_WHITE);  gauge.SetLabel("Gauge");
gauge.SetOnDraw([](SuperUserDraw& c, lgfx::LGFX_Sprite* s, const SuperRect& r, void*) {
    s->drawCircle(r.x + r.w / 2, r.y + r.h / 2, 30, TFT_GREEN);        // draw inside r
});
gauge.SetAutoRedraw(50);                                              // animation: repaint every 50 ms
form.Add(&gauge, false);                                              // display only
```
You get a sprite and the rectangle to draw in; the library handles the frame, the dimming and the push. Add `SetPersistent(true)` for a canvas that
keeps what you draw (sketch pads, traces), and `SetOnPointer(fn)` to react to touch with positions in your drawing area. See `examples/UserDrawExample`.

### 3.17 Gauge

```cpp
#include <SuperGauge.h>
SuperGauge load(&lcd, 10, 10, 200, 120, &lgfx::fonts::Font2);
// setup
load.SetRange(0, 100);
load.AddZone(0, 60, TFT_GREEN);  load.AddZone(60, 85, TFT_YELLOW);  load.AddZone(85, 100, TFT_RED);
load.SetFormat(0, "%");  load.SetLabel("Load");
form.Add(&load, false);                          // display only
// whenever you have a reading
load.SetValue(cpuLoad);                          // the needle glides to it
```
A half dial by default (`SetSweep(-135, 135)` for 270 degrees), with ticks and numbers, colour zones, and a needle that takes the colour of its zone. `load.SetMinMax(5000, 4, 3)` adds lines for the lowest and highest value of the last 5 seconds, 3 pixels thick, in the inverted colour of the dial under them, shown when they are 4+ degrees from the needle. Smoothing is on by default
and is driven by `form.Service()` (without a form call `load.Update()` from `loop()`). See `examples/GaugeExample`.

### 3.18 Label (plain text)

```cpp
#include <SuperLabel.h>
SuperLabel title(&lcd, 0, 6, 240, &lgfx::fonts::Font4, "Settings", SuperControl::ALIGN_CENTER);
SuperLabel help (&lcd, 10, 40, 220, &lgfx::fonts::Font2, "Long text that wraps onto several lines.");
// setup
title.SetTextColor(TFT_CYAN);   help.SetWrap(true);
form.Add(&title, false);  form.Add(&help, false);          // display only
// later
help.SetTextf("%d samples", n);                            // live value; redraws only when the text changed
```
No frame, no title: only text, with `SetAlign` (left / centre / right), `SetVAlign` + `SetBodyHeight` for a fixed height, word wrap, `SetMaxLines`, underline. Set `ColBg` to the colour behind it. See `examples/LabelExample`.

---

## 4. Run them together with SuperForm

`SuperForm` owns focus: the encoder moves a highlight box between controls, A short starts the focused control's edit, and
it calls the control's `Service()` until it ends. `loop()` stays free.

**Step 4.1 - Create and add** (default focus order = the order you add them; `SetTabOrder(n)` changes it, `SetZOrder(n)` changes which overlapping control is in front, lowest number in front):

```cpp
#include <SuperForm.h>
SuperForm form(&Enc, &BtnA);

// setup(), after configuring the controls:
form.Add(&phone);
form.Add(&volume);
form.Add(&beep);
form.Add(&bar, false);       // shown but never focused (display-only controls)
form.Draw();                 // draws all; focused one gets the highlight box, others are dimmed
```

**Step 4.2 - loop:**

```cpp
void loop() {
  form.Service();            // call often; no delay() in loop()
  // your other work
}
```

**Step 4.3 - Handle events.** Two options, and they combine:

1. Per-control `SetOnChange` / `SetOnAccept` (section 3). Best when each control has its own meaning.
2. One form-wide handler, which tells you which control finished and how:

```cpp
void onEdit(SuperControl* c, SuperEditState result, void*) {
  if (result != EDIT_ACCEPTED) return;
  if (c == &phone)  save(phone.GetValue());
  if (c == &volume) setVolume(volume.GetValue());
}
// setup: form.SetOnEdit(onEdit);
```

**Step 4.4 - Skip controls** with `SetEnabled(false)` / `SetVisible(false)`; the form's focus skips them. After changing
visibility of many controls (tab pages), call `form.Draw()`.

**Step 4.5 - Look.** `PreviewBrightness` (default 85) dims unfocused controls. The cursor appears only during a real edit.
Theme everything: `ctl.ApplyTheme(SuperTheme::Dark())` per control.

---

## 5. Add touch

Touch uses `SuperTouch` (from this library) with any position source. LovyanGFX's `getTouch()` is a good one, including on
the desktop SDL build (left mouse = touch).

```cpp
bool readTouch(int16_t& x, int16_t& y) {
  int32_t tx, ty;
  if (!lcd.getTouch(&tx, &ty)) return false;
  x = tx; y = ty; return true;
}
SuperTouch touch(readTouch);

// setup(), after configuring the panel's touch in the LGFX class:
touch.ClickD_MAX = 0;        // taps arrive immediately (no double-click delay)
form.SetTouch(&touch);
```
`form.Service()` now also reads touch. Nothing else changes: the callbacks and `onEdit` fire the same way, and touch mixes freely with the encoder.

| Gesture | Effect |
|---|---|
| Tap a control | focus it and start editing (one tap) |
| Tap a list/combo row, tab, menu row, message-box button | choose it |
| Tap / drag on a slider track or a color picker bar | set the value by position |
| Drag up/down on a value control | change the value (`SuperControl::TouchDragStepPx`, default 4 px per step) |
| Long press | accept |
| Tap outside the editing control | cancel |

Globals you may want to tune once: `SuperControl::TouchPad` (extra hit area in pixels, default 4) and
`SuperControl::TouchDragStepPx` (lower = more sensitive).

---

## 6. Modal and blocking use

- A **message box** is easiest blocking: `box.BeginEdit()` (see 3.15). With touch set on the form it keeps receiving touch.
- Any control can be run blocking: `if (spin.BeginEdit()) { use(spin.GetValue()); }`. It returns true only if accepted, and
  false if the control is disabled/hidden. Don't call it while the form is already editing that control.
- To abort a long edit from outside (window closed, timeout) give the control `SetAbort(fn)`; return true from `fn` to cancel.

---

## 7. Checklist and common mistakes

| Symptom | Fix |
|---|---|
| Nothing responds | `Enc.Begin()`, `BtnA/B.Begin(INPUT_PULLUP)` called? `form.Service()` called every loop with no long `delay()`? |
| Control never draws | Call `Push()` (alone) or `form.Draw()` once after setup. |
| Box edges / ghost pixels around a control | Don't change `KeyColor`; destination must be 16-bit RGB565. |
| Hiding a control leaves a hole in the wrong colour | Set `ColHighlightClear` to your screen background. |
| Combo/message popup restores garbage | Use `SetBackdrop(&sprite)` or `SetRestore(fn)`; most SPI panels can't read back. |
| `GetValue()` shows the old value inside `OnChange` | By design: values change only on accept. Use `GetLiveValue()` (slider, spin box) or `OnAccept`. |
| Selected radio doesn't fire when clicked again | By design: re-selecting does nothing. |
| Spin/slider jumps by large amounts when the encoder is spun fast | `SuperEncoder` accelerates (1/5/20). Intended; lists with <= 20 items step by sign only. |
| Taps feel late | `touch.ClickD_MAX = 0`. |
| Taps miss small controls | Raise `SuperControl::TouchPad`. |
| RAM | Each control has a sprite (width x height x 2 bytes). `ReleaseBuffer()` frees it; see the RAM table in README_Controls.md. |
