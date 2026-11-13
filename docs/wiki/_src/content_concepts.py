def box(kind, t, x): return f'<div class="box {kind}"><b class="h">{t}</b>{x}</div>'
def flow(*steps): return '<div class="flow">' + '<i>&rarr;</i>'.join(f'<span>{s}</span>' for s in steps) + '</div>'

CONCEPTS = []
def add(slug, title, short, body): CONCEPTS.append(dict(slug=slug, title=title, short=short, body=body))

add('getting-started', 'Getting started', 'Blank project to a working control', '''
<p>Every control needs three things: a LovyanGFX destination (a display or a sprite), a position and width, and, for the editable ones, a <code>SuperEncoder</code> and two <code>SuperButton</code>s.</p>
<pre><code>#include &lt;LovyanGFX.hpp&gt;
#include "SuperSpinBox.h"

LGFX         lcd;                         // your LovyanGFX display class
SuperEncoder Enc(2, 3);                   // A / B pins
SuperButton  BtnA(4, true), BtnB(5, true);// active low

SuperSpinBox spin(&amp;lcd, 10, 10, 120, &amp;lgfx::fonts::Font2, &amp;Enc, &amp;BtnA, &amp;BtnB);

void setup() {
  lcd.init(); lcd.fillScreen(TFT_BLACK);
  Enc.Begin(); BtnA.Begin(INPUT_PULLUP); BtnB.Begin(INPUT_PULLUP);
  spin.SetRange(0, 100); spin.SetLabel("Volume"); spin.SetValue(50);
  spin.DrawPreview(true);                 // draw it (with the focus box)
}
void loop() { if (spin.BeginEdit()) { /* accepted: spin.GetValue() */ } }</code></pre>
<h2>Three ways to run a control</h2>
<table><tr><th>Style</th><th>How</th><th>Use when</th></tr>
<tr><td>Blocking</td><td><code>if (c.BeginEdit()) ...</code> returns when the user accepts or cancels</td><td>A single modal dialog, simple sketches</td></tr>
<tr><td>Non-blocking</td><td><code>c.StartEdit()</code> then call <code>c.Service()</code> from <code>loop()</code></td><td>Anything that must keep running (sensors, animation, networking)</td></tr>
<tr><td>Form</td><td><code>SuperForm</code> owns the focus and the edit of many controls</td><td>A real screen of controls. <b>Recommended.</b></td></tr></table>
<p>Most screens should use <a href="form.html">SuperForm</a>. It also drives touch, animation timers (gauge glide, auto-redraw) and redraws.</p>
''')

add('architecture', 'Architecture and class map', 'How the controls are built', '''
<p>Every control <b>draws into its own <code>LGFX_Sprite</code></b> and pushes itself to the destination (<code>Dst</code>) at <code>(PosX, PosY)</code>. Headers depend only on LovyanGFX, SuperGUI's encoder/button classes and the C++ standard library, so the same control runs on a Pico, an ESP32-S3 and the SDL desktop emulator.</p>
<pre><code>SuperControl            frame: border (standard / 3D / Sleek), padding, label, theme, enabled/visible, DrawPreview
 ├─ SuperTextBox        single-line text, scrolling, per-cell styles
 │   ├─ SuperInputBox   template input (# A a B *), validator
 │   ├─ SuperFieldBox   several numeric fields + separators
 │   │   ├─ SuperTimeBox     HH:MM[:SS]
 │   │   └─ SuperDateBox     year / month / day
 │   ├─ SuperSpinBox    integer spinner
 │   └─ SuperComboBox   drop-down (owns a SuperListBox popup)
 ├─ SuperListBox   SuperMenu   SuperTabBar
 ├─ SuperSlider   SuperColorPicker   SuperMessageBox
 ├─ SuperCheckBox ── SuperRadioButton (+ SuperRadioGroup)
 ├─ SuperPushButton
 ├─ SuperLabel   SuperGauge   SuperProgressBar   SuperGraph      display only
 └─ SuperUserDraw       your code draws it
SuperEditIO  encoder + A + B plumbing       SuperForm  focus manager       SuperTheme  colour sets</code></pre>
<h2>The frame every control shares</h2>
<p>The base class <code>SuperControl</code> provides everything around the body: border size/colour/style, padding, an optional label (above or below, own font/colour/alignment), colours, <code>SetEnabled</code>, <code>SetVisible</code>, <code>ApplyTheme</code>, <code>DrawPreview</code>. A subclass only implements <code>RenderBody()</code> (and <code>BodyHeight()</code> if the body is not one font line).</p>
<h2>Instant, editable, display only</h2>
<ul><li><b>Editable</b> controls enter an edit session that ends in accept or cancel (<a href="edit-contract.html">edit contract</a>).</li>
<li><b>Instant</b> controls (check box, radio button, push button) act in <code>StartEdit()</code> and report <code>EDIT_ACCEPTED</code> on the next <code>Service()</code>.</li>
<li><b>Display only</b> controls (label, progress bar, graph, gauge) return false from <code>StartEdit()</code>; add them with <code>form.Add(&amp;c, false)</code>.</li></ul>
<h2>The sprite model</h2>
<p>A control renders its whole picture (frame, label, body) into a sprite, optionally dims it, then pushes it in one go. That is why there is no flicker and why labels can be transparent (key colour <code>0x0001</code>). The price is RAM: <code>width x height x 2</code> bytes per control. See <a href="memory-performance.html">Memory and performance</a>.</p>
''')

