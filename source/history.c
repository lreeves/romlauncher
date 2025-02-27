#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "history.h"
#include "logging.h"
#include "config.h"
#include "uthash.h"

#define MAX_HISTORY_ENTRIES 25

// Structure to store history entries
typedef struct {
    char path[MAX_PATH_LEN];
    time_t timestamp;
    UT_hash_handle hh;
} history_entry;

// Global history hash table
static history_entry *history = NULL;

// Array to store sorted history entries
static history_entry **sorted_history = NULL;
static int history_count = 0;

// Compare function for sorting history entries (newest first)
static int compare_history_entries(const void *a, const void *b) {
    const history_entry *entry_a = *(const history_entry **)a;
    const history_entry *entry_b = *(const history_entry **)b;
    // Sort in descending order (newest first)
    return (int)(entry_b->timestamp - entry_a->timestamp);
}

void load_history(void) {
    FILE *fp = fopen(ROMLAUNCHER_DATA_DIRECTORY "history.txt", "r");
    if (!fp) {
        log_message(LOG_INFO, "No history file found");
        return;
    }

    char line[MAX_PATH_LEN];
    char path[MAX_PATH_LEN];
    time_t timestamp;

    while (fgets(line, sizeof(line), fp)) {
        // Remove newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }

        // Skip empty lines
        if (strlen(line) == 0) {
            continue;
        }

        // Parse line format: "timestamp|path"
        char *separator = strchr(line, '|');
        if (separator) {
            *separator = '\0';
            timestamp = (time_t)atoll(line);
            strncpy(path, separator + 1, MAX_PATH_LEN - 1);
            path[MAX_PATH_LEN - 1] = '\0';

            add_history_entry(path, timestamp);
        }
    }

    fclose(fp);
    log_message(LOG_INFO, "Loaded %d history entries", history_count);
}

void save_history(void) {
    FILE *fp = fopen(ROMLAUNCHER_DATA_DIRECTORY "history.txt", "w");
    if (!fp) {
        log_message(LOG_ERROR, "Could not open history file for writing");
        return;
    }

    // Sort history entries by timestamp (newest first)
    sort_history();

    // Write entries to file (up to MAX_HISTORY_ENTRIES)
    int count = 0;
    for (int i = 0; i < history_count && count < MAX_HISTORY_ENTRIES; i++) {
        const char* path = sorted_history[i]->path;
        if (strncmp(path, "sdmc:/", 6) == 0) {
            fprintf(fp, "%lld|%s\n", (long long)sorted_history[i]->timestamp, path);
        } else {
            fprintf(fp, "%lld|sdmc:/%s\n", (long long)sorted_history[i]->timestamp, path);
        }
        count++;
    }

    fclose(fp);
    log_message(LOG_INFO, "Saved %d history entries", count);
}

void add_history_entry(const char *path, time_t timestamp) {
    if (!path || strlen(path) == 0) {
        return;
    }

    // Check if entry already exists
    history_entry *entry;
    HASH_FIND_STR(history, path, entry);

    if (entry) {
        // Update timestamp of existing entry
        entry->timestamp = timestamp;
        log_message(LOG_DEBUG, "Updated history entry: %s", path);
    } else {
        // Create new entry
        entry = malloc(sizeof(history_entry));
        if (!entry) {
            log_message(LOG_ERROR, "Failed to allocate memory for history entry");
            return;
        }

        strncpy(entry->path, path, sizeof(entry->path) - 1);
        entry->path[sizeof(entry->path) - 1] = '\0';
        entry->timestamp = timestamp;

        HASH_ADD_STR(history, path, entry);
        history_count++;
        log_message(LOG_DEBUG, "Added new history entry: %s", path);
    }
}

void sort_history(void) {
    // Free previous sorted array if it exists
    if (sorted_history) {
        free(sorted_history);
    }

    // Allocate memory for sorted array
    sorted_history = malloc(history_count * sizeof(history_entry*));
    if (!sorted_history) {
        log_message(LOG_ERROR, "Failed to allocate memory for sorted history");
        return;
    }

    // Copy entries to array
    history_entry *entry, *tmp;
    int i = 0;
    HASH_ITER(hh, history, entry, tmp) {
        sorted_history[i++] = entry;
    }

    // Sort array by timestamp
    qsort(sorted_history, history_count, sizeof(history_entry*), compare_history_entries);
}

DirContent* list_history(void) {
    DirContent* content = malloc(sizeof(DirContent));
    if (!content) return NULL;

    memset(content, 0, sizeof(DirContent));
    content->is_history_view = 1;

    // Sort history entries
    sort_history();

    // Allocate memory for file arrays
    content->files = calloc(MAX_HISTORY_ENTRIES, sizeof(char*));
    content->file_textures = calloc(MAX_HISTORY_ENTRIES, sizeof(SDL_Texture*));
    content->file_rects = calloc(MAX_HISTORY_ENTRIES, sizeof(SDL_Rect));

    if (!content->files || !content->file_textures || !content->file_rects) {
        free_dir_content(content);
        return NULL;
    }

    // Copy entries to content (up to MAX_HISTORY_ENTRIES)
    int count = 0;
    for (int i = 0; i < history_count && count < MAX_HISTORY_ENTRIES; i++) {
        content->files[count] = strdup(sorted_history[i]->path);
        if (!content->files[count]) {
            continue;
        }

        count++;
    }

    content->file_count = count;
    log_message(LOG_INFO, "Listed %d history entries", count);

    return content;
}

const char* get_history_entry_path(int index) {
    if (index < 0 || index >= history_count || !sorted_history) {
        return NULL;
    }

    return sorted_history[index]->path;
}

void free_history(void) {
    history_entry *current, *tmp;
    HASH_ITER(hh, history, current, tmp) {
        HASH_DEL(history, current);
        free(current);
    }

    if (sorted_history) {
        free(sorted_history);
        sorted_history = NULL;
    }

    history_count = 0;
}
