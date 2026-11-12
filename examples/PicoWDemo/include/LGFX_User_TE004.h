#pragma once

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
        {
            auto cfg = _bus_instance.config();
            // SPI Bus settings (modify to match your board)
            cfg.spi_host = 0;
            cfg.spi_mode = 0;
            cfg.freq_write = 80000000; // 40MHz for ST7796
            cfg.freq_read  = 16000000;
            cfg.pin_sclk = TFT_SCLK; // Change to match your wiring
            cfg.pin_mosi = TFT_MOSI;
            cfg.pin_miso = TFT_MISO;
            cfg.pin_dc = TFT_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs   = TFT_CS; // Chip Select
            cfg.pin_rst  = TFT_RST;  // Reset
            cfg.pin_busy = -1; // Busy (-1 if not connected)
            cfg.memory_width  = 240;
            cfg.memory_height = 320;
            cfg.panel_width  = 240;
            cfg.panel_height = 320;
            cfg.offset_x = 0;
            cfg.offset_y = 0;      
            cfg.offset_rotation = 1;
            cfg.invert = 0;
            _panel_instance.config(cfg);
        }

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = TFT_BL;
            cfg.invert = false;
            cfg.pwm_channel = 1;
            cfg.freq = 20000;
            _light_instance.config(cfg);
            _panel_instance.light(&_light_instance);
        }
        setPanel(&_panel_instance);
    }
    void setLight(uint8_t Level) { _light_instance.setBrightness(Level); }
};