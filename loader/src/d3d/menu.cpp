// Menu bar of the game window. Settings change live where the port can (render size, anti-aliasing,
// brightness, reverb, mouse, auto-aim); aspect ratio and frame rate are read by the game at boot, so those apply after
// a restart. Every change is saved to black.ini (settings.cpp).
#include "d3d/menu.h"

#include "common.h"
#include "d3d/device.h"
#include "input/autoaim.h"
#include "perf.h"
#include "settings.h"

#include <shellapi.h>

#include <algorithm>
#include <string>

namespace menu {
namespace {

HMENU popup(HMENU bar, const wchar_t *title)
{
    HMENU m = CreatePopupMenu();
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)m, title);
    return m;
}

void item(HMENU m, UINT id, const wchar_t *text) { AppendMenuW(m, MF_STRING, id, text); }
void sep(HMENU m) { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); }
void check(HMENU bar, UINT id, bool on) { CheckMenuItem(bar, id, MF_BYCOMMAND | (on ? MF_CHECKED : MF_UNCHECKED)); }
void radio(HMENU bar, UINT first, UINT last, UINT on) { CheckMenuRadioItem(bar, first, last, on, MF_BYCOMMAND); }

bool close_to(float a, float b) { return a > b - 0.01f && a < b + 0.01f; }

void set_render(bool native, float factor, int height)
{
    g_cfg.render_native = native;
    g_cfg.render_factor = factor;
    g_cfg.render_w = 0;
    g_cfg.render_h = height;
    xd3d::request_render_change();
}

} // namespace

HMENU build()
{
    HMENU bar = CreateMenu();
    HMENU game = popup(bar, L"&Game");
    item(game, ID_NO_AUTOAIM, L"Disable &auto-aim");
    sep(game);
    item(game, ID_EXIT, L"E&xit\tAlt+F4");

    HMENU video = popup(bar, L"&Video");
    item(video, ID_FULLSCREEN, L"&Fullscreen\tF11");
    item(video, ID_AA, L"&Anti-aliasing\tF9");
    HMENU res = CreatePopupMenu();
    AppendMenuW(video, MF_POPUP, (UINT_PTR)res, L"&Render resolution");
    item(res, ID_RES_NATIVE1, L"Window size\tF10");
    item(res, ID_RES_NATIVE15, L"1.5 x window size");
    item(res, ID_RES_NATIVE2, L"2 x window size");
    sep(res);
    item(res, ID_RES_720, L"720p");
    item(res, ID_RES_1080, L"1080p");
    item(res, ID_RES_1440, L"1440p");
    item(res, ID_RES_2160, L"2160p (4K)");
    sep(res);
    item(res, ID_RES_DEFAULT, L"Default (3 x Xbox, --scale)");
    HMENU aspect = CreatePopupMenu();
    AppendMenuW(video, MF_POPUP, (UINT_PTR)aspect, L"Aspect &ratio (after restart)");
    item(aspect, ID_WIDE, L"Widescreen 16:9");
    item(aspect, ID_NARROW, L"Standard 4:3");
    HMENU fps = CreatePopupMenu();
    AppendMenuW(video, MF_POPUP, (UINT_PTR)fps, L"Frame &rate (after restart)");
    item(fps, ID_FPS30, L"30 fps (original)");
    item(fps, ID_FPS60, L"60 fps");
    HMENU bright = CreatePopupMenu();
    AppendMenuW(video, MF_POPUP, (UINT_PTR)bright, L"&Brightness");
    item(bright, ID_GAIN_UP, L"Gain up\tF6");
    item(bright, ID_GAIN_DOWN, L"Gain down\tF5");
    item(bright, ID_GAIN_RESET, L"Gain reset (1.0)");
    sep(bright);
    item(bright, ID_GAMMA_UP, L"Gamma up\tF8");
    item(bright, ID_GAMMA_DOWN, L"Gamma down\tF7");
    item(bright, ID_GAMMA_RESET, L"Gamma reset (1.0)");

    HMENU audio = popup(bar, L"&Audio");
    item(audio, ID_REVERB, L"Room &reverb");
    HMENU level = CreatePopupMenu();
    AppendMenuW(audio, MF_POPUP, (UINT_PTR)level, L"Reverb &level");
    item(level, ID_REVERB_50, L"50%");
    item(level, ID_REVERB_100, L"100%");
    item(level, ID_REVERB_150, L"150%");
    item(level, ID_REVERB_200, L"200%");

    HMENU input = popup(bar, L"&Input");
    HMENU sens = CreatePopupMenu();
    AppendMenuW(input, MF_POPUP, (UINT_PTR)sens, L"Mouse &sensitivity");
    item(sens, ID_SENS_1, L"1.0");
    item(sens, ID_SENS_15, L"1.5");
    item(sens, ID_SENS_2, L"2.0 (default)");
    item(sens, ID_SENS_3, L"3.0");
    item(sens, ID_SENS_4, L"4.0");
    sep(sens);
    item(sens, ID_SENS_UP, L"Higher\tF4");
    item(sens, ID_SENS_DOWN, L"Lower\tF3");
    item(input, ID_INVERT, L"&Invert mouse");
    item(input, ID_RELEASE_MOUSE, L"Release &mouse\tEsc");

    HMENU tools = popup(bar, L"&Tools");
    item(tools, ID_PERF_RECORD, L"&Record performance\tF2");
    item(tools, ID_PERF_FOLDER, L"&Open recordings folder");

    HMENU help = popup(bar, L"&Help");
    item(help, ID_CONTROLS, L"&Controls...");
    item(help, ID_ABOUT, L"&About...");
    return bar;
}

