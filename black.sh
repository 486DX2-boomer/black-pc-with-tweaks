#!/bin/sh
# Runs BLACK (the PC port) under Wine on Linux.
#
# Put this file next to black.exe and blackrt.dll (and the game folder, see README.md), then:
#   ./black.sh [black.exe arguments]
# Linux paths in the arguments (starting with /) are passed to the port as Wine paths, e.g.
#   ./black.sh /mnt/games/Black/game
#
# Needs Wine with 32-bit support (black.exe is a 32-bit program). The port uses Direct3D 9, d3dcompiler_47, XAudio2
# 2.9 and XInput, which Wine provides. If menus or the world draw wrong, install the native shader compiler into the
# prefix (WINEPREFIX=<prefix> winetricks d3dcompiler_47); DXVK in the prefix runs Direct3D 9 on Vulkan.
#
# Environment (all optional):
#   WINE        the wine to run (default: wine)
#   WINEPREFIX  the Wine prefix (default: wineprefix/ beside this script, made on the first run)
#   WINEDEBUG   Wine's own logging (default: off; the port writes black.log)
set -e

dir=$(cd "$(dirname "$0")" && pwd)
cd "$dir"

WINE=${WINE:-wine}
if ! command -v "$WINE" >/dev/null 2>&1; then
    echo "black.sh: '$WINE' not found: install Wine (with 32-bit support) or set WINE" >&2
    exit 1
fi

# black.exe beside this script, else the project's run folder (work/run, a development checkout)
exe=black.exe
[ -f "$exe" ] || exe=work/run/black.exe
if [ ! -f "$exe" ]; then
    echo "black.sh: black.exe not found next to $dir/black.sh" >&2
    exit 1
fi

export WINEPREFIX="${WINEPREFIX:-$dir/wineprefix}"
export WINEDEBUG="${WINEDEBUG:--all}"
# a new prefix without the .NET (Mono) and HTML (Gecko) install prompts; the port needs neither
export WINEDLLOVERRIDES="${WINEDLLOVERRIDES:-mscoree,mshtml=}"

# Linux paths in the arguments as Wine paths (/mnt/x -> Z:\mnt\x)
for arg; do
    shift
    case $arg in
    /*) arg=$("$WINE" winepath -w "$arg" 2>/dev/null | tr -d '\r') ;;
    esac
    set -- "$@" "$arg"
done

exec "$WINE" "$exe" "$@"
