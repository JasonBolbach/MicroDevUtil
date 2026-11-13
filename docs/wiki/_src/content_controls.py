CONTROL_GROUPS = [
 ('Text and numbers', ['label','textbox','inputbox','spinbox','timebox','datebox','slider']),
 ('Choosing', ['listbox','combobox','menu','tabbar','colorpicker']),
 ('Buttons', ['checkbox','radiobutton','pushbutton']),
 ('Dialogs', ['messagebox']),
 ('Display only', ['progressbar','graph','gauge','richtext']),
 ('Custom', ['userdraw']),
]
CONTROLS = []
def L(*a): return '<ul>' + ''.join(f'<li>{x}</li>' for x in a) + '</ul>'
def add(**k): CONTROLS.append(k)
KEYS = 'Encoder, A short/long, B short/long and touch as in <a href="../concepts/input.html">Input</a>.'
EV = '<a href="../concepts/edit-contract.html">edit contract</a>'

add(slug='label', cls='SuperLabel', header='SuperLabel.h', kind='display', short='Plain text: no frame, no title',
 tag='Just text. No border, no title and no padding by default, with left / centre / right alignment, vertical alignment, word wrap and live values.',
 code='''SuperLabel title(&lcd, 0, 6, 240, &lgfx::fonts::Font4, "Settings", SuperControl::ALIGN_CENTER);
title.SetTextColor(TFT_CYAN);
SuperLabel help(&lcd, 10, 40, 220, &lgfx::fonts::Font2, "A long text that wraps onto several lines.");
help.SetWrap(true);
form.Add(&title, false); form.Add(&help, false);   // display only
value.SetTextf("%d.%d C", t / 10, t % 10);           // live value''',
 features=['Alignment: <code>SetAlign(ALIGN_LEFT / ALIGN_CENTER / ALIGN_RIGHT)</code> for every line; <code>SetVAlign(VALIGN_TOP / MIDDLE / BOTTOM)</code> together with a fixed height from <code>SetBodyHeight(h)</code> (0 = as tall as the lines).',
  '<code>\\n</code> starts a new line; <code>SetWrap(true)</code> also breaks at spaces (a word wider than the control is broken inside). The height follows the number of lines.',
  'Without wrapping a line that is too wide is cut and ends in "..." (<code>SetEllipsis(false)</code> just clips). <code>SetMaxLines(n)</code> limits the lines and puts "..." on the last one.',
  '<code>SetTextf(fmt, ...)</code> is printf style and redraws only when the text really changed, so it is cheap to call every loop pass.',
  '<code>SetLineGap</code>, <code>SetUnderline</code>, <code>SetTextColor</code>. It is still a <code>SuperControl</code>, so a border or label is available if you want one.',
  '<b>Rich text (opt-in):</b> <code>SetRich(true)</code> makes the label understand the tags of <a href="richtext.html">SuperRichText</a> (<code>&lt;color=red&gt;</code>, <code>&lt;bg=..&gt;</code>, <code>&lt;u&gt;</code>, <code>&lt;big&gt;</code>, <code>&lt;center&gt;</code>, <code>&lt;br&gt;</code> ...). Off by default, so a plain label shows every character as it is. In rich mode <code>SetMaxLines</code>, <code>SetEllipsis</code> and <code>SetUnderline</code> do not apply (use the tags).',
  '<code>SetStyleFont(SuperLabel::FONT_BOLD / FONT_ITALIC / FONT_BOLDITALIC / FONT_HEADING, font)</code>: in rich mode <code>&lt;b&gt; &lt;i&gt; &lt;b&gt;&lt;i&gt; &lt;h1&gt;</code> use the fonts you register (empty slot = tag ignored).'],
 gui='<p>Display only: nothing to operate, it never takes focus. Add it with <code>form.Add(&amp;label, false)</code>.</p>',
 events='<p>None. Change the text with <code>SetText</code> / <code>SetTextf</code>; the label redraws itself when the text changed.</p>',
 api=[('SetText(s) / SetTextf(fmt,...)','Text; redraws on change'),('SetAlign(a)','Horizontal alignment'),('SetVAlign(v) + SetBodyHeight(h)','Fixed height, vertical alignment'),('SetWrap(b)','Word wrap'),('SetMaxLines(n)','Limit lines'),('SetTextColor(c)','Text colour (ColBg = background)'),('GetLineCount()','Lines after wrapping')],
 tips=['Put the title of a page in a large font centred label and a hint line under it in a small grey wrapped label.'],
 pitfalls=['The control fills its whole area with <code>ColBg</code>. Set it to the colour of what is behind (or use <code>ApplyTheme</code>); there is no transparent mode, so a label over a picture shows a coloured rectangle.','Every label is a sprite: many tiny labels cost RAM, see <a href="../concepts/memory-performance.html">memory</a>. One multi-line label is cheaper than several single-line ones.'],
 see=['<a href="textbox.html">SuperTextBox</a>','<a href="progressbar.html">SuperProgressBar</a>'])

add(slug='textbox', cls='SuperTextBox', header='SuperTextBox.h', kind='display', short='Single-line text with scrolling',
 tag='Single-line text with border, padding, alignment and scrolling. The base class of the input, spin, field and combo boxes.',
 code='''SuperTextBox t(&lcd, 10, 10, 200, &lgfx::fonts::Font2);
t.SetText("Hello");
t.SetLabel("Status");
t.DrawPreview(false);''',
 features=['Per-cell styles (colour, fill) so a subclass can highlight one character or field.',
  'Text wider than the box scrolls; the first visible cell is <code>GetFirstVisible()</code>.',
  '<code>CharAt(sx)</code> maps an x position to a text cell, which the touch code of the subclasses relies on.',
  '<b>Alignment</b>: <code>SetAlign(ALIGN_LEFT / ALIGN_CENTER / ALIGN_RIGHT)</code> positions the text while it fits (the spin, input and combo boxes inherit it); text wider than the box scrolls with the &lt; &gt; arrows instead.','You normally use a subclass; use SuperTextBox directly for a read-only text line.'],
 gui='<p>Nothing to operate by itself: it shows text. As a base it supplies the look the editable boxes share: fixed-height line, cursor cell, <code>&lt;</code> / <code>&gt;</code> scroll chevrons when the text does not fit.</p>',
 events='<p>No events of its own. Subclasses add the edit contract.</p>',
 api=[('SetText(s, keepScroll)','Replace the text'),('GetText()','Current text'),('SetAlign(ALIGN_LEFT/CENTER/RIGHT)','Alignment inside the box')],
 tips=['Subclass it for any single-line control; override <code>CellStyleAt</code> to colour cells and <code>CellWidth</code> for fixed-width digits.'],
 see=['<a href="inputbox.html">SuperInputBox</a>','<a href="spinbox.html">SuperSpinBox</a>'])

