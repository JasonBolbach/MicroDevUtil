"""Wiki pages for the Form Designer (tools/FormDesigner). Same helpers as content_concepts.py."""
def box(kind, t, x): return f'<div class="box {kind}"><b class="h">{t}</b>{x}</div>'
def tip(x): return box('tip', 'Tip', x)
def warn(x): return box('warn', 'Watch out', x)
def fig(src, cap): return f'<figure><img src="../img/{src}" alt="{cap}" style="max-width:100%"><figcaption>{cap}</figcaption></figure>'

DESIGNER = []
def add(slug, title, short, body): DESIGNER.append(dict(slug=slug, title=title, short=short, body=body))

add('designer-start', 'Form Designer: your first form', 'Build, place, export, run', f'''
<p>The Form Designer is a desktop program (macOS, Linux, Windows) for laying out a SuperGUI screen with the mouse. It draws the <b>real SuperGUI controls</b>, so what you see is what the device draws, and it writes the C++ for you: a header that is regenerated every time, and your own file that is written once and never touched again.</p>
{fig('designer_main.png', 'The designer: toolbox, design surface (a form with tab pages), property panel')}
<h2>Build and start it</h2>
<pre><code>brew install sdl2 cmake            # macOS (Linux: your package manager's SDL2 + cmake)
cd tools/FormDesigner
cmake -S . -B build
cmake --build build -j
build/FormDesigner</code></pre>
<p>LovyanGFX is looked up next to the SuperGUI folder (<code>../../../LovyanGFX</code>); otherwise add <code>-DLGFX_ROOT=/path/to/LovyanGFX</code>. On Windows point <code>CMakeLists.txt</code> at your SDL2 development files.</p>
<h2>Your first form in ten steps</h2>
<ol>
<li><b>Name and size the form.</b> With nothing selected the right panel edits the form: Name, Width, Height (32x32 to 1024x768) and Back color. Use your display's real size.</li>
<li><b>Click a toolbox item, then click the form</b> to place a control. It snaps to the grid (press <code>G</code> to change the grid).</li>
<li><b>Click a control to select it.</b> Drag to move it, drag the handles on its left / right edge to resize it, arrow keys nudge (Shift = one grid step), Delete removes it, Tab selects the next one.</li>
<li><b>Name it.</b> The Name is the C++ member you will use in code (<code>okBtn</code>, <code>volume</code>). Names must be safe C++ names; the field turns red and says why when they are not.</li>
<li><b>Set its properties</b> in the right panel: text, colours, font (click the Font row for the font dialog), border, and the control-specific rows (value, range, items ...). Only values that differ from the control's default are saved and generated.</li>
<li><b>Save</b> with Ctrl/Cmd+S. A file dialog always opens, on the form's own file; type another name to save under it.</li>
<li><b>Export</b> with Ctrl/Cmd+X. A folder dialog opens on the folder used last (saved in the form file); choose the folder for the code and click OK. It writes <code>Form1.designer.h</code> (regenerated every time) and, the first time, <code>Form1.h</code> (yours).</li>
<li><b>Copy both files into your sketch</b> and write your code in <code>Form1.h</code>: the handlers (<code>okBtn_Click()</code> ...) are already there as empty stubs.</li>
<li><b>Run it:</b> <code>Form1 ui(&amp;lcd, &amp;Enc, &amp;BtnA, &amp;BtnB);</code> then <code>ui.Show();</code> in <code>setup()</code> and <code>ui.Service();</code> in <code>loop()</code>.</li>
<li><b>Change the layout later:</b> edit in the designer, export again. Your <code>Form1.h</code> is never overwritten.</li>
</ol>
{tip('You do not have to start from nothing: every new form already contains a ready-made <code>msgYesNo</code> confirmation box (see <a href="designer-boxes.html">Message boxes</a>). Delete it if you do not need it.')}
<h2>What the designer can place</h2>
<p>Label, Push button, Check box, Radio button, Progress bar, Spin box, Slider, Text box, Input box, Time box, Date box, List box, Combo box, Tab bar, Menu, Gauge, Graph, User draw and Color picker. Each is described on its own page under <i>Controls</i>; the designer page for a control only adds how its properties are edited. Message boxes are not placed on the surface: they live in a list (see below).</p>
''')

