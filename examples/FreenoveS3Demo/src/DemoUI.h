#pragma once
// DemoUI: every SuperGUI control on four tab pages, laid out for a 480 x 320 landscape touch screen.
// Portable on purpose (LovyanGFX + SuperGUI only), so main.cpp stays about hardware.
//
//   Encoder = move the highlight / change a value      ButtonA short = edit the highlighted control
//   Touch   = tap a control to edit it; tap a row / tab / slider track / color bar to pick; drag to change;
//             long press = accept; tap outside = cancel. Touch and encoder / buttons mix freely.
//   Inside an edit: ButtonA long = accept, ButtonB short = cancel, ButtonB long = revert
//   Page 1 "Input"   input boxes, spin box, time, date, color picker
//   Page 2 "Choose"  combo box, list box, check box, radio buttons
//   Page 3 "Values"  slider -> progress bar, push button (opens a message box)
//   Page 4 "Menu"    nested menu, live graph
#include <LovyanGFX.hpp>
#include <cmath>
#include <string>
#include <vector>
#include <SuperButton.h>
#include <SuperEncoder.h>
#include <SuperInputBox.h>
#include <SuperSpinBox.h>
#include <SuperListBox.h>
#include <SuperComboBox.h>
#include <SuperCheckBox.h>
#include <SuperRadioButton.h>
#include <SuperProgressBar.h>
#include <SuperPushButton.h>
#include <SuperSlider.h>
#include <SuperTimeBox.h>
#include <SuperTabBar.h>
#include <SuperMessageBox.h>
#include <SuperColorPicker.h>
#include <SuperMenu.h>
#include <SuperGraph.h>
#include <SuperTouch.h>
#include <SuperForm.h>

class DemoUI {
public:
  SuperForm* Form = nullptr;
  // Label font for every control (set before Begin()). Font0 = small (6x8), Font2 = 8x16; each control's own body font is no longer used for labels.
  const lgfx::IFont* LabelFont = &lgfx::fonts::Font0;

