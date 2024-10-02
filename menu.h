
#ifndef __MENU_H__
#define __MENU_H__

#include <stdint.h>

extern "C"
{
    #include <pspctrl.h>
}

struct Point;
struct Graphics;

enum {
    MenuEntryTypeOptions = 0,
    MenuEntryTypeBinary,
    MenuEntryTypeNumber
};

// enum {
//     MenuEntryDataPath = 0,
//     MenuEntryLanguage,
//     MenuEntryPart,
//     MenuEntryRenderer
// };

struct MenuEntryOption
{
    const char *name;
};

struct MenuEntry
{
    const char *name;
    uint8_t type;
    uint8_t numOptions;
    MenuEntryOption options[100];
    uint8_t selectedOption;
    bool selectedBinary;
    int selectedNumber;
    int minNumber, maxNumber, minNumber2, maxNumber2;
};

struct Menu
{
    Graphics *_graphics;
    SceCtrlData _pad, _padLastState;
    bool _inputsRead;

    uint8_t _numEntries;
    MenuEntry _entries[20];
    char _dirName[64][64];

    uint8_t _selectedEntry;

    bool _exit;

    int8_t _resourceType;

    int MenuEntryDataPath, MenuEntryLanguage, MenuEntryPart, MenuEntryRenderer, MenuEntryAudio, MenuEntryMusic, MenuEntryEGAPalette, MenuEntryDifficulty, MenuEntryDemoInputs;

    Menu();
    ~Menu();

    void init(int8_t resourceType);
    void clear();
    uint16_t drawString(char *str, uint16_t locX, uint16_t locY, uint32_t color);

    void pollInputs();
    void draw();

    void update();
};

#endif