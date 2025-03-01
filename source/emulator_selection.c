
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emulator_selection.h"

EmulatorConfig* derive_emulator_from_extension(const char *extension);

/**
 * Determines which emulator to use based on the file path
 *
 * @param path The path to the ROM file
 * @return A newly allocated EmulatorConfig structure (must be freed by caller)
 */
EmulatorConfig* derive_emulator_from_path(const char *path) {
    if (!path) return NULL;

    // Extract the file extension from the path
    const char *extension = strrchr(path, '.');
    if (extension) {
        extension++; // Skip the dot
        EmulatorConfig* from_extension = derive_emulator_from_extension(extension);
        if (from_extension) return from_extension;
    }

    return NULL;
}

EmulatorConfig* derive_emulator_from_extension(const char *extension) {
    if (!extension) return NULL;
    
    // Check for Super Famicom ROMs
    if (strcasecmp(extension, "sfc") == 0) {
        EmulatorConfig *config = malloc(sizeof(EmulatorConfig));
        if (!config) {
            fprintf(stderr, "Failed to allocate memory for EmulatorConfig\n");
            return NULL;
        }
        
        config->emulator = EMULATOR_RETROARCH;
        config->core_name = "snes9x";
        return config;
    }
    
    return NULL;
}