  // 'touch' is optional: pass a SuperTouch to let taps and drags work together with the encoder / buttons.
  void Begin(lgfx::LovyanGFX* dst, SuperEncoder* E, SuperButton* A, SuperButton* B, SuperTouch* touch = nullptr) {
    Dst = dst;
    const uint16_t LBL = 0x07FF, EDGE = 0x7BEF;       // label colour, border colour
    const lgfx::IFont* F = &lgfx::fonts::Font2;

    SuperControl::AllocSprite(Status, dst->width(), StatusH);   // follows SuperControl::UsePsram

    Form = new SuperForm(E, A);
    Form->SetOnEdit(&OnEditThunk, this);
    if (touch) Form->SetTouch(touch);

    const lgfx::IFont* F4 = &lgfx::fonts::Font4;
    const int W = dst->width();

    Tabs = new SuperTabBar(dst, 0, 0, W, F, E, A, B);
    Tabs->AddTab("Input"); Tabs->AddTab("Choose"); Tabs->AddTab("Values"); Tabs->AddTab("Menu");
    Tabs->SetBorder(1, EDGE); Tabs->SetOnAccept(&OnTabThunk, this);

    // ---- page 1: Input ----
    Phone = new SuperInputBox(dst, 8, 36, 220, F4, "(###) ###-####", E, A, B);
    Phone->SetLabel("Phone"); Phone->SetLabelColor(LBL); Phone->SetBorder(2, 0xFFFF);
    Phone->SetBorderStyle(SuperControl::BORDER_THREED); Phone->SetValue("5551234567");

    Name = new SuperInputBox(dst, 248, 36, W - 256, F, "BBBBBBBB-####-**", E, A, B);
    Name->SetLabel("Name (scrolls)"); Name->SetLabelColor(LBL); Name->SetBorder(1, EDGE); Name->SetValue("SuperGUI");

    Level = new SuperSpinBox(dst, 8, 108, 110, F4, E, A, B);
    Level->SetLabel("Level"); Level->SetLabelColor(LBL); Level->SetBorder(2, 0xFFFF);
    Level->SetBorderStyle(SuperControl::BORDER_THREED);
    Level->SetRange(0, 100); Level->SetSuffix(" %"); Level->SetValue(40);

    TimeBox = new SuperTimeBox(dst, 138, 108, 150, F4, E, A, B);
    TimeBox->SetLabel("Time"); TimeBox->SetLabelColor(LBL); TimeBox->SetBorder(1, EDGE); TimeBox->SetTime(12, 30, 0);

    DateBox = new SuperDateBox(dst, 308, 108, W - 316, F4, E, A, B);
    DateBox->SetLabel("Date"); DateBox->SetLabelColor(LBL); DateBox->SetBorder(1, EDGE); DateBox->SetDate(2026, 11, 11);

    Picker = new SuperColorPicker(dst, 8, 178, W - 16, F, E, A, B);
    Picker->SetLabel("Color picker (tap / drag a bar, or A short = next bar, A long = accept)"); Picker->SetLabelColor(LBL);
    Picker->SetBorder(1, EDGE); Picker->SetColor565(PickColor);

    Pages[0] = { Phone, Name, Level, TimeBox, DateBox, Picker };

    // ---- page 2: Choose ----
    Combo = new SuperComboBox(dst, 8, 36, 190, F, E, A, B);
    Combo->SetLabel("Color"); Combo->SetLabelColor(LBL); Combo->SetBorder(1, EDGE);
    for (const char* c : {"Red", "Green", "Blue", "Yellow", "Cyan", "Magenta", "Black", "White"}) Combo->AddItem(c);
    Combo->SetSelected(2);
    Combo->SetRestore(&ComboClosedThunk, this);       // SPI panels can't be read back: repaint what the list covered when it closes

    Items = new SuperListBox(dst, 228, 36, W - 236, F, 5, E, A, B);
    Items->SetLabel("Items", SuperControl::LABEL_BELOW); Items->SetLabelColor(LBL); Items->SetBorder(1, EDGE);
    for (int i = 1; i <= 12; ++i) Items->AddItem("Item number " + std::to_string(i));

    Check = new SuperCheckBox(dst, 8, 100, 190, F, "Enabled"); Check->SetChecked(true);
    const char* modes[3] = { "Slow", "Normal", "Fast" };
    for (int i = 0; i < 3; ++i) {
      Radio[i] = new SuperRadioButton(dst, 8, 134 + i * 30, 190, F, modes[i]);
      Mode.Add(Radio[i]);
    }
    Mode.SetSelected(1);

    SleekChk = new SuperCheckBox(dst, 228, 190, 200, F, "Sleek style");          // live switch: every control's frame / label style
    SleekChk->SetOnAccept(&OnSleekThunk, this);

    Pages[1] = { Combo, Items, Check, Radio[0], Radio[1], Radio[2], SleekChk };

    // ---- page 3: Values ----
    Slide = new SuperSlider(dst, 8, 36, W - 16, F, E, A, B);
    Slide->SetLabel("Slider"); Slide->SetLabelColor(LBL); Slide->SetBorder(1, EDGE); Slide->SetValue(35);
    Slide->SetOnChange(&OnSlideThunk, this); Slide->SetOnAccept(&OnSlideThunk, this);

    Bar = new SuperProgressBar(dst, 8, 100, W - 16, F);
    Bar->SetLabel("Progress (follows the slider)"); Bar->SetLabelColor(LBL); Bar->SetBorder(1, EDGE); Bar->SetValue(35);

    Push = new SuperPushButton(dst, 8, 170, 180, F4, "Reset...");
    Push->SetBorder(2, 0xFFFF); Push->SetBorderStyle(SuperControl::BORDER_THREED);

    Pages[2] = { Slide, Bar, Push };

    // ---- page 4: Menu / graph ----
    Menu = new SuperMenu(dst, 8, 36, 220, F, 6, E, A, B);
    Menu->SetLabel("Menu (B = back)"); Menu->SetLabelColor(LBL); Menu->SetBorder(1, EDGE);
    auto* settings = Menu->AddItem("Settings", 0);
    Menu->AddItem("Brightness", 11, settings); Menu->AddItem("Sound", 12, settings);
    auto* net = Menu->AddItem("Network", 0, settings);
    Menu->AddItem("Wi-Fi", 31, net); Menu->AddItem("Bluetooth", 32, net);
    Menu->AddItem("About", 20); Menu->AddItem("Reboot", 21); Menu->AddItem("Power off", 22);

    Graph = new SuperGraph(dst, 248, 36, W - 256, F, 150);
    Graph->SetLabel("Graph (display only)"); Graph->SetLabelColor(LBL); Graph->SetBorder(1, EDGE);
    Graph->SetRange(-10, 10); Graph->ShowRange = true;

    Pages[3] = { Menu, Graph };

    Msg = new SuperMessageBox(dst, 300, F, E, A, B);
    Msg->SetLabel("Confirm");
    Msg->AddButton("Yes"); Msg->AddButton("No"); Msg->SetDefault(1);
    Msg->SetRestore(&MsgClosedThunk, this);

    for (auto* c : Styled()) { c->SetLabelFont(LabelFont); c->SetLabelAlign(SuperControl::ALIGN_LEFT); }   // small, left-aligned labels everywhere

    for (auto* c : std::vector<SuperControl*>{ Tabs })        Form->Add(c);
    for (auto* c : std::vector<SuperControl*>{ Phone, Name, Level, TimeBox, DateBox, Picker, Combo, Items, Check,
                                               Radio[0], Radio[1], Radio[2], SleekChk, Slide, Push, Menu })
      Form->Add(c);
    Form->Add(Bar, false);
    Form->Add(Graph, false);

    for (int i = 1; i < 4; ++i) for (auto* c : Pages[i]) c->SetVisible(false);
    dst->fillScreen(0);
    Form->Draw();
    DrawStatus();
  }

