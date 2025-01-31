
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <math.h>
#include "graphics.h"
#include "util.h"
#include "screenshot.h"
#include "systemstub.h"

extern "C"
{
	#include <pspkernel.h>
	#include <pspdisplay.h>
	#include <pspdmac.h>
	#include <pspgu.h>
	#include <pspgum.h>
}

struct GraphicsSoft: Graphics {
	typedef void (GraphicsSoft::*drawLine)(int16_t x1, int16_t x2, int16_t y, uint8_t col);

	uint8_t *_pagePtrs[4];
	uint8_t *_drawPagePtr;
	uint16_t _bmpBackground[SCREEN_WIDTH*SCREEN_HEIGHT];	
	int _u, _v;
	int _w, _h;
	int _byteDepth;
	Color _pal[16];	
	int _screenshotNum;

	uint16_t *_spriteAtlas;
	uint8_t *_spriteAtlas8bpp;
	int _spriteAtlasW, _spriteAtlasH, _spriteAtlasXSize, _spriteAtlasYSize;

	int _lastVScroll;

	GraphicsSoft();
	~GraphicsSoft();

	int xScale(int x) const { return (x * _u) >> 16; }
	int yScale(int y) const { return (y * _v) >> 16; }

	void setSize(int w, int h);
	void drawPolygon(uint8_t color, const QuadStrip &qs);
	void drawChar(uint8_t c, uint16_t x, uint16_t y, uint8_t color);
	void drawSpriteMask(int x, int y, uint8_t color, const uint8_t *data);
	void drawPoint(int16_t x, int16_t y, uint8_t color);
	void drawLineT(int16_t x1, int16_t x2, int16_t y, uint8_t color);
	void drawLineN(int16_t x1, int16_t x2, int16_t y, uint8_t color);
	void drawLineP(int16_t x1, int16_t x2, int16_t y, uint8_t color);
	uint8_t *getPagePtr(uint8_t page);
	int getPageSize() const { return _w * _h * _byteDepth; }
	void setWorkPagePtr(uint8_t page);

	virtual void init(int targetW, int targetH);
	virtual void fini();
	virtual void setFont(const uint8_t *src, int w, int h);
	virtual void setPalette(const Color *colors, int count);
	virtual void setSpriteAtlas(const uint8_t *src, int w, int h, int xSize, int ySize);
	virtual void drawSprite(int buffer, int num, const Point *pt, uint8_t color);
	virtual void drawBitmap(int buffer, const uint8_t *data, int w, int h, int fmt);
	virtual void drawPoint(int buffer, uint8_t color, const Point *pt);
	virtual void drawQuadStrip(int buffer, uint8_t color, const QuadStrip *qs);
	virtual void drawStringChar(int buffer, uint8_t color, char c, const Point *pt);
	virtual void clearBuffer(int num, uint8_t color);
	virtual void copyBuffer(int dst, int src, int vscroll = 0);
	virtual void drawBuffer(int num, SystemStub *stub);
	virtual void drawRect(int num, uint8_t color, const Point *pt, int w, int h);
	virtual void drawBitmapOverlay(const uint8_t *data, int w, int h, int fmt, SystemStub *stub);
};

static unsigned int vram_buffer_pos = 0;
static void *vram_buffer_back;
static void *edram_buffer_back;
static void *vram_buffer_front;
static void *edram_buffer_front;

static void *current_back_buffer;

GraphicsSoft::GraphicsSoft() {
	vram_buffer_back = (void*)(vram_buffer_pos);
	current_back_buffer = edram_buffer_back = (void*)(sceGeEdramGetAddr() + vram_buffer_pos);
	vram_buffer_pos += ((unsigned int)(512*272*2)); // Buffer is 512 x screen height x 2 bytes (GU_PSM_5551)
	vram_buffer_front = (void*)(vram_buffer_pos);
	edram_buffer_front = (void*)(sceGeEdramGetAddr() + vram_buffer_pos);
	vram_buffer_pos += ((unsigned int)(512*272*2)); // Buffer is 512 x screen height x 4 bytes (GU_PSM_5551)

	_fixUpPalette = FIXUP_PALETTE_NONE;
	memset(_pagePtrs, 0, sizeof(_pagePtrs));
	memset(_pal, 0, sizeof(_pal));
	_screenshotNum = 1;

	_lastVScroll = 0;
}

