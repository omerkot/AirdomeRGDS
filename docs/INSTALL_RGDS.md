# Installing On Anbernic RG DS Linux

These instructions target the Anbernic RG DS official Linux firmware, not Android.

The tested firmware exposes the two physical panels as two SDL/Wayland displays. The port launcher sets the Wayland/SDL environment expected by the stock Linux firmware.

## Build A Package

From the repository root:

```sh
make
make package-rgds
```

The package directory is created at:

```text
dist/rgds/
```

The archive is:

```text
dist/airdomergds-rgds-aarch64.tar.gz
```

## Copy To The Device

Copy both the game directory and launcher script into `/mnt/mmc/Ports`:

```sh
scp -r dist/rgds/AirdomeRGDS dist/rgds/AirdomeRGDS.sh root@<rgds-ip>:/mnt/mmc/Ports/
```

For example:

```sh
scp -r dist/rgds/AirdomeRGDS dist/rgds/AirdomeRGDS.sh root@192.168.1.51:/mnt/mmc/Ports/
```

The resulting layout on the device should be:

```text
/mnt/mmc/Ports/AirdomeRGDS.sh
/mnt/mmc/Ports/AirdomeRGDS/airdomergds
/mnt/mmc/Ports/AirdomeRGDS/rgds_volume_helper.py
/mnt/mmc/Ports/AirdomeRGDS/assets/rgds/*.argb
```

Make sure the launcher and binary are executable:

```sh
ssh root@<rgds-ip> 'chmod +x /mnt/mmc/Ports/AirdomeRGDS.sh /mnt/mmc/Ports/AirdomeRGDS/airdomergds'
```

## Launch

Open the Ports menu on the RG DS and choose `AirdomeRGDS`.

The launcher runs:

```sh
XDG_RUNTIME_DIR=/var/run
WAYLAND_DISPLAY=wayland-0
SDL_VIDEODRIVER=wayland
./airdomergds --fullscreen
```

It also respects the firmware `lcdswap` setting by setting `SDL2_SWAP_LCD=1` when needed.

The launcher intentionally clears inherited SDL controller mapping variables before starting the game. AirdomeRGDS reads the RG DS game buttons directly from the stock Linux input devices and reads the analog stick from `/dev/input/js0`, which keeps the controls independent of PortMaster controller overrides. The bundled `rgds_volume_helper.py` watches the RG DS volume keys and adjusts the ALSA mixer while the game is active.

You can also build, package, and copy in one step:

```sh
make upload-rgds RGDS_HOST=root@<rgds-ip>
```

## Controls

- D-pad or left stick: move
- A: fire
- B: hyperspace
- X/Y: auto-fire
- L/R: speed mode
- SELECT: cycle speed mode upward
- START: pause or restart after game over
- START+R button: quit. The `R` button is the lower-left device button.
- Lower touchscreen: HUD controls

Only the lower touchscreen is known to report touch events on the tested Linux firmware.

## Troubleshooting

If the game does not start, inspect:

```text
/mnt/mmc/Ports/AirdomeRGDS/log.txt
```

Common checks:

- The files are under `/mnt/mmc/Ports`, not the Android storage area.
- `AirdomeRGDS.sh` and `airdomergds` are executable.
- `rgds_volume_helper.py` is present and executable if volume buttons should work in-game.
- The official Linux firmware has SDL2 available as `libSDL2-2.0.so.0`.
- The assets exist under `/mnt/mmc/Ports/AirdomeRGDS/assets/rgds/`.