add(slug='inputbox', cls='SuperInputBox', header='SuperInputBox.h', kind='edit', short='Template-driven text/number input',
 tag='Fixed-format input defined by a template, such as an IP address, a PIN or a licence plate.',
 code='''SuperInputBox ip(&lcd, 10, 10, 160, &lgfx::fonts::Font2, "###.###.###.###", &Enc, &BtnA, &BtnB);
ip.SetLabel("IP address");
ip.SetValue("192168001010");          // digits only, literals are skipped
ip.SetOnAccept([](void*){ /* ip.GetValue() */ });''',
 features=['Template characters: <code>#</code> digit, <code>A</code> upper, <code>a</code> lower, <code>B</code> both, <code>*</code> any typeable key. Everything else is a literal: drawn, skipped by <code>GetValue()</code>.',
  '<code>GetValue()</code> returns only the typed slots (<code>"123456789"</code> for <code>###.###.###</code>); <code>GetDisplay()</code> returns the formatted text.',
  'Short values are padded on the left when every slot is a digit (<code>"42"</code> in <code>###</code> gives <code>042</code>), otherwise on the right.',
  '<code>SetValidator(fn)</code> can refuse an accept; the edit then stays open.'],
 gui='<p>Two levels: <b>selecting</b> a slot (encoder moves between slots, text scrolls), then <b>editing</b> the slot (encoder scrolls its characters, wrapping). A short = slot edit / finish slot, A long = accept, B short = cancel, B long = revert and keep editing at the cursor.</p><p>Touch: touch a character and it highlights at once; drag up/down changes it; lift accepts. Tap <code>&lt;</code> / <code>&gt;</code> to scroll, tap outside to cancel.</p>',
 events='<p>' + EV + ': <code>OnChange</code> on every character change, <code>OnAccept</code> once after the validator agrees (the stored value is then new).</p>',
 api=[('SetValue(v)','Set from typed characters; returns false if it does not fit the template'),('GetValue()','Typed characters only'),('GetDisplay()','Formatted text including literals'),('SetValidator(fn,user)','Veto an accept')],
 tips=['Use a validator for ranges the template cannot express (octets 0-255, a valid month).','<code>TouchAcceptOnLift = false</code> gives the older touch style: lift only selects, tap the slot again to finish.'],
 pitfalls=['A validator that refuses keeps the edit open with no message: show your own hint in <code>OnChange</code> if the user could be confused.'],
 see=['<a href="spinbox.html">SuperSpinBox</a> for plain numbers','<a href="timebox.html">SuperTimeBox</a>'])

add(slug='spinbox', cls='SuperSpinBox', header='SuperSpinBox.h', kind='edit', short='Integer spinner',
 tag='An integer with range, step, wrap, decimals, prefix and suffix. The encoder changes the whole value.',
 code='''SuperSpinBox temp(&lcd, 10, 10, 120, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
temp.SetRange(-400, 1250); temp.SetStep(5);
temp.SetDecimals(1); temp.SetSuffix(" C");   // value 125 shows as 12.5 C
temp.SetValue(215);''',
 features=['<code>SetDecimals(n)</code> only affects display: the value stays an integer (125 shows as 12.5), so store and compare integers.',
  'Optional wrap-around: <code>SetWrap(true)</code> goes from max to min.','Prefix / suffix text (<code>"$"</code>, <code>" C"</code>, <code>" %"</code>).','Digits get a fixed cell width so the text does not shift while the value changes.',
  '<code>SetValidator(fn)</code> can refuse a value at accept.'],
 gui='<p>Encoder changes the value by detents x step (speed scaling applies). A short/long = accept, B short = cancel, B long = revert and keep editing.</p><p>Touch: touching activates it (even idle); drag up/down changes the value live; lift accepts. A plain tap: left third = one step down, right third = one step up (and accepts), middle = accept unchanged. <code>-</code> / <code>+</code> hints are drawn while editing (<code>ShowTouchHints = false</code> hides them).</p>',
 events='<p>' + EV + '. <code>OnChange</code> per step; <code>OnAccept</code> once.</p>',
 api=[('SetRange(min,max)','Clamps the current value'),('SetStep(s)','Change per detent'),('SetWrap(b)','Wrap at the ends'),('SetDecimals(d)','0-6 display decimals'),('SetPrefix/SetSuffix','Text around the number'),('SetValue/GetValue','Integer value')],
 tips=['Use a large step with a fine decimal display for quick coarse changes (step 5 with 1 decimal).'],
 pitfalls=['With decimals, <code>SetRange(0, 100)</code> means 0.0 to 10.0. Think in scaled integers.','Rapid encoder ticks accelerate on purpose; do not "fix" big jumps in a test by faking ticks.'],
 see=['<a href="slider.html">SuperSlider</a>'])

add(slug='timebox', extra_img=('img/timedate_centered.png','Time and date boxes with SetAlign(ALIGN_CENTER), idle and editing'), cls='SuperTimeBox', header='SuperTimeBox.h', kind='edit', short='Time of day HH:MM[:SS], 24 hour',
 tag='A time of day made of numeric fields. Scrolling changes a whole field, like a spin box.',
 code='''SuperTimeBox tm(&lcd, 10, 10, 130, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB /*, seconds=true*/);
tm.SetTime(14, 30, 0);
tm.SetOnAccept([](void*){ /* tm.GetValue() == "14:30:00" */ });''',
 features=['Always 24 hour. HH 0-23, MM 0-59, SS 0-59, each field kept in its range; <code>SetWrap(true)</code> makes a field go round (59 to 00).',
  'Constructor flag <code>seconds=false</code> gives <code>HH:MM</code>.','<code>SetAlign(ALIGN_CENTER)</code> (or right) centres the text in the box; touch picks the right field either way.','<code>GetValue()</code> is <code>"HH:MM:SS"</code>; <code>GetSeconds()</code> is seconds since midnight.',
  'Built on <code>SuperFieldBox</code>: fixed-width digits and separators drawn in <code>ColLiteral</code>.'],
 gui='<p>Encoder changes the current field by whole units. A short = next field (after the last, back to the first); A long = accept; B short = cancel; B long = revert and keep editing.</p><p>Touch a field to select it, drag up/down to change it, lift to accept. Touching a separator does nothing; tap outside cancels.</p>',
 events='<p>' + EV + '. <code>OnChange</code> on every field change; <code>OnAccept</code> once. Stored time changes only on accept.</p>',
 api=[('SetTime(h,m,s)','Returns false when out of range'),('GetHour/GetMinute/GetSecond','Fields'),('GetSeconds()','Since midnight'),('GetValue()/SetValue("HH:MM:SS")','String form'),('SetWrap(b)','Fields wrap round')],
 pitfalls=['There is no AM/PM by design.'],
 see=['<a href="datebox.html">SuperDateBox</a>','<a href="spinbox.html">SuperSpinBox</a>'])

