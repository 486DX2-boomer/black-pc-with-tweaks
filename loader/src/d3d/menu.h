// The window's menu bar: port settings (video, audio, input) without touching the game's own menus.
#pragma once

#include <windows.h>

namespace menu {

enum : UINT {
    ID_EXIT = 100, ID_FULLSCREEN, ID_RELEASE_MOUSE, // handled by the window (host.cpp)
    ID_AA, ID_RES_NATIVE1, ID_RES_NATIVE15, ID_RES_NATIVE2, ID_RES_720, ID_RES_1080, ID_RES_1440, ID_RES_2160, ID_RES_DEFAULT,
    ID_WIDE, ID_NARROW, ID_FPS30, ID_FPS60, ID_GAIN_UP, ID_GAIN_DOWN, ID_GAIN_RESET, ID_GAMMA_UP, ID_GAMMA_DOWN, ID_GAMMA_RESET,
    ID_REVERB, ID_REVERB_50, ID_REVERB_100, ID_REVERB_150, ID_REVERB_200,
    ID_SENS_1, ID_SENS_15, ID_SENS_2, ID_SENS_3, ID_SENS_4, ID_SENS_UP, ID_SENS_DOWN, ID_INVERT,
    ID_CONTROLS, ID_ABOUT, ID_NO_AUTOAIM, ID_PERF_RECORD, ID_PERF_FOLDER, ID_SEAMFILL, ID_EFFECTS_NATIVE,
    ID_KEY_BINDINGS,
};

HMENU build();
void update(HMENU bar, bool fullscreen); // check marks from the current settings (WM_INITMENUPOPUP)
void command(HWND hwnd, UINT id);       // everything except the IDs the window handles itself
void perf_report(HWND hwnd, bool windowed); // a performance recording was saved: offer to open it

} // namespace menu