GraphicsSoft::~GraphicsSoft() {
	for (int i = 0; i < 4; ++i) {
		free(_pagePtrs[i]);
		_pagePtrs[i] = 0;
	}
}

static void convert55512BufferTo8bpp(uint16_t *src, uint8_t *dst, int32_t size, Color *pal)
{
	debug(DBG_INFO, "convert55512BufferTo8bpp");

	int i, j;
	int dist = INT_MAX, current_dist = INT_MAX;
	int current_color = 0;
	uint16_t last_color = 0;
	uint16_t pal5551[16];
	int c = 0, c2 = 0;
	for(i = 0; i < 16; i++)
	{
		pal5551[i] = pal[i].rgb5551();
	}

	for(i = 0; i < size; i++)
	{
		if(i == 0 || (*src) != last_color)
		{
			if ((*src) < 0x8000)
			{
				current_color = 100;
			}
			else
			{
				current_dist = dist = INT_MAX;
				current_color = 0;
				for(j = 0; j < 16; j++)
				{
					c = (*src) & 0x1F; c2 = pal5551[j] & 0x1F;
					current_dist = c < c2 ? c2 - c : c - c2;
					c = ((*src)>>5) & 0x1F; c2 = (pal5551[j]>>5) & 0x1F;
					current_dist += c < c2 ? c2 - c : c - c2;
					if ((*src) != 0xBA18) // small hack to get the face color right
					{
						c = ((*src)>>10) & 0x1F; c2 = (pal5551[j]>>10) & 0x1F;
						current_dist += c < c2 ? c2 - c : c - c2;
					}
					if (current_dist < dist)
					{
						dist = current_dist;
						current_color = j;
					}
				}
			}
			last_color = (*src);
		}
		(*dst) = current_color;
		dst++;
		src++;
	}
}

void GraphicsSoft::setSize(int w, int h) {
	w = SCREEN_WIDTH;
	h = SCREEN_HEIGHT;
	_u = (w << 16) / GFX_W;
	_v = (h << 16) / GFX_H;
	_w = w;
	_h = h;
	_byteDepth = _use555 ? 2 : 1;
	assert(_byteDepth == 1 || _byteDepth == 2);
	for (int i = 0; i < 4; ++i) {
		_pagePtrs[i] = (uint8_t *)realloc(_pagePtrs[i], getPageSize());
		if (!_pagePtrs[i]) {
			error("Not enough memory to allocate offscreen buffers");
		}
		memset(_pagePtrs[i], 0, getPageSize());
	}
	setWorkPagePtr(2);
}

static uint32_t calcStep(const Point &p1, const Point &p2, uint16_t &dy) {
	dy = p2.y - p1.y;
	uint16_t delta = (dy <= 1) ? 1 : dy;
	return ((p2.x - p1.x) * (0x4000 / delta)) << 2;
}

void GraphicsSoft::drawPolygon(uint8_t color, const QuadStrip &quadStrip) {
	QuadStrip qs = quadStrip;
	if (_w != GFX_W || _h != GFX_H) {	
		for (int i = 0; i < qs.numVertices; ++i) {
			qs.vertices[i].scale(_u, _v);
		}
	}

	int i = 0;
	int j = qs.numVertices - 1;

	int16_t x2 = qs.vertices[i].x;
	int16_t x1 = qs.vertices[j].x;
	int16_t hliney = MIN(qs.vertices[i].y, qs.vertices[j].y);

	++i;
	--j;

	drawLine pdl;
	switch (color) {
	default:
		pdl = &GraphicsSoft::drawLineN;
		break;
	case COL_PAGE:
		pdl = &GraphicsSoft::drawLineP;
		break;
	case COL_ALPHA:
		pdl = &GraphicsSoft::drawLineT;
		break;
	}

	uint32_t cpt1 = x1 << 16;
	uint32_t cpt2 = x2 << 16;

	int numVertices = qs.numVertices;
	while (1) {
		numVertices -= 2;
		if (numVertices == 0) {
			return;
		}
		uint16_t h;
		uint32_t step1 = calcStep(qs.vertices[j + 1], qs.vertices[j], h);
		uint32_t step2 = calcStep(qs.vertices[i - 1], qs.vertices[i], h);
		
		++i;
		--j;

		cpt1 = (cpt1 & 0xFFFF0000) | 0x7FFF;
		cpt2 = (cpt2 & 0xFFFF0000) | 0x8000;

		if (h == 0) {
			cpt1 += step1;
			cpt2 += step2;
		} else {
			while (h--) {
				if (hliney >= 0) {
					x1 = cpt1 >> 16;
					x2 = cpt2 >> 16;
					if (x1 < _w && x2 >= 0) {
						if (x1 < 0) x1 = 0;
						if (x2 >= _w) x2 = _w - 1;
						(this->*pdl)(x1, x2, hliney, color);
					}
				}
				cpt1 += step1;
				cpt2 += step2;
				++hliney;
				if (hliney >= _h) return;
			}
		}
	}
}

