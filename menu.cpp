#include "menu.h"

extern "C"
{
    #include <pspkernel.h>
    #include <pspdisplay.h>
	#include <pspdmac.h>
    #include <pspgu.h>
    #include <pspgum.h>
}

#include <unistd.h>

#include <dirent.h>

#include "graphics.h"

#include "resource.h"

#include "util.h"

static void *vram_buffer_back = (void*)0;
static void *vram_buffer_front = (void*)(512*272*2);
static void *vram_font_tex = (void*)(512*272*2*2);
static void *edram_font_tex = (void*)(sceGeEdramGetAddr() + (int)vram_font_tex);

Menu::Menu()
{
    init(-1);
}

Menu::~Menu()
{
    sceGuTerm();
}

void Menu::init(int8_t resourceType)
{
    _resourceType = resourceType;
    _exit = false;
    _inputsRead = false;

    _numEntries = 0;

    if (!g_has_error)
    {
        if (resourceType < 0)
        {
            _entries[_numEntries].name = "Data folder";
            _entries[_numEntries].type = MenuEntryTypeOptions;
            _entries[_numEntries].selectedOption = 0;    
            _entries[_numEntries].numOptions = 0;

            char mainPath[128];
            getcwd(mainPath, 128);
            SceUID dirId = sceIoDopen(mainPath);
            SceIoDirent dirEnt;
            int dirCount = 0;
            while (sceIoDread(dirId, &dirEnt))
            {
                if (FIO_S_ISDIR(dirEnt.d_stat.st_mode)&& strcmp(dirEnt.d_name, ".") && strcmp(dirEnt.d_name, "..") && dirCount < 16)
                {
                    strncpy(_dirName[dirCount], dirEnt.d_name, 64);
                    _entries[_numEntries].options[_entries[_numEntries].numOptions++].name = _dirName[dirCount];
                    dirCount++;
                }
            }
            sceIoDclose(dirId);
            _entries[_numEntries].options[_entries[_numEntries].numOptions++].name = ".";
            MenuEntryDataPath = _numEntries;
            _numEntries++;
        }
        else
        {
            _entries[_numEntries].name = "Language";
            _entries[_numEntries].type = MenuEntryTypeOptions;
            _entries[_numEntries].selectedOption = 1;
            _entries[_numEntries].numOptions = 5;
            _entries[_numEntries].options[0].name = "French";
            _entries[_numEntries].options[1].name = "English";
            _entries[_numEntries].options[2].name = "German";
            _entries[_numEntries].options[3].name = "Spanish";
            _entries[_numEntries].options[4].name = "Italian";
            MenuEntryLanguage = _numEntries;
            _numEntries++;

            _entries[_numEntries].name = "Part";
            _entries[_numEntries].type = MenuEntryTypeNumber;
            _entries[_numEntries].selectedNumber = 1;
            _entries[_numEntries].minNumber = 1;
            _entries[_numEntries].maxNumber = 30;
            _entries[_numEntries].minNumber2 = 16001;
            _entries[_numEntries].maxNumber2 = 16008;
            MenuEntryPart = _numEntries;
            _numEntries++;

            _entries[_numEntries].name = "Renderer";
            _entries[_numEntries].type = MenuEntryTypeOptions;
            _entries[_numEntries].selectedOption = 0;
            _entries[_numEntries].numOptions = 2;
            _entries[_numEntries].options[0].name = "Hardware";
            _entries[_numEntries].options[1].name = "Software";
            MenuEntryRenderer = _numEntries;
            _numEntries++;

            if (resourceType == Resource::DT_DOS)
            {
                _entries[_numEntries].name = "Use EGA palette";
                _entries[_numEntries].type = MenuEntryTypeBinary;
                _entries[_numEntries].selectedBinary = false;
                MenuEntryEGAPalette = _numEntries;
                _numEntries++;

                _entries[_numEntries].name = "Use demo inputs";
                _entries[_numEntries].type = MenuEntryTypeBinary;
                _entries[_numEntries].selectedBinary = false;
                MenuEntryDemoInputs = _numEntries;
                _numEntries++;
            }

            if (resourceType == Resource::DT_20TH_EDITION)
            {
                _entries[_numEntries].name = "Difficulty";
                _entries[_numEntries].type = MenuEntryTypeOptions;
                _entries[_numEntries].selectedOption = 1;
                _entries[_numEntries].numOptions = 3;
                _entries[_numEntries].options[0].name = "Easy";
                _entries[_numEntries].options[1].name = "Medium";
                _entries[_numEntries].options[2].name = "Hard";
                MenuEntryDifficulty = _numEntries;
                _numEntries++;
            }

            MenuEntryAudio = -1;
            if (resourceType == Resource::DT_20TH_EDITION || resourceType == Resource::DT_15TH_EDITION)
            {            
                _entries[_numEntries].name = "Audio";
                _entries[_numEntries].type = MenuEntryTypeOptions;
                _entries[_numEntries].selectedOption = 0;
                _entries[_numEntries].numOptions = 2;
                _entries[_numEntries].options[0].name = "Original";
                _entries[_numEntries].options[1].name = "Remastered";
                MenuEntryAudio = _numEntries;
                _numEntries++;

                _entries[_numEntries].name = "Music";
                _entries[_numEntries].type = MenuEntryTypeBinary;
                _entries[_numEntries].selectedBinary = true;
                MenuEntryMusic = _numEntries;
                _numEntries++;
            }
        }
    }

    sceGuInit();

	sceGuStart(GU_DIRECT,Graphics::_display_list);

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
	sceGumOrtho(0, SCREEN_WIDTH_F, SCREEN_HEIGHT_F, 0, -1, 1);

	sceGumMatrixMode(GU_VIEW);
	sceGumLoadIdentity();

	sceGumMatrixMode(GU_MODEL);
	sceGumLoadIdentity();

    sceGuCopyImage(GU_PSM_T8, 0, 0, 128, 128, 128, (void*)Graphics::_font_transformed, 0, 0, 128, edram_font_tex);
	sceGuTexSync();

    sceGuFinish();
	sceGuSync(0,0);
 
	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);
}

