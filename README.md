# ROM Launcher

A lightweight and user-friendly frontend for RetroArch and PPSSPP designed for handheld gaming devices.

## Features

- Clean, intuitive interface for browsing your ROM collection
- Automatic core selection based on file types
- Favorites system for quick access to your most played games
- Box art display when available

## Usage

1. Place your ROMs in the configured ROM directory ('/roms' on your SD card)
2. Launch the application
3. Browse your collection and select a game to play
4. Press the designated button to add/remove favorites

## Requirements

Obviously you'll need RetroArch installed on your modded Switch.  You'll also
want to use the stable version of RetroArch (tested with 1.20.0) and not the
nightlies which have a crash bug occuring when using RetroAchivements.

## Contributing

The build.sh script is awful but will create a working .nro file in a
Podman/Docker container that works on a real Switch or Ryujinx. Note that
RetroArch explodes when running under an emulator so you'll be able to test a
lot on Ryujinx but for actually handing off to the emulator you'll need a real
switch.

Additionally the regular Makefile will build just a standard Linux binary that
let's you iterate and test on the UI and what not without emulation.

## TODO

PPSSPP support is in the pipeline.

## License

This software is provided as-is under open source principles.
