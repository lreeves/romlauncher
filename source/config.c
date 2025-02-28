#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "config.h"
#include "uthash.h"
#include "logging.h"
#include "path_utils.h"

static config_entry *config = NULL;
config_entry *default_core_mappings = NULL;
config_entry *system_names_mappings = NULL;

void config_put(const char *key, const char *value) {
    config_entry *entry;
    HASH_FIND_STR(config, key, entry);

    if (entry == NULL) {
        entry = malloc(sizeof(config_entry));
        strncpy(entry->key, key, sizeof(entry->key)-1);
        entry->key[sizeof(entry->key)-1] = '\0';
        HASH_ADD_STR(config, key, entry);
    }

    strncpy(entry->value, value, sizeof(entry->value)-1);
    entry->value[sizeof(entry->value)-1] = '\0';
}

const char* config_get(const char *key) {
    config_entry *entry;
    HASH_FIND_STR(config, key, entry);
    return entry ? entry->value : NULL;
}

void load_config(void) {
    FILE *fp = fopen("sdmc:/romlauncher/romlauncher.ini", "r");
    if (!fp) {
        log_message(LOG_ERROR, "Could not open config file");
        return;
    }

    char line[768];
    while (fgets(line, sizeof(line), fp)) {
        // Skip empty lines and comments
        if (line[0] == '\n' || line[0] == '#' || line[0] == ';')
            continue;

        // Remove newline
        line[strcspn(line, "\n")] = 0;

        // Find equals sign
        char *equals = strchr(line, '=');
        if (!equals) continue;

        // Split into key/value
        *equals = '\0';
        char *key = line;
        char *value = equals + 1;

        // Trim whitespace
        while (*key && isspace((unsigned char)*key)) key++;
        char *end = key + strlen(key) - 1;
        while (end > key && isspace((unsigned char)*end)) *end-- = '\0';

        while (*value && isspace((unsigned char)*value)) value++;
        end = value + strlen(value) - 1;
        while (end > value && isspace((unsigned char)*end)) *end-- = '\0';

        if (*key && *value) {
            config_put(key, value);
            log_message(LOG_DEBUG, "Loaded config: %s = %s", key, value);
        }
    }

    fclose(fp);
}

void free_config(void) {
    config_entry *current, *tmp;
    HASH_ITER(hh, config, current, tmp) {
        HASH_DEL(config, current);
        free(current);
    }
}

// Favorites management
config_entry *favorites = NULL;

void load_favorites(void) {
    char favorites_path[256];
    snprintf(favorites_path, sizeof(favorites_path), "%sfavorites.txt", ROMLAUNCHER_DATA_DIRECTORY);
    
    log_message(LOG_INFO, "Trying to load favorites from: %s", favorites_path);
    
    // Check if file exists first
    struct stat file_stat;
    if (stat(favorites_path, &file_stat) != 0) {
        log_message(LOG_INFO, "No favorites file found (stat check failed)");
    } else {
        log_message(LOG_INFO, "Favorites file exists, size: %ld bytes", (long)file_stat.st_size);
    }
    
    FILE *fp = fopen(favorites_path, "r");
    if (!fp) {
        log_message(LOG_INFO, "No favorites file found (fopen failed: %s)", strerror(errno));
        return;
    }

    char line[768];
    while (fgets(line, sizeof(line), fp)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0) {
            // Convert relative path to absolute path
            char* absolute_path = relative_rom_path_to_absolute(line);
            if (!absolute_path) {
                log_message(LOG_ERROR, "Failed to convert favorite path to absolute: %s", line);
                continue;
            }
            
            config_entry *entry = malloc(sizeof(config_entry));
            if (!entry) {
                free(absolute_path);
                log_message(LOG_ERROR, "Failed to allocate memory for favorite entry");
                continue;
            }
            
            strncpy(entry->key, absolute_path, sizeof(entry->key)-1);
            entry->key[sizeof(entry->key)-1] = '\0';
            entry->value[0] = '1';
            entry->value[1] = '\0';
            HASH_ADD_STR(favorites, key, entry);
            log_message(LOG_DEBUG, "Loaded favorite: %s (absolute: %s)", line, absolute_path);
            
            free(absolute_path);
        }
    }
    fclose(fp);
}

void save_favorites(void) {
    char favorites_path[256];
    snprintf(favorites_path, sizeof(favorites_path), "%sfavorites.txt", ROMLAUNCHER_DATA_DIRECTORY);
    
    log_message(LOG_INFO, "Saving favorites to: %s", favorites_path);
    
    FILE *fp = fopen(favorites_path, "w");
    if (!fp) {
        log_message(LOG_ERROR, "Could not open favorites file for writing: %s", strerror(errno));
        return;
    }

    config_entry *entry, *tmp;
    HASH_ITER(hh, favorites, entry, tmp) {
        char* relative_path = absolute_rom_path_to_relative(entry->key);
        if (relative_path) {
            fprintf(fp, "%s\n", relative_path);
            free(relative_path);
        } else {
            // Fallback to original path if conversion fails
            fprintf(fp, "%s\n", entry->key);
            log_message(LOG_ERROR, "Failed to convert favorite path to relative: %s", entry->key);
        }
    }
    fclose(fp);
}

int is_favorite(const char *path) {
    config_entry *entry;
    HASH_FIND_STR(favorites, path, entry);
    return entry != NULL;
}

