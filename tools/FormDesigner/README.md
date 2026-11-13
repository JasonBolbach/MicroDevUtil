# SuperGUI Form Designer

A desktop-only designer for SuperGUI forms: macOS, Linux and Windows, LovyanGFX + SDL2, one CMake project. Forms are saved as JSON (designer
only); the output is C++ (`X.designer.h`, regenerated every time, plus a user file written once).

**Status:** every SuperGUI control type can be placed (see the chunk notes below); the designer draws the real controls, and exports C++.


**Files:** Ctrl/Cmd+O (Open) and Ctrl/Cmd+S (Save) always show the file dialog. Open starts where it was last looking (an application setting, see below). Save starts on the
form's own file; saving over it asks nothing, and typing another name saves under that name and remembers it as the form's file (as Save As). Ctrl/Cmd+X exports the code: it always shows the folder dialog (starting at the folder saved in the form), then writes the code there and remembers the folder (`outputPath`). If that folder no longer exists, the folder of the FormDesigner program is used instead. `FD_FORM=file.json` names the file without a dialog (default `form.json`). JSON layout: `src/FormModel.h`. `outputPath` in the
form's JSON is the last folder the code was exported to successfully (not shown in the designer); when a file is opened (and at start) the designer checks that the folder exists and
otherwise uses the FormDesigner program's folder.

**Font dialog** (`src/FontDialog.h`, `src/FontTable.h`): the real `SuperListBox` with the LovyanGFX fonts that suit US-ASCII (Font0..8, Ascii fonts,
FreeSans / FreeSerif / FreeMono in all styles and 9-24 pt, DejaVu 9-72, Orbitron, Roboto Thin, Satisfy, Yellowtail), and a preview of the control
itself in that font plus lorem ipsum, in the control's own colours. A font's size is part of the font, so picking a font picks the size. The JSON stores
the `lgfx::fonts` name (`"font": "FreeSansBold12pt7b"`); an unknown name falls back to Font2. Click: select, double-click: OK, drag, wheel,
Up/Down/PageUp/PageDown/Home/End, Enter = OK, Esc = Cancel.

**Layout properties** (border style / size, padding, alignment) are overrides: a control keeps its own default (the field shows it) until you change it, an
empty field or choosing the default again removes the override, and only overrides are saved (`"borderStyle": "sleek"`, `"padding": 4`, `"align": "center"`)
and will be generated. Alignment exists for labels and push buttons only.

**File dialog** (`src/FileDialog.h`, modal, drawn inside the designer, plain `std::filesystem`, identical on Windows / macOS / Linux): Open, Save (adds `.json`, asks before
replacing), and a folder chooser. Path line (type a folder or file path + Enter), Up / Home / New folder buttons (and Drives on Windows), the real `SuperListBox` with folders
first then `*.json` files (case-insensitive, hidden `.` names skipped), file name field, double-click / Enter, Backspace = up, Tab between fields, Esc = cancel.
The Windows drive list is written but has not been run on Windows yet.