  // Optional: polled by every editing control; return true to cancel the edit (the desktop demo uses it so closing the window ends a blocking edit).
  void SetAbort(bool (*fn)()) {
    Phone->SetAbort(fn); Name->SetAbort(fn); Level->SetAbort(fn); TimeBox->SetAbort(fn); DateBox->SetAbort(fn); Picker->SetAbort(fn);
    Combo->SetAbort(fn); Items->SetAbort(fn); Slide->SetAbort(fn); Menu->SetAbort(fn); Tabs->SetAbort(fn); Msg->SetAbort(fn);
  }

  // Switch the whole demo to the Sleek border style (label on the frame line) and back, restoring each control's own style.
  // Label font and alignment are not touched here (small / left from Begin()).
  void ApplyStyle(bool sleek) {
    if (Saved.empty()) for (auto* c : Styled()) Saved.push_back({ c, c->GetBorderStyle() });   // remember the original look once
    for (auto& s : Saved) s.first->SetBorderStyle(sleek ? SuperControl::BORDER_SLEEK : s.second);
    SleekChk->SetChecked(sleek);                                      // keep the check box in step (silent: no callback)
    ShowPage(Page);                                                   // sizes changed: clear the page area and draw it once
  }

  // Call from loop(): handles input, feeds the graph, redraws the status strip.
  void Service() {
    if (StyleDirty) { StyleDirty = false; ApplyStyle(SleekChk->GetChecked()); }
    uint32_t now = lgfx::millis();
    if (now - LastSample >= 100) {
      LastSample = now;
      Graph->AddSample(std::sin(Tick * 0.12f) * 8 + std::sin(Tick * 0.5f) * 2, Page == 3);
      ++Tick;
    }
    Form->Service();
    if (Redraw) { Redraw = false; Form->Redraw(RX, RY, RW, RH, 0); }
    if (now - LastStatus >= 100 || StatusDirty) { LastStatus = now; StatusDirty = false; DrawStatus(); }
  }

private:
  static constexpr int StatusH = 32;
  lgfx::LovyanGFX* Dst = nullptr;
  lgfx::LGFX_Sprite Status{ nullptr };

  SuperInputBox* Phone; SuperInputBox* Name; SuperSpinBox* Level; SuperTimeBox* TimeBox; SuperDateBox* DateBox; SuperColorPicker* Picker;
  SuperComboBox* Combo; SuperListBox* Items; SuperCheckBox* Check; SuperCheckBox* SleekChk; SuperRadioButton* Radio[3]; SuperRadioGroup Mode;
  SuperSlider* Slide; SuperProgressBar* Bar; SuperPushButton* Push;
  SuperMenu* Menu; SuperGraph* Graph;
  SuperTabBar* Tabs; SuperMessageBox* Msg;
  std::vector<SuperControl*> Pages[4];
  std::vector<std::pair<SuperControl*, SuperControl::BorderStyle>> Saved;   // original border styles (ApplyStyle)
  std::vector<SuperControl*> Styled() {                                     // every control that can show a label
    std::vector<SuperControl*> v;
    for (int p = 0; p < 4; ++p) for (auto* c : Pages[p]) v.push_back(c);
    v.push_back(Msg);
    return v;
  }

