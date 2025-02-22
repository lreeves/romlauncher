#!/bin/bash

# Variables
IMAGE_NAME="my-switch-builder"
CONTAINER_NAME="switch-build-container"
SOURCE_DIR=$(pwd)
OUTPUT_DIR="${SOURCE_DIR}/output"

[ -f romlauncher.nro ] && rm romlauncher.nro
[ -f romlauncher.nacp ] && rm romlauncher.nacp
[ -f romlauncher.elf ] && rm romlauncher.elf

[ -d "build" ] && rm -Rf build

podman build -t "${IMAGE_NAME}" .
echo "Running container to build the project..."
podman run --name "${CONTAINER_NAME}" --rm -v "${SOURCE_DIR}:/usr/src/project:Z" "${IMAGE_NAME}"

if [ $? != 0 ]; then
	echo "Build has failed"
	exit $?
fi

LINE=`ps aux | grep "Ryujinx /home/luke/Source/romlauncher/romlauncher.nro" | grep -v grep`

if [ $? == 0 ]; then
	PROCESS_ID=`echo $LINE | cut -d ' ' -f 2`

	echo "Killing existing Ryujinx process $PROCESS_ID"
	kill $PROCESS_ID
	sleep 3
fi

if [ "$1" == "--skip-emulator" ]; then
  exit 0
fi

if [ ! -f "/home/luke/Source/ryujinx/build/Ryujinx" ]; then
  echo "Emulator source build not found, exiting"
  exit 0
fi

echo "Launching Ryujinx"

#if [ ! -e ~/.config/Ryujinx/sdcard/romlauncher.ini ]; then
#  echo "Installing default config"
#  cp romlauncher.ini ~/.config/Ryujinx/sdcard/romlauncher.ini
#fi

DOTNET_ROOT=~/Tools/dotnet-90-200 /home/luke/Source/ryujinx/build/Ryujinx /home/luke/Source/romlauncher/romlauncher.nro &>/home/luke/ryujinx.log

cat /home/luke/.config/Ryujinx/sdcard/retrolauncher.log | grep -v DEBUG