void GraphicsSoft::drawChar(uint8_t c, uint16_t x, uint16_t y, uint8_t color) {
	if (x <= GFX_W - 8 && y <= GFX_H - 8) {
		x = xScale(x);
		y = yScale(y);
		const uint8_t *ft = _font + (c - 0x20) * 8;
		const int offset = (x + y * _w) * _byteDepth;
		if (_byteDepth == 1) {
			for (int j = 0; j < 8; ++j) {
				const uint8_t ch = ft[j];
				for (int i = 0; i < 8; ++i) {
					if (ch & (1 << (7 - i))) {
						_drawPagePtr[offset + j * _w + i] = color;
					}
				}
			}
		} else if (_byteDepth == 2) {
			const uint16_t rgbColor = _pal[color].rgb555();
			for (int j = 0; j < 8; ++j) {
				const uint8_t ch = ft[j];
				for (int i = 0; i < 8; ++i) {
					if (ch & (1 << (7 - i))) {
						((uint16_t *)(_drawPagePtr + offset))[j * _w + i] = rgbColor;
					}
				}
			}
		}
	}
}
void GraphicsSoft::drawSpriteMask(int x, int y, uint8_t color, const uint8_t *data) {
	const int w = *data++;
	x = xScale(x - w / 2);
	const int h = *data++;
	y = yScale(y - h / 2);
	assert(_byteDepth == 1);
	for (int j = 0; j < h; ++j) {
		const int yoffset = y + j;
		for (int i = 0; i <= w / 16; ++i) {
			const uint16_t mask = READ_BE_UINT16(data); data += 2;
			if (yoffset < 0 || yoffset >= _h) {
				continue;
			}
			const int xoffset = x + i * 16;
			for (int b = 0; b < 16; ++b) {
				if (xoffset + b < 0 || xoffset + b >= _w) {
					continue;
				}
				if (mask & (1 << (15 - b))) {
					_drawPagePtr[yoffset * _w + xoffset + b] = color;
				}
			}
		}
	}
}

static void blend_rgb555(uint16_t *dst, const uint16_t b) {
	static const uint16_t RB_MASK = 0x7c1f;
	static const uint16_t G_MASK  = 0x03e0;
	uint16_t a = *dst;
	if ((a & 0x8000) == 0) { // use bit 15 to prevent additive blending
		uint16_t r = 0x8000;
		r |= (((a & RB_MASK) + (b & RB_MASK)) >> 1) & RB_MASK;
		r |= (((a &  G_MASK) + (b &  G_MASK)) >> 1) &  G_MASK;
		*dst = r;
	}
}

