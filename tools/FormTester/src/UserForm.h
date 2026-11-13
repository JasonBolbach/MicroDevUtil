// UserForm.h - YOUR code for the form. The Form Designer wrote this file once and never changes it again.
// The controls and their layout are in UserForm.designer.h (regenerated every time: do not edit that one).
//
// In your sketch:
//     #include "UserForm.h"
//     UserForm ui(&lcd, &Enc, &BtnA, &BtnB);   // lcd: your display; Enc, BtnA, BtnB: your encoder and two buttons
//     setup():  ui.Show();                          // draws the form
//     loop():   ui.Service();                       // focus, editing and the handlers below
#pragma once
#include "UserForm.designer.h"

class UserForm : public UserFormBase {
public:
    UserForm(lgfx::LovyanGFX* dst, SuperEncoder* enc, SuperButton* btnA, SuperButton* btnB) : UserFormBase(dst, enc, btnA, btnB) {
        // Your start-up code: set initial values, e.g. rb1.ColFg = 0xFFE0.
    }

protected:
    void rb1_Change() override {
        // rb1 changed: rGrp1.GetSelected() is the selected radio button of the group.
        printf("rb1_Change\n");
    }
    void rb2_Change() override {
        // rb2 changed: rGrp1.GetSelected() is the selected radio button of the group.
        printf("rb2_Change\n");
    }
    void rb3_Change() override {
        // rb3 changed: rGrp1.GetSelected() is the selected radio button of the group.
        printf("rb3_Change\n");
    }
};
