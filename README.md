# ROM Launcher

A lightweight and user-friendly frontend for RetroArch and PPSSPP designed for
handheld gaming devices.

![Screenshot of romlauncher](https://github.com/lreeves/romlauncher/blob/main/screenshot.png?raw=true)

## Features

- Clean, intuitive interface for browsing your ROM collection
- Automatic core selection based on file types
- Favorites system for quick access to your most played games
- Box art display when available

## Installation

Grab the latest .nro file from the releases page and put it in the `/switch`
directory on your SD card. This needs a modded Switch to work.

## Usage

1. Place your ROMs in the configured ROM directory ('/roms' on your SD card)
2. Launch the application

That's it! You can add favorites and it'll track a history of games you've
played. Artwork is loaded automatically but you have to manually put PNG files
in `/romlauncher/media/<short system name>/2dboxart`. I'll eventually link a
script that'll fetch all the art.

## Requirements

Obviously you'll need RetroArch installed on your modded Switch.  You'll also
want to use the stable version of RetroArch (tested with 1.20.0) and not the
nightlies which have a crash bug occuring when using RetroAchivements.

## Log files

When things break the log files created are essential in helping troubleshoot
why - they're located in the /romlauncher/ folder on your SD card and only the
5 most recent are kept around.

## Crash dumps

It can be a bit tricky to get stack traces out of the Atmosphere crash dumps.
Generally when I have to do that I enter the podman container used to build the
project like this:

And then use the addr2line utility with the program offsets from the crash log
(found in `/atmosphere/crash_reports/*.log`):

`/opt/devkitpro/devkitA64/bin/aarch64-none-elf-addr2line -e romlauncher.elf -f -p -C -a 0x11f34`

## Contributing

The build.sh script is barebones but will create a working .nro file in a
Podman/Docker container that works on a real Switch or Ryujinx. Note that
RetroArch explodes when running under an emulator so you'll be able to test a
lot on Ryujinx but for actually handing off to the emulator you'll need a real
switch.

Additionally the regular Makefile will build just a standard Linux binary that
let's you iterate and test on the UI and what not without emulation.

There's also unit tests for some of the more annoying string-handling functions
that can be run with "make test".

Probably 95% of the code was written using Aider with Anthropic's sonnet models
so feel free to use this while contributing but any of that code will obviously
still have to be reviewed and tested.

## TODO

- Core selection and overrides - right now I've picked the cores I use mainly
  but obviously the collection isn't complete and the user should be able to
  override cores per system, directory and individual ROM.
- PPSSPP support is in the pipeline.
- For some reason launching games with the pcsx_rearmed core crashes when
  RetroArch launches
- Include or at least link to a script to automatically fetch artwork

## License

This software is provided as-is under open source principles.
