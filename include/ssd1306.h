#ifndef __SSD1306_H__
#define __SSD1306_H__

#include "stm32f1xx_hal.h"
#include "fonts.h"

#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64

class SSD1306 {
  public:
    enum COLOR {
      Black = 0x00, // Black (pixel off)
      White = 0x01  // White (pixel on)
    };

    SSD1306(I2C_HandleTypeDef* hi2c, uint8_t address);

    uint16_t x;
    uint16_t y;
    uint16_t inverted;

    void fill(SSD1306::COLOR color);
    void swap_buffers();
    void draw_pixel(uint8_t x, uint8_t y, SSD1306::COLOR color);
    void draw_char(char ch, FontDef font, SSD1306::COLOR color);
    void draw_string(const char* str, FontDef font=font_8x15, SSD1306::COLOR color=White);
    void set_cursor(uint8_t x, uint8_t y);
    void set_contrast(uint8_t contrast);

  private:
    uint8_t _address;
    I2C_HandleTypeDef* _hi2c;
    uint8_t _buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];

    void exec_command(uint8_t command);


};

#endif
