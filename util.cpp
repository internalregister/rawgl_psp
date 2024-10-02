
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <cstdarg>
#include "util.h"

#include "menu.h"

uint16_t g_debugMask;
char g_error_message[1024] = "\0";
bool g_has_error = false;

void debug(uint16_t cm, const char *msg, ...) {
	char buf[1024];
	if (cm & g_debugMask) {
		va_list va;
		va_start(va, msg);
		vsprintf(buf, msg, va);
		va_end(va);
		printf("%s\n", buf);
		fflush(stdout);
	}
}

void error(const char *msg, ...) {
	va_list va;
	va_start(va, msg);
	vsprintf(g_error_message, msg, va);
	va_end(va);

	fprintf(stderr, "ERROR: %s!\n", g_error_message);

	g_has_error = true;

	// Show the error
	Menu *menu = new Menu();
	while(!menu->_exit)
	{
		menu->update();
	}
}

void warning(const char *msg, ...) {
	char buf[1024];
	va_list va;
	va_start(va, msg);
	vsprintf(buf, msg, va);
	va_end(va);
	fprintf(stderr, "WARNING: %s!\n", buf);
}

void string_lower(char *p) {
	for (; *p; ++p) {
		if (*p >= 'A' && *p <= 'Z') {
			*p += 'a' - 'A';
		}
	}
}

void string_upper(char *p) {
	for (; *p; ++p) {
		if (*p >= 'a' && *p <= 'z') {
			*p += 'A' - 'a';
		}
	}
}

void *debug_malloc(size_t size)
{
	void *p = malloc(size);
	debug(DBG_INFO, "Allocation of %d bytes to %p (malloc)", size, p);
	return p;
}

void *debug_calloc(size_t n, size_t size)
{
	void *p = calloc(n, size);
	debug(DBG_INFO, "Allocation of %d bytes to %p (calloc)", n * size, p);
	return p;
}

void debug_free(void *p)
{
	free(p);
	debug(DBG_INFO, "Freeing %p", p);
}