add('designer-usage', 'Using the designer', 'Surface, panel, zoom, files, shortcuts', f'''
<h2>The three panels</h2>
<table><tr><th>Panel</th><th>What it does</th></tr>
<tr><td>Toolbox (left)</td><td>Click a type, then the form, to place it. Below it, <i>Controls on form</i> lists every control with its control group and type: click one to select it (handy for controls that are hidden or hard to hit).</td></tr>
<tr><td>Design surface (middle)</td><td>The form drawn by the real controls, pixel-exact, scaled by the zoom. Place, select, drag, resize here. A thin yellow frame is the form's edge.</td></tr>
<tr><td>Property panel (right)</td><td>The selected control's properties, or the form's when nothing is selected. Changes apply live. The panel scrolls with the wheel when a control has many rows.</td></tr></table>
<h2>Mouse and keys</h2>
<table><tr><th>Do this</th><th>Result</th></tr>
<tr><td>Click toolbox item, click form</td><td>Place a control at the clicked spot (snapped to the grid)</td></tr>
<tr><td>Drag a control</td><td>Move it (snapped to the grid)</td></tr>
<tr><td>Drag a side handle</td><td>Change the width (the height follows the font)</td></tr>
<tr><td>Arrows / Shift + arrows</td><td>Nudge by 1 pixel / by one grid step</td></tr>
<tr><td>Tab, Delete</td><td>Select the next control; delete the selected one</td></tr>
<tr><td><code>G</code></td><td>Change the grid step (1 = off)</td></tr>
<tr><td>Wheel, Shift + wheel</td><td>Scroll the surface up/down, sideways</td></tr>
<tr><td><code>+</code> <code>-</code> <code>0</code> <code>F</code></td><td>Zoom in, out, 1:1, fit. Ctrl/Cmd + wheel zooms around the mouse. The zoom box and the Fit and 1:1 buttons are above the surface.</td></tr>
<tr><td>Ctrl/Cmd + O, S, X</td><td>Open, Save, Export (always asks for the output folder).</td></tr></table>
<h2>Text fields in the panel</h2>
<p>Every field is a real editor: arrows, Home/End, Shift to select, Alt/Ctrl + arrows by word, Cmd + Left/Right to the line ends, Ctrl/Cmd + A / C / X / V, undo and redo (Ctrl/Cmd + Z, Shift + Z or Ctrl + Y), double-click selects a word and triple-click everything. The designer is <b>US-ASCII only</b>: any character outside the printable ASCII range is dropped when typed or pasted.</p>
<h2>Strict values: red and refuse</h2>
<p>A value the real control would not accept (24:00 in a time box, 2026-02-30, a maximum below the minimum, a name that is a C++ keyword ...) is <b>refused</b>: the old value stays, the field turns red and the reason is shown under the rows and in the status line. Nothing invalid reaches the generated code.</p>
<h2>Fonts, colours and layout</h2>
<ul>
<li><b>Font</b> and <b>colour</b> rows open a dialog (the font dialog previews the control itself in that font and colours; a font's size is part of the font).</li>
<li><b>Border style</b> (Standard / 3D / Sleek), <b>Border size</b>, <b>Padding</b> and <b>Align</b> are overrides: the field shows the control's default until you change it; clearing the field or choosing the default again removes the override. Alignment exists for labels and push buttons.</li>
<li><b>Fit</b> (on the Width row of text-like controls) sets the exact width that shows the whole text.</li>
<li>Controls with a frame of their own caption (everything except Label, Push button, Check box and Radio button) get a <b>label group</b>: label text, position (Above / Below), alignment, font and colour.</li>
</ul>
<h2>Files</h2>
<p>The form is saved as a readable JSON file (see <a href="designer-json.html">The form file</a>). Open starts in the folder it was last looking at (an application setting kept per user, not per form). Save starts on the form's own file. The folder the code was last exported to is remembered inside the form file (<code>outputPath</code>); if it no longer exists, the folder of the FormDesigner program is used. In scripts you can skip the dialogs: <code>FD_FORM=form.json FD_OPEN=1 FD_EXPORT=1 build/FormDesigner</code> exports from the command line.</p>
''')

