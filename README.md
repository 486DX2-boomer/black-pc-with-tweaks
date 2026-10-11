# BLACK PC port

An unofficial native Windows port of **BLACK** (Criterion Games, 2006), made from the original Xbox version (NTSC-U).

`black.exe` loads your own copy of the game's `default.xbe` and runs the original x86 game code directly on the
PC. Nothing is emulated instruction by instruction: the parts of the Xbox the game talks to (the kernel, Direct3D 8,
DirectSound, the controller library) are replaced by PC code. This is high-level emulation in the style of
Cxbx-Reloaded, built for this one game.

**You need your own copy of the game.** This repository and the release downloads contain no game code or data.

## Status

The game boots through the logos, title screen, menus and videos and is playable. The first two missions have been
played through; later missions have had less testing. Please report problems in the issue tracker and include
`black.log`.

- Any render resolution (720p to 4K and beyond, or a multiple of the window size), anti-aliasing
- Widescreen 16:9 (the game's own widescreen mode) or 4:3, adjustable field of view
- 30 fps (the original) or 60 fps (experimental)
- Mouse look, with the weapon swaying as it does with the stick; keys bound to the game's actions; XInput
  controllers
- Borderless fullscreen (F11), a menu bar for the port's settings, saved to `black.ini`
- Room reverb, the game's own brightness and gamma ramp, an option to turn off auto-aim
- Fixes for things the Xbox hides by its low resolution: flickering seams between pieces of destructible objects,
  a grid pattern in the blur effects

## Requirements

- Windows 10 or 11. The program is 32-bit and runs on 64-bit Windows.
- A graphics card with Direct3D 9 (Shader Model 3). On old laptops with Intel graphics (HD 4400 class) play at 30 fps.
- The NTSC-U (North America) Xbox disc of BLACK. Other regions and builds have different code addresses and are not
  supported.

Nothing else needs installing: the C runtime is built in, and Direct3D 9, the shader compiler and XAudio2 ship with
Windows.

## Setting up

1. Dump your disc to an ISO and extract it with [extract-xiso](https://github.com/XboxDev/extract-xiso)
   (`extract-xiso -x Black.iso`). Rename the extracted folder to `game`.
2. Check that you have the supported build. In PowerShell:

   ```powershell
   Get-FileHash game\default.xbe -Algorithm SHA256
   ```

   It must print `DF2739C372D254A90AEECCF5971D12097F22D89FBB90DB021FDA96DD4DEB68F6`.
3. Download the zip from [Releases](../../releases) and put `black.exe` and `blackrt.dll` next to the `game` folder:

   ```
   BLACK\
     black.exe
     blackrt.dll
     game\
       default.xbe
       ...
   ```

4. Start `black.exe`.

The port keeps its files in that folder: `hdd\` holds the Xbox hard disk (saved games and settings), `black.ini` the
port's settings and `black.log` the log of the last run. The game folder can also be somewhere else:
`black.exe D:\Games\Black\game` (the port's own files then go into the current folder). Only the game window opens;
`black.log` records the details of each run.

## Controls

Keys and mouse buttons are bound to the game's own actions, not to controller buttons, and can be changed in
Input > Key bindings (two keys per action, saved to `black.ini`). Controllers work as well. Click the window to
capture the mouse; Esc lets go of it (in play it also pauses), and play starting or resuming takes it again.

| Action | Default |
|---|---|
| Move | W A S D |
| Look | mouse |
| Fire / Aim | left / right mouse button |
| Reload | R |
| Action (use, pick up) | E |
| Crouch | C |
| Grenade | G |
| Melee | F |
| Next / previous weapon | mouse wheel down / up |
| Change firing mode | Z |
| Toggle suppressor | Q |
| Health pack | X |
| Show objectives | Space |
| Pause | Esc |

In menus the arrow keys move, Enter selects and Backspace goes back. Help > Controls lists the current keys.

| Key | Port function |
|---|---|
| F11 | fullscreen on / off |
| F3 / F4 | mouse sensitivity |
| F5 / F6 | output gain (brightness) |
| F7 / F8 | gamma |
| F9 | anti-aliasing |
| F10 | render size 1 / 1.5 / 2 x the window |
| F2 | start / stop a performance recording (`perf\`) |
| F12 | dump the next frame for bug reports (`work\dumps\`) |

## Settings

The window's menu bar holds the port's settings: render resolution, anti-aliasing, fullscreen, field of view, aspect
ratio, frame rate, brightness, seam filling, reverb, key bindings, mouse sensitivity and inversion, auto-aim. Most apply at once; aspect ratio
and frame rate are read by the game when it starts, so a new choice is used from the next start. Choices are saved to
`black.ini`.

`black.exe --help` lists the command-line options. They override `black.ini` for that run, for example
`--fullscreen`, `--resolution 1080`, `--fps 60`, `--fov 86`, `--normal-aspect`, `--no-autoaim`, `--mouse-sens 2.5`.

Field of view (Video > Field of view, `--fov`) is horizontal, in degrees; the game's is 70. The widescreen picture
keeps that width and shows less height than 4:3; 86 shows the same height as the 4:3 picture. Aiming zooms by the
same factor as before, cutscenes keep their own framing.

## Known issues

- 60 fps is experimental; 30 fps is the game's original rate and the default.
- The game's on-screen prompts show Xbox controller buttons, also when you play with the keyboard.
- Audio: the reverb is an approximation of the Xbox's, some effect sends are not mixed, there is no subwoofer
  channel, and 3D sound uses distance and panning only.
- Only the NTSC-U build is supported (`--force` tries another build, which will most likely fail).

## Building from source

Visual Studio 2022 or 2026 (Build Tools are enough) with the C++ x86 tools, and CMake 3.24 or newer (it ships with
Visual Studio).

```
cd loader
cmake --preset win32            # Visual Studio 2026; use win32-vs2022 for Visual Studio 2022
cmake --build build/win32 --config RelWithDebInfo
```

This builds `build\win32\RelWithDebInfo\black.exe` and `blackrt.dll`. `loader/README.md` has the developer
options and the source layout.

## How it works

`black.exe` is a small stub linked at the XBE's base address, so the game's address range belongs to it before
Windows can put anything there. `blackrt.dll` maps `default.xbe` into that range, provides the 115 Xbox kernel
functions the game imports, and redirects the Xbox libraries that are linked into the game (Direct3D 8,
DirectSound, XAPI) to PC code by patching their entry points. Graphics run on Direct3D 9: the NV2A vertex programs
and register combiners are translated to HLSL. Audio is a software DirectSound mixer on XAudio2.

The game's code is never changed on disk; the few patches (60 fps timing, mouse look, auto-aim switch) are applied
in memory at start. Design notes and findings (addresses, structures, the boot sequence) are in `NOTES.md`; the
original plan is `docs/ROADMAP.md`. `notes/` holds analysis results used to build the address tables: function
addresses and symbol names, the XBE header report and checksums of the disc's files. It contains no game code.

## Legal

This project is not affiliated with or endorsed by Criterion Games or Electronic Arts. BLACK is a trademark of
Electronic Arts Inc.; Xbox is a trademark of Microsoft Corporation. No code or data from the game and no code from
the Microsoft Xbox Development Kit is included. You need your own legally obtained copy of the game.

The port is free software under the GNU General Public License, version 2 or (at your option) any later version.
See `LICENSE`.

## Thanks

[Cxbx-Reloaded](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded) and [xemu](https://xemu.app) served as
references for how the Xbox kernel and the NV2A behave (no code was copied).
[XbSymbolDatabase](https://github.com/Cxbx-Reloaded/XbSymbolDatabase) identified the Xbox library functions in the
game, [Ghidra](https://ghidra-sre.org) with [ghidra-xbe](https://github.com/XboxDev/ghidra-xbe) was used for the
analysis, and the [nxdk](https://github.com/XboxDev/nxdk) headers document the kernel's types.

Developed by Spark198rus with Claude Code.
