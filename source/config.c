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

