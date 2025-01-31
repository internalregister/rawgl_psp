
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef INTERN_H__
#define INTERN_H__

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <stdint.h>
#include <sys/param.h>

#define SCREEN_WIDTH	(480)
#define SCREEN_HEIGHT	(272)

#define ORIGINAL_SCREEN_WIDTH	(320)
#define ORIGINAL_SCREEN_HEIGHT	(200)

#define SCREEN_WIDTH_F	(480.0f)
#define SCREEN_HEIGHT_F	(272.0f)

#define ORIGINAL_SCREEN_WIDTH_F		(320.0f)
#define ORIGINAL_SCREEN_HEIGHT_F	(200.0f)

void *debug_malloc(size_t size);
void *debug_calloc(size_t n, size_t size);
void debug_free(void *p);

#define malloc(s) malloc(s); debug(DBG_INFO, ">>>>>>> Allocating %d bytes (malloc)", s);
#define calloc(n, s) calloc(n, s); debug(DBG_INFO, ">>>>>>> Allocating %d bytes (calloc)", n*s);
#define free(p) free(p); debug(DBG_INFO, ">>>>>>> Freeing %p", p);

#undef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a)/sizeof(a[0]))

#undef ABS
template<typename T>
inline T ABS(T v) {
	return (v < 0) ? -v : v;
}

#undef MIN
template<typename T>
inline T MIN(T v1, T v2) {
	return (v1 < v2) ? v1 : v2;
}

#undef MAX
template<typename T>
inline T MAX(T v1, T v2) {
	return (v1 > v2) ? v1 : v2;
}

template<typename T>
inline void SWAP(T &a, T &b) {
	T tmp = a; 
	a = b;
	b = tmp;
}

inline uint16_t SWAP_UINT16(uint16_t n) {
	return ((n >> 8) & 255) | ((n & 255) << 8);
}

inline uint16_t READ_BE_UINT16(const void *ptr) {
	const uint8_t *b = (const uint8_t *)ptr;
	return (b[0] << 8) | b[1];
}

inline uint16_t READ_LE_UINT16(const uint8_t *ptr) {
	return (ptr[1] << 8) | ptr[0];
}

inline uint32_t READ_BE_UINT32(const void *ptr) {
	const uint8_t *b = (const uint8_t *)ptr;
	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

inline uint32_t READ_LE_UINT32(const uint8_t *ptr) {
	return (ptr[3] << 24) | (ptr[2] << 16) | (ptr[1] << 8) | ptr[0];
}

inline void WRITE_LE_UINT32(uint8_t *ptr, uint32_t value) {
	for (int i = 0; i < 4; ++i) {
		ptr[i] = value & 255; value >>= 8;
	}
}

enum Language {
	LANG_FR,
	LANG_US,
	LANG_DE,
	LANG_ES,
	LANG_IT
};

enum {
	kPartCopyProtection = 16000,
	kPartIntro = 16001,
	kPartWater = 16002,
	kPartPrison = 16003,
	kPartCite = 16004,
	kPartArene = 16005,
	kPartLuxe = 16006,
	kPartFinal = 16007,
	kPartPassword = 16008
};

enum {
	kPaulaFreq = 7159092
};

struct Ptr {
	uint8_t *pc;
	bool byteSwap;
	
	uint8_t fetchByte() {
		return *pc++;
	}
	
	uint16_t fetchWord() {
		const uint16_t i = byteSwap ? READ_LE_UINT16(pc) : READ_BE_UINT16(pc);
		pc += 2;
		return i;
	}
};

struct Point {
	int16_t x, y;

	Point() : x(0), y(0) {}
	Point(int16_t xx, int16_t yy) : x(xx), y(yy) {}
	Point(const Point &p) : x(p.x), y(p.y) {}

	void scale(int u, int v) {
		x = (x * u) >> 16;
		y = (y * v) >> 16;
	}
};

struct QuadStrip {
	enum {
		MAX_VERTICES = 70
	};
	
	uint8_t numVertices;
	Point vertices[MAX_VERTICES];
};

struct Color {
	uint8_t r;
	uint8_t g;
	uint8_t b;

	uint16_t rgb555() const {
		return ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
	}

	uint16_t rgb5551() const {
		return 0x8000 | ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3);
	}
};

struct Frac {
	static const int BITS = 16;
	static const int MASK = (1 << BITS) - 1;
	uint32_t inc;
	uint64_t offset;

	void reset(int n, int d) {
		inc = (((int64_t)n) << BITS) / d;
		offset = 0;
	}

	uint32_t getInt() const {
		return offset >> BITS;
	}
	uint32_t getFrac() const {
		return offset & MASK;
	}
	int interpolate(int sample1, int sample2) const {
		const int fp = getFrac();
		return (sample1 * (MASK - fp) + sample2 * fp) >> BITS;
	}
};

struct Vertex
{
    float x, y, z;
};

struct Vertex_UV
{
	float u, v;
	float x,y,z;
};

enum {
    BITMAP_TYPE_FONT,
    BITMAP_TYPE_HEADS,
    BITMAP_TYPE_BACKGROUND
};

extern uint8_t *_background_bitmap_ptr;

#endif
