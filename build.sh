#!/bin/bash

# Variables
IMAGE_NAME="my-switch-builder"
CONTAINER_NAME="switch-build-container"
SOURCE_DIR=$(pwd)
OUTPUT_DIR="${SOURCE_DIR}/output"

rm source/*.o
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

