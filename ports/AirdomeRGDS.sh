#!/bin/bash

export HOME=/root

SHDIR=$(dirname "$0")
GAMEDIR="$SHDIR/AirdomeRGDS"

cd "$GAMEDIR" || exit 1
chmod +x ./airdomergds 2>/dev/null || true
chmod 666 /dev/tty1 2>/dev/null || true

export XDG_RUNTIME_DIR="/var/run"
export WAYLAND_DISPLAY="wayland-0"
export SDL_VIDEODRIVER="wayland"
export LD_LIBRARY_PATH="/usr/lib:$GAMEDIR/libs:$LD_LIBRARY_PATH"
unset SDL_GAMECONTROLLERCONFIG
unset SDL_GAMECONTROLLERCONFIG_FILE

if [ -x "$GAMEDIR/rgds_volume_helper.py" ]; then
  python3 "$GAMEDIR/rgds_volume_helper.py" "$$" >/dev/null 2>&1 &
fi

LCDSWAP="$(cat /sys/class/anbernic_misc/lcdswap 2>/dev/null)"
if [[ "$LCDSWAP" == "1" ]]; then
  SDL2_SWAP_LCD=1 ./airdomergds --fullscreen 2>&1 | tee "$GAMEDIR/log.txt"
else
  ./airdomergds --fullscreen 2>&1 | tee "$GAMEDIR/log.txt"
fi

systemctl restart oga_events >/dev/null 2>&1 &
printf "\033c" > /dev/tty1 2>/dev/null || true
