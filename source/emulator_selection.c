
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

static const EmulatorConfig RETROARCH_GENESIS_PLUS_GX = {
    .emulator = EMULATOR_RETROARCH,
    .core_name = "genesis_plus_gx"
};

static const EmulatorConfig RETROARCH_GAMBATTE = {
    .emulator = EMULATOR_RETROARCH,
    .core_name = "gambatte"
};

static const EmulatorConfig RETROARCH_MGBA = {
    .emulator = EMULATOR_RETROARCH,
    .core_name = "mgba"
};

static const EmulatorConfig RETROARCH_MUPEN64_PLUS_NEXT = {
    .emulator = EMULATOR_RETROARCH,
    .core_name = "mupen64plus_next"
};

static const EmulatorConfig RETROARCH_MEDNAFEN_PCE = {
    .emulator = EMULATOR_RETROARCH,
    .core_name = "mednafen_pce"
};

static const EmulatorConfig RETROARCH_PCSX_REARMED = {
    .emulator = EMULATOR_RETROARCH,
    .core_name = "pcsx_rearmed"
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
    if (strcasecmp(extension, "gba") == 0) return SYSTEM_GBA;
    if (strcasecmp(extension, "gbc") == 0) return SYSTEM_GBC;
    if (strcasecmp(extension, "gb") == 0) return SYSTEM_GB;
    if (strcasecmp(extension, "n64") == 0) return SYSTEM_N64;
    if (strcasecmp(extension, "z64") == 0) return SYSTEM_N64;
    if (strcasecmp(extension, "pce") == 0) return SYSTEM_TG16;

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
    if(system == SYSTEM_UNKNOWN) {
        if (strcasecmp(path, "playstation portable")) system = SYSTEM_PSP;
        if (strcasecmp(path, "playstation")) system = SYSTEM_PSX;
    }

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
        case SYSTEM_GB:
            return &RETROARCH_GAMBATTE;
        case SYSTEM_GBC:
            return &RETROARCH_GAMBATTE;
        case SYSTEM_GBA:
            return &RETROARCH_MGBA;
        case SYSTEM_NES:
            return &RETROARCH_FCEUMM;
        case SYSTEM_SNES:
            return &RETROARCH_SNES9X;
        case SYSTEM_GENESIS:
            return &RETROARCH_GENESIS_PLUS_GX;
        case SYSTEM_N64:
            return &RETROARCH_MUPEN64_PLUS_NEXT;
        case SYSTEM_TG16:
            return &RETROARCH_MEDNAFEN_PCE;
        case SYSTEM_PSX:
            return &RETROARCH_PCSX_REARMED;
        default:
            return NULL;
    }
}