void GraphicsSoft::drawPoint(int16_t x, int16_t y, uint8_t color) {
	x = xScale(x);
	y = yScale(y);
	const int offset = (y * _w + x) * _byteDepth;
	if (_byteDepth == 1) {
		switch (color) {
		case COL_ALPHA:
			_drawPagePtr[offset] |= 8;
			break;
		case COL_PAGE:
			_drawPagePtr[offset] = *(_pagePtrs[0] + offset);
			break;
		default:
			_drawPagePtr[offset] = color;
			break;
		}
	} else if (_byteDepth == 2) {
		switch (color) {
		case COL_ALPHA:
			blend_rgb555((uint16_t *)(_drawPagePtr + offset), _pal[ALPHA_COLOR_INDEX].rgb555());
			break;
		case COL_PAGE:
			*(uint16_t *)(_drawPagePtr + offset) = *(uint16_t *)(_pagePtrs[0] + offset);
			break;
		default:
			*(uint16_t *)(_drawPagePtr + offset) = _pal[color].rgb555();
			break;
		}
	}
}

void GraphicsSoft::drawLineT(int16_t x1, int16_t x2, int16_t y, uint8_t color) {
	int16_t xmax = MAX(x1, x2);
	int16_t xmin = MIN(x1, x2);
	int w = xmax - xmin + 1;
	const int offset = (y * _w + xmin) * _byteDepth;
	if (_byteDepth == 1) {
		for (int i = 0; i < w; ++i) {
			_drawPagePtr[offset + i] |= 8;
		}
	} else if (_byteDepth == 2) {
		const uint16_t rgbColor = _pal[ALPHA_COLOR_INDEX].rgb555();
		uint16_t *p = (uint16_t *)(_drawPagePtr + offset);
		for (int i = 0; i < w; ++i) {
			blend_rgb555(p + i, rgbColor);
		}
	}
}

void GraphicsSoft::drawLineN(int16_t x1, int16_t x2, int16_t y, uint8_t color) {
	int16_t xmax = MAX(x1, x2);
	int16_t xmin = MIN(x1, x2);
	const int w = xmax - xmin + 1;
	const int offset = (y * _w + xmin) * _byteDepth;
	if (_byteDepth == 1) {
		memset(_drawPagePtr + offset, color, w);
	} else if (_byteDepth == 2) {
		const uint16_t rgbColor = _pal[color].rgb555();
		uint16_t *p = (uint16_t *)(_drawPagePtr + offset);
		for (int i = 0; i < w; ++i) {
			p[i] = rgbColor;
		}
	}
}

void GraphicsSoft::drawLineP(int16_t x1, int16_t x2, int16_t y, uint8_t color) {
	if (_drawPagePtr == _pagePtrs[0]) {
		return;
	}
	int16_t xmax = MAX(x1, x2);
	int16_t xmin = MIN(x1, x2);
	const int w = xmax - xmin + 1;
	const int offset = (y * _w + xmin) * _byteDepth;
	memcpy(_drawPagePtr + offset, _pagePtrs[0] + offset, w * _byteDepth);
}

uint8_t *GraphicsSoft::getPagePtr(uint8_t page) {
	assert(page >= 0 && page < 4);
	return _pagePtrs[page];
}

void GraphicsSoft::setWorkPagePtr(uint8_t page) {
	_drawPagePtr = getPagePtr(page);
}

void GraphicsSoft::init(int targetW, int targetH) {
	Graphics::init(targetW, targetH);
	setSize(targetW, targetH);

	sceGuInit();

	sceGuStart(GU_DIRECT,_display_list);

	sceGuDrawBuffer(GU_PSM_5551,vram_buffer_back,512);
	sceGuDispBuffer(SCREEN_WIDTH,SCREEN_HEIGHT,vram_buffer_front,512);
    sceGuOffset(2048 - (SCREEN_WIDTH/2),2048 - (SCREEN_HEIGHT/2));
    sceGuViewport(2048,2048,SCREEN_WIDTH,SCREEN_HEIGHT);
	sceGuScissor(0,0,SCREEN_WIDTH,SCREEN_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuDisable(GU_CULL_FACE);
	sceGuDisable(GU_DEPTH_TEST);
	sceGuEnable(GU_BLEND);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

	sceGuShadeModel(GU_SMOOTH);
	sceGuDisable(GU_TEXTURE_2D);

	sceGuAmbientColor(0xffffffff);

	sceGumMatrixMode(GU_PROJECTION);
	sceGumLoadIdentity();
	sceGumOrtho(0, ORIGINAL_SCREEN_WIDTH_F, ORIGINAL_SCREEN_HEIGHT_F, 0, -1, 1);

	sceGumMatrixMode(GU_VIEW);
	sceGumLoadIdentity();

	sceGumMatrixMode(GU_MODEL);
	sceGumLoadIdentity();

    sceGuFinish();
	sceGuSync(0,0);
 
	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);
}