add('edit-contract', 'The edit contract', 'StartEdit / Service / callbacks, identical for all controls', '''
<table><tr><th>Call</th><th>Meaning</th></tr>
<tr><td><code>StartEdit()</code></td><td>Non-blocking begin. Returns false if it cannot edit (empty list, display only...).</td></tr>
<tr><td><code>Service()</code></td><td>Call often. Returns <code>EDIT_ACTIVE</code> while editing, then <code>EDIT_ACCEPTED</code> or <code>EDIT_CANCELLED</code> <b>once</b>, then <code>EDIT_IDLE</code>.</td></tr>
<tr><td><code>BeginEdit()</code></td><td>Blocking wrapper. False if disabled/hidden; true only if accepted.</td></tr>
<tr><td><code>SetOnChange(fn,user)</code></td><td>The value being edited changed (live). List / combo / tabs / menu: the cursor moved.</td></tr>
<tr><td><code>SetOnAccept(fn,user)</code></td><td>Accepted. The stored value is already the new one.</td></tr>
<tr><td><code>SetAbort(fn)</code></td><td>Polled in <code>Service()</code>; returning true cancels (desktop: <code>Quitting()</code>).</td></tr></table>
''' + flow('StartEdit()', 'Service() = EDIT_ACTIVE ...', 'EDIT_ACCEPTED or EDIT_CANCELLED (once)', 'EDIT_IDLE') + '''
<h2>The rule that matters most</h2>
''' + box('note', 'Stored values change only on accept', '<p>While editing, the control works on a copy. <code>GetValue()</code> keeps returning the old value until the user accepts. Cancel (B short) throws the copy away; B long reverts the copy to the start and keeps editing.</p>') + '''
<h2>Callbacks in detail</h2>
<ul><li><code>OnChange</code> fires on every live change: use it for previews (a colour swatch, a volume you want to hear while turning).</li>
<li><code>OnAccept</code> fires once when the edit is accepted: use it to store the setting.</li>
<li>Instant controls fire both, <code>OnChange</code> first. The push button has <code>OnAccept</code> only. The radio group passes the new index.</li>
<li>Callbacks are plain function pointers <code>void (*)(void* user)</code>; pass a captureless lambda and put your object in <code>user</code>.</li></ul>
<pre><code>spin.SetOnAccept([](void* u){ auto* s = (SuperSpinBox*)u; save(s-&gt;GetValue()); }, &amp;spin);</code></pre>
<h2>Non-blocking loop</h2>
<pre><code>spin.StartEdit();
for (;;) {
  SuperEditState st = spin.Service();
  if (st == EDIT_ACCEPTED) { ...; break; }
  if (st == EDIT_CANCELLED) break;
  doOtherWork();
}</code></pre>
''')

