
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#include <getopt.h>
#include <sys/stat.h>
#include "engine.h"
#include "graphics.h"
#include "resource.h"
#include "systemstub.h"
#include "util.h"
#include "mixer.h"

#include "menu.h"

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>

static const struct {
	const char *name;
	int lang;
} LANGUAGES[] = {
	{ "fr", LANG_FR },
	{ "us", LANG_US },
	{ "de", LANG_DE },
	{ "es", LANG_ES },
	{ "it", LANG_IT },
	{ 0, -1 }
};

static const struct {
	const char *name;
	int type;
} GRAPHICS[] = {
	{ "original", GRAPHICS_ORIGINAL },
	{ "software", GRAPHICS_SOFTWARE },
	{ "psp", GRAPHICS_PSP },
	{ 0,  -1 }
};

static const struct {
	const char *name;
	int difficulty;
} DIFFICULTIES[] = {
	{ "easy", DIFFICULTY_EASY },
	{ "normal", DIFFICULTY_NORMAL },
	{ "hard", DIFFICULTY_HARD },
	{ 0,  -1 }
};

bool Graphics::_is1991 = false;
bool Graphics::_use555 = false;
bool Video::_useEGA = false;
Difficulty Script::_difficulty = DIFFICULTY_NORMAL;
bool Script::_useRemasteredAudio = true;
bool Mixer::_isMusicActive = true;

static Graphics *createGraphics(int type) {
	switch (type) {
	case GRAPHICS_ORIGINAL:
		Graphics::_is1991 = true;
		// fall-through
	case GRAPHICS_SOFTWARE:
		debug(DBG_INFO, "Using software graphics");
		return GraphicsSoft_create();
	case GRAPHICS_PSP:
		return GraphicsPSP_create();
	}
	return 0;
}

static int getGraphicsType(Resource::DataType type) {
	switch (type) {
	case Resource::DT_15TH_EDITION:
	case Resource::DT_20TH_EDITION:
	case Resource::DT_3DO:
		return GRAPHICS_PSP;
	default:
		return GRAPHICS_ORIGINAL;
	}
}

struct Scaler {
	char name[32];
	int factor;
};

static void parseScaler(char *name, Scaler *s) {
	char *sep = strchr(name, '@');
	if (sep) {
		*sep = 0;
		strncpy(s->name, name, sizeof(s->name) - 1);
		s->name[sizeof(s->name) - 1] = 0;
	}
	if (sep) {
		s->factor = atoi(sep + 1);
	}
}

int main(int argc, char *argv[]) {
	char *dataPath = 0;
	int part = 16001;
	Language lang = LANG_IT;
	int graphicsType = GRAPHICS_SOFTWARE;
	DisplayMode dm;
	dm.mode   = DisplayMode::WINDOWED;
	dm.width  = SCREEN_WIDTH;
	dm.height = SCREEN_HEIGHT;
	dm.opengl = (graphicsType != GRAPHICS_SOFTWARE);
	Scaler scaler;
	scaler.name[0] = 0;
	scaler.factor = 1;
	bool defaultGraphics = true;
	bool demo3JoyInputs = false;

	SDL_Init(SDL_INIT_AUDIO);	

	Menu *menu = new Menu();
	while(!menu->_exit)
	{
		menu->update();
	}

	dataPath = strdup(menu->_entries[menu->MenuEntryDataPath].options[menu->_entries[menu->MenuEntryDataPath].selectedOption].name);

	g_debugMask = 0; //DBG_INFO | DBG_SND | DBG_VIDEO | DBG_SND | DBG_SCRIPT | DBG_BANK | DBG_SER;
	Engine *e = new Engine(dataPath, part);

	menu->init(e->_res.getDataType());
	while(!menu->_exit)
	{
		menu->update();
	}

	lang = (Language)menu->_entries[menu->MenuEntryLanguage].selectedOption;
	graphicsType = menu->_entries[menu->MenuEntryRenderer].selectedOption == 0 ? GRAPHICS_PSP : GRAPHICS_SOFTWARE;
	part = menu->_entries[menu->MenuEntryPart].selectedNumber;
	if (menu->MenuEntryAudio > -1)
	{
		Script::_useRemasteredAudio = menu->_entries[menu->MenuEntryAudio].selectedOption == 1;
	}
	Video::_useEGA = menu->_entries[menu->MenuEntryEGAPalette].selectedBinary;
	Script::_difficulty = (Difficulty)menu->_entries[menu->MenuEntryDifficulty].selectedOption;
	Mixer::_isMusicActive = menu->_entries[menu->MenuEntryMusic].selectedBinary;
	demo3JoyInputs = menu->_entries[menu->MenuEntryDemoInputs].selectedBinary;

	delete menu;

	debug(DBG_INFO, "part = %d", part);

	e->_partNum = part;

	if (e->_res.getDataType() == Resource::DT_3DO) {
		Graphics::_use555 = true;
	}

	Graphics *graphics = createGraphics(graphicsType);
	if (e->_res.getDataType() == Resource::DT_20TH_EDITION) {
		switch (Script::_difficulty) {
		case DIFFICULTY_EASY:
			debug(DBG_INFO, "Using easy difficulty");
			break;
		case DIFFICULTY_NORMAL:
			debug(DBG_INFO, "Using normal difficulty");
			break;
		case DIFFICULTY_HARD:
			debug(DBG_INFO, "Using hard difficulty");
			break;
		}
	}
	if (e->_res.getDataType() == Resource::DT_15TH_EDITION || e->_res.getDataType() == Resource::DT_20TH_EDITION) {
		if (Script::_useRemasteredAudio) {
			debug(DBG_INFO, "Using remastered audio");
		} else {
			debug(DBG_INFO, "Using original audio");
		}
	}
	SystemStub *stub = SystemStub_PSP_create();
	stub->init(e->getGameTitle(lang), &dm);
	e->setSystemStub(stub, graphics);
	if (demo3JoyInputs && e->_res.getDataType() == Resource::DT_DOS) {
		e->_res.readDemo3Joy();
	}
	e->setup(lang, graphicsType, scaler.name, scaler.factor);
	while (!stub->_pi.quit) {
		e->run();
	}
	e->finish();
	delete e;
	stub->fini();
	delete stub;
	return 0;
}
