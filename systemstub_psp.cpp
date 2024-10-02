#include <SDL.h>
#include "graphics.h"
#include "systemstub.h"
#include "util.h"

#include <time.h>
#include <sys/time.h>
#include <pspthreadman.h>

#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspdebug.h>

// Graphics utilities
#include <pspgu.h>


struct SystemStub_PSP : SystemStub {

	static const int kJoystickIndex = 0;
	static const int kJoystickCommitValue = 16384;
	static const float kAspectRatio;

	bool _padUpdated = false;
	SceCtrlData _pad, _padLastState;

	struct timeval start;

	int _w, _h;
	float _aspectRatio[4];
	SDL_Window *_window;
	int _texW, _texH;
	int _screenshot;

	SystemStub_PSP();
	virtual ~SystemStub_PSP() {}

	bool hasButtonBeenReleased(unsigned int button);
	bool hasButtonBeenPressed(unsigned int button);

	virtual void init(const char *title, const DisplayMode *dm);
	virtual void fini();

	virtual void prepareScreen(int &w, int &h, float ar[4]);
	virtual void updateScreen();
	virtual void setScreenPixels555(const uint16_t *data, int w, int h);

	virtual void processEvents();
	virtual void sleep(uint32_t duration);
	virtual uint32_t getTimeStamp();

	void setAspectRatio(int w, int h);
};

SystemStub_PSP::SystemStub_PSP()
{	
}

void SystemStub_PSP::init(const char *title, const DisplayMode *dm)
{
	gettimeofday(&start, NULL);

	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
}

void SystemStub_PSP::fini()
{
    SDL_Quit();
}


void SystemStub_PSP::prepareScreen(int &w, int &h, float ar[4])
{
	// debug(DBG_INFO, "prepareScreen");
}

void SystemStub_PSP::updateScreen()
{
	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
}

void SystemStub_PSP::setScreenPixels555(const uint16_t *data, int w, int h)
{
    // NOT READY!!!
}

bool SystemStub_PSP::hasButtonBeenReleased(unsigned int button)
{
	return (_padLastState.Buttons & button) && !(_pad.Buttons & button);
}

bool SystemStub_PSP::hasButtonBeenPressed(unsigned int button)
{
	return !(_padLastState.Buttons & button) && (_pad.Buttons & button);
}

void SystemStub_PSP::processEvents()
{
	memcpy(&_padLastState, &_pad, sizeof(SceCtrlData));
	sceCtrlReadBufferPositive(&_pad, 1);

	if (_padUpdated)
	{
		if (hasButtonBeenReleased(PSP_CTRL_LEFT))
		{
			_pi.dirMask &= ~PlayerInput::DIR_LEFT;
		}
		if (hasButtonBeenReleased(PSP_CTRL_RIGHT))
		{
			_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
		}
		if (hasButtonBeenReleased(PSP_CTRL_UP))
		{
			_pi.dirMask &= ~PlayerInput::DIR_UP;
		}
		if (hasButtonBeenReleased(PSP_CTRL_DOWN))
		{
			_pi.dirMask &= ~PlayerInput::DIR_DOWN;
		}
		if (hasButtonBeenReleased(PSP_CTRL_CROSS))
		{
			_pi.action = false;
		}
		if (hasButtonBeenReleased(PSP_CTRL_CIRCLE) || hasButtonBeenReleased(PSP_CTRL_SQUARE))
		{
			_pi.jump = false;
		}
		if (hasButtonBeenReleased(PSP_CTRL_START))
		{
			_pi.pause = true;
		}
		if (hasButtonBeenReleased(PSP_CTRL_SELECT))
		{
			_pi.back = true;
		}
		// if (hasButtonBeenReleased(PSP_CTRL_TRIANGLE))
		// {
		// 	_pi.code = true;
		// }

		if (hasButtonBeenPressed(PSP_CTRL_LEFT))
		{
			_pi.dirMask |= PlayerInput::DIR_LEFT;
		}
		if (hasButtonBeenPressed(PSP_CTRL_RIGHT))
		{
			_pi.dirMask |= PlayerInput::DIR_RIGHT;
		}
		if (hasButtonBeenPressed(PSP_CTRL_UP))
		{
			_pi.dirMask |= PlayerInput::DIR_UP;
		}
		if (hasButtonBeenPressed(PSP_CTRL_DOWN))
		{
			_pi.dirMask |= PlayerInput::DIR_DOWN;
		}
		if (hasButtonBeenPressed(PSP_CTRL_CROSS))
		{
			_pi.action = true;
		}
		if (hasButtonBeenPressed(PSP_CTRL_CIRCLE) || hasButtonBeenPressed(PSP_CTRL_SQUARE))
		{
			_pi.jump = true;
		}
	}

	_padUpdated = true;
}

void SystemStub_PSP::sleep(uint32_t duration)
{
	const Uint32 max_delay = 0xffffffffUL / 1000;

	if(duration > max_delay)
	{
		duration = max_delay;
	}

	sceKernelDelayThreadCB(duration * 1000);
}

uint32_t SystemStub_PSP::getTimeStamp()
{
	struct timeval now;
	Uint32 ticks;

	gettimeofday(&now, NULL);
	ticks=(now.tv_sec-start.tv_sec)*1000+(now.tv_usec-start.tv_usec)/1000;
	return(ticks);

	return 0;
}


SystemStub *SystemStub_PSP_create() {
	return new SystemStub_PSP();
}
