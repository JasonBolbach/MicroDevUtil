#pragma once
// LovyanGFX display class for the TE004 board: ST7789 240x320 on SPI0, PWM backlight, used in landscape (320x240).
// All pins and the SPI port come from Pins.h. For another board copy this file and Pins.h and change the panel class and numbers.
#include <LovyanGFX.hpp>
#include "Pins.h"

class LGFX_TE004 : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7789  _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;

public:
    LGFX_TE004(void)
    {
        {   // SPI bus
            auto cfg = _bus_instance.config();
            cfg.spi_host   = TFT_SPI_HOST;
            cfg.spi_mode   = 0;
            cfg.freq_write = 80000000;     // asks for 80 MHz: the RP2040 / RP2350 SPI block runs as fast as its peripheral clock allows
            cfg.freq_read  = 16000000;
            cfg.pin_sclk   = TFT_SCLK;
            cfg.pin_mosi   = TFT_MOSI;
            cfg.pin_miso   = TFT_MISO;
            cfg.pin_dc     = TFT_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {   // panel
            auto cfg = _panel_instance.config();
            cfg.pin_cs         = TFT_CS;
            cfg.pin_rst        = TFT_RST;
            cfg.pin_busy       = -1;       // no busy pin
            cfg.memory_width   = 240;      // the controller's native (portrait) size ...
            cfg.memory_height  = 320;
            cfg.panel_width    = 240;
            cfg.panel_height   = 320;
            cfg.offset_x       = 0;
            cfg.offset_y       = 0;
            cfg.offset_rotation = 1;       // ... rotated to landscape: lcd.width() = 320, lcd.height() = 240
            cfg.invert         = 0;
            _panel_instance.config(cfg);
        }

        {   // backlight
            auto cfg = _light_instance.config();
            cfg.pin_bl      = TFT_BL;
            cfg.invert      = false;
            cfg.pwm_channel = 1;
            cfg.freq        = 20000;
            _light_instance.config(cfg);
            _panel_instance.light(&_light_instance);
        }
        setPanel(&_panel_instance);
    }

    void setLight(uint8_t level) { _light_instance.setBrightness(level); }   // 0..255
};