void update(HMENU bar, bool fullscreen)
{
    check(bar, ID_FULLSCREEN, fullscreen);
    check(bar, ID_AA, g_cfg.aa);
    UINT res = ID_RES_DEFAULT;
    if (g_cfg.render_native) res = g_cfg.render_factor > 1.75f ? ID_RES_NATIVE2 : g_cfg.render_factor > 1.25f ? ID_RES_NATIVE15 : ID_RES_NATIVE1;
    else if (g_cfg.render_h == 720 && !g_cfg.render_w) res = ID_RES_720;
    else if (g_cfg.render_h == 1080 && !g_cfg.render_w) res = ID_RES_1080;
    else if (g_cfg.render_h == 1440 && !g_cfg.render_w) res = ID_RES_1440;
    else if (g_cfg.render_h == 2160 && !g_cfg.render_w) res = ID_RES_2160;
    else if (g_cfg.render_h) res = 0; // a size from the command line that has no menu entry
    if (res) radio(bar, ID_RES_NATIVE1, ID_RES_DEFAULT, res);
    else
        for (UINT id = ID_RES_NATIVE1; id <= ID_RES_DEFAULT; id++) check(bar, id, false);
    radio(bar, ID_WIDE, ID_NARROW, g_cfg.widescreen ? ID_WIDE : ID_NARROW);
    radio(bar, ID_FPS30, ID_FPS60, g_cfg.fps == 60 ? ID_FPS60 : ID_FPS30);
    check(bar, ID_REVERB, g_cfg.reverb);
    UINT lv = close_to(g_cfg.reverb_gain, 0.5f) ? ID_REVERB_50 : close_to(g_cfg.reverb_gain, 1.0f) ? ID_REVERB_100
              : close_to(g_cfg.reverb_gain, 1.5f) ? ID_REVERB_150 : close_to(g_cfg.reverb_gain, 2.0f) ? ID_REVERB_200 : 0;
    for (UINT id = ID_REVERB_50; id <= ID_REVERB_200; id++) check(bar, id, id == lv);
    UINT s = close_to(g_cfg.mouse_sens, 1.0f) ? ID_SENS_1 : close_to(g_cfg.mouse_sens, 1.5f) ? ID_SENS_15 : close_to(g_cfg.mouse_sens, 2.0f) ? ID_SENS_2
             : close_to(g_cfg.mouse_sens, 3.0f) ? ID_SENS_3 : close_to(g_cfg.mouse_sens, 4.0f) ? ID_SENS_4 : 0;
    for (UINT id = ID_SENS_1; id <= ID_SENS_4; id++) check(bar, id, id == s);
    check(bar, ID_INVERT, g_cfg.mouse_invert);
    check(bar, ID_NO_AUTOAIM, !g_cfg.autoaim);
    check(bar, ID_PERF_RECORD, perf::recording());
}

