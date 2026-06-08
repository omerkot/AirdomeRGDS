# Compatibility Notes

## Target Firmware

AirdomeRGDS targets the Anbernic RG DS official Linux firmware. It does not target Android.

The tested firmware exposes two SDL video displays at 640x480 and uses Wayland:

```text
XDG_RUNTIME_DIR=/var/run
WAYLAND_DISPLAY=wayland-0
SDL_VIDEODRIVER=wayland
```

## Display Handling

The game opens one fullscreen SDL window per display. If the firmware reports an LCD swap through:

```text
/sys/class/anbernic_misc/lcdswap
```

the launcher sets:

```text
SDL2_SWAP_LCD=1
```

## Touch Handling

Only the lower touchscreen is known to report touch events on tested firmware. Upper-screen touch has not been observed through SDL.

The lower touchscreen is mapped into the lower HUD coordinate system.

## Input Handling

The stock RG DS Linux firmware exposes the game controls through SDL and Linux input devices. To avoid conflicts with PortMaster controller overrides, AirdomeRGDS does not rely on inherited `SDL_GAMECONTROLLERCONFIG` values from the launcher.

On the tested device, the game reads RG DS buttons directly from `/dev/input/event4` and `/dev/input/event5`, and reads analog stick movement from `/dev/input/js0`. SDL controller input is still initialized for display/audio integration and as a fallback path, but direct device input is preferred when available.

## Audio

Audio uses SDL's callback API and generated embedded PCM samples. The falling bomb tone uses the original discrete note ladder rather than interpolated pitch steps, because interpolated timer changes sounded crackly on device.

On the tested firmware, the loader exposes its logical volume level at:

```text
/sys/class/anbernic_misc/openbor_volume
```

AirdomeRGDS scales its mixed SDL audio to that loader volume level at startup and while running. The packaged launcher starts `rgds_volume_helper.py`, which watches the RG DS volume key events and updates the same loader volume value.

The in-game gain curve is tuned for finer low-volume control: loader levels 0-4 map to 0%, 2.5%, 5%, 7.5%, and 10% gain, then levels 5-10 rise linearly to 100%.

## Quit

On the tested RG DS Linux firmware, START+R button quits the game. The `R` button is the lower-left device button.

START+SELECT is not documented as an RG DS quit combo because it did not exit the game on the tested device.