add(slug='datebox', extra_img=('img/timedate_centered.png','Time and date boxes with SetAlign(ALIGN_CENTER), idle and editing'), cls='SuperDateBox', header='SuperTimeBox.h', kind='edit', short='Year / month / day',
 tag='A date made of year, month and day fields. Changing the month pulls the day back into range.',
 code='''SuperDateBox d(&lcd, 10, 10, 130, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB, DATE_YMD);
d.SetLabel("Date (YMD)");
d.SetDate(2026, 12, 31);
// d.GetValue() == "2026-12-31"   in every style''',
 features=['Two display styles: <code>DATE_YMD</code> and <code>DATE_MDY</code> (<code>SetStyle</code>). The <b>value is always <code>YYYY-MM-DD</code></b>, independent of the style.',
  'Day follows month: changing 12-31 to February gives 28 or 29 depending on the leap year.','<code>SetYearRange(min,max)</code>, default 1970..2099.','<code>SetSeparator(c)</code> (default <code>-</code>).'],
 gui='<p>Same as the time box: encoder changes the current field, A short = next field in <em>display</em> order, A long = accept. Touch: select a field, drag, lift to accept.</p>',
 events='<p>' + EV + '. <code>OnChange</code> also fires when a constrained day moves because the month changed.</p>',
 api=[('SetDate(y,m,d)','false if invalid'),('GetYear/GetMonth/GetDay','Fields'),('GetValue()/SetValue(str)','"YYYY-MM-DD"'),('SetStyle(DATE_YMD|DATE_MDY)','Display order only'),('SetYearRange(a,b)','Allowed years'),('DaysInMonth(y,m)','Leap years handled')],
 tips=['Put the style in the label ("Date (YMD)") so users know the order.'],
 pitfalls=['Parse and store the value, not the displayed text: the display order changes with the style.'],
 see=['<a href="timebox.html">SuperTimeBox</a>'])

add(slug='slider', cls='SuperSlider', header='SuperSlider.h', kind='edit', short='Horizontal slider',
 tag='A horizontal slider with a knob, integer range, step and optional value text.',
 code='''SuperSlider vol(&lcd, 10, 10, 200, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
vol.SetRange(0, 100); vol.SetStep(2); vol.SetValue(40);
vol.SetOnChange([](void*){ setVolume(vol.GetValue()); });   // live preview''',
 features=['Knob turns <code>ColCursor</code> while editing.','<code>ShowValue</code> prints the number at the right.','Touch picks the value by position: touching jumps the knob to the finger, even when idle.'],
 gui='<p>Encoder moves the knob by detents x step (speed scaling applies). A short/long = accept, B short = cancel, B long = revert and keep editing. Touch: the value jumps to the finger, dragging keeps moving live, lift accepts.</p>',
 events='<p>' + EV + '. <code>OnChange</code> is the one to use for live effects (volume, brightness); <code>OnAccept</code> to persist.</p>',
 api=[('SetRange(min,max)','Integer range'),('SetStep(s)','Per detent'),('SetValue/GetValue','Value'),('ShowValue','Number at the right')],
 tips=['Use a step that divides the range so the last position equals the max.'],
 pitfalls=['A full-screen slider redraw costs about 7 ms per push on the S3; fine for dragging, but avoid redrawing it every loop pass.'],
 see=['<a href="spinbox.html">SuperSpinBox</a>','<a href="progressbar.html">SuperProgressBar</a>'])

add(slug='listbox', extra_img=('img/separators.png','A list box (left) and a menu showing its Weapon sub-menu (right) with centred headings (~Weapon, ~Primary, ...) and separator lines (~): the cursor skips them; a too long heading is cut'), cls='SuperListBox', header='SuperListBox.h', kind='edit', short='Scrolling list',
 tag='A list of text items with a fixed number of visible rows.',
 code='''SuperListBox lb(&lcd, 10, 10, 160, &lgfx::fonts::Font2, 5 /*rows*/, &Enc, &BtnA, &BtnB);
lb.SetItems({"Red", "Green", "Blue", "Cyan", "Magenta", "Yellow"});
lb.SetSelected(1);''',
 features=['Height = rows x (font height + 2 x <code>RowPad</code>) + frame.','Selected row uses <code>ColSel/ColSelFg</code>; the cursor row during editing uses <code>ColCursor/ColCursorFg</code>.','Grey triangles at the right edge show hidden rows above / below.','<code>Wrap</code> makes the cursor wrap at the ends.','Lists with 20 items or fewer step by sign only, so a fast spin never skips an item.',
  '<b>Unclickable rows:</b> an item <code>"~"</code> is a separator line, <code>"~Text"</code> a heading with the text centred in the line (<code>---- Text ----</code>, dim; a heading that is too long is cut, always leaving about a character plus 3 pixels of line on each side: <code>SuperSep::SIDE_CHARS</code> / <code>SIDE_PAD</code> in <code>SuperSeparator.h</code>), and <code>"~~Text"</code> an ordinary item that shows <code>~Text</code>. The cursor skips separators and headings (encoder and touch), they are never selected or accepted, and a list with nothing but such rows cannot be edited.'],
 gui='<p>Encoder moves the cursor (stops at the ends or wraps). A short/long accepts the cursor row as the selected item. B short cancels; B long moves the cursor back to the selected item and keeps editing.</p><p>Touch: the highlighted row is the one under the finger. Touching highlights at once; sliding moves the highlight; lift accepts. Holding above the first or below the last visible row auto-scrolls (<code>AutoScrollMs</code>, default 120). Lifting off to the side or tapping outside cancels.</p>',
 events='<p>' + EV + '. <code>OnChange</code> = the cursor moved; <code>OnAccept</code> = a row was chosen (<code>GetSelected()</code> is already new).</p>',
 api=[('AddItem / SetItems / ClearItems','Content'),('SetSelected(i) / GetSelected()','Chosen index (-1 when empty)'),('GetSelectedText()','Chosen text'),('GetCursor()','Index under the cursor while editing'),('SetRows(n)','Visible rows')],
 tips=['Use the live <code>OnChange</code> for a preview of the item under the cursor.'],
 pitfalls=['An empty list returns false from <code>StartEdit()</code>.'],
 see=['<a href="combobox.html">SuperComboBox</a>','<a href="menu.html">SuperMenu</a>'])

