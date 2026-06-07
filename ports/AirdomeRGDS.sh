#!/bin/bash

export HOME=/root

if [ -d "/mnt/ports/PortMaster/" ]; then
  controlfolder="/mnt/ports/PortMaster"
elif [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
else
  controlfolder="/mnt/mmc/Ports/PortMaster"
fi

if [ -f "$controlfolder/control.txt" ]; then
  source "$controlfolder/control.txt"
  get_controls
fi

SHDIR=$(dirname "$0")
GAMEDIR="$SHDIR/AirdomeRGDS"

cd "$GAMEDIR" || exit 1
chmod +x ./airdomergds 2>/dev/null || true
chmod 666 /dev/tty1 2>/dev/null || true

export XDG_RUNTIME_DIR="/var/run"
export WAYLAND_DISPLAY="wayland-0"
export SDL_VIDEODRIVER="wayland"
export LD_LIBRARY_PATH="/usr/lib:$GAMEDIR/libs:$LD_LIBRARY_PATH"

LCDSWAP="$(cat /sys/class/anbernic_misc/lcdswap 2>/dev/null)"
if [[ "$LCDSWAP" == "1" ]]; then
  SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig" SDL2_SWAP_LCD=1 ./airdomergds --fullscreen 2>&1 | tee "$GAMEDIR/log.txt"
else
  SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig" ./airdomergds --fullscreen 2>&1 | tee "$GAMEDIR/log.txt"
fi

systemctl restart oga_events >/dev/null 2>&1 &
printf "\033c" > /dev/tty1 2>/dev/null || true
