
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emulator_selection.h"

// Static EmulatorConfig definitions
static const EmulatorConfig RETROARCH_SNES9X = {
    .emulator = EMULATOR_RETROARCH,
    .core_name = "snes9x"
};

static const EmulatorConfig RETROARCH_FCEUMM = {
    .emulator = EMULATOR_RETROARCH,
    .core_name = "fceumm"
};

static const EmulatorConfig RETROARCH_GENESIS = {
    .emulator = EMULATOR_RETROARCH,
    .core_name = "genesis_plus_gx"
};

// Forward declarations
SystemType derive_system_from_extension(const char *extension);
const EmulatorConfig* derive_emulator_for_system(SystemType system);

/**
 * Determines the system type based on the file path
 *
 * @param path The path to the ROM file
 * @return The identified system type
 */
SystemType derive_system_from_path(const char *path) {
    if (!path) return SYSTEM_UNKNOWN;

    // Extract the file extension from the path
    const char *extension = strrchr(path, '.');
    if (extension) {
        extension++; // Skip the dot
        return derive_system_from_extension(extension);
    }

    return SYSTEM_UNKNOWN;
}

/**
 * Determines the system type based on the file extension
 *
 * @param extension The file extension (without the dot)
 * @return The identified system type
 */
SystemType derive_system_from_extension(const char *extension) {
    if (!extension) return SYSTEM_UNKNOWN;

    if (strcasecmp(extension, "nes") == 0) return SYSTEM_NES;
    if (strcasecmp(extension, "sfc") == 0) return SYSTEM_SNES;
    if (strcasecmp(extension, "md") == 0) return SYSTEM_GENESIS;

    return SYSTEM_UNKNOWN;
}

/**
 * Determines which emulator to use based on the file path. This should be
 * the only entrypoint into this logic.
 *
 * @param path The path to the ROM file
 * @return Pointer to a static EmulatorConfig structure (do not free)
 */
const EmulatorConfig* derive_emulator_from_path(const char *path) {
    SystemType system = derive_system_from_path(path);
    return derive_emulator_for_system(system);
}

/**
 * Determines which emulator to use based on the system type
 *
 * @param system The system type
 * @return Pointer to a static EmulatorConfig structure (do not free)
 */
const EmulatorConfig* derive_emulator_for_system(SystemType system) {
    switch (system) {
        case SYSTEM_NES:
            return &RETROARCH_FCEUMM;
        case SYSTEM_SNES:
            return &RETROARCH_SNES9X;
        case SYSTEM_GENESIS:
            return &RETROARCH_GENESIS;
        default:
            return NULL;
    }
}
