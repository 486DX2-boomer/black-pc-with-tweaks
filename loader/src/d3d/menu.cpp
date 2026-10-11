// Menu bar of the game window. Settings change live where the port can (render size, anti-aliasing,
// brightness, reverb, mouse, auto-aim); aspect ratio and frame rate are read by the game at boot, so a new choice is
// only saved for the next run (a notice says so) and the running session keeps its values. Every change is saved to
// black.ini (settings.cpp).
#include "d3d/menu.h"

#include "common.h"
#include "d3d/device.h"
#include "fov.h"
#include "input/autoaim.h"
#include "input/bindings.h"
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

// Field of view presets (horizontal degrees; fov.cpp). 86 in widescreen shows the height of the 4:3 picture.
const float kFovs[] = {70, 75, 80, 86, 90, 95, 100, 105, 110};
static_assert(sizeof kFovs / sizeof kFovs[0] == ID_FOV_LAST - ID_FOV_FIRST + 1, "one menu ID per preset");

void set_render(bool native, float factor, int height)
{
    g_cfg.render_native = native;
    g_cfg.render_factor = factor;
    g_cfg.render_w = 0;
    g_cfg.render_h = height;
    xd3d::request_render_change();
}

int shown_fps() { return g_cfg.fps_next ? g_cfg.fps_next : g_cfg.fps; }
bool shown_widescreen() { return g_cfg.widescreen_next >= 0 ? g_cfg.widescreen_next != 0 : g_cfg.widescreen; }

void next_run_notice(HWND hwnd, const wchar_t *chosen, const wchar_t *running)
{
    std::wstring text = std::wstring(chosen) + L" will be used the next time you start BLACK.\n\nUntil then the game keeps "
                        L"running with " + running + L".";
    MessageBoxW(hwnd, text.c_str(), L"Applies after restart", MB_OK | MB_ICONINFORMATION);
}

// Saved for the next run; the notice comes after the save so the choice is kept whatever happens while it is open.
void choose_fps(HWND hwnd, int fps)
{
    if (fps == shown_fps()) return;
    g_cfg.fps_next = fps == g_cfg.fps ? 0 : fps;
    settings_save();
    if (g_cfg.fps_next) next_run_notice(hwnd, fps == 60 ? L"60 fps" : L"30 fps", g_cfg.fps == 60 ? L"60 fps" : L"30 fps");
}

void choose_widescreen(HWND hwnd, bool wide)
{
    if (wide == shown_widescreen()) return;
    g_cfg.widescreen_next = wide == g_cfg.widescreen ? -1 : wide;
    settings_save();
    if (g_cfg.widescreen_next >= 0)
        next_run_notice(hwnd, wide ? L"Widescreen 16:9" : L"Standard 4:3", g_cfg.widescreen ? L"widescreen 16:9" : L"standard 4:3");
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
    item(video, ID_SEAMFILL, L"Fill geometry &seams");
    item(video, ID_EFFECTS_NATIVE, L"&Effects at original resolution");
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
    HMENU fov = CreatePopupMenu();
    AppendMenuW(video, MF_POPUP, (UINT_PTR)fov, L"Field of &view");
    for (UINT i = 0; i <= ID_FOV_LAST - ID_FOV_FIRST; i++) {
        wchar_t text[64];
        swprintf(text, 64, L"%.0f°%s", kFovs[i], kFovs[i] == 70 ? L" (original)" : kFovs[i] == 86 ? L" (4:3 height in widescreen)" : L"");
        item(fov, ID_FOV_FIRST + i, text);
    }
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
    item(input, ID_KEY_BINDINGS, L"&Key bindings...");
    sep(input);
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
    item(input, ID_RELEASE_MOUSE, L"Release &mouse");

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
    check(bar, ID_SEAMFILL, g_cfg.seam_fill);
    check(bar, ID_EFFECTS_NATIVE, g_cfg.effects_native);
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
    radio(bar, ID_WIDE, ID_NARROW, shown_widescreen() ? ID_WIDE : ID_NARROW); // the choice for the next run, if any
    radio(bar, ID_FPS30, ID_FPS60, shown_fps() == 60 ? ID_FPS60 : ID_FPS30);
    check(bar, ID_REVERB, g_cfg.reverb);
    UINT lv = close_to(g_cfg.reverb_gain, 0.5f) ? ID_REVERB_50 : close_to(g_cfg.reverb_gain, 1.0f) ? ID_REVERB_100
              : close_to(g_cfg.reverb_gain, 1.5f) ? ID_REVERB_150 : close_to(g_cfg.reverb_gain, 2.0f) ? ID_REVERB_200 : 0;
    for (UINT id = ID_REVERB_50; id <= ID_REVERB_200; id++) check(bar, id, id == lv);
    UINT s = close_to(g_cfg.mouse_sens, 1.0f) ? ID_SENS_1 : close_to(g_cfg.mouse_sens, 1.5f) ? ID_SENS_15 : close_to(g_cfg.mouse_sens, 2.0f) ? ID_SENS_2
             : close_to(g_cfg.mouse_sens, 3.0f) ? ID_SENS_3 : close_to(g_cfg.mouse_sens, 4.0f) ? ID_SENS_4 : 0;
    for (UINT id = ID_SENS_1; id <= ID_SENS_4; id++) check(bar, id, id == s);
    check(bar, ID_INVERT, g_cfg.mouse_invert);
    check(bar, ID_NO_AUTOAIM, !g_cfg.autoaim);
    for (UINT i = 0; i <= ID_FOV_LAST - ID_FOV_FIRST; i++) check(bar, ID_FOV_FIRST + i, close_to(g_cfg.fov, kFovs[i]));
    check(bar, ID_PERF_RECORD, perf::recording());
}

