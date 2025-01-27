#!/bin/bash

# Variables
IMAGE_NAME="my-switch-builder"
CONTAINER_NAME="switch-build-container"
SOURCE_DIR=$(pwd)
OUTPUT_DIR="${SOURCE_DIR}/output"

rm project.*

if [ -d "build" ]; then
	rm -Rf build
fi

podman build -t "${IMAGE_NAME}" .
echo "Running container to build the project..."
podman run --name "${CONTAINER_NAME}" --rm -v "${SOURCE_DIR}:/usr/src/project:Z" "${IMAGE_NAME}"

if [ $? != 0 ]; then
	echo "Build has failed"
	exit $?
fi

LINE=`ps aux | grep "Ryujinx /home/codedemo/Source/romlauncher/project.nro" | grep -v grep`

if [ $? == 0 ]; then
	PROCESS_ID=`echo $LINE | cut -d ' ' -f 2`

	echo "Killing existing Ryujinx process $PROCESS_ID"
	kill $PROCESS_ID
	sleep 3
fi

echo "Launching Ryujinx"

DOTNET_ROOT=/home/codedemo/Tools/dotnet-sdk-8.0.405 /home/codedemo/Source/ryujinx/build/Ryujinx /home/codedemo/Source/romlauncher/project.nro >/home/codedemo/ryujinx.log

cat /home/codedemo/.config/Ryujinx/sdcard/retrolauncher.log

