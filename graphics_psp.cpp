
/*
 */

#include <SDL.h>
#include <math.h>
#include <vector>
#include "graphics.h"
#include "util.h"
#include "systemstub.h"

extern "C"
{
	#include <pspkernel.h>
	#include <pspdisplay.h>
	#include <pspdmac.h>
	#include <pspgu.h>
	#include <pspgum.h>
}

enum {
	DrawListEntryQuadStrip = 0,
	DrawListEntryCharacter = 1
};

struct DrawListEntry
{
	int type;
	QuadStrip quadStrip;
	uint8_t color;
	char c;
	Point p;
};

struct DrawList
{
	int numEntries;
	struct DrawListEntry entries[1000];
	uint8_t clearColor;
};

struct GraphicsPSP : Graphics {
	Color _palette[16];
	struct DrawList _drawLists[4];

	uint32_t getColor(uint8_t color);
	uint16_t get5551Color(uint8_t color);
	void innerClearBuffer(int listNum, uint8_t color);
	void innerDrawQuadStrip(int listNum, uint8_t color, const QuadStrip *qs, bool allowStencil);
	void innerDrawStringChar(int listNum, uint8_t color, char c, const Point *pt);

	GraphicsPSP();
	virtual ~GraphicsPSP() {}

	virtual void init(int targetW, int targetH);
	virtual void fini();
	virtual void setFont(const uint8_t *src, int w, int h);
	virtual void setPalette(const Color *colors, int count);
	virtual void setSpriteAtlas(const uint8_t *src, int w, int h, int xSize, int ySize);
	virtual void drawSprite(int listNum, int num, const Point *pt, uint8_t color);
	virtual void drawBitmap(int listNum, const uint8_t *data, int w, int h, int fmt);
	virtual void drawPoint(int listNum, uint8_t color, const Point *pt);
	virtual void drawQuadStrip(int listNum, uint8_t color, const QuadStrip *qs);
	virtual void drawStringChar(int listNum, uint8_t color, char c, const Point *pt);
	virtual void clearBuffer(int listNum, uint8_t color);
	virtual void copyBuffer(int dstListNum, int srcListNum, int vscroll = 0);
	virtual void drawBuffer(int listNum, SystemStub *stub);
	virtual void drawRect(int num, uint8_t color, const Point *pt, int w, int h);
	virtual void drawBitmapOverlay(const uint8_t *data, int w, int h, int fmt, SystemStub *stub);
};

static uint32_t vram_buffer_pos = 0;
static void *vram_buffer[4];
static void *edram_buffer[4];
static void *vram_buffer_back;
static void *edram_buffer_back;
static void *vram_buffer_front;
static void *edram_buffer_front;
static void *vram_font_tex;
static void *edram_font_tex;
static void *vram_sprite_tex;
static void *edram_sprite_tex;

uint32_t GraphicsPSP::getColor(uint8_t color)
{
	uint32_t result = 0xFFFFFFFF;

	if (color == COL_ALPHA) // alpha
	{
		result = 0xC0000000 | (_palette[12].b << 16) | (_palette[12].g << 8) | (_palette[12].r);
	}
	else if (color < 16)
	{
		result = 0xFF000000 | (_palette[color].b << 16) | (_palette[color].g << 8) | (_palette[color].r);
	}

	return result;
}

uint16_t GraphicsPSP::get5551Color(uint8_t color)
{
	uint16_t result = 0xFFFF;

	if (color == COL_ALPHA) // alpha
	{
		result = 0x0000;
	}
	else if (color < 16)
	{
		result = 0x8000 | ((_palette[color].b >> 3) << 10) | ((_palette[color].g >> 3) << 5) | ((_palette[color].r >> 3));
	}

	return result;
}