add('designer-code', 'Generated code and your file', 'What Export writes and how to use it', f'''
<p>Export writes two files next to each other.</p>
<table><tr><th>File</th><th>Who owns it</th><th>What is in it</th></tr>
<tr><td><code>Form1.designer.h</code></td><td>The designer. Regenerated on every export: <b>never edit it</b>.</td><td>Class <code>Form1Base</code>: every control as a public member, the <code>SuperForm</code>, constants, <code>InitializeComponent()</code>, <code>Show()</code>, <code>Service()</code>, and one empty virtual handler per event.</td></tr>
<tr><td><code>Form1.h</code></td><td>You. Written once, never changed again.</td><td>Class <code>Form1 : Form1Base</code> with a constructor for your start-up code and an <code>override</code> stub per handler.</td></tr></table>
<h2>Using it in a sketch</h2>
<pre><code>#include "Form1.h"
Form1 ui(&amp;lcd, &amp;Enc, &amp;BtnA, &amp;BtnB);   // display, encoder, two buttons

void setup() {{ /* init lcd, Enc, BtnA, BtnB */ ui.Show(); }}
void loop()  {{ ui.Service(); }}              // focus, editing, touch, handlers</code></pre>
<h2>Handlers</h2>
<table><tr><th>Control</th><th>Handlers</th></tr>
<tr><td>Push button</td><td><code>okBtn_Click()</code></td></tr>
<tr><td>Check box, radio button</td><td><code>wifi_Change()</code></td></tr>
<tr><td>Spin box, slider, input / time / date box, color picker</td><td><code>name_Change()</code> (live, while editing) and <code>name_Accept()</code> (the user confirmed)</td></tr>
<tr><td>List box, combo box, tab bar</td><td><code>name_Change()</code> (cursor moved) and <code>name_Accept()</code> (item chosen)</td></tr>
<tr><td>Menu</td><td><code>menu1_Choose(uint16_t id)</code> with one <code>case</code> per command, plus <code>menu1_Change()</code></td></tr>
<tr><td>User draw</td><td><code>scope_Draw(spr, body)</code>, and with Touch input <code>scope_Pointer(e)</code> and <code>scope_Accept()</code></td></tr>
<tr><td>Gauge, graph, text box, label, progress bar</td><td>none (display only). Update them from your code: <code>load.SetValue(v)</code>, <code>trace.AddSample(v)</code>.</td></tr></table>
<p>To react to an event, write the <code>override</code> in <code>Form1.h</code>:</p>
<pre><code>protected:
  void okBtn_Click() override {{ if (msgYesNo_Ask() == MSG_YESNO_YES) Save(); }}
  void volume_Change() override {{ SetVolume(volume.GetLiveValue()); }}</code></pre>
<h2>When you add controls later</h2>
<p>The designer never edits <code>Form1.h</code>. Instead the status line (and <code>FD_EXPORT=1</code>) lists the handlers your file does not mention yet, for example <code>add to it: cancelBtn_Click +1 more</code>. Copy the stub you want from the <code>protected:</code> list of <code>Form1.designer.h</code>. Delete <code>Form1.h</code> to get a fresh one (it will not remember your code!).</p>
<h2>Constants you get</h2>
<ul><li><code>MNU_...</code> for every menu command, <code>GRP_...</code> for every control group, <code>MSG_...</code> for named message box buttons, and <code>MSG_CANCELLED</code> / <code>MSG_PENDING</code>. Use them instead of numbers.</li></ul>
{tip('The constructor takes <b>both</b> buttons: <code>Form1(dst, enc, btnA, btnB)</code>. Older exports took only one; re-export and pass <code>&amp;BtnB</code> too.')}
{warn('Do not put your code in <code>Form1.designer.h</code>. The next export replaces the whole file and your changes are gone. Everything of yours goes in <code>Form1.h</code>.')}
''')

