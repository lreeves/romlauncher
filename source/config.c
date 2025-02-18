#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "config.h"
#include "uthash.h"
#include "logging.h"

// Config hash table structure
typedef struct {
    char key[256];           // key string
    char value[512];         // value string
    UT_hash_handle hh;       // makes this structure hashable
} config_entry;

static config_entry *config = NULL;

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
    FILE *fp = fopen("sdmc:/romlauncher.ini", "r");
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
        while (*key && isspace(*key)) key++;
        char *end = key + strlen(key) - 1;
        while (end > key && isspace(*end)) *end-- = '\0';

        while (*value && isspace(*value)) value++;
        end = value + strlen(value) - 1;
        while (end > value && isspace(*end)) *end-- = '\0';

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