**Code generation, step 1** (`src/CodeGen.h`): Ctrl/Cmd+X (Export) asks for the folder (starting at the last successful export folder; the FormDesigner program's folder the first time) and writes `<FormName>.designer.h` there and saves the form's JSON so that folder is remembered (`outputPath`). `FD_EXPORT=1 FD_FORM=x.json FD_OPEN=1 build/FormDesigner` does the same
from the command line. The header is regenerated every time and must not be edited: it defines `<FormName>Base` with the controls as public members,
the `SuperForm`, `Show()`, `Service()`, an `InitializeComponent()` that writes only the properties that differ from each control's default, and one empty
virtual handler per event (`okBtn_Click()` for push buttons, `wifi_Change()` for check / radio buttons). Your own class derives from it:

```cpp
#include "Form1.designer.h"
struct Form1 : Form1Base { using Form1Base::Form1Base; protected: void okBtn_Click() override { /* ... */ } };
Form1 ui(&lcd, &Enc, &BtnA);   // setup(): ui.Show();   loop(): ui.Service();
```
Radio buttons have a **Group** property (a name): buttons with the same group name share one `SuperRadioGroup`, a public member named after the group. A group name must be an identifier, not a C++ keyword and not the name of a control. Control and form names cannot be C++ keywords or the generator's own names (`Form`, `Show`, ...).
The generated header was compile-checked against SuperGUI and LovyanGFX (clang, `-Wall -Wextra`).

**Code generation, step 2: your file.** The first time, Export also writes `<FormName>.h` next to the designer header: a class `<FormName>` that derives from
`<FormName>Base`, with a constructor for your start-up code and an empty `override` stub per handler (and a comment on how to use it in a sketch). That file is
yours: the designer never changes it again, even when you add controls later. Instead the status line (and `FD_EXPORT=1`) names the handlers your file does not
mention yet (`add to it: cancelBtn_Click +1 more`), and you copy the stubs you want from the designer header's `protected:` list. Delete the file to get a fresh one.
The pair was built and run as a real SDL app (LovyanGFX) to check that it draws what the designer shows.

**Start-up:** the designer reopens the form file you opened or saved last (`lastForm` in the application settings). If that file is not accessible it looks for **`template.json` in the folder of the program**:
if it exists it is opened as a starting point (no file yet: Save asks for a name) and the form is named `NewForm` (`NewForm2` ... if a control has that name); if neither works you get the regular default new form
(with the ready-made `msgYesNo` box) at the **last form size used** (`lastFormWidth` / `lastFormHeight` in the application settings; kept when you type a Width / Height or open or save a form; **320 x 240** when nothing is remembered yet), zoomed to fit. An explicit `FD_FORM=file.json` skips all this (with `FD_OPEN=1` it loads that file).

**Application settings** (`src/AppSettings.h`): the folders the Open and Save dialogs were last looking at, and the last form file, are kept per user, not per form, in
`~/Library/Application Support/SuperGUI FormDesigner/settings.json` (macOS), `$XDG_CONFIG_HOME` or `~/.config/SuperGUI-FormDesigner/settings.json` (Linux),
`%APPDATA%\SuperGUI FormDesigner\settings.json` (Windows). `FD_SETTINGS=file` overrides the location (the self-test does).

**Zoom:** the surface header has a zoom spin box (a real `SuperSpinBox`, 10-800 %: click its left / right third, drag up / down, or use the wheel over it) with **Fit** and **1:1** buttons.
Keys: `+` `-` `0` (1:1) `F` (fit); Ctrl/Cmd + wheel zooms around the mouse. The form is drawn once at 1:1 by the real controls and scaled pixel-exactly, so what you see
is what the device draws, only bigger. Everything (placing, selecting, dragging, resizing) maps through the zoom; the selection handles stay 8 screen pixels.

**More control types, in chunks.** Chunk 1 adds **Spin box** and **Slider**. Each control type declares its own extra properties in a table (`ExtraDef` in `src/FormModel.h`: key, label,
kind number / text / Yes-No / choice / colour / font, default, limits); the property panel, the JSON, the real-control preview and the code generator are all driven from it, so a new control
type is mostly a table entry plus its generator lines. Only values that differ from the default are saved and generated. Every control except those that have a caption of their own (Label, Push button, Check box, Radio button) also gets the **label group** (Label text,
position Above / Below, alignment, font, colour; with the Sleek border style the label sits on the frame line). The property panel scrolls (wheel) when a control has many rows.
Spin box: Value, Minimum, Maximum, Step, Decimals (a decimal point is only added when drawing: the numbers stay integers), Wrap around, Prefix, Suffix. Slider: Value, Minimum, Maximum, Step,
Show value. Both raise **`<name>_Change()`** (live, while being edited: `GetLiveValue()`) and **`<name>_Accept()`** (`GetValue()`). Because editable controls need both buttons, the generated
constructor is now `Form1(dst, enc, btnA, btnB)` (it was `(dst, enc, btnA)` before chunk 1: re-export and pass `&BtnB`). **Check box and radio button** have no label group (their own caption is enough, and so does the push button) and an initial state: **Checked** / **Selected** (Yes / No), set silently in the generated
code with `SetChecked(true)` (no event). Only one radio button per group can be selected: choosing Yes deselects the others of the group, a selected button that joins a group that already has a
selection does not take it over, and a file with two selected buttons in one group keeps the first.

**Chunk 2: text and time.** **Text box** (display only: Text, Align, label), **Input box**, **Time box**, **Date box** (`SuperFieldBox` is only their base class, it is not placed on its own).
Input box: *Template* (`#` digit, `A` upper, `a` lower, `B` any letter, `*` any character, everything else a literal; so `###-###-####` is a phone number), *Value* (the value without the literals),
Trim right, Literal color. The template row has a legend and two buttons: **Presets** (a list of phone, ZIP, date, time, IPv4, MAC, SSN, card, plate ... with a preview in the control's own font and
colours) and **Build** (a cell-by-cell builder: click a cell, pick a slot type or a literal, or just type: `# A a B *` make slots, any other character a literal; Insert / Delete / Move; live preview;
at most 40 cells; OK needs at least one slot). Time box: Seconds, Value `HH:MM[:SS]`, Wrap. Date box: Style (Y-M-D / M-D-Y), Separator, Year from / to, Value `YYYY-MM-DD`, Wrap.
**Strict values:** a value or setting the real control would not accept (a character a slot cannot hold, too long a value, 24:00, 2026-02-30, a date outside the year range, a separator that is a digit,
a template without a slot) is refused: the model keeps the old value, the field turns red, and the reason is shown under the rows and in the status line. Valid values are stored the way the box prints them
(`7:5` becomes `07:05:00`). A new template that the old value no longer fits clears the value (and says so). **Fit** (on the Width row of text-like controls) sets the exact width that shows the whole text;
new input / time / date boxes are placed that wide. They raise `<name>_Change()` / `<name>_Accept()` like the spin box (`GetLiveValue()` / `GetValue()`). Library additions: `GetLiveValue()` on the input,
time and date boxes, `GetContentWidth()` on `SuperTextBox`, `EditIO()` on the input / field boxes.

**Chunk 3a: lists.** **List box**, **Combo box**, **Tab bar**. Their items are edited in a modal **items dialog** (the **Edit...** button on the Items / Tabs row): the real `SuperListBox` of items, a text
field for the selected item (changes apply live; an empty item is refused), Add / Insert / Delete / Move up / Move down / Sort A-Z / **Paste lines** (each non-empty clipboard line becomes an item) / Clear all,
and a live preview of the real control. Enter in the field adds the next item (fast entry). Items are printable ASCII, 1-60 characters, at most 200; non-ASCII characters (also `\u00e9` escapes in a file)
are dropped. The JSON stores them as a readable array: `"items": ["Red", "Green", "Blue"]`. Other settings: Selected (the initial item; must be an item, pulled back when the list shrinks), list box Visible rows /
Wrap around / Row padding, combo box Popup rows and **Popup restore** (default *Redraw form*: the generated code gives the popup a `SetRestore` thunk that calls `Form.Redraw(...)`, which works on every panel and
saves the read-back sprite; *Read back* leaves the library default), tab bar Tab padding. Events `<name>_Change()` (the cursor moved) and `<name>_Accept()` (a row / item / tab was chosen). Library additions
(all additive): `InsertItem` / `RemoveItem` / `SetItem` / `GetItem` on the list box, combo box and tab bar (the tab bar also gets `SetItems`; `GetTab` stays), and on `SuperMenu` `RemoveItem(item)` (with everything
below it; the memory is reused by `AddItem`), `FindById(id)` and `Root()`. **Chunk 3b: the Menu.** A nested tree of items, stored as readable, hand-editable JSON:
```json
{ "type": "SuperMenu", "name": "mnu1", ...,
  "items": [
    { "text": "System", "items": [
        { "text": "Reboot",    "itemId": "MNU_REBOOT",    "id": 7 },
        { "text": "Power off", "itemId": "MNU_POWER_OFF", "id": 8 } ] },
    { "text": "About", "itemId": "MNU_ABOUT", "id": 20 } ] }
```
Every command (a leaf) has an **itemId** (a C++-safe name, unique across the whole form, refused if it is a keyword / macro / control name / ...) and a numeric **id** (0-65535, unique within the menu).
The generated class gets `static constexpr uint16_t MNU_REBOOT = 7;` for each, the `AddItem` calls use the constants (`mnu1.AddItem("Reboot", MNU_REBOOT, fd_p1)`), and the handler is
**`menu1_Choose(uint16_t id)`** (plus `menu1_Change()`); your file gets a `switch (id)` with one `case` per command. Parent items (sub-menus) may have an itemId and an id, or neither.
**Minimal JSON is enough** (`"About"` or `{ "text": "About" }`): on load the designer fills in the missing itemIds (`<ID prefix><TEXT>`, default prefix `MNU_`, so "Power off" becomes `MNU_POWER_OFF`)
and the next free ids (above the highest), and repairs problems (the first owner of a repeated id or name keeps it; unsafe names are replaced; empty items dropped; at most 5 levels and 200 items).
Export refuses a menu that still has a problem. The **menu editor** (Edit... on the Menu items row) is the optional graphical way: the tree (real `SuperListBox`), Text / Item ID / Id fields with strict
validation (red + reason; an automatic item ID follows the text until you edit it), Add / Add child / Delete / Move up / Move down / Indent / Outdent, **Paste outline** (indented clipboard lines
become a tree; spaces or tabs) and a live preview of the real menu. Library: `SuperMenu::EditIO()` joins the other controls. Self-test hardening: a crash in the self-test is now reported with the last
check instead of raising the OS crash dialog.

**Visible, Enabled, control groups and tab pages.** Every control has three more rows: **Control group** (a name; empty = no group = group 0), **Visible** and **Enabled** (the initial state). A control group
is a safe C++ name, shared by any number of controls (it must not be a control, radio group, menu item or form class name); each used group becomes `static constexpr uint16_t GRP_X = n;` in the generated class
(numbered 1, 2, 3 by first use, stable in the JSON: `"groupIds"` in the form, `"controlGroup"` per control) and the controls call `SetGroup(GRP_X)`. Not to be confused with a radio button's Group. At run time:
`Form.SetGroupVisible(GRP_X, bool)`, `Form.SetGroupEnabled(GRP_X, bool)` and `Form.ShowPage(pages, n, show)` (new `SuperForm` methods; `SuperControl::SetGroup / GetGroup`): hidden controls are erased and free
their buffer, the focus moves to a control that can take it, and group operations override a control's own Visible / Enabled (showing a group shows all of its controls; the initial Visible = No is applied after the
first page is shown, so it wins at start). **Tab pages:** in the tab bar's items dialog every tab has a **Page group**; the generated code gets a `tabs1_ShowPage(index)` that shows the chosen tab's controls and hides
the other tabs', calls it when a tab is chosen (then your `tabs1_Accept()`), and shows the first tab's page at start. JSON: a tab with a page is `{ "text": "Home", "group": "GRP_HOME" }`, others plain strings.
The design surface has a **View** button (All -> each group in turn): only the controls of that group, and those without a group, are shown and clickable, so overlapping pages can be edited one at a time; selecting a control
of another group from the list switches the view; initially hidden controls are drawn faded with an H. Export refuses a control group with an unsafe or clashing name. Library: `SuperTabBar::EditIO()`.

