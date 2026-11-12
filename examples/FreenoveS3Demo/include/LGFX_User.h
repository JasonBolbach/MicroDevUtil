#include <LovyanGFX.hpp>
#include "Pins.h"

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7796 _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Touch_FT5x06  _touch_instance;

public:
    LGFX(void)
    {
        {
            auto cfg = _bus_instance.config();
            // SPI Bus settings (modify to match your board)
            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 80000000; // 40MHz for ST7796
            cfg.freq_read = 16000000;
            cfg.pin_sclk = TFT_SCLK; // Change to match your wiring
            cfg.pin_mosi = TFT_MOSI;
            cfg.pin_miso = TFT_MISO;
            cfg.pin_dc = TFT_DC;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs   = TFT_CS; // Chip Select
            cfg.pin_rst  = TFT_RST;  // Reset
            cfg.pin_busy = -1; // Busy (-1 if not connected)
            cfg.memory_width  = 320;
            cfg.memory_height = 480;
            cfg.panel_width  = 320;
            cfg.panel_height = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;      
            cfg.offset_rotation = 1;
            cfg.invert = 0;
            _panel_instance.config(cfg);
        }

        {
            auto cfg = _touch_instance.config();
            cfg.x_min      = 0;
            cfg.x_max      = 320;
            cfg.y_min      = 0;
            cfg.y_max      = 480;
            cfg.pin_int    = TOUCH_INT; // Interrupt Pin
            cfg.pin_sda    = TOUCH_SDA; // I2C SDA
            cfg.pin_scl    = TOUCH_SCL; // I2C SCL
            cfg.i2c_addr   = 0x38; // FT6336 Default I2C Address
            cfg.i2c_port   = 1;
            cfg.freq       = 400000;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }
        setPanel(&_panel_instance);
    }
};