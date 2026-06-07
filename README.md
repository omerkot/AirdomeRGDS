# AirdomeRGDS

AirdomeRGDS is a native Linux fixed shooter for the Anbernic RG DS running the official Linux firmware. It is inspired by classic falling-object arcade defense games.

The game runs directly through SDL2 on the RG DS Linux environment, opens both physical displays, and renders the playfield and lower HUD as native 640x480 panels.

## Status

- Target device: Anbernic RG DS official Linux firmware.
- Display: two SDL/Wayland displays at 640x480 each.
- Controls: RG DS buttons, d-pad, left stick, and lower touchscreen.
- Audio: SDL audio with generated/embedded PCM samples.
- Packaging: Port-style folder under `/mnt/mmc/Ports`.

The upper digitizer has not been exposed by the tested Linux firmware. Lower-screen touch works and is used for HUD controls.

## Controls

- D-pad / left stick: move the cannon
- A: fire
- B: hyperspace
- X or Y: toggle auto-fire
- L/R: decrease/increase speed mode
- START: pause, or restart after game over
- START+R button: quit on RG DS firmware. The `R` button is the lower-left device button.
- Lower touchscreen: lower-screen HUD controls

Any button or lower-screen touch starts from the title screen. A title-screen touch is consumed only as a start action, so it will not also change difficulty after gameplay begins.

## Gameplay

- Large and small asteroids, with large asteroids sometimes splitting
- Large and small rotating bombs, which cost a life if they reach the ground
- Guided missiles that may track and slide along the ground
- UFOs from level 4 onward, firing salvos at the cannon
- Six score bands with level multiplier, background changes, speed changes, and point changes
- Extra cannon each time peak score crosses another 1,000 points
- Auto-fire and hyperspace relocation

Base scoring before the level multiplier:

- Big asteroid: 10
- Small asteroid: 20
- Big rotating bomb: 40
- Small rotating bomb: 80
- Guided missile: 50
- UFO: 100
- Big asteroid miss: -5
- Small asteroid miss: -10
- Cannon lost: -100

## Install On RG DS

See [docs/INSTALL_RGDS.md](docs/INSTALL_RGDS.md) for the full installation flow.

Short version:

```sh
make package-rgds
scp -r dist/rgds/AirdomeRGDS dist/rgds/AirdomeRGDS.sh root@<rgds-ip>:/mnt/mmc/Ports/
```

Then launch `AirdomeRGDS` from the Ports menu on the device.

## Build And Package

The repository is configured for RG DS builds only. The default target cross-compiles the ARM64 Linux binary:

```sh
make
```

Create the copy-ready Ports package:

```sh
make package-rgds
```

Optional upload:

```sh
make upload-rgds RGDS_HOST=root@<rgds-ip>
```

This repository includes all RG DS assets and generated audio/background headers needed for normal builds.

For asset provenance and IP guidance, see [docs/ASSET_PROVENANCE.md](docs/ASSET_PROVENANCE.md).

## Repository Notes

- `src/` contains the SDL2 native port and generated headers.
- `assets/rgds/` contains RG DS-ready 640x480 raw ARGB assets.
- `ports/AirdomeRGDS.sh` is the launcher script copied to `/mnt/mmc/Ports`.
- `dist/`, `build/`, and `.deps/` are local outputs and are ignored.

## Legal And IP Notice

AirdomeRGDS is an unofficial homebrew project. It is not affiliated with, endorsed by, sponsored by, or approved by Anbernic, Nintendo, Mattel, Intellivision, or any classic falling-object arcade defense game developer.

No commercial ROM, copyrighted game asset, Nintendo SDK file, Mattel artwork, Intellivision artwork, or third-party arcade game asset should be included in this repository.

See [NOTICE.md](NOTICE.md) and [docs/ASSET_PROVENANCE.md](docs/ASSET_PROVENANCE.md) for details.
