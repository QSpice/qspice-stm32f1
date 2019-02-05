#include"ssd1306.h"

SSD1306::SSD1306(): x(0), y(0), inverted(false), _address(0x00), _hi2c(nullptr) {}

SSD1306::SSD1306(I2C_HandleTypeDef* hi2c, uint8_t address)
    : x(0), y(0), inverted(false), _address(address), _hi2c(hi2c) {

  HAL_Delay(100);

  exec_command(0xAE); //display off
  exec_command(0x20); //Set Memory Addressing Mode
  exec_command(0x10); // 00, Horizontal Addressing Mode;01, Vertical Addressing Mode;10, Page Addressing Mode(RESET);11
  exec_command(0xB0); // set Page Start Address for Page Addressing Mode,0-7
  exec_command(0xC8); // set COM Output Scan Direction
  exec_command(0x00); // set low column address
  exec_command(0x10); // set high column address
  exec_command(0x40); // set start line address
  exec_command(0x81); // set contrast control register
  exec_command(0xFF);
  exec_command(0xA1); // set segment re-map 0 to 127
  exec_command(0xA6); // set normal display
  exec_command(0xA8); // set multiplex ratio(1 to 64)
  exec_command(0x3F); //
  exec_command(0xA4); // 0xa4,Output follows RAM content;0xa5,Output ignores RAM content
  exec_command(0xD3); // set display offset
  exec_command(0x00); // not offset
  exec_command(0xD5); // set display clock divide ratio/oscillator frequency
  exec_command(0xF0); // set divide ratio
  exec_command(0xD9); // set pre-charge period
  exec_command(0x22); //
  exec_command(0xDA); // set com pins hardware configuration
  exec_command(0x12);
  exec_command(0xDB); // set vcomh
  exec_command(0x20); // 0x20,0.77xVcc
  exec_command(0x8D); // set DC-DC enable
  exec_command(0x14); //
  exec_command(0xAF); // turn on SSD1306 panel

  fill(Black);
  swap_buffers();
}

void SSD1306::fill(SSD1306::COLOR color) {
  uint32_t i;

  for(i = 0; i < sizeof(_buffer); i++) {
    _buffer[i] = (color == Black) ? 0x00 : 0xFF;
  }
}

void SSD1306::draw_pixel(uint8_t x, uint8_t y, SSD1306::COLOR color, bool inverted) {
  if (x >= SSD1306_WIDTH or y >= SSD1306_HEIGHT) {
    return;
  }

  // Check if pixel should be inverted
  if (this->inverted || inverted)
    color = (SSD1306::COLOR) !color;

  // Draw in the right color
  if (color == White) {
    _buffer[x + (y / 8) * SSD1306_WIDTH] |= 1 << (y % 8);
  } else {
    _buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
  }
}

void SSD1306::draw_hline(uint8_t x1, uint8_t x2, uint8_t y, SSD1306::COLOR color, bool inverted) {
  for (uint8_t x = x1; x <= x2; x++) {
    draw_pixel(x, y, color, inverted);
  }
}

void SSD1306::draw_char(char ch, FontDef font, SSD1306::COLOR color, bool inverted) {
  uint32_t pixel;

  uint8_t desc_offset = ch - font.start_char;
  FontDescriptor desc = font.descriptors[desc_offset];
  uint8_t cols = 1 + (desc.width % 8 == 0 ? desc.width / 8 - 1 : desc.width / 8);

  // Check remaining space on current line
  if (SSD1306_WIDTH <= (this->x + desc.width) or SSD1306_HEIGHT <= (this->y + font.height)) {
    return;
  }

  // Use the font to write
  for (uint32_t i = 0; i < font.height; i++) {
    for (uint8_t j = 0; j < cols; j++) {
      pixel = font.bitmaps[desc.offset + (i * cols) + j];

      for (uint8_t k = 8; k > 0; k--) {
        if (pixel & 0x0001) {
          draw_pixel(this->x + k + 8 * j, this->y + i, (SSD1306::COLOR) color, inverted);
        } else {
          draw_pixel(this->x + k + 8 * j, this->y + i, (SSD1306::COLOR) !color, inverted);
        }

        pixel >>= 1;
      }
    }
  }

  // The current space is now taken
  this->x += desc.width;
}

void SSD1306::draw_string(const char* str, FontDef font, SSD1306::COLOR color, bool inverted) {
  while(*str) {
    draw_char(*str, font, color, inverted);

    str++;
  }
}

void SSD1306::set_cursor(uint8_t x, uint8_t y) {
  this->x = x;
  this->y = y;
}

void SSD1306::set_contrast(uint8_t contrast) {
  exec_command(0x81);
  exec_command(contrast);
}

void SSD1306::swap_buffers() {
  for (uint8_t i = 0; i < 8; i++) {
    exec_command(0xB0 + i);
    exec_command(0x00);
    exec_command(0x10);

    HAL_I2C_Mem_Write(_hi2c, _address, 0x40, 1, &_buffer[SSD1306_WIDTH * i], SSD1306_WIDTH, 100);
  }
}

void SSD1306::exec_command(uint8_t command) {
  HAL_I2C_Mem_Write(_hi2c, _address, 0x00, 1, &command, 1, 10);
}