add('input', 'Input: encoder and buttons', 'The key scheme every control follows', '''
<table><tr><th>Input</th><th>Action</th></tr>
<tr><td>Encoder</td><td>Move / change. <code>SuperEncoder</code> already scales speed (1 / 5 / 20); lists with 20 items or fewer step by sign only.</td></tr>
<tr><td>Button A short</td><td>Select / next slot. Input box: edit the slot. Colour picker: next component. Menu / list / tabs: choose.</td></tr>
<tr><td>Button A long</td><td>Accept.</td></tr>
<tr><td>Button B short</td><td>Cancel. Menu: back one level.</td></tr>
<tr><td>Button B long</td><td>Revert and keep editing. Menu: leave.</td></tr>
<tr><td>Medium click, double flag</td><td>Ignored on purpose: a reset must feel intentional.</td></tr></table>
<h2>Desktop emulation</h2>
<p>Mouse wheel = encoder, wheel click = Button A, right click = Button B (<code>desktop_input.h</code>). The left mouse button is the touch panel.</p>
<h2>Where touch and keys meet</h2>
<p>Touch events are converted into the same keys and encoder steps (they are <em>injected</em> into the control's <code>SuperEditIO</code>), so a user can start with the finger and finish with the encoder in the same edit.</p>
<div class="box warn"><b class="h">Pins</b>A phantom Button B or random cancels on a touch board usually means a button or encoder pin collides with another part's pin such as <code>TOUCH_INT</code>. Check <code>Pins.h</code> before choosing pins.</div>
''')

add('touch', 'Touch', 'The press / drag / lift model', '''
<p>Touch is optional. Give the form a <code>SuperTouch</code> wrapped around <code>lcd.getTouch()</code> and call <code>form.Service()</code>; every control then understands fingers.</p>
<pre><code>SuperTouch touch(readFn); touch.ClickD_MAX = 0;
form.SetTouch(&amp;touch);</code></pre>
<h2>The model</h2>
''' + flow('Finger down (PRESS): highlight the slot under it', 'Drag: change / move', 'Lift: accept') + '''
<table><tr><th>Gesture</th><th>Idle control</th><th>Control being edited</th></tr>
<tr><td>Tap</td><td>Focus it and start editing (one tap)</td><td>Same as A short; <b>tap outside = cancel</b></td></tr>
<tr><td>Long press</td><td>-</td><td>Same as A long (accept)</td></tr>
<tr><td>Drag up/down</td><td>-</td><td>Encoder steps, <code>SuperControl::TouchDragStepPx</code> pixels per step (default 4)</td></tr></table>
<h2>Which controls react how</h2>
<ul><li><b>StartOnPress</b> controls start editing on touch-down: input box family, time/date box, spin box, list box, combo box (press opens it), menu, slider, colour picker bands.</li>
<li><b>IdleTapActs</b> controls act on the first tap: tab bar and menu. Instant controls (check box, radio button, push button) simply act on a tap.</li>
<li><b>Precise picking</b>: list, combo, menu, tabs, message box, slider and colour picker map the finger position directly to the item or value.</li></ul>
<h2>Under the hood</h2>
<p><code>SuperForm::PumpTouch()</code> converts <code>SuperTouchEvent</code>s into <code>SuperPointerEvent</code>s (PRESS, DRAG_START, DRAG_MOVE, DRAG_END, TAP, LONG_PRESS) and routes them to the edited control or the form's active one. Controls override <code>OnPointer()</code>; most call <code>SuperPointerDefault(IO, *this, e, stepPx)</code>.</p>
<h2>Globals</h2>
<table><tr><th>Setting</th><th>Default</th><th>Effect</th></tr>
<tr><td><code>SuperControl::TouchPad</code></td><td>4</td><td>Extra pixels around a control that still count as a hit</td></tr>
<tr><td><code>SuperControl::TouchDragStepPx</code></td><td>4</td><td>Pixels of drag per encoder step (smaller = more sensitive)</td></tr>
<tr><td><code>TouchAcceptOnLift</code></td><td>true</td><td>Per control: lift accepts. False = lift only selects, long press accepts</td></tr></table>
''' + box('warn', 'Desktop mouse coordinates', '<p>Never map SDL mouse coordinates by hand: the window is resizable and hi-dpi. Use LovyanGFX\'s own SDL touch emulation through <code>lcd.getTouch()</code>.</p>') + box('tip', 'Drag anchors', '<p>A DRAG_START event can already carry a moved position (moves are merged). Anchor drags at <code>StartX/StartY</code>.</p>'))

