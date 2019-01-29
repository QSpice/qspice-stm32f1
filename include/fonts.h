
#include "stm32f1xx_hal.h"

#ifndef __FONTS_H__
#define __FONTS_H__

#ifdef __cplusplus
 extern "C" {
#endif

typedef struct {
  uint8_t width;
  uint16_t offset;
} FontDescriptor;

typedef struct {
  uint8_t height;
  char start_char;
  char end_char;
  const FontDescriptor* descriptors;
  const uint8_t* bitmaps;
} FontDef;

extern const FontDef consolas_11ptFontInfo;
extern const FontDef balooBhaina_28ptFontInfo;

#ifdef __cplusplus
 }
#endif

#endif