**Chunk 4: display controls.** **Gauge**, **Graph** and **User draw** (not editable: no encoder, no buttons; each has a **Body height**: the height of the picture inside the frame). New: **decimal numbers**
(property kind `PK_FLOAT`, typed with a decimal point, saved as JSON numbers like `55.5`, written in code as `55.5f`).
*Gauge:* Minimum / Maximum / Value (decimals), Sweep start / end in degrees (-90..90 half dial, -135..135 a 270 degree dial), Major / Minor ticks, Tick numbers, Decimals + Unit, Show value, Needle (Taper / Line),
Zone needle, Smoothing (+ ms), Track / Tick / Needle colours, Min/Max window (ms, 0 = off) with minimum degrees and thickness, and the **Zones**: coloured bands on the dial, edited in a **zones dialog** (list of
From / To / Color with a palette strip, Add / Delete / Move / **Fill 3 bands** (green / yellow / red over 60 / 85 / 100 % of the range) / Clear all, live preview of the real gauge). Strict: a zone must end above where it starts,
Maximum above Minimum, the sweep must end after it starts, and a Value outside the range is refused; a new Minimum / Maximum moves the needle into the range. A new gauge starts with the three bands and the value 42.
JSON: `"zones": [ { "from": 0, "to": 60, "color": "0x07E0" }, ... ]`. The generated code calls `SetValue()` last (after the range and the zones); update it at run time with `gg1.SetValue(v)`.
*Graph:* Samples (capacity; 0 = one per pixel), Auto range or a fixed Range from / to, Grid, Show range, Line / Grid colours, and **Preview data** (Sine / Rising / Noise / None): sample data so the designer shows a
graph. It is **designer only** and is not generated; at run time call `grf1.AddSample(v)`.
*User draw:* Persistent canvas, Auto redraw (ms), Touch input. The designer cannot run your code, so it shows a placeholder (diagonals and the control's name). The generated class has `<name>_Draw(spr, body)`
(and, with Touch input on, `<name>_Pointer(const SuperUserPointer& e)` and `<name>_Accept()`); your file gets stubs with a drawing example. With Touch input the control takes focus.

**Names** (`src/ReservedNames.h`, `FD::ValidName`): a control, radio group or form name must be a plain identifier that is safe in the generated C++ for Arduino, ESP32, Pico and the desktop.
Refused: every C++ keyword (C++23, incl. `wchar_t`, `static_assert`, `bitand`, `co_await`, contextual `final` / `override`), the generated class's own names (`Form`, `Show`, `Service`, ...),
reserved identifier shapes (leading underscore, double underscore), handler names (`x_Click`, `x_Change`, `..Thunk`), `Super*` / `lgfx*` / `LGFX*`, board families (`TFT_*`, `ESP*`, `GPIO*`, `PICO*`, pins
`A0` / `D13`), Arduino and C library names (`HIGH`, `PI`, `min`, `max`, `delay`, `Serial`, `String`, `NULL`, `EOF`, the stdint types, ...) and **every macro the generated code's headers define**
(`src/ReservedMacros.h`, ~3900 names, produced by `sh scripts/gen_reserved_macros.sh`: rerun it when LovyanGFX or SuperGUI change). Inside one form no name may repeat, and none may equal the form's
class names (`<Form>`, `<Form>Base`); a form cannot be renamed onto a control or group name. Files that break these rules are repaired on load. The curated Arduino list comes from the Arduino API, not
from the Arduino core headers (they are not in this repository): extend `IsPlatformName()` if a board package adds more.

The input layer also hosts the reusable text field (`src/DesignerTextField.h`, used by the property panel): arrows, Home/End, Shift to
select, Alt/Ctrl+arrows by word, Cmd+Left/Right line start/end, Backspace/Delete (+Alt = word), Ctrl/Cmd+A/C/X/V, undo/redo (Ctrl/Cmd+Z,
Shift+Z, Ctrl+Y), mouse click / drag / double-click word / triple-click all, scrolling. US-ASCII only (0x20..0x7E): other characters are dropped.

**Chunk 5: colour picker and message boxes.** `SuperColorPicker` is a normal control (property: Color). Message boxes are not on the design surface: they live in a form-level list, stored as readable JSON
(hand-editable, or use the **message box editor**: select nothing, then Edit... on the form's Message boxes row; it has the list of boxes, Name / Title / Width / Default / X / Y, Centered, an Edit message... lines dialog, three button text + optional name rows, Auto names, **Add Yes/No** (a ready-made `msgYesNo` box, also present in every new form: "Confirm?" with Yes / No, named buttons `MSG_YESNO_YES` / `MSG_YESNO_NO`, No highlighted) and a live preview; fonts and colours stay JSON-only):
```json
"messageBoxes": [ { "name": "msgDelete", "title": "Careful", "message": ["Delete all data?", "Cannot be undone."],
                    "buttons": [ { "text": "Yes", "itemId": "MSG_DELETE_YES" }, "No" ], "default": 1 } ]
```
A button may have an optional name (`itemId`): it becomes `static constexpr int MSG_DELETE_YES = 0;` (the button's index) in the generated class. The generated class also gets
`int msgDelete_Ask()` (blocking: the chosen button's index, or `MSG_CANCELLED`) and the non-blocking `msgDelete_Open()` / `msgDelete_Poll()` (`MSG_PENDING` while open). After a box closes the form is redrawn
over the area it covered (generated restore thunk). Library: `SuperMessageBox::EditIO()`. Export refuses unsafe or clashing box and button names.

**Help, backups and rich text.** Press **?** (or F1) for a keyboard help window (a `SuperRichText`; `src/HelpDialog.h`). Every Export of an existing user file also copies it to
`<Form>.yyyy_mm_dd_hh_mm_ss.h.txt` next to it. New control type **Rich text** (`SuperRichText`: scrollable text with basic HTML-like tags, see `src/SuperRichCore.h`): its Text row opens the
**rich text editor** (`src/RichTextEditor.h`): Source mode edits the markup in a multi-line field (`src/DesignerRichTextField.h`, which extends `TextField`: lines, Up / Down keep the column, Home / End, Enter),
Rendered mode shows the real control; Ctrl/Cmd+R switches, Ctrl/Cmd+Enter = OK. JSON: `"richText": ["line", ...]`; generated: `SetTextStatic("..." "...")` (a literal in flash). Library: `SuperRichText`, and
`SetRich(true)` on `SuperLabel` and `SuperMessageBox` (off by default).

**Scroll bars.** A zoomed form that is larger than the design surface gets a vertical and a horizontal scroll bar (drag the thumb, click the track to move a page; the wheel and Shift+wheel still work); the property panel has one too.
They are one generic class, `src/DesignerScrollBar.h` (`FD::ScrollBar`, vertical or horizontal, no SDL: give it a rectangle, the content size and the visible size), it stays in the designer: the micro controls have their own small position indicators.
Also fixed: a gauge has about 42 property rows, more than the old 40-row field table, which overwrote memory next to it (now 64, with a guard and a test).

**Rich options, style fonts, Keep hidden, list scroll.** *Style fonts:* `SuperRichText`, `SuperLabel` and `SuperMessageBox` have `SetStyleFont(slot, font)` (`FONT_BOLD`, `FONT_ITALIC`, `FONT_BOLDITALIC`, `FONT_HEADING`):
`<b> <i> <b><i> <h1>` use the registered font, a slot with no font leaves the tag ignored; the line is as tall as its tallest font. The designer: a **Rich text** row on Label (off by default) and four optional
style-font rows on Label and Rich text; the **message box editor** now also has the box's **Font**, **Text / Back / Border colours** (real font and colour dialogs), **Rich text** Yes/No and the four style fonts.
*Keep hidden:* a form row (off by default). On, every Visible = No control is generated with `SetKeepHidden(true)` and group shows / tab pages leave it hidden (`SuperControl::SetKeepHidden`, `SuperForm` group
operations skip it; a direct `SetVisible(true)` shows it and clears the mark). *Controls On Form:* that list now scrolls (wheel / scroll bar) and follows the selection.

**Control order and big forms.** The **Up / Down / Front / Back** buttons above the "Controls On Form" list move the selected control in the list. The list order is the order the controls are added to the form
(`Form.Add`), so it is the draw order (the last one is in front; the designer's click-hit test agrees). The encoder's **focus (tab) order** is separate: every focusable control has a **Tab order** number that is set once when it is added (the next free number) and never changes by itself, so moving a control in the list does not change its tab position; you edit the number in the Tab order row (the list shows it in front of the name). It is saved as `"tabOrder"` and generated as `SetTabOrder(n)` only when the focus order differs from the list order; equal numbers keep the list order, as the library does. Forms can now be up to **1920 x 1080** (`MAX_FORM_W` / `MAX_FORM_H`
in `src/DesignerConfig.h`); the zoom and scroll bars handle them.

**Progress bar options.** The progress bar has a **Bar color**, **Percent visible** (Yes / No) and **Percent align** (Left / Center / Right) row. The percentage is drawn in the control's **Text color**, on the bar and off it
(library change: `SuperProgressBar` no longer has `ColBarFg`; it also gained `SetPercentAlign()`).

**Separators and headings.** In `SuperListBox`, `SuperComboBox` and `SuperMenu` an item `~` is a separator line, `~Text` a heading with its text centred in the line (`---- Text ----`, dim, unclickable; too long ones are cut, leaving about a character plus 3 pixels of line on each side: constants in `SuperSeparator.h`) and `~~Text` an ordinary item showing `~Text` (`src/SuperSeparator.h`).
The cursor skips them (encoder, touch, drag); the initial selection moves to a real item; a list or menu with nothing else cannot be edited. In the designer the items dialog and the menu editor accept them (the dialog shows
them as ordinary rows to edit), a separator cannot be the initial Selected item, and menu separators get no Item ID / id (not commands, no `case` in the user file). Tab bars do not have them.

**Sub-menu preview.** The menu editor's preview shows the level that holds the selected item (siblings + Back row). On the design surface a menu shows its top level, or the sub-menu named in the new **Preview sub-menu** row (`Settings` or `Settings/Sound`; designer only, not generated, dropped if that item is renamed or removed). Library: `SuperMenu::ShowLevel(item)` draws a level while idle.

**Variable height.** List box, menu, gauge, graph, user draw and rich text have a **Height** row (the total height including the frame) and **top and bottom handles** (the top one keeps the bottom edge fixed).
Lists and menus snap to **whole rows** (the height is `rows`), rich text to whole lines of its font (the body height is a multiple of the line height), gauge / graph / user draw follow the grid; typed heights snap too. It edits the
same value as *Visible rows* / *Body height* (nothing new is saved). Dragging stops at the form's edges. The other controls' height follows their font, so they have no height handles.

**Default names.** A new control is named `<prefix><number>` with the smallest number that is free (and a safe C++ name): Label `lbl`, Push Button `cmd`, Check Box `ck`, Radio Button `rb`, Progress Bar `pb`, Spin Box `sp`,
Slider `sld`, Text Box `txt`, Input Box `inp`, Time Box `tm`, Date Box `dt`, List Box `lb`, Combo Box `cb`, Tab Bar `tab`, Menu `mnu`, Gauge `gg`, Graph `grf`, User Draw `ud`, Color Picker `cp`, Rich Text `rtb` (so `cmd1`, `cmd2`, `lbl1` ...;
the table is `Info()` in `src/FormModel.h`). A radio group defaults to `rGrp1`, `rGrp2` ...; the Control group row and the tab bar's Page group field show `cGrp1`, `cGrp2` ... as the next free name (a hint: you type the name you want). Message boxes keep `msg1` / `msgYesNo`.
Existing forms keep their names.

## Build and run
```
brew install sdl2 cmake            # macOS (Linux: your package manager's SDL2 + cmake)
cmake -S . -B build
cmake --build build -j
build/FormDesigner
```
LovyanGFX is looked up next to the SuperGUI repository (`../../../LovyanGFX`); otherwise `-DLGFX_ROOT=/path/to/LovyanGFX`.
On Windows point `CMakeLists.txt` at your SDL2 development files (`C:/SDL2`).

## Checks
`FD_SELFTEST=1 build/FormDesigner` runs an automatic check of the input layer with synthetic events (opens the window briefly, exits 0 / 1).
`FD_SNAPSHOT=frame.raw build/FormDesigner` (with `FD_OPEN=1` to load the form file first) writes the first frame (raw big-endian RGB565, 1400x820) and exits.

## Notes
- LovyanGFX's SDL panel treats plain `1`-`6` as "rescale the window" and `r` / `l` as "rotate the screen". The designer has text fields, so
  `main.cpp` sets an impossible modifier combination (`Panel_sdl::setShortcutKeymod`) to switch those shortcuts off.
- Mouse positions are scaled from window points to logical pixels with the window's current size, so resizing the window works.