add('events', 'Events and workflow', 'How input travels through the library', '''
<h2>From finger or knob to your callback</h2>
<pre><code>Encoder ISR / button debounce          touch panel (lcd.getTouch)
        \\                                   |
         SuperEditIO  &lt;--- Inject keys / steps --- SuperForm::PumpTouch -> OnPointer()
              |
      control.Service()  -> changes its working copy -> IO.Changed()  =&gt; OnChange
              |
      A long / lift      -> commits the copy          -> IO.Accepted() =&gt; OnAccept
              |
      SuperForm::SetOnEdit(fn)  fn(control, EDIT_ACCEPTED | EDIT_CANCELLED, user)</code></pre>
<h2>Three places to listen</h2>
<table><tr><th>Where</th><th>Granularity</th><th>Best for</th></tr>
<tr><td>Control <code>SetOnChange</code></td><td>Every live change of one control</td><td>Previews, live read-outs</td></tr>
<tr><td>Control <code>SetOnAccept</code></td><td>One control, once per accepted edit</td><td>Saving a setting</td></tr>
<tr><td>Form <code>SetOnEdit</code></td><td>Any control, accepted or cancelled</td><td>One central handler, page switching, logging</td></tr></table>
<h2>Instant controls</h2>
<p>Check box, radio button and push button act in <code>StartEdit()</code> and report <code>EDIT_ACCEPTED</code> on the next <code>Service()</code>; their <code>OnChange</code> and <code>OnAccept</code> fire together.</p>
<h2>Animation clocks</h2>
<p><code>SuperControl::Tick()</code> is driven by <code>SuperForm::Service()</code>. The gauge glide and MinMax expiry and the user-draw auto-redraw use it; without a form call the control's <code>Update()</code> from <code>loop()</code>.</p>
''' + box('tip', 'Keep callbacks short', '<p>Callbacks run inside <code>Service()</code>, i.e. inside the UI loop. Set a flag or store a value; do not block, and do not open a modal control from inside another control\'s <code>OnChange</code>.</p>'))

add('form', 'SuperForm: focus and screens', 'Focus handling, pages, redraws', '''
<pre><code>SuperForm form(&amp;Enc, &amp;BtnA);
form.Add(&amp;spin); form.Add(&amp;slider); form.Add(&amp;bar, false);   // false = shown, never focused
form.SetOnEdit(onEdit, nullptr);
form.Draw();
void loop() { form.Service(); }</code></pre>
<h2>What it does</h2>
<ul><li>The encoder moves the focus box (<code>DrawPreview(true)</code> on the focused control, dimmed <code>DrawPreview(false)</code> on the rest).</li>
<li>Button A short starts the focused control's edit; <code>Service()</code> runs it without blocking.</li>
<li>Polls the touch panel and routes touch.</li>
<li>Calls <code>Tick()</code> on controls (animation), and batches DMA pushes during <code>Draw()</code>.</li></ul>
<h2>Pages</h2>
<p>For tabbed screens: put each page's controls in a list, hide the old page with <code>SetVisible(false, false)</code>, show the new one the same way, then call <code>form.Draw()</code> once. Hidden controls are erased with <code>ColHighlightClear</code> (your screen background) and free their sprite.</p>
<h2>Drawing order and focus order</h2>
<table><tr><th>Setting</th><th>Effect</th></tr>
<tr><td><code>ctl.SetZOrder(n)</code></td><td>Drawing order for overlapping controls. The <b>lowest number is in front</b> (drawn last). Equal numbers are drawn in the order they were added. Touch hits the front-most control first.</td></tr>
<tr><td><code>ctl.SetTabOrder(n)</code></td><td>Focus order. The encoder visits the lowest number first and wraps round. Equal numbers are visited in the order they were added. Until you call <code>form.SetFocus()</code> (or touch a control) the first control in this order holds the focus.</td></tr></table>
<p>Both default to 0, which is simply the order of <code>form.Add()</code>. They are plain numbers on the control, so gaps are fine (10, 20, 30 lets you slot one in later).</p>
<pre><code>c.SetTabOrder(0); a.SetTabOrder(1); b.SetTabOrder(2);   // wheel goes C, A, B
back.SetZOrder(1); front.SetZOrder(0);                    // 'front' is drawn over 'back'</code></pre>
<div class="box warn"><b class="h">Overlap and redraws</b>Order is applied when the form draws everything (<code>Draw()</code>, <code>Redraw()</code>). A single focus change or animation tick redraws just that control, so on overlapping controls call <code>form.Draw()</code> after such changes if the stacking must stay exact.</div>
<h2>Popups</h2>
<p>When a popup (combo list, message box) closes, call <code>form.Redraw(x, y, w, h, bg)</code> to repaint the covered rectangle.</p>
''' + box('warn', 'Set the backdrop colour', '<p>Hidden controls and focus boxes are erased with <code>ColHighlightClear</code> or a theme <code>Backdrop</code>. If your screen is not black, set it or you will leave black holes.</p>'))

