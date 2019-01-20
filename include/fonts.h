
#include "stm32f1xx_hal.h"

#ifndef __FONTS_H__
#define __FONTS_H__

#ifdef __cplusplus
 extern "C" {
#endif

typedef struct {
  uint8_t width;
  uint8_t height;
  const unsigned char *data;
} FontDef;

extern FontDef font_8x15;

#ifdef __cplusplus
 }
#endif

#endif

