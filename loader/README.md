# BLACK loader

Runs the original `default.xbe` natively as a 32-bit Windows process with an HLE Xbox kernel, D3D (on
Direct3D 9), DirectSound (XAudio2) and input (XInput, keyboard and mouse). The game is playable.

## Build

Needs Visual Studio 2026 (or 2022) Build Tools with the C++ x86 toolchain. CMake ships with VS
(`...\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`) if it is not on PATH.

```
cmake --preset win32                 # or win32-vs2022
cmake --build build/win32 --config RelWithDebInfo
```

Output: `build/win32/RelWithDebInfo/black.exe` + `blackrt.dll` (keep them together).

## Run (from the project root)

```
loader\build\win32\RelWithDebInfo\black.exe --info game\default.xbe     # print XBE structure
loader\build\win32\RelWithDebInfo\black.exe --timeout 30 game\default.xbe
```

Options: `--hdd <dir>` (host folder for HDD partitions, default `.\hdd`), `--log <file>` (default `.\black.log`),
`--verbose` (the whole log on the console too), `--trace` (log every kernel call), `--strict` (stop at the first unimplemented kernel call), `--multicore`,
`--symbols <functions.csv>` (default `notes\functions.csv`), `--force` (other XBE builds),
`--skip-videos` (dev: 9-second logo video instead of the long ones). `black.exe --help` lists the debugging
options (`--timeout`, `--screenshot`, `--shot-every`, `--shot-start`, `--debug-frame`, `--press`, `--watch`,
`--watch-pad`, `--probe`, `--background`, `--wav`).

Display: widescreen 16:9 by default (the console reports a widescreen TV and BLACK switches to its own 16:9
camera and UI layouts); `--normal-aspect` for 4:3. `--scale K` renders at K x the Xbox resolution (any
factor, default 3 = 2560x1440 in widescreen, 1920x1440 in 4:3); `--resolution 1920x1080` (or just `1080`, or
`native` = the window's size, and then it follows the window when it is resized) sets the render size directly. `--window WxH` sets the window size (default: the
largest that fits the screen), `--fullscreen` starts borderless fullscreen. `--gain K` / `--gamma G` adjust the
output after the game's own gamma ramp (default 1.0 / 1.0; `--no-ramp` shows the raw framebuffer). `--mouse-sens K` (0.022 degrees x K per count, default 2.0) and
`--mouse-invert` tune mouse look; `--no-autoaim` turns off the game's auto-aim (the view pulling toward enemies). `--fps 30|60` (default 30, the original; 60 is experimental). `--reverb-gain K`
(default 1.0) / `--no-reverb` for the room reverb. `--no-aa` turns off the anti-aliasing, `--no-seamfill` the
filling of hairline cracks between geometry pieces (Video > Fill geometry seams).

`black.exe` is a windowed program: no console window opens. Started from a command prompt it prints a short summary
there (startup, setup errors, exit, crashes); the full log is in `black.log`.

Relative paths resolve against the current folder; if the game is not found there, black.exe looks upwards
from its own folder for `game\default.xbe` and uses that folder (so it can be started from anywhere).

