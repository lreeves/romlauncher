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
 * Configuration structure for emulators
 * Contains the emulator type and core name (if applicable)
 */
typedef struct {
    Emulator emulator;
    const char *core_name;  // Only used for RetroArch
} EmulatorConfig;

/**
 * Determines which emulator to use based on the file path
 * 
 * @param path The path to the ROM file
 * @return A newly allocated EmulatorConfig structure (must be freed by caller)
 */
EmulatorConfig* derive_emulator_from_path(const char *path);

#endif /* EMULATOR_SELECTION_H */
