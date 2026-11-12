#pragma once
// Pin map for the Pico W demo on the TE004 board (ST7789 320x240 SPI display, rotary encoder with push button, one board button).
// Every pin is a GP number. The static_asserts at the bottom stop the build if two parts share a pin, a pin is reserved by the
// wireless chip, or the display's SPI pins do not belong to the SPI port chosen in LGFX_User_TE004.h.
#include <initializer_list>

// ---- display (SPI0) ----
constexpr int TFT_SPI_HOST = 0;      // SPI0
constexpr int TFT_SCLK = 18;
constexpr int TFT_MOSI = 19;
constexpr int TFT_MISO = -1;         // not connected (the panel is write-only here)
constexpr int TFT_CS   = 17;
constexpr int TFT_DC   = 14;
constexpr int TFT_RST  = 15;
constexpr int TFT_BL   = 13;         // backlight (PWM)

// ---- inputs ----
constexpr int PIN_ENC_A = 0;         // encoder phase A
constexpr int PIN_ENC_B = 1;         // encoder phase B   (GP0 / GP1 are also UART0: do not use Serial1 for debug while these are the encoder)
constexpr int PIN_BTN_A = 3;         // ButtonA: the encoder's push button
constexpr int PIN_BTN_B = 2;         // ButtonB: the board's push button

// ---- compile-time checks ----
namespace pin_check {
constexpr bool distinct(std::initializer_list<int> pins) {
    for (int a : pins)
        for (int b : pins)
            if (&a != &b && a == b && a >= 0) {                  // same value seen twice (ignores -1 = unused)
                int n = 0;
                for (int c : pins) if (c == a) ++n;
                if (n > 1) return false;
            }
    return true;
}
// Pico W / Pico 2 W: GP23, GP24, GP25 and GP29 belong to the CYW43 wireless chip / power circuit.
constexpr bool usable(int p) { return p < 0 || (p <= 28 && p != 23 && p != 24 && p != 25); }
constexpr bool all_usable(std::initializer_list<int> pins) { for (int p : pins) if (!usable(p)) return false; return true; }
// SPI0 pins: SCK 2/6/18/22, TX 3/7/19/23. SPI1 pins: SCK 10/14, TX 11/15.
constexpr bool spi_ok(int host, int sck, int mosi) {
    return host == 0 ? (sck == 2 || sck == 6 || sck == 18 || sck == 22) && (mosi == 3 || mosi == 7 || mosi == 19)
                     : (sck == 10 || sck == 14)                         && (mosi == 11 || mosi == 15);
}
}  // namespace pin_check

static_assert(pin_check::distinct({TFT_SCLK, TFT_MOSI, TFT_MISO, TFT_CS, TFT_DC, TFT_RST, TFT_BL, PIN_ENC_A, PIN_ENC_B, PIN_BTN_A, PIN_BTN_B}),
              "Pins.h: two parts use the same GPIO");
static_assert(pin_check::all_usable({TFT_SCLK, TFT_MOSI, TFT_MISO, TFT_CS, TFT_DC, TFT_RST, TFT_BL, PIN_ENC_A, PIN_ENC_B, PIN_BTN_A, PIN_BTN_B}),
              "Pins.h: a pin is reserved by the Pico W wireless chip (GP23, GP24, GP25) or is not a GPIO");
static_assert(pin_check::spi_ok(TFT_SPI_HOST, TFT_SCLK, TFT_MOSI),
              "Pins.h: TFT_SCLK / TFT_MOSI are not valid pins for the chosen SPI port (TFT_SPI_HOST)");
