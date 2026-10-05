// Mouse look straight into the player's view angles, bypassing the stick path.
//
// FUN_001ec450 (player input, ebx = player controller) reads the remappable stick channels into look values
// (+0xD8 horizontal = right-stick left minus right, +0xDC vertical, an invert flag at +0x101; channels below
// 0.1 are zeroed after the controller's own dead zone) and calls FUN_001bd3f0(vertical, horizontal) with
// esi = the controller. That function shapes the input (power curve with exponent +0xC8, turn acceleration
// near full deflection via +0xC0/+0xC4/+0xD0/+0xD4, smoothing +0xCC into +0xA0/+0xA4, x0.7 while aiming) and
// integrates the view angles in degrees:
//   yaw   [esi+0x18] += speed +0xB8 * input +0xA4 * zoom factor * frame time   (wrapped to -180..180)
//   pitch [esi+0x1C] += speed +0xBC * input +0xA0 * zoom factor * frame time   (clamped to -70..70)
// The zoom factor (1 / (1 + (zoom - 1) * 0.8)) is in xmm2 at both additions.
//
// The two additions are redirected here and also add the mouse angle of the current frame, times the zoom
// factor (scopes slow the mouse down like the stick). The game then wraps and clamps as usual. Menus,
// pauses and cutscenes do not run this code; each pad poll replaces the previous frame's unused angle.
// (FUN_001e5fa0 also reads the look values, but only for the weapon's sway, not the view.)
#include "input/mouselook.h"

#include "common.h"

#include <cstring>

extern "C" {
float g_ml_pending_yaw, g_ml_pending_pitch; // degrees for the next view update (set at each pad poll)
float g_ml_xmm7;                            // scratch save
}

namespace {

bool g_installed;

// 0x1BD6A8: addss xmm3, [esi+0x18]  (xmm3 = this frame's stick yaw step; xmm2 = zoom factor)
__declspec(naked) void yaw_thunk()
{
    __asm {
        addss xmm3, dword ptr [esi + 0x18]
        movss dword ptr [g_ml_xmm7], xmm7
        movss xmm7, dword ptr [g_ml_pending_yaw]
        mulss xmm7, xmm2
        addss xmm3, xmm7
        movss xmm7, dword ptr [g_ml_xmm7]
        mov dword ptr [g_ml_pending_yaw], 0
        ret
    }
}

// 0x1BD6C7: addss xmm0, [esi+0x1C]  (xmm0 = this frame's stick pitch step; clamped right after)
__declspec(naked) void pitch_thunk()
{
    __asm {
        addss xmm0, dword ptr [esi + 0x1C]
        movss dword ptr [g_ml_xmm7], xmm7
        movss xmm7, dword ptr [g_ml_pending_pitch]
        mulss xmm7, xmm2
        addss xmm0, xmm7
        movss xmm7, dword ptr [g_ml_xmm7]
        mov dword ptr [g_ml_pending_pitch], 0
        ret
    }
}

struct Site {
    uint32_t addr;
    uint8_t expect[5];
    void (*thunk)();
};

const Site kSites[] = {
    {0x001BD6A8, {0xF3, 0x0F, 0x58, 0x5E, 0x18}, yaw_thunk},
    {0x001BD6C7, {0xF3, 0x0F, 0x58, 0x46, 0x1C}, pitch_thunk},
};

} // namespace

void install_mouselook_patches()
{
    for (const Site &s : kSites)
        if (memcmp((const void *)(uintptr_t)s.addr, s.expect, 5) != 0) {
            logf("mouselook: code at 0x%08x does not match this build; mouse look disabled", s.addr);
            return;
        }
    for (const Site &s : kSites) { // call thunk (replaces the 5-byte addss)
        auto *code = (uint8_t *)(uintptr_t)s.addr;
        code[0] = 0xE8;
        int32_t rel = (int32_t)((uintptr_t)s.thunk - (s.addr + 5));
        memcpy(code + 1, &rel, 4);
    }
    g_installed = true;
    logf("mouselook: view angles take mouse input directly (FUN_001bd3f0)");
}

bool mouselook_installed() { return g_installed; }

void mouselook_set_frame(float yaw_deg, float pitch_deg)
{
    g_ml_pending_yaw = yaw_deg;
    g_ml_pending_pitch = pitch_deg;
}
