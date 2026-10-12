// Calls from the front end's menu scripts into the port.
//
// The menu pages reach game code through TriggerGamecode1/2/3(name, ...) (defined in each page's script): it sets the
// native externObject's FEFunctionTrigger to 1 and FEFunctionName / FEFunctionParam1..3 (strings). Those are fields
// of the front-end object (set up by FUN_00114A10): trigger +0x370, name +0x374 (32 bytes), parameters +0x394, +0x3D0
// and +0x40C (60 bytes each). Once per front-end update (calls at 0x12465C and 0x1246CA) FUN_00112F30 (esi = that
// object) hashes the name (FUN_000BF2A0), finds it in the 0x6E-entry table {hash, fn} at the object's start (filled
// per page by FUN_00112FC0), calls fn(parameter 1) (cdecl) and clears the trigger and the strings; an unknown name is
// dropped. A thunk at the dispatcher's entry shows every call to the port first: names starting with "PC" are the
// port's own (handled and cleared here), the others go on to the game and are logged.
#include "fe/calls.h"

#include "common.h"
#include "fe/bind_list.h"
#include "fe/pause_ui.h"
#include "fe/pc_settings.h"
#include "input/bindings.h"
#include "fe/vars.h"

#include <cstdlib>
#include <cstring>

namespace {

constexpr uint32_t kDispatch = 0x00112F30;
constexpr uint8_t kDispatchCode[] = {0x83, 0xBE, 0x70, 0x03, 0x00, 0x00, 0x01, 0x75, 0x44}; // cmp [esi+370h],1; jne
constexpr uint32_t kTrigger = 0x370, kName = 0x374, kParam[3] = {0x394, 0x3D0, 0x40C};
constexpr size_t kNameSize = 0x20, kParamSize = 0x3C;

// a copy of a field, cut at its size (the strings are written by the script runtime)
struct Field {
    char s[kParamSize + 1];
    Field(const uint8_t *fe, uint32_t off, size_t size)
    {
        memcpy(s, fe + off, size);
        s[size] = 0;
    }
};

void pc_debug(const char *a, const char *b, const char *c) { logf("fe: PCDebug '%s' '%s' '%s'", a, b, c); }
// tests: the pages' event log (fe/pages.cpp dbg_log), 150 characters a call
void pc_log(const char *a, const char *b, const char *c) { logf("fe: PCLog %s%s%s", a, b, c); }

// The page that is up, which the port's values follow: a main-menu group's page (PCPage(group)), its KEY BINDINGS page
// (PCPage(100)) or the pause page (PCOpen); they all show the same few values (fe/vars.h).
enum class Ui { None, Page, Keys, Pause };
Ui g_ui;
int g_page;
constexpr int kKeysPage = 100;

void refresh_ui()
{
    switch (g_ui) {
    case Ui::Page: pcset::fill_page(g_page); break;
    case Ui::Keys: bindlist::refresh(); break;
    case Ui::Pause: pauseui::refresh(); break;
    case Ui::None: break;
    }
}

void pc_page(const char *id, const char *, const char *)
{
    g_page = atoi(id);
    g_ui = g_page == kKeysPage ? Ui::Keys : Ui::Page;
    if (g_ui == Ui::Keys) bindlist::open();
}

// a PC SETTINGS list box: (row, -1 / +1)
void pc_change(const char *row, const char *dir, const char *) { pcset::step(atoi(row), atoi(dir)); }

// the KEY BINDINGS page: a row's input (row, FEInputId, MovedCursor), and the page being prepared
void pc_bind(const char *row, const char *id, const char *moved)
{
    bindlist::input(atoi(row), atoi(id), strcmp(moved, "true") == 0 || strcmp(moved, "1") == 0);
}

// the PC SETTINGS pause page: prepared (bar frames), a row's input (row, FEInputId, MovedCursor), Back below the top
void pc_open(const char *frames, const char *, const char *)
{
    g_ui = Ui::Pause;
    pauseui::open(atoi(frames));
}
void pc_row(const char *row, const char *id, const char *moved)
{
    pauseui::row(atoi(row), atoi(id), strcmp(moved, "true") == 0 || strcmp(moved, "1") == 0);
}
void pc_back(const char *, const char *, const char *) { pauseui::back(); }

// a page found its menu gone and made it again (fe/pages.cpp emit_heal)
void pc_healed(const char *, const char *, const char *) { logf("fe: a page's menu was gone; made again"); }

struct Handler {
    const char *name;
    void (*fn)(const char *, const char *, const char *);
};
const Handler kHandlers[] = {
    {"PCDebug", pc_debug},
    {"PCLog", pc_log},
    {"PCChange", pc_change},
    {"PCBind", pc_bind},
    {"PCPage", pc_page},
    {"PCOpen", pc_open},
    {"PCRow", pc_row},
    {"PCBack", pc_back},
    {"PCHealed", pc_healed},
};

} // namespace

