#pragma once
// ----------------------------------------------------------------------------------------------
// YOUR LovyanGFX display class goes here: replace everything below with your own `class LGFX`.
// Requirements: the class must be called LGFX, derive from lgfx::LGFX_Device, and give a 320 x 240
// landscape screen (set the rotation / offsets in your class, or call lcd.setRotation() in setup()).
// The placeholder below is an ST7789 on SPI0 with guessed pins: it only exists so the project compiles.
// ----------------------------------------------------------------------------------------------
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 panel;
  lgfx::Bus_SPI      bus;
public:
  LGFX() {
    auto b = bus.config();
    b.spi_host = 0;                         // Pico: SPI0
    b.freq_write = 40000000;
    b.pin_sclk = 18; b.pin_mosi = 19; b.pin_miso = -1; b.pin_dc = 20;
    bus.config(b);
    panel.setBus(&bus);
    auto p = panel.config();
    p.pin_cs = 17; p.pin_rst = 21; p.pin_busy = -1;
    p.panel_width = 240; p.panel_height = 320;
    p.offset_rotation = 1;                  // landscape 320 x 240 (adjust for your panel)
    panel.config(p);
    setPanel(&panel);
  }
};
