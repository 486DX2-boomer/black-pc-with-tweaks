// The Xbox time-stamp counter. XAPI's QueryPerformanceCounter is a bare `rdtsc` and its
// QueryPerformanceFrequency a constant 733,333,333 Hz (the Xbox CPU clock); the game's own timer helpers
// make the same assumption. On a PC `rdtsc` ticks at the host clock (3-5 GHz), so everything the game
// times in seconds (fades, blur ramps) ran several times too fast. These patches return a 733.33 MHz
// counter derived from the host performance counter instead. The game's calibrated timer (FUN_00180c00,
// which measures raw `rdtsc` against QueryPerformanceCounter) then calibrates itself correctly.
#include "timing.h"

#include "common.h"

#include <cstring>

namespace {

constexpr uint64_t XBOX_TSC_HZ = 733333333;

uint64_t xbox_tsc()
{
    static LARGE_INTEGER freq = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f;
    }();
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    uint64_t t = (uint64_t)now.QuadPart, f = (uint64_t)freq.QuadPart;
    return t / f * XBOX_TSC_HZ + t % f * XBOX_TSC_HZ / f;
}

// XAPI QueryPerformanceCounter (0x232da), stdcall
BOOL __stdcall xapi_QueryPerformanceCounter(LARGE_INTEGER *p)
{
    p->QuadPart = (LONGLONG)xbox_tsc();
    return TRUE;
}

void __cdecl store_tsc() { *(uint64_t *)(uintptr_t)0x002C0468 = xbox_tsc(); }
uint32_t __cdecl tsc_low() { return (uint32_t)xbox_tsc(); }

// The game's helpers keep ecx; LTCG callers may rely on that, so wrap the C functions.
__declspec(naked) void store_tsc_thunk() // FUN_00084190: *(uint64 *)0x2c0468 = rdtsc
{
    __asm {
        push ecx
        call store_tsc
        pop ecx
        ret
    }
}

__declspec(naked) void tsc_low_thunk() // FUN_000841a0: return (uint32)rdtsc
{
    __asm {
        push ecx
        call tsc_low
        pop ecx
        ret
    }
}

struct Patch {
    uint32_t addr;
    uint8_t expect[6];
    size_t expect_len;
    const void *target;
    const char *what;
};

bool patch(const Patch &p)
{
    auto *code = (uint8_t *)(uintptr_t)p.addr;
    if (memcmp(code, p.expect, p.expect_len) != 0) {
        logf("timing: %s at 0x%08x does not match this build; not patched", p.what, p.addr);
        return false;
    }
    code[0] = 0xE9;
    int32_t rel = (int32_t)((uintptr_t)p.target - (p.addr + 5));
    memcpy(code + 1, &rel, 4);
    return true;
}

} // namespace

namespace {
float g_frame_60 = 1.0f / 60.0f;

// A disp32/imm32 operand at `at` inside the instruction at `insn` (whose first bytes must match).
struct Operand {
    uint32_t insn;
    uint8_t expect[6];
    uint32_t at, old_value;
};
} // namespace

bool install_60fps_patches()
{
    uint32_t frame = (uint32_t)(uintptr_t)&g_frame_60;
    const Operand ops[] = {
        {0x0020C2A4, {0xF3, 0x0F, 0x10, 0x05, 0xC0, 0x5F}, 0x0020C2A8, 0x00295FC0}, // movss xmm0, [1/30] (NTSC frame time)
        {0x0020C2AC, {0xC7, 0x05, 0x00, 0x38, 0x2F, 0x00}, 0x0020C2B2, 0x21},       // mov [0x2F3800], 33 (ms per frame)
        {0x00193EAE, {0xF3, 0x0F, 0x59, 0x05, 0xC0, 0x5F}, 0x00193EB2, 0x00295FC0}, // mulss xmm0, [1/30]
        {0x00212210, {0xF3, 0x0F, 0x10, 0x05, 0xC0, 0x5F}, 0x00212214, 0x00295FC0}, // movss xmm0, [1/30] -> 0x2F57F4
    };
    for (const Operand &o : ops)
        if (memcmp((const void *)(uintptr_t)o.insn, o.expect, 6) || *(uint32_t *)(uintptr_t)o.at != o.old_value) {
            logf("timing: 60 fps patch site 0x%08x does not match this build; staying at 30 fps", o.insn);
            return false;
        }
    for (const Operand &o : ops) *(uint32_t *)(uintptr_t)o.at = o.old_value == 0x21 ? 16u : frame;
    logf("timing: world steps 1/60 s per frame (60 fps)");
    return true;
}

void install_timing_patches()
{
    const Patch patches[] = {
        {0x000232DA, {0x8B, 0x4C, 0x24, 0x04, 0x0F, 0x31}, 6, (const void *)&xapi_QueryPerformanceCounter, "QueryPerformanceCounter"},
        {0x00084190, {0x53, 0x0F, 0x31}, 3, (const void *)&store_tsc_thunk, "timer helper (store rdtsc)"},
        {0x000841A0, {0x83, 0xEC, 0x08, 0x53, 0x0F, 0x31}, 6, (const void *)&tsc_low_thunk, "timer helper (rdtsc low)"},
    };
    int n = 0;
    for (const Patch &p : patches) n += patch(p);
    logf("timing: %d of %zu rdtsc users now tick at 733.33 MHz (host QPC based)", n, sizeof patches / sizeof patches[0]);
}