add(slug='combobox', cls='SuperComboBox', header='SuperComboBox.h', kind='edit', short='Drop-down list',
 tag='A text box showing the selected item with a drop-down arrow; editing opens a list popup.',
 code='''SuperComboBox cb(&lcd, 10, 10, 160, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
cb.SetItems({"Auto", "Manual", "Eco"});
cb.SetBackdrop(&screenSprite);      // needed on SPI panels (see Pitfalls)''',
 features=['Owns a <code>SuperListBox</code> popup you can style through <code>ListBox()</code> (colours, border, RowPad, Wrap).','The popup opens below, or above if there is no room; up to <code>MaxRows</code> (5) items.','When it closes, the covered pixels are put back.','The box shows the item under the cursor live while the popup is open.',
  '<b>Unclickable rows:</b> an item <code>"~"</code> is a separator line, <code>"~Text"</code> a heading with the text centred in the line (<code>---- Text ----</code>, dim; a heading that is too long is cut, always leaving about a character plus 3 pixels of line on each side: <code>SuperSep::SIDE_CHARS</code> / <code>SIDE_PAD</code> in <code>SuperSeparator.h</code>), and <code>"~~Text"</code> an ordinary item that shows <code>~Text</code>. The cursor skips separators and headings (encoder and touch), they are never selected or accepted, and a list with nothing but such rows cannot be edited.'],
 gui='<p>Encoder moves through the list; A short/long accepts the item; B short cancels; B long returns to the selected item and keeps the popup open.</p><p>Touch: touching the closed box opens the list at once. Slide down into the list (the highlight follows) and lift to accept, or lift without sliding and the list stays open for a second touch. Tapping the box again or outside closes it.</p>',
 events='<p>' + EV + '. <code>OnChange</code> = cursor moved; <code>OnAccept</code> = item chosen.</p>',
 api=[('AddItem / SetItems / ClearItems','Content'),('SetSelected / GetSelected / GetValue','Chosen item'),('SetBackdrop(&sprite)','Where covered pixels come from'),('SetRestore(fn,user)','Redraw the covered area yourself'),('GetPopupX/Y/Width/Height','Popup rectangle (for your own redraw)')],
 tips=['If your destination is a sprite you need no extra setup.'],
 pitfalls=['Most SPI panels cannot be read back. Without <code>SetBackdrop</code>, a sprite destination or <code>SetRestore</code> the popup leaves garbage when it closes.','After the popup closes in a <a href="../concepts/form.html">form</a>, repaint the covered area with <code>form.Redraw(...)</code>.'],
 see=['<a href="listbox.html">SuperListBox</a>'])

add(slug='menu', cls='SuperMenu', header='SuperMenu.h', kind='edit', short='Nested menu',
 tag='Items with children open sub-menus; leaves are commands with your own id.',
 code='''SuperMenu m(&lcd, 10, 10, 200, &lgfx::fonts::Font2, 5, &Enc, &BtnA, &BtnB);
auto* settings = m.AddItem("Settings", 0);
m.AddItem("Brightness", 11, settings);
m.AddItem("Sound", 12, settings);
m.AddItem("About", 20);
if (m.BeginEdit()) { int id = m.GetResultId(); }''',
 features=['<code>AddItem(text, id, parent)</code>; a nullptr parent is the top level. Items are pooled in a deque so pointers stay valid.','Sub-levels start with a "&lt; Back" row (<code>ShowBack = false</code> hides it).','Grey triangles show hidden items.','Touches are ignored for <code>TouchGuardMs</code> (250) after the level changes, so a quick second touch cannot hit the Back row that just appeared.',
  '<b>Unclickable rows:</b> an item <code>"~"</code> is a separator line, <code>"~Text"</code> a heading with the text centred in the line (<code>---- Text ----</code>, dim; a heading that is too long is cut, always leaving about a character plus 3 pixels of line on each side: <code>SuperSep::SIDE_CHARS</code> / <code>SIDE_PAD</code> in <code>SuperSeparator.h</code>), and <code>"~~Text"</code> an ordinary item that shows <code>~Text</code>. The cursor skips separators and headings (encoder and touch), they are never selected or accepted, and a menu with nothing but such rows cannot be edited. In a menu they are leaf items (no sub-menu); the Form Designer gives them no Item ID or id.',
  '<code>ShowLevel(item)</code> draws a sub-menu level while the menu is idle (a preview for design tools; <code>nullptr</code> = the top level). <code>StartEdit()</code> always begins at the top level.'],
 gui='<p>Encoder moves the cursor. A short/long opens a sub-menu or chooses a leaf. B short = back one level (cancel at the top). B long = leave from any depth.</p><p>Touch: the row under the finger highlights; slide to follow (edge hold scrolls); lift opens / chooses / goes back. Lifting over blank space does nothing; lifting off to the side or tapping outside leaves; long press = back.</p>',
 events='<p>' + EV + '. <code>OnChange</code> = the cursor or level changed; <code>OnAccept</code> = a leaf was chosen. Read <code>GetResultId()</code> / <code>GetResultItem()</code>.</p>',
 api=[('AddItem(text,id,parent)','Returns the item pointer to use as a parent'),('GetResultId()','Chosen leaf id, -1 if none'),('GetResultItem()','Chosen item'),('GetCurrentLevel()','nullptr = top level'),('Clear()','Remove all items')],
 tips=['Choose ids with meaning (an enum of commands) rather than indices.'],
 pitfalls=['A non-leaf item never returns a result; only leaves do.'],
 see=['<a href="listbox.html">SuperListBox</a>','<a href="tabbar.html">SuperTabBar</a>'])