add('theming', 'Themes and styles', 'Colours, borders, Sleek, label defaults', '''
<h2>Theme</h2>
<pre><code>SuperTheme t = SuperTheme::Dark();     // or Light(), or your own
t.Border = TFT_CYAN; t.Backdrop = TFT_BLACK;
t.LabelFont = &amp;lgfx::fonts::Font0; t.LabelAlign = 0; t.BorderStyle = 2;
ctl.ApplyTheme(t);                     // colours, border size/style, padding, label defaults</code></pre>
<p>Theme fields: <code>Bg Fg Arrow Border Bezel BezelEdge Label Highlight Cursor CursorFg Sel SelFg Backdrop BorderSize BorderStyle Padding PressMs LabelFont LabelAlign</code>. <code>LabelFont = nullptr</code> and <code>LabelAlign = -1</code> mean "leave each control's own".</p>
<h2>Border styles</h2>
<table><tr><th>Style</th><th>Look</th></tr>
<tr><td><code>BORDER_STANDARD</code></td><td>Plain frame of <code>SetBorder(size, colour)</code></td></tr>
<tr><td><code>BORDER_THREED</code></td><td>Bevelled frame (<code>SetBezelColors</code>)</td></tr>
<tr><td><code>BORDER_SLEEK</code></td><td>One-colour frame with the label sitting on the frame line like a fieldset legend. Left / right alignment draws a short stub; a too-long label is cut and ends in "...". The visible frame extends half a label height beyond the control's box.</td></tr></table>
<figure><img src="../img/sleek.png" alt="Sleek style" style="image-rendering:pixelated;max-width:100%"><figcaption>Sleek style: the label sits on the frame line</figcaption></figure>
<h2>Labels</h2>
<p><code>SetLabel(text, LABEL_ABOVE | LABEL_BELOW)</code>, <code>SetLabelFont</code>, <code>SetLabelColor</code>, <code>SetLabelAlign</code>, <code>SetLabelGap</code>. By default a label uses the control's body font at construction (so a big-font box gets a big label) and left alignment.</p>
<h2>Focus look</h2>
<p><code>DrawPreview</code>: unfocused = normal look dimmed to <code>PreviewBrightness</code> (default 85 %); focused = full brightness plus a highlight box. There is never a cursor outside a real edit. <code>SetEnabled(false)</code> draws the control dimmed and makes it uneditable and skipped by the form.</p>
''')

add('memory-performance', 'Memory and performance', 'Sprites, shared buffer, DMA, PSRAM', '''
<p>Each control owns one 16-bit sprite: <code>width x height x 2</code> bytes, where height = font height + 2 x (border + padding) [+ label height]. On a Pico W (264 KB RAM) this matters; on an S3 with PSRAM it rarely does.</p>
<table><tr><th>Switch</th><th>What it does</th><th>When</th></tr>
<tr><td><code>SuperControl::SharedBuffer = true</code></td><td>One render buffer, the size of the biggest control, shared by all. Identical pixels, about the same speed.</td><td>RAM is tight</td></tr>
<tr><td><code>SuperControl::UseDMA = true</code></td><td>Two alternating DMA buffers; render the next control while the last one is sent.</td><td>SPI panels, with SharedBuffer</td></tr>
<tr><td><code>SuperControl::LabelOpaque = true</code></td><td>Solid label background instead of key-colour transparency (DMA needs it).</td><td>With DMA</td></tr>
<tr><td><code>SuperControl::UsePsram = true</code></td><td>Allocate sprites in PSRAM.</td><td>ESP32-S3 with PSRAM</td></tr>
<tr><td><code>SetVisible(v, false)</code> + one <code>Form.Draw()</code></td><td>Switch pages without a redraw per control. Hidden controls free their sprite.</td><td>Page changes</td></tr></table>
<h2>Measured (Freenove S3, 80 MHz SPI, octal PSRAM)</h2>
<p>Full page redraw 28 ms (was 44 ms); drags about 80 fps; internal heap about 162 KB free with 2 x 78 KB DMA buffers. Slider / progress / graph pushes are about 7 ms each; partial (dirty-rectangle) redraws are not built.</p>
<h2>Counters</h2>
<p><code>SuperControl::CollectStats = true</code> accumulates <code>StatDraws, StatRenderUs, StatDimUs, StatPushUs</code>; <code>ResetStats()</code> clears them.</p>
''' + box('warn', 'Blank control, only the focus box shows', '<p>Sprite allocation failed (out of RAM). The control draws a red outline when it happens. Turn on <code>SharedBuffer</code> or <code>UsePsram</code>, or reduce the number of visible controls.</p>'))

