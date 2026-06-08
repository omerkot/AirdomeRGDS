#!/usr/bin/env python3
import os
import select
import struct
import sys

EV_KEY = 1
KEY_VOLUMEDOWN = 114
KEY_VOLUMEUP = 115
EVENTS = ["/dev/input/event4", "/dev/input/event5"]
EVENT_STRUCT = struct.Struct("llHHi")
LOADER_VOLUME = "/sys/class/anbernic_misc/openbor_volume"
MIN_LEVEL = 0
MAX_LEVEL = 10


def clamp_level(level):
    return max(MIN_LEVEL, min(MAX_LEVEL, int(level)))


def parent_alive(ppid):
    if ppid <= 1:
        return True
    try:
        os.kill(ppid, 0)
        return True
    except OSError:
        return False


def get_level():
    try:
        with open(LOADER_VOLUME, "r") as fh:
            return clamp_level(fh.read().strip() or 0)
    except (OSError, ValueError):
        return MAX_LEVEL


def set_level(level):
    level = clamp_level(level)
    try:
        with open(LOADER_VOLUME, "w") as fh:
            fh.write(f"{level}\n")
    except OSError:
        pass
    return level


def main():
    ppid = int(sys.argv[1]) if len(sys.argv) > 1 else os.getppid()
    current_level = get_level()
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
                    if ev_type != EV_KEY or value != 1:
                        continue
                    if code == KEY_VOLUMEDOWN:
                        current_level = set_level(current_level - 1)
                    elif code == KEY_VOLUMEUP:
                        current_level = set_level(current_level + 1)
    finally:
        for fd in fds:
            try:
                os.close(fd)
            except OSError:
                pass


if __name__ == "__main__":
    main()
