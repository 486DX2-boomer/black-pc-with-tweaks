// Field of view in play.
//
// The renderer's camera (renderer *0x2D12B8, camera at +0x740) keeps its FOV in degrees at +0x750 (= camera +0x10).
// It is horizontal: the view setup (FUN_000D2F40, FUN_002033A0) takes tan(fov / 2) x near (+0x70) as the half
// width and divides by the aspect +0x74 (1.778 in widescreen, 1.333 in 4:3) for the half height, so the widescreen
// picture shows less height than 4:3 (43 instead of 55 degrees; 86 degrees in widescreen matches the 4:3 height).
// In play the player's weapon code FUN_001BB110 rewrites it every frame as 70 / zoom (zoom 1 at the hip, more
// while aiming: 44.1 with the pistol); the camera system resets it to 70 (FUN_00202850) and the renderer sets it
// and the blend target +0x7A0 to 70 at start (FUN_00203610). Cutscene cameras blend their own authored values.
// All three take 70 from the constant 0x2888C8, which the compiler shares with the pitch clamp (+-70 degrees,
// FUN_001BD3F0) and a few unrelated uses, so the constant stays and those three loads read the port's value
// instead. Aiming keeps its magnification: the aimed FOV is the setting / zoom.
#include "fov.h"

#include "common.h"

#include <algorithm>
#include <cstring>

namespace {

float g_fov_deg = 70.0f;

struct Site {
    uint32_t addr;
    uint8_t expect[8]; // movss xmmN, dword ptr ds:[0x2888C8]
};
const Site kSites[] = {
    {0x001BB2CA, {0xF3, 0x0F, 0x10, 0x0D, 0xC8, 0x88, 0x28, 0x00}}, // xmm1: 70 / zoom in play (FUN_001BB110)
    {0x00202F92, {0xF3, 0x0F, 0x10, 0x05, 0xC8, 0x88, 0x28, 0x00}}, // xmm0: camera back to the player's FOV (FUN_00202850)
    {0x002038E5, {0xF3, 0x0F, 0x10, 0x05, 0xC8, 0x88, 0x28, 0x00}}, // xmm0: renderer start, +0x750 and +0x7A0 (FUN_00203610)
};

} // namespace

void fov_set(float degrees) { g_fov_deg = std::clamp(degrees, 50.0f, 120.0f); }

void install_fov_patch()
{
    fov_set(g_cfg.fov);
    for (const Site &s : kSites)
        if (memcmp((const void *)(uintptr_t)s.addr, s.expect, sizeof s.expect) != 0) {
            logf("fov: code at 0x%08x does not match this build; field of view stays the game's", s.addr);
            return;
        }
    uint32_t target = (uint32_t)(uintptr_t)&g_fov_deg;
    for (const Site &s : kSites) memcpy((void *)(uintptr_t)(s.addr + 4), &target, 4); // the load's address operand
    logf("fov: field of view in play %.0f degrees (horizontal; the game's is 70)", g_fov_deg);
}
