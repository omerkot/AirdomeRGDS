# Assets

`assets/rgds/` contains the RG DS-ready visual assets packaged with the port.

The `.argb` files are raw little-endian 32-bit ARGB buffers at 640x480:

- `level_1.argb` through `level_6.argb`
- `level_1_death.argb` through `level_6_death.argb`
- `title_top.argb`
- `title_bottom.argb`
- `hud_easy.argb`
- `hud_normal.argb`
- `hud_hard.argb`

`title.png` is the retained 640x960 title source image corresponding to `title_top.argb` and `title_bottom.argb`.

The raw assets are checked in so normal builds and packaging are self-contained.

For provenance notes, see `../docs/ASSET_PROVENANCE.md`.