add('designer-groups', 'Lists, menus, tab pages and groups', 'Items dialogs, the menu editor, pages', f'''
<h2>List box, combo box, tab bar: the items dialog</h2>
<p>Click <b>Edit...</b> on the Items (or Tabs) row.</p>
{fig('designer_items.png', 'The items dialog for a tab bar: the list, the selected item, its page group and a live preview')}
<ul>
<li>Type in <i>Item text</i>: changes apply live. An empty item is refused.</li>
<li><b>Enter</b> in the text field adds the next item: type a whole list with Enter between the entries.</li>
<li>Add, Insert, Delete, Move up / down, Sort A-Z, Clear all. <b>Paste lines</b> adds every non-empty line of the clipboard (copy a column from a spreadsheet).</li>
<li>Items are printable ASCII, 1 to 60 characters, at most 200. Enter = OK, Esc (in the list) = Cancel.</li>
<li>In your code the lists change at run time too: <code>InsertItem</code>, <code>RemoveItem</code>, <code>SetItem</code>, <code>GetItem</code>.</li>
</ul>
<h2>Menu</h2>
<p>A menu is a tree. Every command has an <b>item ID</b> (a C++ name that becomes <code>static constexpr uint16_t MNU_REBOOT = 7;</code>) and a number. Edit it in the <b>menu editor</b> (Edit... on the Menu items row).</p>
{fig('designer_menu.png', 'The menu editor: tree, fields, indent / outdent, live preview')}
<ul>
<li><b>Add</b> adds a sibling, <b>Add child</b> a sub-item; <b>Indent / Outdent</b> change the level; Move up / down reorder.</li>
<li>New items get the next free number and an ID made from the text (<code>Power off</code> becomes <code>MNU_POWER_OFF</code>). The ID follows the text until you type your own.</li>
<li><b>Paste outline</b>: every clipboard line becomes an item, indentation (spaces or tabs) makes sub-items. The quickest way to enter a big menu.</li>
<li>At most 5 levels and 200 items.</li>
</ul>
<p>In code the handler is <code>menu1_Choose(uint16_t id)</code>:</p>
<pre><code>void menu1_Choose(uint16_t id) override {{
  switch (id) {{ case MNU_REBOOT: Reboot(); break; case MNU_ABOUT: ShowAbout(); break; }}
}}</code></pre>
<h2>Control groups</h2>
<p>Every control has a <b>Control group</b> row: a name (empty = no group). Controls with the same name are a group, and each group becomes a constant <code>GRP_X</code>. Do not confuse it with the radio button's <i>Group</i>, which only says which radio buttons exclude each other.</p>
<pre><code>Form.SetGroupVisible(GRP_ADVANCED, false);   // hide a whole set
Form.SetGroupEnabled(GRP_ADVANCED, true);    // enable / disable a set</code></pre>
<h2>Tab pages</h2>
<p>In the tab bar's items dialog every tab has a <b>Page group</b>: the control group shown while that tab is chosen. The generated code does the switching (<code>tabs1_ShowPage(i)</code>), then calls your <code>tabs1_Accept()</code>. The first tab's page is shown at start; the other pages start hidden.</p>
<p>Use the <b>View</b> button above the surface to look at one page at a time (All, then each group): only that group's controls, and the ones without a group, are shown and clickable, so pages that overlap on the form can be edited separately.</p>
{tip('Put controls that appear on every page (a title, a footer) in <b>no group</b>: they are always shown.')}
{warn('A group operation overrides a control&rsquo;s own Visible / Enabled: <code>SetGroupVisible(g, true)</code> shows every control in the group, including ones you set to Visible = No. A control that must stay hidden should not share a group with controls that get shown.')}
''')