GraphicsPSP::GraphicsPSP() {
	_fixUpPalette = FIXUP_PALETTE_NONE;

	vram_buffer[0] = (void*)(vram_buffer_pos);
	edram_buffer[0] = (void*)((int)sceGeEdramGetAddr() + vram_buffer_pos);
	vram_buffer_pos += ((unsigned int)(512*272*2)); // Buffer is 512 x 512 x 2 bytes (GU_PSM_5551)
	vram_buffer[1] = (void*)(vram_buffer_pos);
	edram_buffer[1] = (void*)((int)sceGeEdramGetAddr() + vram_buffer_pos);
	vram_buffer_pos += ((unsigned int)(512*272*2)); // Buffer is 512 x screen height x 2 bytes (GU_PSM_5551)
	vram_buffer[2] = (void*)(vram_buffer_pos);
	edram_buffer[2] = (void*)((int)sceGeEdramGetAddr() + vram_buffer_pos);
	vram_buffer_pos += ((unsigned int)(512*272*2)); // Buffer is 512 x screen height x 2 bytes (GU_PSM_5551)
	vram_buffer[3] = (void*)(vram_buffer_pos);
	edram_buffer[3] = (void*)((int)sceGeEdramGetAddr() + vram_buffer_pos);
	vram_buffer_pos += ((unsigned int)(512*272*2)); // Buffer is 512 x screen height x 2 bytes (GU_PSM_5551)
	vram_buffer_back = (void*)(vram_buffer_pos);
	edram_buffer_back = (void*)((int)sceGeEdramGetAddr() + vram_buffer_pos);
	vram_buffer_pos += ((unsigned int)(512*272*2)); // Buffer is 512 x screen height x 2 bytes (GU_PSM_5551)
	vram_buffer_front = (void*)(vram_buffer_pos);
	edram_buffer_front = (void*)((int)sceGeEdramGetAddr() + vram_buffer_pos);
	vram_buffer_pos += ((unsigned int)(512*272*2)); // Buffer is 512 x screen height x 2 bytes (GU_PSM_5551)
	vram_font_tex = (void*)(vram_buffer_pos);
	edram_font_tex = (void*)((int)sceGeEdramGetAddr() + vram_buffer_pos);
	vram_buffer_pos += ((unsigned int)(128*128)); // Buffer is 128 x 128 x 1 byte (GU_PSM_T8)
	vram_sprite_tex = (void*)(vram_buffer_pos);
	edram_sprite_tex = (void*)((int)sceGeEdramGetAddr() + vram_buffer_pos);
	vram_buffer_pos += ((unsigned int)(128*128*2)); // Buffer is 128 x 128 x 2 byte (GU_PSM_5551)

	for (int i = 0; i < 4; i++)
	{
		_drawLists[i].numEntries = 0;
		_drawLists[i].clearColor = 0;
	}
}

