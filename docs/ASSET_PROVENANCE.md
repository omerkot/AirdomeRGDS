# Asset Provenance

This repository should not include copied commercial game art, extracted console assets, proprietary SDK files, or third-party media without a compatible license.

AirdomeRGDS uses project assets prepared for this repository and checked in directly so the build is self-contained.

## Checked-in Visual Assets

`assets/rgds/` contains raw RG DS-ready `.argb` buffers and the title source image:

- Level backgrounds and death-tinted variants
- Title top and bottom screens
- Easy/Normal/Hard HUD backgrounds

The `.argb` files are raw little-endian 32-bit ARGB buffers at 640x480. They are part of the repository’s build inputs, not local build outputs.

`assets/rgds/title.png` is a 640x960 title source image retained as editable project material.

Gameplay sprites such as the cannon, UFO, falling asteroids, bombs, missiles, salvos, and flickering effects are drawn procedurally by the native renderer rather than stored as copied third-party art assets.

## Generated Headers

The following generated headers are also checked in as build inputs:

- `src/generated_backgrounds_rgds.h`
- `src/generated_audio_rgds.h`

They keep the repository buildable from the checked-in files alone.

## Normal Builds

Normal build and package commands are standalone:

```sh
make
make package-rgds
```

All required build inputs live in this repository.

## IP Guidance For Future Changes

- Prefer generated, hand-authored, or clearly licensed assets.
- Document generation prompts, scripts, source files, or licenses when adding assets.
- Do not add screenshots or ripped media from commercial games.
- Do not add Nintendo SDK files, firmware files, ROMs, BIOS images, or proprietary Anbernic firmware files.
- If a new asset is based on third-party material, record the source and license in this document before committing it.
