
#ifndef BITMAP_H__
#define BITMAP_H__

#include "intern.h"

uint8_t *decode_bitmap(const uint8_t *src, bool alpha, int colorKey, int *w, int *h, int bitmap_type);
uint16_t *decode_bitmap_toRGB5551(const uint8_t *src, bool alpha, int colorKey, int *w, int *h, bool flipY);

#endif