add(slug='tabbar', cls='SuperTabBar', header='SuperTabBar.h', kind='edit', short='Row of tabs',
 tag='Variable-width tabs; you show the page that belongs to the selected tab.',
 code='''SuperTabBar tabs(&lcd, 0, 0, 320, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
tabs.AddTab("Input"); tabs.AddTab("Values"); tabs.AddTab("Menu");
tabs.SetOnAccept([](void*){ showPage(tabs.GetSelected()); });''',
 features=['Tabs are as wide as their text plus <code>TabPad</code>.','When they do not fit, the bar scrolls and grey &lt; &gt; arrows show hidden tabs.','The bar draws only the tabs; the page below is your job (usually <code>SetVisible</code> on a control list + <code>form.Draw()</code>).'],
 gui='<p>Encoder moves the cursor along the tabs (stops at the ends). A short/long selects the tab under the cursor. B short = cancel; B long = cursor back to the selected tab.</p><p>Touch: one tap selects a tab (it acts on the first tap even when idle). The scroll arrows are indicators, not buttons.</p>',
 events='<p>' + EV + '. <code>OnChange</code> = cursor moved; <code>OnAccept</code> = a tab was selected: switch the page here.</p>',
 api=[('AddTab / ClearTabs','Content'),('SetSelected / GetSelected','Chosen tab'),('GetCursor()','Cursor while editing')],
 tips=['Switch pages with <code>SetVisible(v, false)</code> on each control and a single <code>form.Draw()</code> afterwards.'],
 see=['<a href="../concepts/form.html">SuperForm</a>'])

add(slug='colorpicker', cls='SuperColorPicker', header='SuperColorPicker.h', kind='edit', short='HSV colour picker',
 tag='A swatch plus three gradient bars (hue, saturation, value). Gives RGB565 or RGB888.',
 code='''SuperColorPicker cp(&lcd, 10, 10, 220, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
cp.SetColor565(TFT_ORANGE);
cp.SetOnAccept([](void*){ uint8_t r,g,b; cp.GetRGB(r,g,b); });''',
 features=['H 0-359, S 0-100, V 0-100; each bar shows the colour along that axis. The colour is stored as HSV.','Set and get as RGB565 (<code>SetColor565</code>), RGB888 (<code>SetRGB / GetRGB</code>) or HSV.'],
 gui='<p>Encoder changes the active component (marked with the cursor colour). A short = next component (H, S, V, H...). A long = accept. B short = cancel; B long = revert and keep editing.</p><p>Touch needs no activation: touching a band sets that component at once; dragging adjusts it live; lifting commits. Tapping the swatch or a long press starts / accepts an edit.</p>',
 events='<p>' + EV + '. <code>OnChange</code> = the colour changed live (use for previews); <code>OnAccept</code> = accepted.</p>',
 api=[('SetHSV / GetHSV','Components'),('SetRGB / GetRGB','8-bit'),('SetColor565','RGB565 in')],
 pitfalls=['Round-tripping RGB565 through HSV can change a low bit; keep your own copy if you need exact values.'],
 see=['<a href="slider.html">SuperSlider</a>'])

add(slug='checkbox', cls='SuperCheckBox', header='SuperCheckBox.h', kind='instant', short='Check box with a caption',
 tag='A check box with a caption. Instant: a click toggles it; there is no edit state.',
 code='''SuperCheckBox cb(&lcd, 10, 10, 160, &lgfx::fonts::Font2, "Enable sound");
cb.SetOnAccept([](void*){ sound = cb.GetChecked(); });''',
 features=['A square glyph about one font tall, caption on its right; a caption that does not fit is clipped.','<code>SetCaption</code> is separate from <code>SetLabel</code> (a label above / below).','<code>Toggle()</code> flips and fires callbacks; <code>SetChecked()</code> changes silently (no callbacks).'],
 gui='<p>Focus it with the encoder and press A short, or tap it: it flips at once. There is no cursor because nothing stays "being edited".</p>',
 events='<p>Both <code>OnChange</code> and <code>OnAccept</code> fire on a change, <code>OnChange</code> first. In a form it reports <code>EDIT_ACCEPTED</code> on the next <code>Service()</code>.</p>',
 api=[('SetChecked(b)','Silent'),('GetChecked()','State'),('Toggle()','With callbacks'),('SetCaption(s)','Caption text')],
 tips=['Use <code>SetChecked</code> to initialise from saved settings without triggering your handler.'],
 pitfalls=['<code>SetChecked</code> never calls your callbacks; update dependent UI yourself.'],
 see=['<a href="radiobutton.html">SuperRadioButton</a>'])

add(slug='radiobutton', cls='SuperRadioButton', header='SuperRadioButton.h', kind='instant', short='Pick-one buttons with a group',
 tag='A round check box. A SuperRadioGroup (not a control) keeps several of them exclusive.',
 code='''SuperRadioButton r1(&lcd, 10, 10, 120, &lgfx::fonts::Font2, "Red");
SuperRadioButton r2(&lcd, 10, 30, 120, &lgfx::fonts::Font2, "Green");
SuperRadioGroup g; g.Add(&r1); g.Add(&r2);
g.SetSelected(0);
g.SetOnAccept([](int index, void*){ pick(index); });''',
 features=['Place the buttons anywhere; the group only links them.','A selected button stays selected when clicked again.','A button without a group simply turns on.','Each button is its own stop for focus, the group is not a screen control.'],
 gui='<p>Instant like the check box: A short or a tap selects the button and clears the others.</p>',
 events='<p>The button\'s <code>OnChange</code> / <code>OnAccept</code> and the group\'s <code>OnChange</code> / <code>OnAccept</code> (passing the new index) fire, in that order.</p>',
 api=[('SuperRadioGroup::Add(&b)','Join a button'),('SetSelected(i)','Silent; -1 clears'),('Select(i)','With callbacks'),('GetSelected()','-1 when none'),('Get(i)','Button pointer')],
 tips=['Use the group callback with an index: one handler for all buttons.'],
 pitfalls=['The group stores pointers: keep the buttons alive as long as the group.'],
 see=['<a href="checkbox.html">SuperCheckBox</a>'])

