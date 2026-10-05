// Shared declarations for blackrt.dll: config, logging, shutdown, symbols.
#pragma once

#include <windows.h>

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>

typedef LONG NTSTATUS;

// NTSTATUS values used by the HLE kernel (Xbox uses the NT values).
constexpr NTSTATUS XSTATUS_SUCCESS = 0x00000000;
constexpr NTSTATUS XSTATUS_ALERTED = 0x00000101;
constexpr NTSTATUS XSTATUS_TIMEOUT = 0x00000102;
constexpr NTSTATUS XSTATUS_PENDING = 0x00000103;
constexpr NTSTATUS XSTATUS_USER_APC = 0x000000C0;
constexpr NTSTATUS XSTATUS_NO_MORE_FILES = (NTSTATUS)0x80000006;
constexpr NTSTATUS XSTATUS_UNSUCCESSFUL = (NTSTATUS)0xC0000001;
constexpr NTSTATUS XSTATUS_NOT_IMPLEMENTED = (NTSTATUS)0xC0000002;
constexpr NTSTATUS XSTATUS_INVALID_HANDLE = (NTSTATUS)0xC0000008;
constexpr NTSTATUS XSTATUS_INVALID_PARAMETER = (NTSTATUS)0xC000000D;
constexpr NTSTATUS XSTATUS_NO_MEMORY = (NTSTATUS)0xC0000017;
constexpr NTSTATUS XSTATUS_ACCESS_DENIED = (NTSTATUS)0xC0000022;
constexpr NTSTATUS XSTATUS_BUFFER_TOO_SMALL = (NTSTATUS)0xC0000023;
constexpr NTSTATUS XSTATUS_OBJECT_TYPE_MISMATCH = (NTSTATUS)0xC0000024;
constexpr NTSTATUS XSTATUS_OBJECT_NAME_INVALID = (NTSTATUS)0xC0000033;
constexpr NTSTATUS XSTATUS_OBJECT_NAME_NOT_FOUND = (NTSTATUS)0xC0000034;
constexpr NTSTATUS XSTATUS_OBJECT_NAME_COLLISION = (NTSTATUS)0xC0000035;
constexpr NTSTATUS XSTATUS_OBJECT_PATH_NOT_FOUND = (NTSTATUS)0xC000003A;
constexpr NTSTATUS XSTATUS_NOT_SUPPORTED = (NTSTATUS)0xC00000BB;
constexpr NTSTATUS XSTATUS_INVALID_DEVICE_REQUEST = (NTSTATUS)0xC0000010;

#define NT_OK(s) ((NTSTATUS)(s) >= 0)