The window has a menu bar (Game, Video, Audio, Input, Tools, Help) for the port's settings: render resolution,
anti-aliasing, fullscreen, aspect ratio and frame rate (a new choice is saved for the next start, with a notice; the
running game keeps its values), brightness, reverb, key bindings, mouse, auto-aim. Choices are saved to `black.ini` in the game folder; command-line flags override it for that run. Pausing (Esc) releases the mouse
so the menu can be reached. In the game window: F11 fullscreen on/off (the window resizes at the game's aspect; other shapes get black
bars), F3/F4 mouse sensitivity, F5/F6 gain, F7/F8 gamma, F9 anti-aliasing, F10 render size 1 / 1.5 / 2 x the window (all shown in the title), F12 dumps the next frame
(draw log, every render target as PNG) into `work\dumps` for bug reports. F2 (or Tools > Record performance) starts and
stops a performance recording: frame times, where the CPU time goes, threads; saved in `perf\<date_time>\` next to
`black.ini` (summary.txt, frames.csv, threads.csv). `--perf-at S:D` records D seconds from S seconds after the first frame. On a ~60 Hz display the frame clock follows the
display's refresh (smooth frame delivery); `--timer-vblank` uses a fixed 59.94 Hz timer instead. The game threads run on
one CPU like the Xbox (the last physical core, its hyperthread sibling kept free); `--game-cpu N` picks another. On weak
PCs (old laptops with Intel graphics) play at 30 fps (Video > Frame rate), the game's original rate.

Controls: every PC controller drives the Xbox pad on port 0. Keys and mouse buttons are bound to the game's actions
(`src/input/bindings.cpp`; Input > Key bindings, `key_<action> = Key[,Key]` in black.ini): each poll a held key looks
up its action in the game's live binding table and presses the pad input the game reads for it, so the keys follow
the game's controller layout. Defaults: WASD move, left / right mouse button = fire / aim, R reload, E action, C crouch,
G grenade, F melee, wheel down / up = next / previous weapon, Z firing mode, Q suppressor, X health pack, Space
objectives, Esc pause (always; Esc also lets go of the mouse, in menus too, and play starting or resuming takes it again). Menus, pauses and cutscenes
(no player update) use arrows = D-pad, Enter = A, Backspace = B. Mouse = look; click the window to capture it, Alt+Tab
releases it. Tests: `--press "95s:k:W*1500"` holds a bound key (black.ini spelling) without focus.

## Local launcher (icon + shortcut)

The icon comes from the user's XBE, so it is stamped into a local copy only (`work\` is git-ignored;
nothing game-derived goes into the repo or the build):

```
copy loader\build\win32\RelWithDebInfo\black.exe work\run\ & copy loader\build\win32\RelWithDebInfo\blackrt.dll work\run\
python tools\xbe_images.py game\default.xbe work\icons
python tools\set_exe_icon.py work\run\black.exe work\icons\title_image.ico
powershell -File tools\make_shortcut.ps1 -Link BLACK.lnk -Target work\run\black.exe -Arguments "--hdd work\hdd --log work\play.log"
```

`BLACK.lnk` stores the target relative to itself as well (found again after the folder moves or the drive
letter changes); its "Start in" is empty because black.exe finds the project folder itself.

## Layout

| File | What |
|---|---|
| `stub/stub.c` | `black.exe`: no CRT, linked at `0x10000` with a large BSS so its image owns the XBE range |
| `src/main.cpp` | `BlackMain`: arguments, build check (SHA-256), startup order |
| `src/image.cpp` | maps the XBE over the stub image, splices a PE header in for Windows, `XeLoadSection` |
| `src/xthread.cpp` | per-thread Xbox state; rewrites `fs:[0x04/0x20/0x24/0x28/0x58]` to TLS slots; IRQL lock |
| `src/hooks.cpp` | library hooks (tables in `*_functions.inc`), crash reports, `--watch`/`--probe` |
| `src/kernel/` | HLE kernel: `ktable.cpp` (ordinal table + logging stub), memory, threads, sync, DPC/timers, files, Rtl, misc |
| `src/d3d/` | D3D HLE on Direct3D 9: device/state, resources, textures, draws, push buffer, NV2A vertex programs and register combiners to HLSL |
| `src/audio/` | DirectSound HLE: software mixer (`dsound.cpp`), room reverb (`reverb.cpp`) and XAudio2 output (`output.cpp`) |
| `src/input/` | XInput HLE: PC controllers, keyboard and mouse; `mouselook.cpp` feeds the mouse into the game's view angles |
| `src/*_functions.inc` | generated by `tools/gen_section_table.py` from `notes/functions.csv` (addresses and names only) |

Design notes and findings are in `../NOTES.md`.