add(slug='pushbutton', cls='SuperPushButton', header='SuperPushButton.h', kind='instant', short='Captioned button',
 tag='A captioned button with an instant click and a short inverted flash.',
 code='''SuperPushButton ok(&lcd, 10, 10, 100, &lgfx::fonts::Font2, "Save");
ok.SetOnAccept([](void*){ save(); });''',
 features=['A click, tap or <code>BeginEdit()</code> flashes the face inverted for <code>PressMs</code> (default 50 ms, blocking), fires <code>OnAccept</code> and returns true.','Caption centred in a bordered box.'],
 gui='<p>Focus it and press A short, or tap it.</p>',
 events='<p><code>OnAccept</code> only (OnChange runs just before it internally). <code>Service()</code> then reports <code>EDIT_ACCEPTED</code> once.</p>',
 api=[('SetCaption(s)','Change the text'),('PressMs','Flash length (theme: <code>PressMs</code>)')],
 pitfalls=['The flash blocks for <code>PressMs</code>; keep it short on a busy UI.'],
 see=['<a href="checkbox.html">SuperCheckBox</a>'])

add(slug='messagebox', cls='SuperMessageBox', header='SuperMessageBox.h', kind='edit', short='Modal message with 1-3 buttons',
 tag='A modal message that restores what it covered.',
 code='''SuperMessageBox box(&lcd, 220, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
box.SetMessage("Delete all data?\\nThis cannot be undone.");
box.AddButton("Yes"); box.AddButton("No"); box.SetDefault(1);
if (box.BeginEdit()) { int which = box.GetResult(); }''',
 features=['Centred on the destination (<code>SetCentered(false)</code> + <code>SetDisplayPos</code> to place it). Word-wrapped; <code>\\n</code> starts a new line.','<code>SetLabel()</code> adds a title.','Puts back the covered pixels like the combo popup: <code>SetBackdrop</code> or <code>SetRestore</code> on SPI panels.',
  '<b>Rich text (opt-in):</b> <code>SetRich(true)</code> lets the message use the tags of <a href="richtext.html">SuperRichText</a> (colour, underline, big text, <code>&lt;br&gt;</code> ...); lines are centred unless you say <code>&lt;left&gt;</code> / <code>&lt;right&gt;</code>. Off by default.',
  '<code>SetStyleFont(SuperMessageBox::FONT_BOLD / FONT_ITALIC / FONT_BOLDITALIC / FONT_HEADING, font)</code>: in rich mode <code>&lt;b&gt; &lt;i&gt; &lt;b&gt;&lt;i&gt; &lt;h1&gt;</code> use the fonts you register.'],
 gui='<p>Encoder moves between buttons (stops at the ends). A short/long chooses the highlighted one. B short cancels (returns false, result -1). B long moves back to the default button. Touch: tap a button to choose it.</p>',
 events='<p>' + EV + '. <code>OnChange</code> = highlighted button changed; <code>OnAccept</code> = a button was chosen; <code>GetResult()</code> is its index.</p>',
 api=[('SetMessage(text)','Message'),('AddButton(text)','Up to 3'),('SetDefault(i)','Initially highlighted'),('GetResult()','Chosen index or -1'),('SetBackdrop / SetRestore','Background restore')],
 tips=['Make the safe choice the default button.'],
 pitfalls=['Without a backdrop on an SPI panel the covered area is not restored correctly.'],
 see=['<a href="combobox.html">SuperComboBox</a>'])

add(slug='progressbar', cls='SuperProgressBar', header='SuperProgressBar.h', kind='display', short='Progress bar',
 tag='A display-only bar, 0..Max, with optional percent text.',
 code='''SuperProgressBar pb(&lcd, 10, 10, 200, &lgfx::fonts::Font2);
form.Add(&pb, false);          // shown, never focused
pb.SetRange(100); pb.SetValue(42);''',
 features=['<code>ColBar</code> is the bar colour (green by default). <code>ShowPercent = false</code> hides the percentage.','The percentage is centred by default: <code>SetPercentAlign(ALIGN_LEFT / ALIGN_CENTER / ALIGN_RIGHT)</code>. It is drawn in <code>ColFg</code>, the text colour, on the bar and off it (the old <code>ColBarFg</code> is gone).','<code>StartEdit()</code> returns false: it never takes focus.'],
 gui='<p>Nothing to operate. It redraws when <code>SetValue()</code> changes the value.</p>',
 events='<p>None. <code>SetValue</code> redraws immediately if the value changed.</p>',
 api=[('SetRange(max)','Maximum, minimum 1'),('SetValue(v)','Clamped to 0..Max'),('GetPercent()','Integer percent'),('ColBar','Bar colour'),('ShowPercent','Show the percentage at all'),('SetPercentAlign(a)','Left / centre / right')],
 tips=['Only call <code>SetValue</code> when the value actually changes; an unchanged value is skipped anyway.'],
 pitfalls=['Each push is about 7 ms on the S3, so do not update at kHz rates.'],
 see=['<a href="gauge.html">SuperGauge</a>'])

add(slug='graph', cls='SuperGraph', header='SuperGraph.h', kind='display', short='Scrolling line graph',
 tag='A display-only scrolling line graph.',
 code='''SuperGraph g(&lcd, 10, 10, 200, &lgfx::fonts::Font2, 80 /*body height*/);
form.Add(&g, false);
g.AddSample(sensor());''',
 features=['One sample per pixel of inner width by default (<code>SetCapacity(n)</code> changes that; the line is scaled).','Automatic vertical range (min..max of what is shown plus a margin) unless <code>SetRange(lo,hi)</code>.','<code>ShowRange</code> prints top / bottom values; <code>ShowGrid</code> draws three faint lines.','Memory: capacity x 4 bytes plus the sprite.'],
 gui='<p>Nothing to operate.</p>',
 events='<p>None. <code>AddSample(v, redraw=true)</code> appends and redraws; pass <code>false</code> to batch.</p>',
 api=[('AddSample(v, redraw)','Append'),('SetRange / SetAutoRange','Vertical scale'),('SetCapacity(n)','Samples kept'),('Clear()','Remove all')],
 tips=['Use a fixed range for stable visuals; auto range rescales as extremes scroll out.'],
 see=['<a href="gauge.html">SuperGauge</a>'])