struct Config {
    std::wstring xbe_path;     // game\default.xbe
    std::wstring game_dir;     // directory holding default.xbe = \Device\CdRom0
    std::wstring hdd_dir;      // host folder backing \Device\Harddisk0\PartitionN
    std::wstring log_path;     // black.log
    std::wstring symbols_path; // optional functions.csv for crash reports
    std::wstring screenshot_dir; // --screenshot: PNG frames go here
    bool trace = false;        // log every kernel call, not just the first of each kind
    bool strict = false;       // stop at the first call to an unimplemented kernel function
    bool multicore = false;    // default: pin to one CPU like the Xbox
    bool skip_videos = false;  // dev: unskippable videos are replaced by the 9-second logo video
    uint32_t shot_every = 60;  // --screenshot cadence in frames
    uint32_t shot_start = 0;   // --shot-start: no cadence screenshots before this many seconds
    unsigned debug_frame_end = 0; // --debug-frame N-M: last frame of a traced range (0 = single frame)
    float gamma = 1.0f;        // output gamma applied when presenting (F7/F8 adjust it live)
    float scale = 3.0f;        // render resolution = Xbox resolution x scale (any factor, e.g. 2.25 = 1080 lines)
    int render_w = 0, render_h = 0; // --resolution WxH or H: the picture's render size instead of a scale
    bool render_native = false;     // --resolution native: render at the window's (or screen's) size
    float render_factor = 1.0f;     // with native: render at this multiple of the window (F10 cycles 1 / 1.5 / 2)
    std::wstring rescale_at;        // tests: --rescale-at "S:factor,..." = F10 to that factor at S seconds
    int window_w = 0, window_h = 0; // window client size; 0 = fit the screen
    // The console's video settings report a widescreen TV (EEPROM XC_VIDEO bit 16), as the Xbox Series does
    // for original Xbox games: BLACK then uses a 16:9 projection and its `_wide` UI layouts in the same 640x480
    // buffer (anamorphic), and the loader renders and shows that buffer at 16:9. --normal-aspect turns it off.
    bool widescreen = true;
    bool fullscreen = false;   // --fullscreen: start borderless fullscreen (F11 toggles)
    // Mouse look rotates the player directly (input/mouselook.cpp): mouse_sens x 0.022 degrees per count, the
    // scale most PC shooters use (F3/F4 adjust live).
    float mouse_sens = 2.0f;
    bool mouse_invert = false;
    bool timer_vblank = false; // --timer-vblank: free-running 59.94 Hz vblank instead of following the display
    bool display_vblank = false; // --display-vblank (tests): follow the display even with --background
    bool autoaim = true;       // the game's view pull toward targets (input/autoaim.cpp; --no-autoaim, menu)
    int fps = 30;              // --fps 60: swap interval TWO paced as ONE and the world steps 1/60 s (timing.cpp);
                               // not the default yet: weapon fire still runs per frame (twice as fast)
    bool reverb = true;        // mix the I3DL2 reverb send (--no-reverb to compare)
    float reverb_gain = 1.0f;  // --reverb-gain: overall reverb level
    bool background = false;   // --background (tests): window minimised without focus, audio muted
    std::wstring wav_path;     // --wav: record the mixer output (16-bit stereo 48 kHz)
    std::wstring rawvideo;     // --rawvideo: the shown picture as raw BGRX frames (from --shot-start, --clip-seconds)
    uint32_t clip_seconds = 4;
    // Output gain after the game's gamma ramp (F5/F6 adjust it live). The ramp (d3d/device.cpp, ramp_texture)
    // already doubles the framebuffer like the Xbox's video DAC did (BLACK's art uses 0x80 = full brightness).
    float gain = 1.0f;
    bool ramp = true;          // apply the game's gamma ramp (fades, brightness); --no-ramp = identity
    bool aa = true;            // post-process anti-aliasing on the final picture (--no-aa, F9 toggles)
    bool info_only = false;    // print XBE info and exit
    bool force = false;        // run even if the XBE is not the build the address tables were made for
    unsigned timeout_s = 0;    // exit after N seconds (0 = never)
    uint32_t watch[4] = {};    // --watch: hardware write breakpoints (Xbox addresses)
    int watch_count = 0;
    uint32_t probe[16] = {};   // --probe: log each time execution reaches these addresses
    int probe_count = 0;
    unsigned debug_draws = 0;  // --debug-draws: describe the first N draws
    unsigned debug_frame = 0;  // --debug-frame: describe every draw of this frame
    std::wstring press_script; // --press: "frame:button,frame:button" scripted controller input
    std::wstring perf_at;      // --perf-at S:D: record performance for D seconds from S seconds after the first frame
    std::wstring memdump;      // --memdump [*]ADDR:SIZE:F1,F2,...: raw memory to the --screenshot dir at those frames
    unsigned watch_pad = 0;    // --watch-pad N: from frame N, log game code that reads the right stick (DR3)
    uint32_t watch_read = 0;   // --watch-read [w][*]ADDR[+OFF]: with --watch-pad, watch these 4 bytes instead
    uint32_t watch_read_off = 0; // (* = ADDR holds a pointer, read when the watch starts; w = writes only)
    bool watch_read_deref = false, watch_write_only = false;
};
extern Config g_cfg;
extern DWORD_PTR g_game_cpu; // affinity of the threads that run game code (0 = any, --multicore)
extern uint32_t g_pad_watch; // address DR3 watches for --watch-pad (input/xinput.cpp), 0 = off
extern std::atomic<uint32_t> g_frame_count; // frames presented so far (d3d/device.cpp)

// Logging: every line goes to the log file and the console, prefixed with time and host thread id.
void log_init(const std::wstring &path);
void logf(const char *fmt, ...);
void logv(const char *fmt, va_list ap);
[[noreturn]] void fatal(const char *fmt, ...);
std::string narrow(const std::wstring &w);
std::wstring widen(const std::string &s);

// Request process exit; the host main thread performs it.
void request_exit(int code, const char *why);
bool exit_requested();

// Address -> "name+off" using functions.csv if it was loaded (crash reports and hook logs).
void symbols_load(const std::wstring &csv_path);
std::string sym_name(uint32_t addr);
bool sym_function(uint32_t addr, std::string *name, uint32_t *start); // the function containing addr, if known

// Kernel call tracing: first call of each function is always logged; with --trace every call.
#define KTRACE(fmt, ...)                                                                    \
    do {                                                                                    \
        static std::atomic<bool> s_seen_{false};                                            \
        if (g_cfg.trace || !s_seen_.exchange(true))                                         \
            logf("[k] %s(" fmt ")%s", __func__, ##__VA_ARGS__, g_cfg.trace ? "" : "  <first call>"); \
    } while (0)

// One-shot note about a partially implemented path.
#define KNOTE_ONCE(fmt, ...)                                     \
    do {                                                         \
        static std::atomic<bool> s_noted_{false};                \
        if (!s_noted_.exchange(true)) logf("[k] note: " fmt, ##__VA_ARGS__); \
    } while (0)