add('designer-boxes', 'Message boxes', 'Confirmations without placing anything', f'''
<p>A message box is a modal question ("Delete all data?" with Yes / No). It is not drawn on the form: you open it from code and get the answer. The designer keeps the boxes in a list. <b>Select nothing</b> (click an empty spot of the surface, or press Esc) and click <b>Edit...</b> on the <i>Message boxes</i> row.</p>
{fig('designer_boxes.png', 'The message box editor with a live preview of the real box')}
<h2>The editor</h2>
<ul>
<li><b>Name</b>: the member (<code>msgDelete</code>). <b>Title</b>: optional, shown above the box.</li>
<li><b>Width</b>, <b>Default button</b> (the highlighted one), <b>Centered</b> or an X / Y position.</li>
<li><b>Edit message...</b>: the message is a list of lines (one entry per line, at most 12). Long lines are word-wrapped on the device.</li>
<li><b>Buttons</b>: 1 to 3. Type the text; the <i>name</i> next to it is optional and becomes a constant. Emptying a button's text removes the button.</li>
<li><b>Auto names</b> fills in <code>MSG_DELETE_YES</code>-style names. <b>Add Yes/No</b> adds a ready-made confirmation box ("Confirm?", Yes / No, No highlighted).</li>
</ul>
<h2>Using a box in your code</h2>
<pre><code>// blocking: waits for the answer
if (msgDelete_Ask() == MSG_DELETE_YES) DeleteAll();

// the answer is the button's INDEX; -1 (MSG_CANCELLED) when the user cancelled
int r = msgYesNo_Ask();
if (r == MSG_YESNO_YES) {{ ... }} else if (r == MSG_YESNO_NO) {{ ... }}

// not blocking: keep your loop running
msgDelete_Open();                           // show it
int r = msgDelete_Poll();                   // every pass: MSG_PENDING while it is open</code></pre>
<p>When a box closes, the form is redrawn over the area it covered. The text can be changed at run time with <code>msgDelete.SetMessage("...")</code>. Encoder turn moves between buttons, ButtonA chooses, ButtonB cancels (touch works too).</p>
{tip('Name the buttons you test for: <code>MSG_DELETE_YES</code> reads better than <code>== 0</code> and survives re-ordering the buttons in the designer.')}
{warn('<code>_Ask()</code> blocks until the user answers: the rest of your <code>loop()</code> (sensors, networking, animation) does not run meanwhile. Use <code>_Open()</code> / <code>_Poll()</code> when that matters.')}
{warn('Fonts and colours of a box (<code>"font"</code>, <code>"textColor"</code>, <code>"backColor"</code>, <code>"borderColor"</code>) are only editable in the JSON file for now.')}
''')

add('designer-json', 'The form file (JSON)', 'Readable, hand-editable, repaired on load', f'''
<p>A form is a plain JSON file you can read, diff and edit by hand (or let Claude edit). Only values that differ from the defaults are written.</p>
<pre><code>{{ "form": {{ "name": "Form1", "width": 320, "height": 240 }},
  "controls": [
    {{ "type": "SuperPushButton", "name": "okBtn", "text": "OK", "x": 16, "y": 80, "width": 90 }},
    {{ "type": "SuperListBox", "name": "colors", "items": ["Red", "Green", "Blue"], "selected": 2 }},
    {{ "type": "SuperMenu", "name": "menu1", "items": [
        {{ "text": "System", "items": [ {{ "text": "Reboot", "itemId": "MNU_REBOOT", "id": 7 }} ] }},
        {{ "text": "About" }} ] }}
  ],
  "messageBoxes": [
    {{ "name": "msgDelete", "title": "Careful", "message": ["Delete all data?", "Cannot be undone."],
      "buttons": [ {{ "text": "Yes", "itemId": "MSG_DELETE_YES" }}, "No" ], "default": 1 }} ] }}</code></pre>
<h2>Minimal is enough</h2>
<ul>
<li>A menu item can be just <code>"About"</code>: on load the designer adds the missing item ID (<code>MNU_ABOUT</code>) and the next free number.</li>
<li>A list item is a string. A tab with a page is <code>{{ "text": "Home", "group": "GRP_HOME" }}</code>.</li>
<li>A message box <code>"message"</code> can be one string with <code>\\n</code>; a button can be a plain string.</li>
</ul>
<h2>Repairs on load</h2>
<p>A hand-edited file is checked and repaired, never trusted: unsafe or repeated names are replaced (the <b>first</b> owner of a repeated name or ID keeps it), empty items are dropped, numbers are pulled into range, a selected index past the last item is pulled back, two selected radio buttons in one group keep only the first, non-ASCII characters are dropped. Export refuses a menu, group or message box that still has a problem, and says which.</p>
{tip('Saving writes the repaired form: open a hand-edited file, look at what changed, and save.')}
''')