  int Page = 0;
  bool Redraw = false, StatusDirty = true, StyleDirty = false;
  int RX = 0, RY = 0, RW = 0, RH = 0;                  // area to repaint after a popup closed
  uint32_t LastSample = 0, LastStatus = 0, Tick = 0;
  uint16_t PickColor = 0xF800;
  std::string Last = "encoder = move, A = edit";

  void ShowPage(int p) {
    Page = p;
    for (int i = 0; i < 4; ++i) if (i != p) for (auto* c : Pages[i]) c->SetVisible(false, false);   // flags only: no per-control erase
    for (auto* c : Pages[p]) c->SetVisible(true, false);                                           // ... and no draw at full brightness
    int top = Tabs->GetY() + Tabs->GetHeight() + 6;                                                 // below the tab bar's focus box
    Dst->fillRect(0, top, Dst->width(), Dst->height() - StatusH - top, 0);                          // clear the old page in one go
    Form->Draw();                                                                                   // draw the new page once, in its final look
  }

  // A popup closed: remember the rectangle it covered; Service() repaints it (clear + redraw the controls that touch it).
  static void ComboClosedThunk(void* u) { auto* d = static_cast<DemoUI*>(u); d->Cover(d->Combo->GetPopupX(), d->Combo->GetPopupY(), d->Combo->GetPopupWidth(), d->Combo->GetPopupHeight()); }
  static void MsgClosedThunk(void* u)   { auto* d = static_cast<DemoUI*>(u); d->Cover(d->Msg->GetX(), d->Msg->GetY(), d->Msg->GetWidth(), d->Msg->GetHeight()); }
  void Cover(int x, int y, int w, int h) {
    const int m = 4;                                                           // a little margin for borders / shadows
    int x0 = x - m, y0 = y - m, x1 = x + w + m, y1 = y + h + m;
    if (Redraw) { x0 = std::min(x0, RX); y0 = std::min(y0, RY); x1 = std::max(x1, RX + RW); y1 = std::max(y1, RY + RH); }
    RX = x0; RY = y0; RW = x1 - x0; RH = y1 - y0; Redraw = true;
  }
  static void OnSleekThunk(void* u) { static_cast<DemoUI*>(u)->StyleDirty = true; }   // applied from Service(), not from inside the touch handler
  static void OnTabThunk(void* u)  { auto* d = static_cast<DemoUI*>(u); d->ShowPage(d->Tabs->GetSelected()); }
  static void OnSlideThunk(void* u) { auto* d = static_cast<DemoUI*>(u); d->Bar->SetValue(d->Slide->GetLiveValue()); }
  static void OnEditThunk(SuperControl* c, SuperEditState s, void* u) { static_cast<DemoUI*>(u)->OnEdit(c, s); }

  void OnEdit(SuperControl* c, SuperEditState s) {
    Last = s == EDIT_ACCEPTED ? "accepted" : "cancelled";
    StatusDirty = true;
    if (s != EDIT_ACCEPTED) return;
    if (c == Push) {
      Msg->SetMessage("Reset the slider and the progress bar to zero?");
      bool yes = Msg->BeginEdit() && Msg->GetResult() == 0;
      Last = yes ? "reset" : "not reset";
      if (yes) { Slide->SetValue(0); Bar->SetValue(0); }
    }
    if (c == Menu)   Last = "menu item id " + std::to_string(Menu->GetResultId());
    if (c == Picker) PickColor = Picker->GetColor565();
  }

  void DrawStatus() {
    Status.fillSprite(0);
    Status.setFont(&lgfx::fonts::Font2);
    Status.setTextColor(0xFFFF);
    Status.setCursor(4, 0);   Status.printf("%s", Last.c_str());
    Status.setCursor(4, 16);  Status.printf("ph=%s lvl=%d chk=%d mode=%d sl=%d", Phone->GetValue().c_str(), (int)Level->GetValue(),
                                            (int)Check->GetChecked(), Mode.GetSelected(), (int)Slide->GetValue());
    Status.fillRect(Status.width() - 36, 4, 32, 12, PickColor);
    Status.pushSprite(Dst, 0, Dst->height() - StatusH);
  }
};
