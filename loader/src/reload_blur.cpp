// Reload blur switch, on by default (PC SETTINGS > TWEAKS, --no-reload-blur / --reload-blur).
//
// The blur is mode 1 of the post-process controller at *0x2D1990 + 0x830A0 (mode +0x34 = game +0x830D4, timer +0x14,
// strength +0x10 = blend alpha of the blur pass FUN_001772E0). The controller update FUN_0015E260 runs mode 1 through
// FUN_001595F0: fade in over +0x1C, hold +0x20, fade out +0x24, then back to mode 0.
//
// Mode 1 is started in one place: the first-person animation event handler FUN_0017B130, for animation names
// ending in "101" or "501" that do not start with "FP_RPG". It computes the hold time from the animation length
// (0x17B314, see timing.cpp), then only starts the blur when no other effect runs:
//   0x17B356  mov ecx, [eax+0x830D4]
//   0x17B35C  test ecx, ecx
//   0x17B35E  jne 0x17B3AA        ; another mode is active: leave
//   0x17B380  mov dword [eax+0x830D4], 1
// With the blur off the jne becomes a jmp, so the handler always leaves there and the controller never enters
// mode 1. The other modes (weapon events ending in "180" / "182") are untouched. Mode 1 also moves the gun's draw
// after the effects pass (FUN_001EE820, see fov.cpp); without the blur it simply stays in its usual place.
// One byte, written on every change, so the menu switches it live.
#include "reload_blur.h"

#include "common.h"

#include <cstring>

namespace {

constexpr uint32_t kSite = 0x0017B35C;
const uint8_t kExpect[4] = {0x85, 0xC9, 0x75, 0x4A}; // test ecx, ecx; jne +0x4A (0x17B3AA)
constexpr uint8_t kJne = 0x75, kJmp = 0xEB;
bool g_installed;

} // namespace

void install_reload_blur_patch()
{
    auto *code = (uint8_t *)(uintptr_t)kSite;
    if (memcmp(code, kExpect, sizeof kExpect) != 0) {
        logf("reload blur: code at 0x%08x does not match this build; the reload blur switch does nothing", kSite);
        return;
    }
    g_installed = true;
    reload_blur_set(g_cfg.reload_blur);
    logf("reload blur: switch installed at 0x%08x (reload blur %s)", kSite + 2, g_cfg.reload_blur ? "on" : "off");
}

void reload_blur_set(bool on)
{
    g_cfg.reload_blur = on;
    if (g_installed) *(volatile uint8_t *)(uintptr_t)(kSite + 2) = on ? kJne : kJmp;
}
