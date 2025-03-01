
#include <stdio.h>
#include <stdlib.h>

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
