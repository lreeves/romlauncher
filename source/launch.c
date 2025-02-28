#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "logging.h"
#include "config.h"
#include "launch.h"
#include "history.h"

#ifndef ROMLAUNCHER_BUILD_LINUX
#include <switch.h>
#endif

// Helper function to get file extension
const char* get_file_extension(const char* filename) {
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot + 1;
}

int launch_retroarch(const char* rom_path) {
    log_message(LOG_INFO, "Launch request for ROM: %s", rom_path);

    // Add to history before launching
    add_history_entry(rom_path, time(NULL));
    save_history();

    // Get file extension and look up core
    const char* ext = get_file_extension(rom_path);
    log_message(LOG_INFO, "ROM extension: %s", ext);

    config_entry *core_entry;
    HASH_FIND_STR(default_core_mappings, ext, core_entry);

    if (!core_entry) {
        log_message(LOG_ERROR, "No core mapping found for extension: %s", ext);
        return 0;
    }

    log_message(LOG_INFO, "Found core mapping: %s -> %s", ext, core_entry->value);

    // Construct core path
    char core_path[MAX_PATH_LEN];
    int written = snprintf(core_path, sizeof(core_path), "sdmc:/retroarch/cores/%s_libretro_libnx.nro", core_entry->value);
    if (written < 0 || (size_t)written >= sizeof(core_path)) {
        log_message(LOG_ERROR, "Core path construction failed (truncation or error)");
        return 0;
    }

    // Check if core exists
    FILE *core_file = fopen(core_path, "r");
    if (core_file == NULL) {
        log_message(LOG_ERROR, "Core not found at: %s", core_path);
        return 0;
    }
    fclose(core_file);

    // Skip the "sdmc:" prefix if present for the arguments
    const char* rom_arg = strncmp(rom_path, "sdmc:", 5) == 0 ? rom_path + 5 : rom_path;

    // Construct the arguments string
    char full_arguments[MAX_PATH_LEN];
    written = snprintf(full_arguments, sizeof(full_arguments), "%s \"%s\"", core_path, rom_arg);
    if (written < 0 || (size_t)written >= sizeof(full_arguments)) {
        log_message(LOG_ERROR, "Arguments string construction failed (truncation or error)");
        return 0;
    }

    // Log launch details
    log_message(LOG_INFO, "Launching RetroArch: %s with args: %s", core_path, full_arguments);

#ifdef ROMLAUNCHER_BUILD_LINUX
    log_message(LOG_INFO, "Skipping actual launch since we're not in a Switch environment");
    return 0;
#else
    // Launch RetroArch with the selected ROM
    Result rc = envSetNextLoad(core_path, full_arguments);
    if (R_SUCCEEDED(rc)) {
        log_message(LOG_INFO, "Successfully set next load");
        return 1;
    } else {
        log_message(LOG_ERROR, "Failed to set next load, error: %x", rc);
        return 0;
    }
#endif
}
