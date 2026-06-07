# Notice

AirdomeRGDS is an unofficial homebrew project for the Anbernic RG DS Linux firmware.

It is not affiliated with, endorsed by, sponsored by, or approved by Anbernic, Nintendo, Mattel, Intellivision, or any classic falling-object arcade defense game developer.

The project is intended for homebrew development, learning, and preservation of the source code.

No commercial ROM, copyrighted game asset, Nintendo SDK file, Mattel artwork, Intellivision artwork, or third-party arcade game asset is included in this repository.

## Assets

The checked-in RG DS visual assets are project assets prepared for AirdomeRGDS. They are provided so the project can build as a standalone repository.

Do not replace these assets with copied proprietary artwork, screenshots from commercial games, extracted console assets, or third-party media unless that material has a clearly compatible license and is documented.

Audio data is generated project material embedded in `src/generated_audio_rgds.h`.

See `docs/ASSET_PROVENANCE.md` for asset-generation details.

## Third-party Software

This project uses SDL2 at runtime on Linux.

The RG DS cross-build uses Zig as a compiler driver in the local development environment. The repository does not include Zig, SDL2, or the RG DS Linux firmware.

The source code and original assets in this repository are licensed under the MIT License, unless otherwise stated. Third-party tools and libraries used to build or run the project remain under their own licenses.
