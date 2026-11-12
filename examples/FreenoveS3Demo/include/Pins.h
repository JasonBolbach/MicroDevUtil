#define SD_CS_PIN     39
#define SD_CLK_PIN    41
#define SD_MISO_PIN   42
#define SD_MOSI_PIN   40
#define SPI_CLOCK SD_SCK_MHZ(32)
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK, &SD_SPI)

#define TFT_MISO  13
#define TFT_MOSI  11
#define TFT_SCLK  12
#define TFT_CS    10
#define TFT_DC    5
#define TFT_RST   4

#define TOUCH_RST   6
#define TOUCH_INT   7
#define TOUCH_SDA   8
#define TOUCH_SCL   9