void toggle_favorite(const char *path) {
    config_entry *entry;
    HASH_FIND_STR(favorites, path, entry);

    if (entry) {
        HASH_DEL(favorites, entry);
        free(entry);
        log_message(LOG_INFO, "Removed favorite: %s", path);
    } else {
        entry = malloc(sizeof(config_entry));
        strncpy(entry->key, path, sizeof(entry->key)-1);
        entry->key[sizeof(entry->key)-1] = '\0';
        entry->value[0] = '1';
        entry->value[1] = '\0';
        HASH_ADD_STR(favorites, key, entry);
        log_message(LOG_INFO, "Added favorite: %s", path);
    }
    save_favorites();
}

void free_favorites(void) {
    config_entry *current, *tmp;
    HASH_ITER(hh, favorites, current, tmp) {
        HASH_DEL(favorites, current);
        free(current);
    }
}

// Helper function to add a mapping to the default cores hashmap
static void add_default_core(const char *extension, const char *core) {
    config_entry *entry = malloc(sizeof(config_entry));
    if (!entry) {
        log_message(LOG_ERROR, "Failed to allocate memory for core mapping");
        return;
    }
    
    strncpy(entry->key, extension, sizeof(entry->key) - 1);
    entry->key[sizeof(entry->key) - 1] = '\0';
    
    strncpy(entry->value, core, sizeof(entry->value) - 1);
    entry->value[sizeof(entry->value) - 1] = '\0';
    
    HASH_ADD_STR(default_core_mappings, key, entry);
}

void init_default_core_mappings(void) {
    // Initialize with NULL first to ensure clean state
    default_core_mappings = NULL;
    
    // Add all the default mappings
    add_default_core("gb", "gambatte");
    add_default_core("gbc", "gambatte");
    add_default_core("gba", "mgba");
    add_default_core("md", "genesis_plus_gx");
    add_default_core("nes", "fceumm");
    add_default_core("n64", "mupen64plus_next");
    add_default_core("pce", "mednafen_pce");
    add_default_core("sfc", "snes9x");
    add_default_core("sms", "genesis_plus_gx");
    add_default_core("z64", "mupen64plus_next");
}

void free_default_core_mappings(void) {
    config_entry *current, *tmp;
    HASH_ITER(hh, default_core_mappings, current, tmp) {
        HASH_DEL(default_core_mappings, current);
        free(current);
    }
}

// Helper function to add a mapping to the system names hashmap
static void add_system_name(const char *short_name, const char *full_name) {
    config_entry *entry = malloc(sizeof(config_entry));
    if (!entry) {
        log_message(LOG_ERROR, "Failed to allocate memory for system name mapping");
        return;
    }
    
    strncpy(entry->key, short_name, sizeof(entry->key) - 1);
    entry->key[sizeof(entry->key) - 1] = '\0';
    
    strncpy(entry->value, full_name, sizeof(entry->value) - 1);
    entry->value[sizeof(entry->value) - 1] = '\0';
    
    HASH_ADD_STR(system_names_mappings, key, entry);
}

void init_system_names_mappings(void) {
    // Initialize with NULL first to ensure clean state
    system_names_mappings = NULL;
    
    // Add all the system name mappings
    add_system_name("nes", "Nintendo Entertainment System");
    add_system_name("snes", "Super Nintendo Entertainment System");
    add_system_name("sfc", "Super Famicom");
    add_system_name("gb", "Game Boy");
    add_system_name("gbc", "Game Boy Color");
    add_system_name("gba", "Game Boy Advance");
    add_system_name("n64", "Nintendo 64");
    add_system_name("md", "Sega Mega Drive");
    add_system_name("genesis", "Sega Genesis");
    add_system_name("sms", "Sega Master System");
    add_system_name("gg", "Game Gear");
    add_system_name("32x", "Sega 32X");
    add_system_name("psx", "PlayStation");
    add_system_name("ps2", "PlayStation 2");
    add_system_name("psp", "PlayStation Portable");
    add_system_name("tg16", "TurboGrafx-16");
    add_system_name("pce", "PC Engine");
    add_system_name("ngp", "Neo Geo Pocket");
    add_system_name("ngpc", "Neo Geo Pocket Color");
    add_system_name("ws", "WonderSwan");
    add_system_name("wsc", "WonderSwan Color");
    add_system_name("arcade", "Arcade");
    add_system_name("mame", "MAME");
    add_system_name("fba", "Final Burn Alpha");
    add_system_name("ngcd", "Neo Geo CD");
    add_system_name("atari2600", "Atari 2600");
    add_system_name("lynx", "Atari Lynx");
    add_system_name("jaguar", "Atari Jaguar");
    add_system_name("segacd", "Sega CD");
    add_system_name("saturn", "Sega Saturn");
    add_system_name("dreamcast", "Sega Dreamcast");
    add_system_name("coleco", "ColecoVision");
    add_system_name("intellivision", "Intellivision");
    add_system_name("vectrex", "Vectrex");
    add_system_name("pc", "DOS/PC");
    add_system_name("amiga", "Commodore Amiga");
    add_system_name("c64", "Commodore 64");
    add_system_name("z64", "Nintendo 64 (z64)");
}

void free_system_names_mappings(void) {
    config_entry *current, *tmp;
    HASH_ITER(hh, system_names_mappings, current, tmp) {
        HASH_DEL(system_names_mappings, current);
        free(current);
    }
}

// Get the full name of a system from its short name
const char* get_system_full_name(const char *short_name) {
    if (!short_name) return "Unknown System";
    
    config_entry *entry;
    HASH_FIND_STR(system_names_mappings, short_name, entry);
    return entry ? entry->value : short_name; // Return the short name if not found
}