add(slug='gauge', cls='SuperGauge', header='SuperGauge.h', kind='display', short='Dial with zones and MinMax lines',
 tag='A display-only dial with colour zones, ticks, a needle that takes the zone colour, and optional MinMax lines.',
 code='''SuperGauge g(&lcd, 10, 10, 200, 120, &lgfx::fonts::Font2);
g.SetRange(0, 100);
g.AddZone(0, 60, TFT_GREEN); g.AddZone(60, 85, TFT_YELLOW); g.AddZone(85, 100, TFT_RED);
g.SetFormat(0, "%"); g.SetLabel("Load");
g.SetMinMax(6000, 6.f);        // optional: min / max of the last 6 s as lines
form.Add(&g, false);
g.SetValue(42);''',
 features=['Sweep in degrees clockwise from 12 o\'clock: default -90..+90 (half dial); -135..+135 gives a 270 degree dial. A sweep that fits the upper half uses the half-dial layout.',
  'Zones paint coloured bands; with <code>NeedleByZone</code> (default) the needle and hub take the colour of the zone the value is in.',
  'Ticks: <code>SetTicks(major, minor)</code>; major ticks carry numbers when there is room (<code>ShowTickLabels</code>). <code>NEEDLE_TAPER</code> (default) or <code>NEEDLE_LINE</code>.',
  'Smoothing is on by default: <code>SetValue</code> starts a glide of about <code>SmoothMs</code>; only redraws while moving.',
  '<b>MinMax</b>: <code>SetMinMax(windowMs, minDegrees, thickness)</code> draws the lowest and highest value of the last <code>windowMs</code> as lines, only when they differ from the needle by at least <code>minDegrees</code>. Each line is centred on its value, <code>thickness</code> px thick, in the <em>inverted</em> colour of the dial segment under it. Old extremes age out by themselves.',
  'Built from LovyanGFX primitives; no bitmaps.'],
 gui='<p>Display only: nothing to operate. The user sees the needle glide to a new value and, in MinMax mode, the two extreme markers appear when the reading has moved away from them and disappear when the window forgets them.</p>',
 events='<p>None. Feed it with <code>SetValue()</code>. Animation is driven by <code>SuperControl::Tick()</code> from <code>SuperForm::Service()</code>; without a form call <code>Update()</code> from <code>loop()</code>.</p>',
 api=[('SetRange(min,max)','Value range'),('AddZone(from,to,colour)','Coloured band'),('SetSweep(startDeg,endDeg)','Dial angles'),('SetTicks(major,minor)','Tick marks'),('SetFormat(decimals, unit)','Value text'),('SetSmoothing(bool)','Glide on / off'),('SetMinMax(ms, deg, thick)','Window lines; windowMs 0 turns them off'),('SetValue(v)','Needle target')],
 tips=['Feed it at a modest rate (the demo uses every 500 ms); the glide fills the gaps.','Pick the window long enough to show a trend (seconds), and <code>minDegrees</code> a few degrees so a steady value shows no lines.'],
 pitfalls=['A sweep wider than a half dial makes the control centre a full circle and needs a taller body.','Without a <a href="../concepts/form.html">form</a> the glide and MinMax expiry only run if you call <code>Update()</code>.'],
 see=['<a href="progressbar.html">SuperProgressBar</a>','<a href="graph.html">SuperGraph</a>','<a href="userdraw.html">SuperUserDraw</a>'])

add(slug='userdraw', cls='SuperUserDraw', header='SuperUserDraw.h', kind='user', short='A control YOUR code draws',
 tag='The usual frame, label, focus and dimming; everything inside is yours.',
 code='''SuperUserDraw pad(&lcd, 10, 10, 160, 100 /*body height*/);
pad.SetBorder(1, TFT_WHITE); pad.SetLabel("Pad");
pad.SetOnDraw([](SuperUserDraw& c, lgfx::LGFX_Sprite* s, const SuperRect& r, void*) {
    s->drawCircle(r.x + r.w/2, r.y + r.h/2, 30, TFT_GREEN);
});
pad.SetAutoRedraw(50);            // animation''',
 features=['<b>Normal mode</b>: <code>spr</code> is the control\'s render sprite, valid only during the callback, arriving holding the background and frame. The callback runs on every redraw (including focus changes) and must paint everything.',
  '<b>Persistent canvas</b> (<code>SetPersistent(true)</code>): a body-size canvas that keeps what you drew (traces, sketch pads, games). The callback runs only on <code>Repaint()</code> or auto-redraw; focus changes just re-blit. Costs width x height x 2 bytes.',
  '<code>SetAutoRedraw(ms)</code> repaints on a timer (driven by SuperForm); <code>DeltaMs()</code> gives the time since the previous draw event.',
  '<b>Pointer events</b>: <code>SetOnPointer(fn)</code> receives PRESS, MOVE, TAP, LONG_PRESS and always RELEASE, positioned relative to the body (may be outside while dragging).',
  'Clip rectangle is already set to the body: drawing outside it is clipped.'],
 gui='<p>Without a pointer callback the control is display only (add with <code>focusable=false</code>). With one, touching the control focuses it and runs the gesture as an edit: it starts on touch-down, and when the finger lifts the edit ends and <code>SetOnAccept</code> fires. A key-started edit with no finger ends after <code>PointerGraceMs</code> (150 ms).</p>',
 events='''<pre><code>draw event:    void fn(SuperUserDraw&amp; ctl, lgfx::LGFX_Sprite* spr, const SuperRect&amp; body, void* user)
pointer event: void fn(SuperUserDraw&amp; ctl, const SuperUserPointer&amp; e, void* user)   // e.Type, e.X, e.Y, e.StartX, e.StartY</code></pre>
<p>Inside the pointer callback call <code>ctl.Invalidate()</code> or <code>Repaint()</code> to redraw; the control also refreshes after every pointer event.</p>''',
 api=[('SetOnDraw(fn,user)','Your drawing'),('SetOnPointer(fn,user)','Touch events'),('SetOnAccept(fn,user)','Gesture finished'),('SetPersistent(bool)','Own canvas that keeps its pixels'),('GetCanvas()','Draw outside the event (persistent mode)'),('Repaint() / Invalidate()','Ask for a redraw'),('SetAutoRedraw(ms)','Timer repaint'),('GetBodyRect()','Body rectangle in control coordinates')],
 tips=['Use persistent mode for anything incremental; use normal mode for things that are cheap to redraw (it needs no extra RAM).','Draw in the <code>r</code> rectangle you are given, not at fixed (0,0).'],
 pitfalls=['Do not keep the sprite pointer in normal mode, block in the callback or draw other controls from it.','The sprite is 16-bit byte-swapped RGB565: use LovyanGFX drawing calls, not the raw buffer. Avoid colour 0x0001 (transparent key when labels are not opaque).','Resizing the body recreates the persistent canvas: the picture is lost and the draw event runs again.'],
 see=['<a href="gauge.html">SuperGauge</a>','<a href="../concepts/new-control.html">Writing your own control</a>'])

