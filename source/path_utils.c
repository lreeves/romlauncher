#include <string.h>
#include <stdlib.h>
#include "path_utils.h"
#include "config.h"
#include "logging.h"

/**
 * Converts an absolute ROM path to a relative path by removing ROM_DIRECTORY prefix.
 * Caller is responsible for freeing the returned string.
 */
char* absolute_rom_path_to_relative(char* path) {
    if (!path) return NULL;

    // Check if path starts with ROM_DIRECTORY
    if (strncmp(path, ROM_DIRECTORY, strlen(ROM_DIRECTORY)) == 0) {
        // Skip the ROM directory prefix
        char* result = strdup(path + strlen(ROM_DIRECTORY));
        
        // Remove leading slash if present to make it fully relative
        if (result && result[0] == '/') {
            char* fully_relative = strdup(result + 1);
            free(result);
            result = fully_relative;
        }

        return result;
    }
    
    // Path is already relative or has a different format
    return strdup(path);
}

/**
 * Converts a relative ROM path to an absolute path by adding ROM_DIRECTORY prefix.
 * Caller is responsible for freeing the returned string.
 */
char* relative_rom_path_to_absolute(char* path) {
    if (!path) return NULL;
    
    // Check if path already starts with ROM_DIRECTORY
    if (strncmp(path, ROM_DIRECTORY, strlen(ROM_DIRECTORY)) == 0) {
        // Already an absolute path
        return strdup(path);
    }
    
    // Add ROM_DIRECTORY prefix
    char* result = malloc(strlen(ROM_DIRECTORY) + strlen(path) + 1);
    if (!result) {
        log_message(LOG_ERROR, "Failed to allocate memory for absolute path");
        return NULL;
    }
    
    strcpy(result, ROM_DIRECTORY);
    strcat(result, path);
    
    return result;
}