void GraphicsPSP::init(int targetW, int targetH) {
	Graphics::init(targetW, targetH);

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

void GraphicsPSP::fini() {
	sceGuTerm();
}

void GraphicsPSP::setFont(const uint8_t *src, int w, int h) {
	debug(DBG_INFO, "setFont %p %d %d", src, w, h);
	if (src == NULL)
	{
		sceKernelDcacheWritebackAll();
		sceGuStart(GU_DIRECT,_display_list);
		sceGuCopyImage(GU_PSM_T8, 0, 0, 128, 128, 128, (void*)_font_transformed, 0, 0, 128, edram_font_tex);
		sceGuTexSync();
		sceGuFinish();
		sceGuSync(0,0);
	}
	else
	{
		// TODO
	}
}

void GraphicsPSP::setPalette(const Color *colors, int n) {
	debug(DBG_INFO, "setPalette");	

	assert(n <= 16);	

	for (int i = 0; i < n; ++i) {
		_palette[i] = colors[i];
	}

	// Palette changed, must redraw all in screen
	if (_fixUpPalette == FIXUP_PALETTE_REDRAW)
	{
		for (int l = 0; l < 4; l++)
		{			
			if (_drawLists[l].clearColor != COL_BMP)
			{
				innerClearBuffer(l, _drawLists[l].clearColor);
			}
			
			for (int i = 0; i < _drawLists[l].numEntries; i++)
			{
				switch (_drawLists[l].entries[i].type)
				{
					case DrawListEntryQuadStrip:
						innerDrawQuadStrip(l, _drawLists[l].entries[i].color, &(_drawLists[l].entries[i].quadStrip), true);
						break;
					case DrawListEntryCharacter:
						innerDrawStringChar(l, _drawLists[l].entries[i].color, _drawLists[l].entries[i].c, &_drawLists[l].entries[i].p);
						break;
				}
			}
		}
	}
}

void GraphicsPSP::setSpriteAtlas(const uint8_t *src, int w, int h, int xSize, int ySize) {
	debug(DBG_INFO, "useSpriteAtlas %d %d %d %d", w, h, xSize, ySize);

	sceKernelDcacheWritebackAll();
	sceGuStart(GU_DIRECT,_display_list);
	sceGuCopyImage(GU_PSM_5551, 0, 0, 128, 128, 128, (void*)src, 0, 0, 128, edram_sprite_tex);
	sceGuTexSync();
	sceGuFinish();
	sceGuSync(0,0);
}

void GraphicsPSP::drawSprite(int listNum, int num, const Point *pt, uint8_t color) {
	debug(DBG_INFO, "drawSprite %d %d %d %d", listNum, num, pt->x, pt->y);

	sceGuStart(GU_DIRECT, _display_list);
	sceGuDrawBufferList(GU_PSM_5551,vram_buffer[listNum],512);

	sceGuDisable(GU_TEXTURE_2D);

	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexMode(GU_PSM_5551,0,0,0);
	sceGuTexImage(0,128,128,128,edram_sprite_tex);
	sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
	sceGuTexFilter(GU_NEAREST,GU_NEAREST);
	sceGuTexScale(1.0f,1.0f);
	sceGuTexOffset(0.0f,0.0f);

	sceGuColor(getColor(color));

	struct Vertex_UV *vertices = (struct Vertex_UV*)sceGuGetMemory(4*sizeof(struct Vertex_UV));

	vertices[0].x = pt->x;			vertices[0].y = pt->y;			vertices[0].z = 0.0f; 
	vertices[1].x = pt->x; 			vertices[1].y = pt->y + 18.0f;	vertices[1].z = 0.0f;
	vertices[2].x = pt->x + 18.0f; 	vertices[2].y = pt->y; 			vertices[2].z = 0.0f;
	vertices[3].x = pt->x + 18.0f; 	vertices[3].y = pt->y + 18.0f; 	vertices[3].z = 0.0f;

	float u = (float)(num % 2) / 2.0f;
	float v = (float)(num / 2) / 2.0f;

	vertices[0].u = u;			vertices[0].v = v;
	vertices[1].u = u;			vertices[1].v = v + 0.5f;
	vertices[2].u = u + 0.5f;	vertices[2].v = v;
	vertices[3].u = u + 0.5f;	vertices[3].v = v + 0.5f;

	sceGumDrawArray(GU_TRIANGLE_STRIP,GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D,4,0,vertices);

	sceGuDisable(GU_TEXTURE_2D);

	sceGuFinish();
	sceGuSync(0,0);
}

void GraphicsPSP::drawBitmap(int listNum, const uint8_t *data, int w, int h, int fmt) {
	debug(DBG_INFO, "drawBitmap %d %d %d %d", listNum, w, h, fmt);	

	float width_ratio = (float)w / SCREEN_WIDTH_F;
	float height_ratio = (float)h / SCREEN_HEIGHT_F;
	long address = 0, src_address = 0;
	uint16_t color = 0xFFFF;
	uint8_t r, g, b;

	for (int j = 0; j < SCREEN_HEIGHT; j++)
	{
		for (int i = 0; i < SCREEN_WIDTH; i++)
		{
			switch(fmt)
			{
				case FMT_RGB555:
					src_address = int(float(j) * height_ratio) * w + int(float(i) * width_ratio);
					color = ((uint16_t*)data)[src_address];
					color = 0x8000 | ((color & 0x1F) << 10) | (color & 0x3E0) | ((color & 0x7C00) >> 10);
					break;
				case FMT_RGB:
					src_address = int(float(j) * height_ratio) * w * 3 + int(float(i) * width_ratio) * 3;
					r = data[src_address];
					g = data[src_address + 1];
					b = data[src_address + 2];
					color = 0x8000 | (((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3));
					break;
				case FMT_RGBA:
					// TODO
					break;
			}
			_colorBuffer[address++] = color;
		}
		address += 512-SCREEN_WIDTH;
	}
	sceKernelDcacheWritebackAll();
	sceGuStart(GU_DIRECT,_display_list);
	sceGuCopyImage(GU_PSM_5551, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 512, (void*)_colorBuffer, 0, 0, 512, edram_buffer[listNum]);
	sceGuTexSync();
	sceGuFinish();
	sceGuSync(0,0);

	_drawLists[listNum].numEntries = 0;
	_drawLists[listNum].clearColor = COL_BMP;
}

void GraphicsPSP::drawPoint(int listNum, uint8_t color, const Point *pt) {
	sceGuStart(GU_DIRECT, _display_list);
	sceGuDrawBufferList(GU_PSM_5551,vram_buffer[listNum],512);

	sceGuColor(getColor(color));

	struct Vertex *vertices = (struct Vertex*)sceGuGetMemory(sizeof(struct Vertex));
	vertices[0].x = pt->x;
	vertices[0].y = pt->y;
	vertices[0].z = 0.0f;

	sceGumDrawArray(GU_POINTS,GU_VERTEX_32BITF|GU_TRANSFORM_3D,1,0,vertices);

	sceGuFinish();
	sceGuSync(0,0);

	if (_fixUpPalette != FIXUP_PALETTE_NONE)
	{
		int newEntry = _drawLists[listNum].numEntries;
		_drawLists[listNum].entries[newEntry].type = DrawListEntryQuadStrip;
		_drawLists[listNum].entries[newEntry].color = color;
		_drawLists[listNum].entries[newEntry].quadStrip.numVertices = 1;
		_drawLists[listNum].entries[newEntry].quadStrip.vertices[0].x = pt->x;
		_drawLists[listNum].entries[newEntry].quadStrip.vertices[0].y = pt->y;
		_drawLists[listNum].numEntries++;
	}
}

void GraphicsPSP::innerDrawQuadStrip(int listNum, uint8_t color, const QuadStrip *qs, bool allowStencil)
{
	sceGuStart(GU_DIRECT, _display_list);	
	sceGuDrawBufferList(GU_PSM_5551,vram_buffer[listNum],512);

	if (color == COL_PAGE && qs->numVertices > 3) // paint with list number 0
	{
		sceGuEnable(GU_TEXTURE_2D);
		sceGuTexMode(GU_PSM_5551,0,0,0);
		sceGuTexImage(0,512,512,512,edram_buffer[0]);
		sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGB);
		sceGuTexFilter(GU_LINEAR,GU_LINEAR);
		sceGuTexScale(1.0f,1.0f);
		sceGuTexOffset(0.0f,0.0f);

		sceGuColor(0xFFFFFFFF);

		struct Vertex_UV *vertices = (struct Vertex_UV*)sceGuGetMemory(qs->numVertices*sizeof(struct Vertex_UV));
	
		for(int i = 0; i < qs->numVertices / 2; i++)
		{
			const int j = qs->numVertices - 1 - i;
			if (qs->vertices[j].x > qs->vertices[i].x) {
				vertices[i*2+0].x = qs->vertices[i].x;
				vertices[i*2+0].v = vertices[i*2+0].y = qs->vertices[i].y;			

				vertices[i*2+1].x = qs->vertices[j].x + 1;
				vertices[i*2+1].y = qs->vertices[j].y;
			} else {
				vertices[i*2+0].x = qs->vertices[j].x;
				vertices[i*2+0].y = qs->vertices[j].y;

				vertices[i*2+1].x = qs->vertices[i].x + 1;
				vertices[i*2+1].y = qs->vertices[i].y;
			}

			vertices[i*2+0].u = (float)vertices[i*2+0].x * (SCREEN_WIDTH_F / ORIGINAL_SCREEN_WIDTH_F) / 512.0f;
			vertices[i*2+0].v = (float)vertices[i*2+0].y * (SCREEN_HEIGHT_F / ORIGINAL_SCREEN_HEIGHT_F) / 512.0f;
			vertices[i*2+1].u = (float)vertices[i*2+1].x * (SCREEN_WIDTH_F / ORIGINAL_SCREEN_WIDTH_F) / 512.0f;
			vertices[i*2+1].v = (float)vertices[i*2+1].y * (SCREEN_HEIGHT_F / ORIGINAL_SCREEN_HEIGHT_F) / 512.0f;

			vertices[i*2+0].z = 0.0f;
			vertices[i*2+1].z = 0.0f;
		}

		sceGumDrawArray(GU_TRIANGLE_STRIP,GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D,qs->numVertices,0,vertices);

		sceGuDisable(GU_TEXTURE_2D);
	}
	else
	{
		sceGuColor(getColor(color));

		struct Vertex *vertices = (struct Vertex*)sceGuGetMemory(qs->numVertices*sizeof(struct Vertex));
		
		if (qs->numVertices < 4)
		{
			for(int i = 0; i < qs->numVertices; i++)
			{
				vertices[i].x = qs->vertices[i].x;
				vertices[i].y = qs->vertices[i].y;
				vertices[i].z = 0.0f;
			}
		}
		else
		{
			for(int i = 0; i < qs->numVertices / 2; i++)
			{
				const int j = qs->numVertices - 1 - i;
				if (qs->vertices[j].x > qs->vertices[i].x) {
					vertices[i*2+0].x = qs->vertices[i].x;
					vertices[i*2+0].y = qs->vertices[i].y;			

					vertices[i*2+1].x = qs->vertices[j].x + 1;
					vertices[i*2+1].y = qs->vertices[j].y;
				} else {
					vertices[i*2+0].x = qs->vertices[j].x;
					vertices[i*2+0].y = qs->vertices[j].y;

					vertices[i*2+1].x = qs->vertices[i].x + 1;
					vertices[i*2+1].y = qs->vertices[i].y;
				}
				vertices[i*2+0].z = 0.0f;
				vertices[i*2+1].z = 0.0f;
			}
		}

		switch(qs->numVertices)
		{
			case 1:
				sceGumDrawArray(GU_POINTS,GU_VERTEX_32BITF|GU_TRANSFORM_3D,qs->numVertices,0,vertices);		
				break;
			case 2:
				sceGumDrawArray(GU_LINES,GU_VERTEX_32BITF|GU_TRANSFORM_3D,qs->numVertices,0,vertices);
				break;
			default:
				sceGumDrawArray(GU_TRIANGLE_STRIP,GU_VERTEX_32BITF|GU_TRANSFORM_3D,qs->numVertices,0,vertices);
		}
	}

	sceGuFinish();
	sceGuSync(0,0);
}

void GraphicsPSP::drawQuadStrip(int listNum, uint8_t color, const QuadStrip *qs) {
	innerDrawQuadStrip(listNum, color, qs, true);

	if (_fixUpPalette != FIXUP_PALETTE_NONE)
	{
		int newEntry = _drawLists[listNum].numEntries;
		_drawLists[listNum].entries[newEntry].type = DrawListEntryQuadStrip;
		_drawLists[listNum].entries[newEntry].color = color;
		memcpy(&(_drawLists[listNum].entries[newEntry].quadStrip), qs, sizeof(struct QuadStrip));
		_drawLists[listNum].numEntries++;	
	}
}

void GraphicsPSP::innerDrawStringChar(int listNum, uint8_t color, char c, const Point *pt)
{
	sceGuStart(GU_DIRECT, _display_list);
	sceGuDrawBufferList(GU_PSM_5551,vram_buffer[listNum],512);

	sceGuEnable(GU_TEXTURE_2D);
	sceGuClutMode(GU_PSM_8888,0,0xff,0);
	sceGuClutLoad((256/8),_clut_font);
	sceGuTexMode(GU_PSM_T8,0,0,0);
	sceGuTexImage(0,128,128,128,edram_font_tex);
	sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
	sceGuTexFilter(GU_NEAREST,GU_NEAREST);
	sceGuTexScale(1.0f,1.0f);
	sceGuTexOffset(0.0f,0.0f);

	sceGuColor(getColor(color));

	struct Vertex_UV *vertices = (struct Vertex_UV*)sceGuGetMemory(4*sizeof(struct Vertex_UV));

	vertices[0].x = pt->x;			vertices[0].y = pt->y;			vertices[0].z = 0.0f; 
	vertices[1].x = pt->x; 			vertices[1].y = pt->y + 8.0f;	vertices[1].z = 0.0f;
	vertices[2].x = pt->x + 8.0f; 	vertices[2].y = pt->y; 			vertices[2].z = 0.0f;
	vertices[3].x = pt->x + 8.0f; 	vertices[3].y = pt->y + 8.0f; 	vertices[3].z = 0.0f;

	uint8_t col = ((c - 0x20) % 8);
	uint8_t row = ((c - 0x20) / 8);
	

	vertices[0].u = (col * 16.0f) / 128.0f;			vertices[0].v = (row * 8.0f) / 128.0f;
	vertices[1].u = (col * 16.0f) / 128.0f;			vertices[1].v = (row * 8.0f + 8.0f) / 128.0f;
	vertices[2].u = (col * 16.0f + 8.0f) / 128.0f;	vertices[2].v = (row * 8.0f) / 128.0f;
	vertices[3].u = (col * 16.0f + 8.0f) / 128.0f;	vertices[3].v = (row * 8.0f + 8.0f) / 128.0f;

	sceGumDrawArray(GU_TRIANGLE_STRIP,GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D,4,0,vertices);

	sceGuDisable(GU_TEXTURE_2D);

	sceGuFinish();
	sceGuSync(0,0);
}

void GraphicsPSP::drawStringChar(int listNum, uint8_t color, char c, const Point *pt) {
	innerDrawStringChar(listNum, color, c, pt);

	if (_fixUpPalette != FIXUP_PALETTE_NONE)
	{
		int newEntry = _drawLists[listNum].numEntries;
		_drawLists[listNum].entries[newEntry].type = DrawListEntryCharacter;
		_drawLists[listNum].entries[newEntry].color = color;
		_drawLists[listNum].entries[newEntry].c = c;
		_drawLists[listNum].entries[newEntry].p.x = pt->x;
		_drawLists[listNum].entries[newEntry].p.y = pt->y;
		_drawLists[listNum].numEntries++;
	}
}

void GraphicsPSP::innerClearBuffer(int listNum, uint8_t color)
{
	sceGuStart(GU_DIRECT, _display_list);
	sceGuDrawBufferList(GU_PSM_5551,vram_buffer[listNum],512);
	sceGuClearColor(getColor(color));
    sceGuClear(GU_COLOR_BUFFER_BIT);
	sceGuFinish();
	sceGuSync(0,0);
}

void GraphicsPSP::clearBuffer(int listNum, uint8_t color) {	
	debug(DBG_INFO, "clearBuffer %d %d", listNum, color);

	innerClearBuffer(listNum, color);

	_drawLists[listNum].clearColor = color;
	_drawLists[listNum].numEntries = 0;
}

void GraphicsPSP::copyBuffer(int dstListNum, int srcListNum, int vscroll) {
	debug(DBG_INFO, "copyBuffer %d -> %d (%d)", srcListNum, dstListNum, vscroll);

	sceKernelDcacheWritebackAll();
	if (vscroll == 0)
	{
		sceDmacMemcpy((void*)edram_buffer[dstListNum], (void*)edram_buffer[srcListNum], 512*272*2);
	}
	else
	{
		const int dy = (int)(272.0f * (float)vscroll / 200.0f);
		if (vscroll < 0)
		{			
			sceDmacMemcpy(edram_buffer[dstListNum], (void *)((int)(edram_buffer[srcListNum]) - dy * 512 * 2), 512*(272 + dy)*2);
		}
		else
		{
			sceDmacMemcpy((void *)((int)(edram_buffer[dstListNum]) + dy * 512 * 2), edram_buffer[srcListNum], 512*(272 - dy)*2);
		}
	}
	sceGuTexSync();

	_drawLists[dstListNum].clearColor = _drawLists[srcListNum].clearColor;
	_drawLists[dstListNum].numEntries = _drawLists[srcListNum].numEntries;
	memcpy(_drawLists[dstListNum].entries, _drawLists[srcListNum].entries, sizeof(struct DrawListEntry) * _drawLists[srcListNum].numEntries);
}

void GraphicsPSP::drawBuffer(int listNum, SystemStub *stub) {
	debug(DBG_INFO, "drawBuffer %d", listNum);

	sceKernelDcacheWritebackAll();
	sceDmacMemcpy(edram_buffer_back, edram_buffer[listNum], 512*272*2);
	sceGuTexSync();

	stub->updateScreen();

	void *temp = vram_buffer_back;
	vram_buffer_back = vram_buffer_front;
	vram_buffer_front = temp;
	temp = edram_buffer_back;
	edram_buffer_back = edram_buffer_front;
	edram_buffer_front = temp;
}

void GraphicsPSP::drawRect(int num, uint8_t color, const Point *pt, int w, int h) {
	debug(DBG_INFO, "drawRect");
}

void GraphicsPSP::drawBitmapOverlay(const uint8_t *data, int w, int h, int fmt, SystemStub *stub) {
	debug(DBG_INFO, "drawBitmapOverlay %d %d %d", w, h, fmt);
}

Graphics *GraphicsPSP_create() {
	return new GraphicsPSP();
}
