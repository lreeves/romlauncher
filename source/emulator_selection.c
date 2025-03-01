
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emulator_selection.h"

// Static EmulatorConfig definitions
static const EmulatorConfig RETROARCH_SNES9X = {
    .emulator = EMULATOR_RETROARCH,
    .core_name = "snes9x"
};

// Forward declaration
const EmulatorConfig* derive_emulator_from_extension(const char *extension);

/**
 * Determines which emulator to use based on the file path
 *
 * @param path The path to the ROM file
 * @return Pointer to a static EmulatorConfig structure (do not free)
 */
const EmulatorConfig* derive_emulator_from_path(const char *path) {
    if (!path) return NULL;

    // Extract the file extension from the path
    const char *extension = strrchr(path, '.');
    if (extension) {
        extension++; // Skip the dot
        return derive_emulator_from_extension(extension);
    }

    return NULL;
}

/**
 * Determines which emulator to use based on the file extension
 *
 * @param extension The file extension (without the dot)
 * @return Pointer to a static EmulatorConfig structure (do not free)
 */
const EmulatorConfig* derive_emulator_from_extension(const char *extension) {
    if (!extension) return NULL;

    if (strcasecmp(extension, "sfc") == 0) return &RETROARCH_SNES9X;

    return NULL;
}