void GraphicsSoft::fini()
{
	sceGuTerm();
}

void GraphicsSoft::setFont(const uint8_t *src, int w, int h) {
	if (_is1991) {
		// no-op for 1991
	}
}

void GraphicsSoft::setPalette(const Color *colors, int count) {
	memcpy(_pal, colors, sizeof(Color) * MIN(count, 16));

	convert55512BufferTo8bpp(_spriteAtlas, _spriteAtlas8bpp, _spriteAtlasW * _spriteAtlasH, _pal);
}

void GraphicsSoft::setSpriteAtlas(const uint8_t *src, int w, int h, int xSize, int ySize) {
	debug(DBG_INFO, "useSpriteAtlas %d %d %d %d", w, h, xSize, ySize);

	if (_is1991) {
		// no-op for 1991
	}

	int targetW = 52, targetH = 48;
	float ratioX = (float)w / (float)targetW;
	float ratioY = (float)h / (float)targetH;

	_spriteAtlas = (uint16_t*)malloc(targetW*targetH*sizeof(uint16_t));
	_spriteAtlas8bpp = (uint8_t*)malloc(targetW*targetH*sizeof(uint8_t));
	for (int y = 0; y < targetH; y++)
	{
		for (int x = 0; x < targetW; x++)
		{
			_spriteAtlas[y * targetW + x] = ((uint16_t*)src)[(int)((float)y * ratioY) * w + (int)((float)x * ratioX)];
		}
	}
	//memcpy(_spriteAtlas, src, w*h*sizeof(uint8_t));
	_spriteAtlasW = targetW;
	_spriteAtlasH = targetH;
	_spriteAtlasXSize = xSize;
	_spriteAtlasYSize = ySize;
}

void GraphicsSoft::drawSprite(int buffer, int num, const Point *pt, uint8_t color) {
	debug(DBG_INFO, "drawSprite %d %d %d %d %d %d", buffer, num, pt->x, pt->y, color);

	if (_is1991) {
		if (num < _shapesMaskCount) {
			setWorkPagePtr(buffer);
			const uint8_t *data = _shapesMaskData + _shapesMaskOffset[num];
			drawSpriteMask(pt->x, pt->y, color, data);
		}
		return;
	}

	int posX = (int)((float)pt->x * SCREEN_WIDTH_F / ORIGINAL_SCREEN_WIDTH_F);
	int posY = (int)((float)pt->y * SCREEN_HEIGHT_F / ORIGINAL_SCREEN_HEIGHT_F);

	uint32_t address = posY * _w + posX;
	uint32_t address2 = ((num & 2) >> 1) * (_spriteAtlasH / 2) * _spriteAtlasW + (num & 1) * (_spriteAtlasW / 2);
	uint8_t *target = getPagePtr(buffer);
	for(int y = posY; y < posY + (_spriteAtlasH / 2); y++)
	{
		for(int x = posX; x < posX + (_spriteAtlasW / 2); x++)
		{
			if (posX >= 0 && posY >= 0 && posX < SCREEN_WIDTH && posY < SCREEN_HEIGHT && _spriteAtlas8bpp[address2] < 16)
			{
				target[address] = _spriteAtlas8bpp[address2];
			}
			address++; address2++;
		}
		address += _w - (_spriteAtlasW / 2);
		address2 += _spriteAtlasW - (_spriteAtlasW / 2);
	}
}

