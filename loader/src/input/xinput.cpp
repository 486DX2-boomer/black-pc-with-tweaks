// HLE replacement for the Xbox input library (XPP section: USB host controller + XID gamepads).
// Xbox port 0 is fed by every PC XInput controller, merged with keyboard and mouse while the game window has
// focus. The user's layout:
//   mouse = look (rotates the player directly, input/mouselook.cpp; a click in the window captures the mouse,
//   Alt+Tab releases it); left button = right trigger, right button = right stick click,
//   wheel up / down = D-pad left / right (one press per notch)
//   WASD = left stick, R = A, F = B, E = X, Q = Y, G = left trigger, C = left stick click, Z / X = D-pad up / down,
//   Enter = START, Space = BACK (select), arrows = D-pad (menus). BLACK and WHITE have no keys (unused).
#include "d3d/dispatch.h"
#include "d3d/host.h"
#include "input/mouselook.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

uint32_t g_pad_watch;

namespace xinput {
namespace {

#pragma pack(push, 1)
struct XGamepad { // Xbox XINPUT_GAMEPAD
    uint16_t wButtons;
    uint8_t bAnalogButtons[8]; // A B X Y BLACK WHITE LTRIGGER RTRIGGER
    int16_t sThumbLX, sThumbLY, sThumbRX, sThumbRY;
};
struct XState {
    uint32_t dwPacketNumber;
    XGamepad Gamepad;
};
#pragma pack(pop)

// PC XInput (loaded dynamically: xinput1_4.dll ships with Windows 10)
struct PcGamepad {
    WORD wButtons;
    BYTE bLeftTrigger, bRightTrigger;
    SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY;
};
struct PcState {
    DWORD dwPacketNumber;
    PcGamepad Gamepad;
};
struct PcVibration {
    WORD wLeftMotorSpeed, wRightMotorSpeed;
};
typedef DWORD(WINAPI *PFN_GetState)(DWORD, PcState *);
typedef DWORD(WINAPI *PFN_SetState)(DWORD, PcVibration *);
PFN_GetState g_pc_get;
PFN_SetState g_pc_set;

struct Device {
    uint32_t magic;
    uint32_t port;
    uint32_t packet;
    XGamepad last;
};
constexpr uint32_t DEV_MAGIC = 0x44495858; // 'XXID'

bool g_reported_insertion;

void load_xinput()
{
    static bool tried;
    if (tried) return;
    tried = true;
    HMODULE m = LoadLibraryW(L"xinput1_4.dll");
    if (!m) m = LoadLibraryW(L"xinput9_1_0.dll");
    if (m) {
        g_pc_get = (PFN_GetState)GetProcAddress(m, "XInputGetState");
        g_pc_set = (PFN_SetState)GetProcAddress(m, "XInputSetState");
    }
    logf("[input] PC XInput %s; keyboard mapping active when the game window has focus", g_pc_get ? "loaded" : "not available");
}

bool key(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

// --press "frame:button,..." or "SECONDSs:button,...": scripted presses for unattended test runs, held for
// 10 frames, or 300 ms when timed in seconds since the first input poll. "button*MS" holds for MS
// milliseconds; lx/ly/rx/ry+NN or -NN pushes a stick to NN percent (e.g. "80s:rx+25*2000"); mx/my+NN or -NN
// moves the mouse NN counts per poll.
struct Press {
    uint32_t frame;   // or milliseconds when timed
    bool timed;
    std::string button;
    uint32_t hold_ms;
};
std::vector<Press> g_presses;
ULONGLONG g_first_poll_ms;
int g_script_mx, g_script_my; // scripted mouse counts for this poll

void parse_presses()
{
    static bool done;
    if (done) return;
    done = true;
    std::string s = narrow(g_cfg.press_script);
    size_t pos = 0;
    while (pos < s.size()) {
        size_t comma = s.find(',', pos);
        std::string item = s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        size_t colon = item.find(':');
        if (colon != std::string::npos) {
            bool timed = colon > 0 && item[colon - 1] == 's';
            uint32_t at = timed ? (uint32_t)(atof(item.c_str()) * 1000.0) : (uint32_t)atoi(item.c_str());
            std::string b = item.substr(colon + 1);
            size_t star = b.find('*');
            uint32_t hold = 300;
            if (star != std::string::npos) {
                hold = (uint32_t)atoi(b.c_str() + star + 1);
                b.resize(star);
            }
            g_presses.push_back({at, timed, b, hold});
        }
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    if (!g_presses.empty()) logf("[input] %zu scripted presses", g_presses.size());
}

void apply_presses(XGamepad &g)
{
    parse_presses();
    g_script_mx = g_script_my = 0;
    uint32_t f = g_frame_count;
    if (!g_first_poll_ms) g_first_poll_ms = GetTickCount64();
    uint32_t ms = (uint32_t)(GetTickCount64() - g_first_poll_ms);
    for (const Press &p : g_presses) {
        if (p.timed ? (ms < p.frame || ms >= p.frame + p.hold_ms) : (f < p.frame || f >= p.frame + 10)) continue;
        const std::string &b = p.button;
        if (b.size() > 3 && (b[0] == 'l' || b[0] == 'r') && (b[1] == 'x' || b[1] == 'y') && (b[2] == '+' || b[2] == '-')) {
            int16_t v = (int16_t)std::clamp(atoi(b.c_str() + 2) * 32767 / 100, -32767, 32767);
            int16_t &axis = b[0] == 'l' ? (b[1] == 'x' ? g.sThumbLX : g.sThumbLY) : (b[1] == 'x' ? g.sThumbRX : g.sThumbRY);
            axis = v;
        }
        if (b.size() > 3 && b[0] == 'm' && (b[1] == 'x' || b[1] == 'y') && (b[2] == '+' || b[2] == '-'))
            (b[1] == 'x' ? g_script_mx : g_script_my) += atoi(b.c_str() + 2);
        if (b == "up") g.wButtons |= 0x01;
        else if (b == "down") g.wButtons |= 0x02;
        else if (b == "left") g.wButtons |= 0x04;
        else if (b == "right") g.wButtons |= 0x08;
        else if (b == "start") g.wButtons |= 0x10;
        else if (b == "back") g.wButtons |= 0x20;
        else if (b == "a") g.bAnalogButtons[0] = 255;
        else if (b == "b") g.bAnalogButtons[1] = 255;
        else if (b == "x") g.bAnalogButtons[2] = 255;
        else if (b == "y") g.bAnalogButtons[3] = 255;
        else if (b == "black") g.bAnalogButtons[4] = 255;
        else if (b == "white") g.bAnalogButtons[5] = 255;
        else if (b == "lt") g.bAnalogButtons[6] = 255;
        else if (b == "rt") g.bAnalogButtons[7] = 255;
        else if (b == "lthumb") g.wButtons |= 0x40;
        else if (b == "rthumb") g.wButtons |= 0x80;
        static uint32_t logged_frame;
        if (logged_frame != p.frame) {
            logged_frame = p.frame;
            logf("[input] scripted press '%s' at frame %u", b.c_str(), f);
        }
    }
}

uint8_t max8(uint8_t a, uint8_t b) { return a > b ? a : b; }

// Keep whichever stick is pushed further.
void merge_stick(int16_t &x, int16_t &y, int16_t px, int16_t py)
{
    if ((int64_t)px * px + (int64_t)py * py > (int64_t)x * x + (int64_t)y * y) {
        x = px;
        y = py;
    }
}

// Mouse: movement since the previous poll (the game polls once per frame) becomes this frame's rotation
// (input/mouselook.cpp); buttons and the wheel feed the triggers and buttons listed at the top.
void mouse_look(XGamepad &g, bool focus)
{
    int dx, dy, wheel;
    host3d::mouse_take(&dx, &dy, &wheel);
    bool live = focus && host3d::mouse_captured();
    if (!live) dx = dy = wheel = 0;
    dx += g_script_mx; // scripted moves work without focus
    dy += g_script_my;
    float k = 0.022f * g_cfg.mouse_sens; // degrees per count
    // The game's yaw rate is positive to the left and its pitch rate positive downwards (mouse dy > 0 = towards
    // the user = look down).
    mouselook_set_frame(-dx * k, (g_cfg.mouse_invert ? -dy : dy) * k);
    if (!live) return;

    int b = host3d::mouse_buttons();
    if (b & 1) g.bAnalogButtons[7] = 255; // left = right trigger
    if (b & 2) g.wButtons |= 0x80;        // right = right stick click
    // Each wheel notch is one D-pad press (up = left, down = right): held 2 polls, then 2 released so quick
    // notches stay separate presses.
    static int pending, hold, gap; // pending: notches, + = up/left, - = down/right
    static uint16_t held;
    pending = std::clamp(pending + wheel, -4, 4);
    if (hold) {
        hold--;
        g.wButtons |= held;
        if (!hold) gap = 2;
    } else if (gap) {
        gap--;
    } else if (pending) {
        held = pending > 0 ? 0x04 : 0x08; // D-pad left / right
        pending += pending > 0 ? -1 : 1;
        hold = 1;
        g.wButtons |= held;
    }
}

// The game plays on Xbox port 0; every connected PC controller (any XInput user index) feeds it, so a
// pad that Windows put on index 1-3 works without setup.
void read_pad(uint32_t port, XGamepad &g)
{
    memset(&g, 0, sizeof g);
    if (port != 0) return;
    static uint32_t polls, absent[4]; // absent: the poll count when a slot was last found empty
    polls++;
    for (DWORD i = 0; g_pc_get && i < 4; i++) {
        if (absent[i] && polls - absent[i] < 120) continue;
        PcState pc = {};
        if (g_pc_get(i, &pc) != ERROR_SUCCESS) {
            absent[i] = polls;
            continue;
        }
        absent[i] = 0;
        const PcGamepad &p = pc.Gamepad;
        g.wButtons |= p.wButtons & 0x00FF; // D-pad, START, BACK, thumb clicks map 1:1
        if (p.wButtons & 0x1000) g.bAnalogButtons[0] = 255; // A
        if (p.wButtons & 0x2000) g.bAnalogButtons[1] = 255; // B
        if (p.wButtons & 0x4000) g.bAnalogButtons[2] = 255; // X
        if (p.wButtons & 0x8000) g.bAnalogButtons[3] = 255; // Y
        if (p.wButtons & 0x0200) g.bAnalogButtons[4] = 255; // BLACK = right shoulder
        if (p.wButtons & 0x0100) g.bAnalogButtons[5] = 255; // WHITE = left shoulder
        g.bAnalogButtons[6] = max8(g.bAnalogButtons[6], p.bLeftTrigger);
        g.bAnalogButtons[7] = max8(g.bAnalogButtons[7], p.bRightTrigger);
        merge_stick(g.sThumbLX, g.sThumbLY, p.sThumbLX, p.sThumbLY);
        merge_stick(g.sThumbRX, g.sThumbRY, p.sThumbRX, p.sThumbRY);
    }
    apply_presses(g);
    bool focus = GetForegroundWindow() == host3d::window();
    mouse_look(g, focus);
    if (!focus) return;
    if (key(VK_UP)) g.wButtons |= 0x01;
    if (key(VK_DOWN)) g.wButtons |= 0x02;
    if (key(VK_LEFT)) g.wButtons |= 0x04;
    if (key(VK_RIGHT)) g.wButtons |= 0x08;
    if (key('Z')) g.wButtons |= 0x01;        // D-pad up
    if (key('X')) g.wButtons |= 0x02;        // D-pad down
    if (key(VK_RETURN)) g.wButtons |= 0x10;  // START
    if (key(VK_SPACE)) g.wButtons |= 0x20;   // BACK (select)
    if (key('C')) g.wButtons |= 0x40;        // left stick click
    if (key('R')) g.bAnalogButtons[0] = 255; // A
    if (key('F')) g.bAnalogButtons[1] = 255; // B
    if (key('E')) g.bAnalogButtons[2] = 255; // X
    if (key('Q')) g.bAnalogButtons[3] = 255; // Y
    if (key('G')) g.bAnalogButtons[6] = 255; // left trigger
    // Keys push the stick fully, unless a pad already pushes that axis further.
    auto push = [](int16_t &axis, int16_t v) {
        if (abs(v) > abs(axis)) axis = v;
    };
    if (key('A')) push(g.sThumbLX, -32767);
    if (key('D')) push(g.sThumbLX, 32767);
    if (key('W')) push(g.sThumbLY, 32767);
    if (key('S')) push(g.sThumbLY, -32767);
}

} // namespace

void __stdcall XInitDevices(uint32_t dwPreallocTypeCount, void *PreallocTypes)
{
    DTRACE("%u, %p", dwPreallocTypeCount, PreallocTypes);
    load_xinput();
}

// Port bitmasks: bit n = port n (gamepads). One gamepad on port 0, reported once as an insertion.
BOOL __stdcall XGetDeviceChanges(void *DeviceType, uint32_t *pdwInsertions, uint32_t *pdwRemovals)
{
    DTRACE("%p", DeviceType);
    *pdwInsertions = 0;
    *pdwRemovals = 0;
    if (DeviceType == (void *)(uintptr_t)0x0027cf4c && !g_reported_insertion) { // XDEVICE_TYPE_GAMEPAD
        g_reported_insertion = true;
        *pdwInsertions = 1;
        return TRUE;
    }
    return FALSE;
}

HANDLE __stdcall XInputOpen(void *DeviceType, uint32_t dwPort, uint32_t dwSlot, void *pPollingParameters)
{
    auto *d = new Device();
    d->magic = DEV_MAGIC;
    d->port = dwPort;
    logf("[input] XInputOpen(type %p, port %u, slot %u) -> %p", DeviceType, dwPort, dwSlot, d);
    return d;
}

void __stdcall XInputClose(Device *h)
{
    DTRACE("%p", h);
    if (h && h->magic == DEV_MAGIC) h->magic = 0;
}

uint32_t __stdcall XInputGetState(Device *h, XState *pState)
{
    if (!h || h->magic != DEV_MAGIC) return ERROR_DEVICE_NOT_CONNECTED;
    if (g_cfg.watch_pad && !g_pad_watch && g_frame_count >= g_cfg.watch_pad) {
        // Debug: hardware read/write breakpoint (DR3, 2 bytes) on this thread over sThumbRX of the game's buffer.
        uint32_t base = g_cfg.watch_read_deref ? *(uint32_t *)(uintptr_t)g_cfg.watch_read : g_cfg.watch_read;
        g_pad_watch = g_cfg.watch_read ? base + g_cfg.watch_read_off : (uint32_t)(uintptr_t)&pState->Gamepad.sThumbRX;
        CONTEXT c = {};
        c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        GetThreadContext(GetCurrentThread(), &c);
        c.Dr3 = g_pad_watch;
        uint32_t len = g_cfg.watch_read ? 0x3u : 0x1u; // LEN3: 4 bytes for --watch-read, else 2
        uint32_t rw = g_cfg.watch_write_only ? 0x1u : 0x3u;
        c.Dr7 = (c.Dr7 & ~(0xFu << 28)) | (1u << 6) | (rw << 28) | (len << 30); // L3, RW3 = write or read/write
        BOOL ok = SetThreadContext(GetCurrentThread(), &c);
        logf("[input] watching right-stick reads at %08x (%s)", g_pad_watch, ok ? "set" : "FAILED");
    }
    static std::atomic<uint32_t> polls{0};
    if ((++polls % 600) == 1) logf("[input] XInputGetState poll #%u (frame %u)", polls.load(), g_frame_count.load());
    XGamepad g;
    read_pad(h->port, g);
    if (memcmp(&g, &h->last, sizeof g)) {
        h->last = g;
        h->packet++;
    }
    pState->dwPacketNumber = h->packet;
    pState->Gamepad = g;
    return ERROR_SUCCESS;
}

// XINPUT_FEEDBACK: header {dwStatus, hEvent, Reserved[58]} (68 bytes with padding), then rumble.
uint32_t __stdcall XInputSetState(Device *h, uint8_t *pFeedback)
{
    if (!h || h->magic != DEV_MAGIC) return ERROR_DEVICE_NOT_CONNECTED;
    PcVibration v = {*(uint16_t *)(pFeedback + 68), *(uint16_t *)(pFeedback + 70)};
    for (DWORD i = 0; g_pc_set && h->port == 0 && i < 4; i++) g_pc_set(i, &v); // port 0 = every PC pad
    *(uint32_t *)pFeedback = ERROR_SUCCESS; // completed immediately
    if (HANDLE ev = *(HANDLE *)(pFeedback + 4)) SetEvent(ev);
    return ERROR_IO_PENDING;
}

#define H(addr, fn, params, ret) {addr, #fn, (void *)&fn, params, ret}

static const xd3d::Hle kTable[] = {
    H(0x0027dd24, XInitDevices, "s s", false),
    H(0x0027dd29, XGetDeviceChanges, "s s s", true),
    H(0x0027dd96, XInputOpen, "s s s s", true),
    H(0x0027ddec, XInputClose, "s", false),
    H(0x0027ddf8, XInputGetState, "s s", true),
    H(0x0027de6b, XInputSetState, "s s", true),
};

} // namespace xinput

namespace xd3d {
const Hle *hle_input_table(size_t *count)
{
    *count = sizeof xinput::kTable / sizeof xinput::kTable[0];
    return xinput::kTable;
}
} // namespace xd3d