void command(HWND hwnd, UINT id)
{
    if (id >= ID_FOV_FIRST && id <= ID_FOV_LAST) { // live: the game reads it every frame in play
        g_cfg.fov = kFovs[id - ID_FOV_FIRST];
        fov_set(g_cfg.fov);
        settings_save();
        return;
    }
    switch (id) {
    case ID_AA: g_cfg.aa = !g_cfg.aa; break;
    case ID_SEAMFILL: g_cfg.seam_fill = !g_cfg.seam_fill; break;
    case ID_EFFECTS_NATIVE:
        g_cfg.effects_native = !g_cfg.effects_native;
        xd3d::request_render_rebuild();
        break;
    case ID_RES_NATIVE1: set_render(true, 1.0f, 0); break;
    case ID_RES_NATIVE15: set_render(true, 1.5f, 0); break;
    case ID_RES_NATIVE2: set_render(true, 2.0f, 0); break;
    case ID_RES_720: set_render(false, 1.0f, 720); break;
    case ID_RES_1080: set_render(false, 1.0f, 1080); break;
    case ID_RES_1440: set_render(false, 1.0f, 1440); break;
    case ID_RES_2160: set_render(false, 1.0f, 2160); break;
    case ID_RES_DEFAULT: set_render(false, 1.0f, 0); break;
    case ID_WIDE: choose_widescreen(hwnd, true); return;
    case ID_NARROW: choose_widescreen(hwnd, false); return;
    case ID_FPS30: choose_fps(hwnd, 30); return;
    case ID_FPS60: choose_fps(hwnd, 60); return;
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
    case ID_KEY_BINDINGS: bindings::dialog(hwnd); return; // saves on OK
    case ID_CONTROLS: {
        std::wstring t = L"Mouse: look (click the picture to capture it; pausing lets go of it)\n\n";
        int n;
        const bindings::Action *acts = bindings::actions(&n);
        for (int i = 0; i < n; i++) {
            std::wstring k1 = bindings::key_name(bindings::key(i, 0)), k2 = bindings::key_name(bindings::key(i, 1));
            if (k1.empty() && k2.empty()) continue;
            t += std::wstring(acts[i].name) + L": " + k1 + (k1.empty() || k2.empty() ? L"" : L" / ") + k2 + L"\n";
        }
        t += L"\nMenus: arrow keys, Enter = select, Backspace = back; Esc frees the mouse. A controller works too.\n"
             L"Input > Key bindings changes the keys.\n\n"
             L"F3 / F4: mouse sensitivity      F5 / F6: gain      F7 / F8: gamma\n"
             L"F9: anti-aliasing      F10: render size      F11: fullscreen\n"
             L"F2: performance recording      F12: dump the next frame (bug reports)";
        MessageBoxW(hwnd, t.c_str(), L"BLACK - controls", MB_OK | MB_ICONINFORMATION);
        return;
    }
    case ID_ABOUT:
        MessageBoxW(hwnd,
                    L"BLACK PC port " _CRT_WIDE(BLACK_VERSION) L"\n\nRuns your own copy of the original Xbox game (default.xbe) natively, with the Xbox "
                    L"kernel, Direct3D, DirectSound and input replaced by PC code.\n\nSettings are saved to black.ini in the game folder."
                    L"\n\nFree software under the GNU GPL, version 2 or later. Not affiliated with Criterion Games or Electronic Arts.",
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