void Menu::clear()
{
    sceGuStart(GU_DIRECT,Graphics::_display_list);
    sceGuClearColor(0xFF777777);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuFinish();
	sceGuSync(0,0);
}

uint16_t Menu::drawString(char *str, uint16_t locX, uint16_t locY, uint32_t color)
{
    sceGuStart(GU_DIRECT, Graphics::_display_list);

	sceGuEnable(GU_TEXTURE_2D);
	sceGuClutMode(GU_PSM_8888,0,0xff,0);
	sceGuClutLoad((256/8),Graphics::_clut_font);
	sceGuTexMode(GU_PSM_T8,0,0,0);
	sceGuTexImage(0,128,128,128,edram_font_tex);
	sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
	sceGuTexFilter(GU_NEAREST,GU_NEAREST);
	sceGuTexScale(1.0f,1.0f);
	sceGuTexOffset(0.0f,0.0f);

	sceGuColor(color);
    
    char c = *str++;
    uint16_t posX = locX;
    while (c != '\0')
    {
        struct Vertex_UV *vertices = (struct Vertex_UV*)sceGuGetMemory(4*sizeof(struct Vertex_UV));

        vertices[0].x = posX;        vertices[0].y = locY;        vertices[0].z = 0.0f; 
        vertices[1].x = posX;        vertices[1].y = locY + 8.0f; vertices[1].z = 0.0f;
        vertices[2].x = posX + 8.0f; vertices[2].y = locY;        vertices[2].z = 0.0f;
        vertices[3].x = posX + 8.0f; vertices[3].y = locY + 8.0f; vertices[3].z = 0.0f;

        uint8_t col = ((c - 0x20) % 8);
        uint8_t row = ((c - 0x20) / 8);

        vertices[0].u = (col * 16.0f) / 128.0f;			vertices[0].v = (row * 8.0f) / 128.0f;
        vertices[1].u = (col * 16.0f) / 128.0f;			vertices[1].v = (row * 8.0f + 8.0f) / 128.0f;
        vertices[2].u = (col * 16.0f + 8.0f) / 128.0f;	vertices[2].v = (row * 8.0f) / 128.0f;
        vertices[3].u = (col * 16.0f + 8.0f) / 128.0f;	vertices[3].v = (row * 8.0f + 8.0f) / 128.0f;

        sceGumDrawArray(GU_TRIANGLE_STRIP,GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D,4,0,vertices);

        c = *str++;
        posX += 8;
    }

	sceGuDisable(GU_TEXTURE_2D);

	sceGuFinish();
	sceGuSync(0,0);

    return posX;
}