add('designer-tips', 'Designer tips, tricks and pitfalls', 'Things that save time', f'''
<h2>Tips and tricks</h2>
{tip('<b>Design at 1:1, check at Fit.</b> Press <code>0</code> for the true pixel size and <code>F</code> to see the whole form. Everything you see is drawn by the real controls, so a gap or a clipped text on screen is a gap or a clipped text on the device.')}
{tip('<b>Name controls as you place them.</b> <code>okBtn</code>, <code>cancelBtn</code>, <code>volume</code> make the handlers (<code>okBtn_Click</code>) read like sentences. A prefix such as <code>btn</code>, <code>lbl</code>, <code>chk</code> keeps the Controls-on-form list tidy.')}
{tip('<b>Lists from a spreadsheet:</b> copy a column, open the items dialog and click <b>Paste lines</b>. For menus, copy an indented outline and click <b>Paste outline</b>.')}
{tip('<b>Input box templates:</b> <b>Presets</b> has phone numbers, ZIP codes, dates, times, IPv4, MAC addresses and more. <b>Build</b> makes a template cell by cell. <code>#</code> is a digit, <code>A</code> upper case, <code>a</code> lower case, <code>B</code> any letter, <code>*</code> anything; every other character is a literal, so <code>###-###-####</code> is a phone number.')}
{tip('<b>Gauge:</b> the Zones dialog has <b>Fill 3 bands</b> (green / yellow / red at 60 / 85 / 100 % of the range). Update the needle with <code>load.SetValue(v)</code>.')}
{tip('<b>Graph:</b> <i>Preview data</i> (Sine / Rising / Noise) only exists so the designer shows something. It is not generated: feed the real graph with <code>trace.AddSample(v)</code>.')}
{tip('<b>Pages:</b> design every page in the same area of the form and use the <b>View</b> button to edit one page at a time.')}
{tip('<b>Hand edits:</b> the JSON file is meant to be edited. Rename a group, reorder menu items, paste a long message: save, open in the designer and it repairs anything unsafe.')}
<h2>Pitfalls</h2>
{warn('<b>Editing <code>Form1.designer.h</code>.</b> It is overwritten on every export. Your code belongs in <code>Form1.h</code>.')}
{warn('<b>A new handler is not in your file.</b> After you add a control, export and read the status line: it names the handlers <code>Form1.h</code> does not have. Copy the stubs from the <code>protected:</code> list.')}
{warn('<b>Names are strictly checked.</b> A C++ keyword (<code>class</code>, <code>int</code>), a library or Arduino name (<code>delay</code>, <code>Serial</code>, <code>PI</code>, <code>HIGH</code>), a pin name (<code>A0</code>, <code>D13</code>), a macro defined by the headers, a name starting with an underscore or <code>fd_</code>, or one that ends like a handler (<code>x_Click</code>) is refused. Two things can never share a name: controls, radio groups, menu IDs, control groups, message boxes and their button names, and the form (<code>Form1</code>, <code>Form1Base</code>).')}
{warn('<b>The constructor needs both buttons</b>: <code>(dst, enc, btnA, btnB)</code>.')}
{warn('<b>A group show overrides Visible.</b> <code>SetGroupVisible(g, true)</code> shows every control of the group, even those set to Visible = No.')}
{warn('<b>Text is US-ASCII.</b> Accented letters and symbols outside the printable ASCII range are dropped when typed, pasted or loaded.')}
{warn('<b>Message boxes block.</b> <code>_Ask()</code> does not return until the user answers. Use <code>_Open()</code> / <code>_Poll()</code> in a loop that must keep running.')}
{warn('<b>User draw cannot be previewed.</b> The designer cannot run your drawing code, so it shows a placeholder. Your <code>name_Draw(spr, body)</code> is what the device draws.')}
<h2>Checking the designer itself</h2>
<p><code>FD_SELFTEST=1 build/FormDesigner</code> runs hundreds of automatic checks with synthetic mouse and keyboard events and exits 0 when all pass. <code>FD_SNAPSHOT=frame.raw</code> writes the first frame (raw big-endian RGB565, 1400x820) and exits; useful for screenshots.</p>
''')
