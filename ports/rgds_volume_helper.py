#!/usr/bin/env python3
import os
import re
import select
import struct
import subprocess
import sys
import time

EV_KEY = 1
KEY_VOLUMEDOWN = 114
KEY_VOLUMEUP = 115
EVENTS = ["/dev/input/event4", "/dev/input/event5"]
EVENT_STRUCT = struct.Struct("llHHi")
GET_MIXER = ["amixer", "-c", "0", "cget", "numid=15"]
SET_MIXER = ["amixer", "-c", "0", "cset", "numid=15"]
MIN_VOL = 40
MAX_VOL = 255
STEP = 15


def parent_alive(ppid):
    if ppid <= 1:
        return True
    try:
        os.kill(ppid, 0)
        return True
    except OSError:
        return False


def get_volume():
    out = subprocess.check_output(GET_MIXER, stderr=subprocess.DEVNULL, text=True)
    match = re.search(r": values=(\d+),(\d+)", out)
    return int(match.group(1)) if match else 160


def set_volume(vol):
    vol = max(MIN_VOL, min(MAX_VOL, int(vol)))
    subprocess.run(SET_MIXER + [f"{vol},{vol}"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def adjust(delta):
    try:
        set_volume(get_volume() + delta)
    except Exception:
        pass


def main():
    ppid = int(sys.argv[1]) if len(sys.argv) > 1 else os.getppid()
    fds = []
    for path in EVENTS:
        try:
            fds.append(os.open(path, os.O_RDONLY | os.O_NONBLOCK))
        except OSError:
            pass

    try:
        while fds and parent_alive(ppid):
            readable, _, _ = select.select(fds, [], [], 1.0)
            for fd in readable:
                try:
                    data = os.read(fd, EVENT_STRUCT.size * 16)
                except BlockingIOError:
                    continue
                for off in range(0, len(data) - EVENT_STRUCT.size + 1, EVENT_STRUCT.size):
                    _sec, _usec, ev_type, code, value = EVENT_STRUCT.unpack(data[off:off + EVENT_STRUCT.size])
                    if ev_type != EV_KEY or value not in (1, 2):
                        continue
                    if code == KEY_VOLUMEDOWN:
                        adjust(-STEP)
                    elif code == KEY_VOLUMEUP:
                        adjust(STEP)
    finally:
        for fd in fds:
            try:
                os.close(fd)
            except OSError:
                pass


if __name__ == "__main__":
    main()
