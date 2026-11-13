// PLACEHOLDER: overwrite with the UserForm.h (and UserForm.designer.h) exported by the Form Designer.
// main.cpp needs: class UserForm(lgfx::LovyanGFX*, SuperEncoder*, SuperButton* A, SuperButton* B) with
//   static FormWidth / FormHeight, public member SuperForm Form, Show() and Service().
#pragma once
#include <LovyanGFX.hpp>
#include <SuperLabel.h>
#include <SuperPushButton.h>
#include <SuperForm.h>

class UserForm {
public:
  static constexpr int FormWidth = 480, FormHeight = 320;
  SuperLabel title;
  SuperPushButton button;
  SuperForm Form;

  UserForm(lgfx::LovyanGFX* dst, SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB)
    : title(dst, 20, 20, 440, &lgfx::fonts::Font4, "Placeholder UserForm"),
      button(dst, 20, 80, 200, &lgfx::fonts::Font2, "Click me"),
      Form(enc, btnA), Dst(dst) {
    Form.Add(&button);
  }
  void Show() { Dst->fillScreen(0); Form.Draw(); }
  SuperEditState Service() { return Form.Service(); }
private:
  lgfx::LovyanGFX* Dst;
};
