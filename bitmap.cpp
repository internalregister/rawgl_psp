
#include "bitmap.h"
#include "util.h"

uint8_t *_background_bitmap_ptr;

static void clut(const uint8_t *src, const uint8_t *pal, int pitch, int w, int h, int bpp, bool flipY, int colorKey, uint8_t *dst) {
	int dstPitch = bpp * w;
	if (flipY) {
		dst += (h - 1) * bpp * w;
		dstPitch = -bpp * w;
	}
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			const int color = src[x];
			const int b = pal[color * 4];
			const int g = pal[color * 4 + 1];
			const int r = pal[color * 4 + 2];
			dst[x * bpp]     = r;
			dst[x * bpp + 1] = g;
			dst[x * bpp + 2] = b;
			if (bpp == 4) {
				dst[x * bpp + 3] = (color == 0 || (colorKey == ((r << 16) | (g << 8) | b))) ? 0 : 255;
			}
		}
		src += w;
		dst += dstPitch;
	}
}

uint8_t *decode_bitmap(const uint8_t *src, bool alpha, int colorKey, int *w, int *h, int bitmap_type) {
	if (memcmp(src, "BM", 2) != 0) {
		warning("Not a bitmap");
		return 0;
	}
	const uint32_t imageOffset = READ_LE_UINT32(src + 0xA);
	const int width = READ_LE_UINT32(src + 0x12);
	const int height = READ_LE_UINT32(src + 0x16);
	const int depth = READ_LE_UINT16(src + 0x1C);
	const int compression = READ_LE_UINT32(src + 0x1E);
	if ((depth != 8 && depth != 32) || compression != 0) {
		warning("Unhandled bitmap depth %d compression %d", depth, compression);
		return 0;
	}
	const int bpp = (!alpha && colorKey < 0) ? 3 : 4;

	uint8_t *dst = nullptr;
	if (bitmap_type == BITMAP_TYPE_BACKGROUND)
	{
		// Don't always allocate backgrounds (also, they should be the same size all the time)
		if (_background_bitmap_ptr == nullptr)
		{
			debug(DBG_INFO, "Allocating the background bitmap buffer");
			_background_bitmap_ptr = (uint8_t *)malloc(width * height * bpp);
		}
		dst = _background_bitmap_ptr;
	}
	else
	{
		dst = (uint8_t *)malloc(width * height * bpp);
	}

	if (!dst) {
		warning("Failed to allocate bitmap buffer, width %d height %d bpp %d", width, height, bpp);
		return 0;
	}
	if (depth == 8) {
		const uint8_t *palette = src + 14 /* BITMAPFILEHEADER */ + 40 /* BITMAPINFOHEADER */;
		const bool flipY = true;
		clut(src + imageOffset, palette, (width + 3) & ~3, width, height, bpp, flipY, colorKey, dst);
	} else {
		assert(depth == 32 && bpp == 3);
		const uint8_t *p = src + imageOffset;
		for (int y = height - 1; y >= 0; --y) {
			uint8_t *q = dst + y * width * bpp;
			for (int x = 0; x < width; ++x) {
				const uint32_t color = READ_LE_UINT32(p); p += 4;
				*q++ = (color >> 16) & 255;
				*q++ = (color >>  8) & 255;
				*q++ =  color        & 255;
			}
		}
	}
	*w = width;
	*h = height;
	return dst;
}

static void clut_toRGB5551(const uint8_t *src, const uint8_t *pal, int pitch, int w, int h, bool flipY, int colorKey, uint16_t *dst) {
	int dstPitch = w;
	if (flipY) {
		dst += (h - 1) * w;
		dstPitch = -w;
	}
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			const int color = src[x];
			const int b = pal[color * 4];
			const int g = pal[color * 4 + 1];
			const int r = pal[color * 4 + 2];			
			dst[x] = ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3);
			if (color != 0 && (colorKey != ((r << 16) | (g << 8) | b)))
			{
				dst[x] = 0x8000 | dst[x];
			}
		}
		src += w;
		dst += dstPitch;
	}
}

uint16_t *decode_bitmap_toRGB5551(const uint8_t *src, bool alpha, int colorKey, int *w, int *h, bool flipY) {
	debug(DBG_INFO, "decode_bitmap_toRGB5551 %d", flipY ? 1 : 0);
	if (memcmp(src, "BM", 2) != 0) {
		return 0;
	}
	const uint32_t imageOffset = READ_LE_UINT32(src + 0xA);
	const int width = READ_LE_UINT32(src + 0x12);
	const int height = READ_LE_UINT32(src + 0x16);
	const int depth = READ_LE_UINT16(src + 0x1C);
	const int compression = READ_LE_UINT32(src + 0x1E);
	if ((depth != 8 && depth != 32) || compression != 0) {
		warning("Unhandled bitmap depth %d compression %d", depth, compression);
		return 0;
	}
	// const int bpp = (!alpha && colorKey < 0) ? 3 : 4;
	uint16_t *dst = (uint16_t *)malloc(width * height * 2);
	if (!dst) {
		warning("Failed to allocate bitmap buffer, width %d height %d", width, height);
		return 0;
	}
	if (depth == 8) {
		const uint8_t *palette = src + 14 /* BITMAPFILEHEADER */ + 40 /* BITMAPINFOHEADER */;
		// const bool flipY = true;
		clut_toRGB5551(src + imageOffset, palette, (width + 3) & ~3, width, height, flipY, colorKey, dst);
	} else {
		// assert(depth == 32 && bpp == 3);
		const uint8_t *p = src + imageOffset;
		for (int y = height - 1; y >= 0; --y) {
			uint16_t *q = dst + y * width;
			for (int x = 0; x < width; ++x) {
				const uint32_t color = READ_LE_UINT32(p); p += 4;
				*q++ = ((color >> 3) << 10) | (((color >>  8) >> 3) << 5) | ((color >> 16) >> 3);
			}
		}
	}
	*w = width;
	*h = height;
	return dst;
}