void GraphicsSoft::drawBitmap(int buffer, const uint8_t *data, int w, int h, int fmt) {
	debug(DBG_INFO, "drawBitmap %d %p %d %d %d %p", buffer, data, w, h, fmt, getPagePtr(buffer));

	switch (_byteDepth) {
	case 1:
		if (fmt == FMT_CLUT && _w == w && _h == h) {
			memcpy(getPagePtr(buffer), data, w * h);
			return;
		}
		if (fmt == FMT_RGB)
		{
			float xFactor = (float)w / (float)_w;
			float yFactor = (float)h / (float)_h;
			int srcX, srcY;
			int address = 0;			
			for(int j = 0; j < _h; j++)
			{
				for(int i = 0; i < _w; i++)
				{
					srcX = (int) ((float)i * xFactor);
					srcY = (int) ((float)j * yFactor);
					_bmpBackground[address++] = (data[srcY * w * 3 + srcX * 3] >> 3) | ((data[srcY * w * 3 + (srcX * 3 + 1)] >> 3) << 5) | ((data[srcY * w * 3 + (srcX * 3 + 2)] >> 3) << 10);
				}
			}

			memset(getPagePtr(buffer), 0xFF, getPageSize());
			return;
		}
		break;
	case 2:
		if (fmt == FMT_RGB555) {
			if (_w == w && _h == h)
			{
				memcpy(getPagePtr(buffer), data, getPageSize());
				return;
			}
			else
			{
				debug(DBG_INFO, "drawBitmap scaled");
				float xFactor = (float)w / (float)_w;
				float yFactor = (float)h / (float)_h;
				uint16_t *dst = (uint16_t*)getPagePtr(buffer);
				uint16_t *src = (uint16_t*)data;
				int address = 0;
				int srcX, srcY;
				for(int j = 0; j < _h; j++)
				{
					for(int i = 0; i < _w; i++)
					{
						srcX = (int) ((float)i * xFactor);
						srcY = (int) ((float)j * yFactor);
						dst[address++] = src[srcY * w + srcX];
					}
				}
				return;
			}
		}
		break;
	}

	warning("GraphicsSoft::drawBitmap() unhandled fmt %d w %d h %d", fmt, w, h);
}

void GraphicsSoft::drawPoint(int buffer, uint8_t color, const Point *pt) {
	setWorkPagePtr(buffer);
	drawPoint(pt->x, pt->y, color);
}

void GraphicsSoft::drawQuadStrip(int buffer, uint8_t color, const QuadStrip *qs) {
	setWorkPagePtr(buffer);
	drawPolygon(color, *qs);
}

void GraphicsSoft::drawStringChar(int buffer, uint8_t color, char c, const Point *pt) {
	setWorkPagePtr(buffer);
	drawChar(c, pt->x, pt->y, color);
}

void GraphicsSoft::clearBuffer(int num, uint8_t color) {
	debug(DBG_INFO, "clearBuffer %d %d", num, color);
	if (_byteDepth == 1) {
		memset(getPagePtr(num), color, getPageSize());
	} else if (_byteDepth == 2) {
		const uint16_t rgbColor = _pal[color].rgb555();
		uint16_t *p = (uint16_t *)getPagePtr(num);
		for (int i = 0; i < _w * _h; ++i) {
			p[i] = rgbColor;
		}
	}
}

void GraphicsSoft::copyBuffer(int dst, int src, int vscroll) {
	debug(DBG_INFO, "copyBuffer %d -> %d (%d) %p -> %p", src, dst, vscroll, getPagePtr(src), getPagePtr(dst));

	if (vscroll == 0) {
		memcpy(getPagePtr(dst), getPagePtr(src), getPageSize());
		_lastVScroll = 0;
	} else if (vscroll >= -199 && vscroll <= 199) {
		memcpy(getPagePtr(dst), getPagePtr(src), getPageSize());
		const int dy = yScale(vscroll);		
		_lastVScroll = dy;
	}
}

static void dumpBuffer555(const uint16_t *src, int w, int h, int num) {
	char name[32];
	snprintf(name, sizeof(name), "screenshot-%d.tga", num);
	saveTGA(name, src, w, h);
	debug(DBG_INFO, "Written '%s'", name);
}

static void dumpPalette555(uint16_t *dst, int w, const Color *pal) {
	static const int SZ = 16;
	for (int color = 0; color < 16; ++color) {
		uint16_t *p = dst + (color & 7) * SZ;
		for (int y = 0; y < SZ; ++y) {
			for (int x = 0; x < SZ; ++x) {
				p[x] = pal[color].rgb555();
			}
			p += w;
		}
		if (color == 7) {
			dst += SZ * w;
		}
	}
}