void command(HWND hwnd, UINT id)
{
    switch (id) {
    case ID_AA: g_cfg.aa = !g_cfg.aa; break;
    case ID_RES_NATIVE1: set_render(true, 1.0f, 0); break;
    case ID_RES_NATIVE15: set_render(true, 1.5f, 0); break;
    case ID_RES_NATIVE2: set_render(true, 2.0f, 0); break;
    case ID_RES_720: set_render(false, 1.0f, 720); break;
    case ID_RES_1080: set_render(false, 1.0f, 1080); break;
    case ID_RES_1440: set_render(false, 1.0f, 1440); break;
    case ID_RES_2160: set_render(false, 1.0f, 2160); break;
    case ID_RES_DEFAULT: set_render(false, 1.0f, 0); break;
    case ID_WIDE: g_cfg.widescreen = true; break;
    case ID_NARROW: g_cfg.widescreen = false; break;
    case ID_FPS30: g_cfg.fps = 30; break;
    case ID_FPS60: g_cfg.fps = 60; break;
    case ID_GAIN_UP: g_cfg.gain = std::min(3.0f, g_cfg.gain + 0.1f); break;
    case ID_GAIN_DOWN: g_cfg.gain = std::max(0.5f, g_cfg.gain - 0.1f); break;
    case ID_GAIN_RESET: g_cfg.gain = 1.0f; break;
    case ID_GAMMA_UP: g_cfg.gamma = std::min(3.0f, g_cfg.gamma + 0.1f); break;
    case ID_GAMMA_DOWN: g_cfg.gamma = std::max(0.5f, g_cfg.gamma - 0.1f); break;
    case ID_GAMMA_RESET: g_cfg.gamma = 1.0f; break;
    case ID_REVERB: g_cfg.reverb = !g_cfg.reverb; break;
    case ID_REVERB_50: g_cfg.reverb_gain = 0.5f; break;
    case ID_REVERB_100: g_cfg.reverb_gain = 1.0f; break;
    case ID_REVERB_150: g_cfg.reverb_gain = 1.5f; break;
    case ID_REVERB_200: g_cfg.reverb_gain = 2.0f; break;
    case ID_SENS_1: g_cfg.mouse_sens = 1.0f; break;
    case ID_SENS_15: g_cfg.mouse_sens = 1.5f; break;
    case ID_SENS_2: g_cfg.mouse_sens = 2.0f; break;
    case ID_SENS_3: g_cfg.mouse_sens = 3.0f; break;
    case ID_SENS_4: g_cfg.mouse_sens = 4.0f; break;
    case ID_SENS_UP: g_cfg.mouse_sens = std::min(50.0f, g_cfg.mouse_sens * 1.1f); break;
    case ID_SENS_DOWN: g_cfg.mouse_sens = std::max(0.1f, g_cfg.mouse_sens / 1.1f); break;
    case ID_INVERT: g_cfg.mouse_invert = !g_cfg.mouse_invert; break;
    case ID_NO_AUTOAIM: autoaim_set(!g_cfg.autoaim); break;
    case ID_PERF_RECORD: perf::toggle(); return;
    case ID_PERF_FOLDER: {
        CreateDirectoryW(L"perf", nullptr);
        wchar_t full[MAX_PATH];
        GetFullPathNameW(L"perf", MAX_PATH, full, nullptr);
        ShellExecuteW(hwnd, L"open", full, nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
    case ID_CONTROLS:
        MessageBoxW(hwnd,
                    L"Mouse: look (click the picture to capture it, Esc or Alt+Tab releases it)\n"
                    L"Left button: right trigger      Right button: right stick click\n"
                    L"Wheel up / down: D-pad left / right\n\n"
                    L"WASD: move (left stick)      Arrows: D-pad\n"
                    L"R: A      F: B      E: X      Q: Y\n"
                    L"G: left trigger      C: left stick click\n"
                    L"Z / X: D-pad up / down\n"
                    L"Enter: START      Space: BACK (select)\n\n"
                    L"F3 / F4: mouse sensitivity      F5 / F6: gain      F7 / F8: gamma\n"
                    L"F9: anti-aliasing      F10: render size      F11: fullscreen\n"
                    L"F12: dump the next frame (bug reports)\n\n"
                    L"The game's own controls screen reassigns actions to pad buttons.",
                    L"BLACK - controls", MB_OK | MB_ICONINFORMATION);
        return;
    case ID_ABOUT:
        MessageBoxW(hwnd,
                    L"BLACK PC port\n\nRuns your own copy of the original Xbox game (default.xbe) natively, with the Xbox "
                    L"kernel, Direct3D, DirectSound and input replaced by PC code.\n\nSettings are saved to black.ini in the game folder.",
                    L"About", MB_OK | MB_ICONINFORMATION);
        return;
    default: return;
    }
    settings_save();
}

void perf_report(HWND hwnd, bool windowed)
{
    if (!windowed || g_cfg.background) return; // fullscreen: the title says it was saved; tests: no dialog
    std::wstring text = widen(perf::last_report_text());
    text += L"\n\nSaved in ";
    text += perf::last_report_dir();
    text += L"\n\nOpen the folder?";
    if (MessageBoxW(hwnd, text.c_str(), L"Performance recording", MB_YESNO | MB_ICONINFORMATION) == IDYES)
        ShellExecuteW(hwnd, L"open", perf::last_report_dir(), nullptr, nullptr, SW_SHOWNORMAL);
}

} // namespace menu