void Menu::draw()
{
    char temp_str[16];
    clear();

    if (g_has_error)
    {
        drawString((char*)"Error found", 196, 20, 0xFF8888FF);
        drawString(g_error_message, 10, 80, 0xFF6666FF);
    }
    else
    {
        switch(_resourceType)
        {
            case Resource::DT_15TH_EDITION:
                drawString((char*)"15th Anniversary Edition version detected", 76, 20, 0xFFEAD999);
                break;
            case Resource::DT_20TH_EDITION:
                drawString((char*)"20th Anniversary Edition version detected", 76, 20, 0xFFEAD999);
                break;
            case Resource::DT_3DO:
                drawString((char*)"3DO version detected", 160, 20, 0xFFEAD999);
                break;
            case Resource::DT_AMIGA:
                drawString((char*)"Amiga version detected", 152, 20, 0xFFEAD999);
                break;
            case Resource::DT_ATARI:
                drawString((char*)"Atari version detected", 152, 20, 0xFFEAD999);
                break;
            case Resource::DT_ATARI_DEMO:
                drawString((char*)"Atari demo version detected", 132, 20, 0xFFEAD999);
                break;
            case Resource::DT_DOS:
                drawString((char*)"DOS version detected", 160, 20, 0xFFEAD999);
                break;
            case Resource::DT_WIN31:
                drawString((char*)"Windows 3.1 version detected", 128, 20, 0xFFEAD999);
                break;
            default:
                drawString((char*)"Choose the folder with data files", 108, 20, 0xFFEAD999);
                break;
        }

        uint16_t y = 80, x;
        for(uint8_t i = 0; i < _numEntries; i++)
        {
            drawString((char*)_entries[i].name, 10, y, _selectedEntry == i ? 0xFFFFFFFF : 0xFF999999);

            x = 150;

            switch (_entries[i].type)
            {
                case MenuEntryTypeOptions:
                {                
                    x = drawString((char*)(_selectedEntry == i ? "< " : "  "), x, y, 0xFFFFFFFF);
                    x = drawString((char*)_entries[i].options[_entries[i].selectedOption].name, x, y, _selectedEntry == i ? 0xFFFFFFFF : 0xFF999999);
                    x = drawString((char*)(_selectedEntry == i ? " >" : "  "), x, y, 0xFFFFFFFF);
                    break;
                }
                case MenuEntryTypeBinary:
                {
                    x = drawString((char*)(_selectedEntry == i ? "< " : "  "), x, y, 0xFFFFFFFF);
                    x = drawString((char*)(_entries[i].selectedBinary ? "Yes" : "No"), x, y, _selectedEntry == i ? 0xFFFFFFFF : 0xFF999999);
                    x = drawString((char*)(_selectedEntry == i ? " >" : "  "), x, y, 0xFFFFFFFF);
                    break;
                }
                case MenuEntryTypeNumber:
                {
                    x = drawString((char*)(_selectedEntry == i ? "< " : "  "), x, y, 0xFFFFFFFF);
                    sprintf(temp_str, "%d", _entries[i].selectedNumber);
                    x = drawString(temp_str, x, y, _selectedEntry == i ? 0xFFFFFFFF : 0xFF999999);
                    x = drawString((char*)(_selectedEntry == i ? " >" : "  "), x, y, 0xFFFFFFFF);
                    break;
                }
            }

            y += 16;
        }

        if (_resourceType < 0)
        {
            drawString((char*)"Press Start or X to proceed", 132, 220, 0xFF1DE6B5);
        }
        else
        {
            drawString((char*)"Press Start or X to launch the game", 100, 220, 0xFF1DE6B5);
        }
    }

    drawString((char*)"raw(gl) PSP", 364, 252, 0xFF8A8A8A);
 
	sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

void Menu::pollInputs()
{
    memcpy(&_padLastState, &_pad, sizeof(SceCtrlData));
    sceCtrlReadBufferPositive(&_pad, 1);

    if (_inputsRead && !g_has_error)
    {
        if ((_pad.Buttons & PSP_CTRL_DOWN) && !(_padLastState.Buttons & PSP_CTRL_DOWN))
        {
            _selectedEntry++;
            if (_selectedEntry == _numEntries) _selectedEntry = 0;
        }

        if ((_pad.Buttons & PSP_CTRL_UP) && !(_padLastState.Buttons & PSP_CTRL_UP))
        {
            _selectedEntry--;
            if (_selectedEntry == 0xFF) _selectedEntry = _numEntries - 1;
        }

        if ((_pad.Buttons & PSP_CTRL_LEFT) && !(_padLastState.Buttons & PSP_CTRL_LEFT))
        {
            switch (_entries[_selectedEntry].type)
            {
                case MenuEntryTypeOptions:
                {
                    _entries[_selectedEntry].selectedOption--;
                    if (_entries[_selectedEntry].selectedOption == 0xFF) _entries[_selectedEntry].selectedOption = _entries[_selectedEntry].numOptions - 1;
                    break;
                }
                case MenuEntryTypeBinary:
                {
                    _entries[_selectedEntry].selectedBinary = !_entries[_selectedEntry].selectedBinary;
                    break;
                }
                case MenuEntryTypeNumber:
                {
                    _entries[_selectedEntry].selectedNumber--;                    
                    if (_entries[_selectedEntry].selectedNumber < _entries[_selectedEntry].minNumber)
                    {
                        if (_entries[_selectedEntry].maxNumber2 > -1)
                        {
                            _entries[_selectedEntry].selectedNumber = _entries[_selectedEntry].maxNumber2;
                        }
                        else
                        {
                            _entries[_selectedEntry].selectedNumber = _entries[_selectedEntry].maxNumber;
                        }
                    }
                    else if (_entries[_selectedEntry].selectedNumber < _entries[_selectedEntry].minNumber2 && _entries[_selectedEntry].selectedNumber > _entries[_selectedEntry].maxNumber)
                    {
                        _entries[_selectedEntry].selectedNumber = _entries[_selectedEntry].maxNumber;
                    }
                    break;
                }
            }
        }

        if ((_pad.Buttons & PSP_CTRL_RIGHT) && !(_padLastState.Buttons & PSP_CTRL_RIGHT))
        {
            switch (_entries[_selectedEntry].type)
            {
                case MenuEntryTypeOptions:
                {
                    _entries[_selectedEntry].selectedOption++;
                    if (_entries[_selectedEntry].selectedOption == _entries[_selectedEntry].numOptions) _entries[_selectedEntry].selectedOption = 0;
                    break;
                }
                case MenuEntryTypeBinary:
                {
                    _entries[_selectedEntry].selectedBinary = !_entries[_selectedEntry].selectedBinary;
                    break;
                }
                case MenuEntryTypeNumber:
                {
                    _entries[_selectedEntry].selectedNumber++;
                    if (_entries[_selectedEntry].selectedNumber > _entries[_selectedEntry].maxNumber && _entries[_selectedEntry].selectedNumber < _entries[_selectedEntry].minNumber2)
                    {
                        if (_entries[_selectedEntry].minNumber2 >= 0)
                        {
                            _entries[_selectedEntry].selectedNumber = _entries[_selectedEntry].minNumber2;
                        }
                        else
                        {
                            _entries[_selectedEntry].selectedNumber = _entries[_selectedEntry].minNumber;
                        }
                    }
                    else if (_entries[_selectedEntry].selectedNumber > _entries[_selectedEntry].maxNumber2 && _entries[_selectedEntry].maxNumber2 > -1)
                    {
                        _entries[_selectedEntry].selectedNumber = _entries[_selectedEntry].minNumber;
                    }
                    break;
                }
            }
        }

        if (((_pad.Buttons & PSP_CTRL_START) && !(_padLastState.Buttons & PSP_CTRL_START)) ||
            ((_pad.Buttons & PSP_CTRL_CROSS) && !(_padLastState.Buttons & PSP_CTRL_CROSS)))
        {
            _exit = true;
        }
    }

    _inputsRead = true;
}

void Menu::update()
{
    pollInputs();
    draw();
}

