#pragma once
// Hard-coded limits and defaults of the Form Designer.
namespace FD {
    constexpr int MAX_FORM_W = 1024, MAX_FORM_H = 768;          // largest form the designer will make
    constexpr int DEFAULT_FORM_W = 320, DEFAULT_FORM_H = 240;   // a new form
    constexpr int WIN_W = 1400, WIN_H = 820;                    // logical size of the designer window (the window can be resized: content scales)
}