void GraphicsSoft::drawBuffer(int num, SystemStub *stub) {
	debug(DBG_INFO, "drawBuffer %d", num);

	int w, h;
	float ar[4];
	stub->prepareScreen(w, h, ar);
	if (_byteDepth == 1) {
		const uint8_t *src = getPagePtr(num);
		int address = 0, addressDst = 0;

		if (_lastVScroll < 0)
		{
			addressDst = -_lastVScroll * 512;
		}
		else if (_lastVScroll > 0)
		{
			address = _lastVScroll * SCREEN_WIDTH;
		}

		for(int j = 0; j < _h; ++j)
		{
			for (int i = 0; i < _w; ++i)
			{
				if (src[address] != 0xFF)
				{
					_colorBuffer[addressDst] = _pal[src[address]].rgb5551();
				}
				else
				{
					_colorBuffer[addressDst] = _bmpBackground[address];
				}
				address++;				
				addressDst++;
			}
			if (address == SCREEN_WIDTH * SCREEN_HEIGHT) { address -= SCREEN_WIDTH; }
			if (addressDst >= 512 * SCREEN_HEIGHT) break;
			addressDst += 512 - _w;
		}
		sceKernelDcacheWritebackAll();
		sceGuStart(GU_DIRECT,_display_list);
		sceGuCopyImage(GU_PSM_5551, 0, 0, _w, _h, 512, (void*)_colorBuffer, 0, 0, 512, current_back_buffer);
		sceGuTexSync();
		sceGuFinish();
		sceGuSync(0,0);
	} else if (_byteDepth == 2) {
		const uint16_t *src = (uint16_t *)getPagePtr(num);
		int address = 0, addressDst = 0;
		uint16_t srcColor;
		for(int j = 0; j < _h; ++j)
		{
			for (int i = 0; i < _w; ++i)
			{
				srcColor = src[address];
				_colorBuffer[addressDst] = ((srcColor & 0x1F) << 10) | (srcColor & 0x3E0) | ((srcColor & 0x7C00) >> 10);
				address++;
				addressDst++;
			}
			addressDst += 512 - _w;
		}
		sceKernelDcacheWritebackAll();
		sceGuStart(GU_DIRECT,_display_list);
		sceGuCopyImage(GU_PSM_5551, 0, 0, _w, _h, 512, (void*)_colorBuffer, 0, 0, 512, current_back_buffer);
		sceGuTexSync();
		sceGuFinish();
		sceGuSync(0,0);
	}

	stub->updateScreen();

	current_back_buffer = (current_back_buffer == edram_buffer_back) ? edram_buffer_front : edram_buffer_back;
}

void GraphicsSoft::drawRect(int num, uint8_t color, const Point *pt, int w, int h) {
	assert(_byteDepth == 2);
	setWorkPagePtr(num);
	const uint16_t rgbColor = _pal[color].rgb555();
	const int x1 = xScale(pt->x);
	const int y1 = yScale(pt->y);
	const int x2 = xScale(pt->x + w - 1);
	const int y2 = yScale(pt->y + h - 1);
	// horizontal
	for (int x = x1; x <= x2; ++x) {
		*(uint16_t *)(_drawPagePtr + (y1 * _w + x) * _byteDepth) = rgbColor;
		*(uint16_t *)(_drawPagePtr + (y2 * _w + x) * _byteDepth) = rgbColor;
	}
	// vertical
	for (int y = y1; y <= y2; ++y) {
		*(uint16_t *)(_drawPagePtr + (y * _w + x1) * _byteDepth) = rgbColor;
		*(uint16_t *)(_drawPagePtr + (y * _w + x2) * _byteDepth) = rgbColor;
	}
}

void GraphicsSoft::drawBitmapOverlay(const uint8_t *data, int w, int h, int fmt, SystemStub *stub) {
	if (fmt == FMT_RGB555) {
		stub->setScreenPixels555((const uint16_t *)data, w, h);
		stub->updateScreen();
	}
}

Graphics *GraphicsSoft_create() {
	return new GraphicsSoft();
}