add(slug='richtext', cls='SuperRichText', header='SuperRichText.h', kind='edit', short='Scrollable text with basic HTML-like tags',
 tag='A scrollable, read-only text area that understands a few HTML-like tags: colours, background, underline, strike, big text, alignment, tab stops, rules. One font, no syntax checking.',
 code='''SuperRichText help(&lcd, 10, 10, 220, 200, &lgfx::fonts::Font2, &Enc, &BtnA, &BtnB);
help.SetTextStatic("<center><color=yellow><big>Help</big></color></center>\\n"
                   "<hr>\\n"
                   "Turn the <u>encoder</u> to scroll.<br>Press A to close.");
help.BeginEdit();                    // the user scrolls; A closes (accepted), B closes (cancelled)''',
 features=['<b>Tags:</b> <code>&lt;br&gt;</code> <code>&lt;tab&gt;</code> <code>&lt;hr&gt;</code> <code>&lt;color=red&gt;..&lt;/color&gt;</code> (<code>#RGB</code>, <code>#RRGGBB</code> or a name) <code>&lt;bg=#003366&gt;..&lt;/bg&gt;</code> <code>&lt;font color=.. bg=..&gt;</code> <code>&lt;b&gt;</code> <code>&lt;i&gt;</code> <code>&lt;h1&gt;</code> (with style fonts) <code>&lt;u&gt;</code> <code>&lt;s&gt;</code> <code>&lt;big&gt;</code> (double size) <code>&lt;left&gt;</code> <code>&lt;center&gt;</code> <code>&lt;right&gt;</code>, and the entities <code>&amp;lt; &amp;gt; &amp;amp; &amp;quot; &amp;apos; &amp;nbsp; &amp;#65;</code>.',
  '<b>Lenient on purpose:</b> unknown tags (<code>&lt;p&gt;</code>, <code>&lt;img&gt;</code>, and <code>&lt;b&gt;</code> / <code>&lt;i&gt;</code> when no style font is registered) are ignored, a <code>&lt;</code> that does not start a tag is shown as it is, an unclosed tag stays in effect to the end. Nothing is ever reported: bad text shows as bad text. A closing tag returns to the control\'s default (there is no nesting stack).',
  '<b>One font by default.</b> Only what LovyanGFX does natively is supported: colours, filled backgrounds, lines (underline / strike / rule), text scaling and fonts you register. <b>Style fonts:</b> <code>SetStyleFont(SuperRichText::FONT_BOLD, &lgfx::fonts::FreeSansBold9pt7b)</code> (also <code>FONT_ITALIC</code>, <code>FONT_BOLDITALIC</code>, <code>FONT_HEADING</code>) makes <code>&lt;b&gt;</code> <code>&lt;i&gt;</code> <code>&lt;b&gt;&lt;i&gt;</code> and <code>&lt;h1&gt;</code> use that font (<code>&lt;strong&gt;</code>, <code>&lt;em&gt;</code>, <code>&lt;h2&gt;</code>, <code>&lt;h3&gt;</code> are the same tags). Fonts of different sizes on one line share a baseline. A slot with no font leaves its tag ignored, and a line is as tall as its tallest font. Costs nothing unless you register a font (it is then linked into your program).',
  'A plain new line in the text is a line break (like an editor), <code>&lt;br&gt;</code> too. Words wrap at the width, a word wider than the control is broken inside. Tab stops: <code>SetTabChars(n)</code>.',
  '<b>Memory:</b> <code>SetTextStatic(const char*)</code> does not copy: a string literal stays in flash. The layout is a table of 16 bytes per visual line (200 lines = 3 KB); the text is read again when drawn, never parsed into a tree. <code>SetText(std::string)</code> copies.',
  '<code>SetShowSource(true)</code> draws the text with every tag visible: handy to debug your markup.',
  'The same tags work in <a href="label.html">SuperLabel</a> and <a href="messagebox.html">SuperMessageBox</a> after <code>SetRich(true)</code> (off by default: plain text stays plain).'],
 gui='<p>' + KEYS + ' Like the list box it is edited by scrolling: the <b>encoder</b> scrolls a line, <b>ButtonA</b> (short or long) closes it (accepted), <b>ButtonB</b> short closes it (cancelled), <b>ButtonB long</b> jumps back to the top. Touch: drag to scroll. A thin bar at the right edge shows the position when the text is longer than the area. In code: <code>ScrollTo</code>, <code>ScrollBy</code>.</p>',
 events='<p>The ' + EV + ': <code>SetOnAccept</code> when the user closes the text with ButtonA. Use it to leave the help screen.</p>',
 api=[('SetText(s) / SetTextStatic(p)','Copy the text / point at a text that stays valid (flash)'),('SetShowSource(b)','Show the markup instead of interpreting it'),('SetTabChars(n)','Tab stop distance in spaces'),('SetLineGap(px)','Extra space between lines'),('ScrollTo(line) / ScrollBy(n)','Scroll in code'),('GetLineCount() / GetFirstLine() / NeedsScroll()','Layout information'),('SetTextColor(c) / SetAlign(a)','Default colour and alignment (what closing tags return to)'),('StartEdit() / Service() / BeginEdit()','Scrolling, the usual contract')],
 tips=['Author the text in the Form Designer: the Rich text control has a source editor with a Rendered view (see <a href="../designer/designer-usage.html">Using the designer</a>).','The Form Designer\'s own <b>?</b> help window is a SuperRichText: <code>tools/FormDesigner/src/HelpDialog.h</code> is a complete example.','Use <code>&lt;tab&gt;</code> for two-column lists (key, description): set the stop with <code>SetTabChars</code>.'],
 pitfalls=['Alignment applies to a whole line and is taken where the line\'s first text starts: <code>&lt;center&gt;Title&lt;/center&gt;</code> centres that line, <code>text &lt;center&gt;more</code> does not.','There is no HTML whitespace collapsing: spaces and new lines are kept as written.','A text longer than the area needs the encoder (or touch) to scroll: add the control to a form so it can take focus, or scroll it with <code>ScrollBy</code>.'],
 see=['<a href="label.html">SuperLabel</a>','<a href="messagebox.html">SuperMessageBox</a>','<a href="listbox.html">SuperListBox</a>'])