add('pitfalls', 'Pitfalls and fixes', 'Problems that already cost time', '''
<table><tr><th>Symptom</th><th>Cause / fix</th></tr>
<tr><td>Ghost pixels, label area not transparent</td><td><code>pushSprite</code> key must be <code>lgfx::rgb565_t(KeyColor)</code>; a plain int is read as RGB888. KeyColor = 0x0001, so avoid that colour in your own drawing.</td></tr>
<tr><td>Sprite buffer maths gives garbage</td><td>16-bit LGFX buffers are <b>byte-swapped</b> RGB565; <code>readRect</code> into a sprite needs <code>(lgfx::swap565_t*)</code>.</td></tr>
<tr><td>Combo / message box restore is garbage</td><td>Most SPI panels cannot <code>readRect</code>. Use <code>SetBackdrop(&amp;sprite)</code>, a sprite destination, or <code>SetRestore(fn)</code>.</td></tr>
<tr><td>Wheel events missing (desktop)</td><td>Panel_sdl swallows them; the library uses <code>SDL_AddEventWatch</code>.</td></tr>
<tr><td>Huge value jumps while testing</td><td>Encoder speed scaling on rapid ticks (spinner and slider keep it on purpose).</td></tr>
<tr><td>Hidden controls leave a coloured patch</td><td>Set <code>ColHighlightClear</code> (or a theme <code>Backdrop</code>) to your screen background.</td></tr>
<tr><td>Blank control, red outline</td><td>Out of RAM. See <a href="memory-performance.html">Memory and performance</a>.</td></tr>
<tr><td>S3: "PSRAM chip is not connected"</td><td>The module's PSRAM is octal: <code>board_build.psram_type = opi</code>, <code>memory_type = qio_opi</code>. Forcing quad PSRAM kills it.</td></tr>
<tr><td>Phantom button B / random cancels on a touch board</td><td>A button or encoder pin collides with <code>TOUCH_INT</code> or another part. Check <code>Pins.h</code>; keep <code>USE_ENCODER</code> / <code>USE_BUTTONS</code> false until fitted.</td></tr>
<tr><td>Upload fails "Could not open /dev/cu.usbmodemXXXX"</td><td>VS Code PlatformIO saved a per-project <code>customPort</code>. Reset the status-bar port selector to Auto.</td></tr>
<tr><td>Slow redraw on SPI</td><td><code>SharedBuffer</code> + <code>UseDMA</code> + <code>LabelOpaque</code>; page changes with <code>SetVisible(v,false)</code> + one <code>Draw()</code>.</td></tr></table>
<h2>Portability rules for the headers</h2>
<ul><li>Only LovyanGFX, SuperGUI and the C++ standard library. No SDL, no desktop code, no <code>&lt;chrono&gt;</code> or threads.</li>
<li>Use <code>lgfx::millis()</code> / <code>lgfx::delay()</code>, not Arduino-only calls, inside the controls.</li></ul>
''')

add('new-control', 'Writing your own control', 'The recipe', '''
<p>Two options: use <a href="../controls/userdraw.html">SuperUserDraw</a> (no subclassing, you draw in a callback), or derive a real control.</p>
<ol><li>Derive from <code>SuperControl</code> (or <code>SuperTextBox</code> for text). Implement <code>RenderBody(x,y,w,h)</code>, plus <code>BodyHeight()</code> if the body is not one font line.</li>
<li>Editable? Own a <code>SuperEditIO IO</code>; override <code>StartEdit / Service / IsEditing</code>. Call <code>IO.Flush()</code> on start, <code>IO.Changed()</code> on a live change, <code>IO.Accepted()</code> after an accepted <code>Finish()</code>. Draw the cursor only while <code>State == EDIT_ACTIVE</code>.</li>
<li>Touch: override <code>OnPointer()</code> and usually call <code>SuperPointerDefault(IO, *this, e, stepPx)</code>; <code>StartOnPress()</code> / <code>IdleTapActs()</code> choose when it starts.</li>
<li>Animated? Override <code>Tick()</code> and return true when a redraw is due.</li>
<li>Name it <code>Super&lt;Thing&gt;.h</code>, add a README row, an <code>examples/&lt;Name&gt;Example</code> and a demo page.</li>
<li>Compile for both micros, run a headless drive, look at the picture.</li></ol>
''')
