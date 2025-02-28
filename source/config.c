#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "config.h"
#include "uthash.h"
#include "logging.h"

static config_entry *config = NULL;
config_entry *default_core_mappings = NULL;

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
    FILE *fp = fopen("sdmc:/romlauncher/favorites.txt", "r");
    if (!fp) {
        log_message(LOG_INFO, "No favorites file found");
        return;
    }

    char line[768];
    while (fgets(line, sizeof(line), fp)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0) {
            config_entry *entry = malloc(sizeof(config_entry));
            strncpy(entry->key, line, sizeof(entry->key)-1);
            entry->key[sizeof(entry->key)-1] = '\0';
            entry->value[0] = '1';
            entry->value[1] = '\0';
            HASH_ADD_STR(favorites, key, entry);
            log_message(LOG_DEBUG, "Loaded favorite: %s", line);
        }
    }
    fclose(fp);
}

void save_favorites(void) {
    FILE *fp = fopen(ROMLAUNCHER_DATA_DIRECTORY "/favorites.txt", "w");
    if (!fp) {
        log_message(LOG_ERROR, "Could not open favorites file for writing");
        return;
    }

    config_entry *entry, *tmp;
    HASH_ITER(hh, favorites, entry, tmp) {
        fprintf(fp, "%s\n", entry->key);
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
