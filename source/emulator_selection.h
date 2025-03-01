#ifndef EMULATOR_SELECTION_H
#define EMULATOR_SELECTION_H

/**
 * Enum representing the available emulators
 */
typedef enum {
    EMULATOR_RETROARCH,
    EMULATOR_PPSSPP
} Emulator;

/**
 * Enum representing the supported system types
 */
typedef enum {
    SYSTEM_GB,
    SYSTEM_GBA,
    SYSTEM_GBC,
    SYSTEM_NES,
    SYSTEM_SNES,
    SYSTEM_TG16,
    SYSTEM_GENESIS,
    SYSTEM_MASTERSYSTEM,
    SYSTEM_N64,
    SYSTEM_UNKNOWN
} SystemType;

/**
 * Configuration structure for emulators
 * Contains the emulator type and core name (if applicable)
 */
typedef struct {
    Emulator emulator;
    const char *core_name;  // Only used for RetroArch
} EmulatorConfig;

/**
 * Determines the system type based on the file path
 *
 * @param path The path to the ROM file
 * @return The identified system type
 */
SystemType derive_system_from_path(const char *path);

/**
 * Determines which emulator to use based on the file path
 *
 * @param path The path to the ROM file
 * @return Pointer to a static EmulatorConfig structure (do not free)
 */
const EmulatorConfig* derive_emulator_from_path(const char *path);

#endif /* EMULATOR_SELECTION_H */