// The thunk's callee: true when the port took the call (the dispatcher then returns at once).
static int __cdecl fe_dispatch(uint8_t *fe)
{
    fe_vars_poll();
    static int s_tick;
    bool tick = s_tick++ % 15 == 0; // values changed elsewhere (hotkeys, the window)
    static bool s_was_capturing; // the bindings list follows a capture every update, and once more when it ends
    bool capturing = bindings::capturing();
    if (tick || capturing || s_was_capturing) refresh_ui();
    if (tick) pcset::refresh_sliders();
    s_was_capturing = capturing;
    if (*(const int32_t *)(fe + kTrigger) != 1) return 0;
    Field name(fe, kName, kNameSize), p1(fe, kParam[0], kParamSize), p2(fe, kParam[1], kParamSize),
        p3(fe, kParam[2], kParamSize);
    if (strncmp(name.s, "PC", 2) != 0) {
        logf("fe: game call %s('%s', '%s', '%s')", name.s, p1.s, p2.s, p3.s);
        return 0;
    }
    logf("fe: port call %s('%s', '%s', '%s')", name.s, p1.s, p2.s, p3.s);
    bool known = false;
    if (strncmp(name.s, "PCSlider", 8) == 0) { // a PC SETTINGS slider: PCSlider<row>(position 0-100)
        pcset::slide(atoi(name.s + 8), (float)atof(p1.s));
        known = true;
    }
    for (const Handler &h : kHandlers)
        if (strcmp(h.name, name.s) == 0) {
            h.fn(p1.s, p2.s, p3.s);
            known = true;
        }
    if (!known) logf("fe: unknown port call %s('%s', '%s', '%s')", name.s, p1.s, p2.s, p3.s);
    refresh_ui(); // the page shows the call's effect at once
    pcset::refresh_sliders();
    *(int32_t *)(fe + kTrigger) = 0; // as the dispatcher does after a call
    fe[kName] = 0;
    for (uint32_t off : kParam) fe[off] = 0;
    return 1;
}

extern "C" uint32_t g_fe_dispatch_resume = kDispatch + 7; // the dispatcher's jne

__declspec(naked) static void dispatch_thunk()
{
    __asm {
        push esi
        call fe_dispatch
        add esp, 4
        test eax, eax
        jnz handled
        cmp dword ptr [esi+0x370], 1 // the instruction the jump replaced
        jmp dword ptr [g_fe_dispatch_resume]
    handled:
        ret
    }
}

void install_fe_calls()
{
    auto *code = (uint8_t *)(uintptr_t)kDispatch;
    if (memcmp(code, kDispatchCode, sizeof kDispatchCode) != 0) {
        logf("fe: the front-end call dispatcher at 0x%08x does not match this build; menu calls to the port are off", kDispatch);
        return;
    }
    int32_t rel = (int32_t)((uintptr_t)&dispatch_thunk - (kDispatch + 5));
    code[0] = 0xE9;
    memcpy(code + 1, &rel, 4);
    code[5] = code[6] = 0x90;
}
