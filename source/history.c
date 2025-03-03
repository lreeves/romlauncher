#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "history.h"
#include "logging.h"
#include "config.h"
#include "uthash.h"
#include "path_utils.h"

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
    FILE *fp = fopen(ROMLAUNCHER_DATA_DIRECTORY "/history.txt", "r");
    if (!fp) {
        log_message(LOG_INFO, "No history file found");
        return;
    }

    char line[MAX_PATH_LEN];
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

            // Get the relative path from the file
            char rel_path[MAX_PATH_LEN];
            strncpy(rel_path, separator + 1, MAX_PATH_LEN - 1);
            rel_path[MAX_PATH_LEN - 1] = '\0';

            // Convert relative path to absolute path
            char* absolute_path = relative_rom_path_to_absolute(rel_path);
            if (absolute_path) {
                // Add the entry with the absolute path
                add_history_entry(absolute_path, timestamp);
                free(absolute_path);
            } else {
                log_message(LOG_ERROR, "Failed to convert history path to absolute: %s", rel_path);
            }
        }
    }

    fclose(fp);
    log_message(LOG_INFO, "Loaded %d history entries", history_count);

    // Sort history entries after loading
    sort_history();

    // Debug dump of history entries
    dump_history_entries();
}

void save_history(void) {
    FILE *fp = fopen(ROMLAUNCHER_DATA_DIRECTORY "/history.txt", "w");
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
        char* relative_path = absolute_rom_path_to_relative((char*)path);

        if (relative_path) {
            fprintf(fp, "%lld|%s\n", (long long)sorted_history[i]->timestamp, relative_path);
            free(relative_path);
            count++;
        } else {
            log_message(LOG_ERROR, "Failed to convert path to relative: %s", path);
        }
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

    log_message(LOG_DEBUG, "Sorted %d history entries", history_count);
}

DirContent* list_history(void) {
    DirContent* content = malloc(sizeof(DirContent));
    if (!content) return NULL;

    memset(content, 0, sizeof(DirContent));
    content->is_history_view = 1;

    // Sort history entries
    sort_history();

    // Allocate memory for file arrays
    content->dirs = calloc(1, sizeof(char*));  // No directories in history view
    content->files = calloc(MAX_HISTORY_ENTRIES, sizeof(char*));
    content->dir_textures = calloc(1, sizeof(SDL_Texture*));
    content->file_textures = calloc(MAX_HISTORY_ENTRIES, sizeof(SDL_Texture*));
    content->dir_rects = calloc(1, sizeof(SDL_Rect));
    content->file_rects = calloc(MAX_HISTORY_ENTRIES, sizeof(SDL_Rect));

    if (!content->dirs || !content->files || !content->dir_textures ||
        !content->file_textures || !content->dir_rects || !content->file_rects) {
        free_dir_content(content);
        return NULL;
    }

    content->dir_count = 0;

    // If no history entries, show a message
    if (history_count == 0) {
        content->files[0] = strdup("No history yet - play some games!");
        content->file_count = 1;
        // Initialize rect position for centering (will be adjusted in set_selection)
        content->file_rects[0].x = 400; // Temporary value, will be centered
        content->file_rects[0].y = 300; // Temporary value, will be centered
        log_message(LOG_INFO, "No history entries found");
        return content;
    }

    // Copy entries to content (up to MAX_HISTORY_ENTRIES)
    int count = 0;
    for (int i = 0; i < history_count && count < MAX_HISTORY_ENTRIES; i++) {
        // Get the filename from the path
        const char* path = sorted_history[i]->path;
        const char* filename = strrchr(path, '/');

        if (filename) {
            filename++; // Skip the slash

            // Format the display name with timestamp
            char display_name[MAX_PATH_LEN];
            time_t timestamp = sorted_history[i]->timestamp;
            struct tm* timeinfo = localtime(&timestamp);

            char time_str[32];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", timeinfo);

            // Get the file extension
            const char* ext = strrchr(filename, '.');
            if (ext) {
                // Remove extension for display
                int name_len = ext - filename;
                char base_name[MAX_PATH_LEN];
                if (name_len >= MAX_PATH_LEN) name_len = MAX_PATH_LEN - 1;
                strncpy(base_name, filename, name_len);
                base_name[name_len] = '\0';

                // Make sure we don't overflow the buffer
                int time_len = strlen(time_str);
                int base_len = strlen(base_name);
                // Need space for: "[", time_str, "] ", base_name, and null terminator
                // That's 3 extra chars plus the strings
                if (time_len + base_len + 3 >= MAX_PATH_LEN) {
                    // Truncate the base_name to fit
                    base_len = MAX_PATH_LEN - time_len - 4;
                    if (base_len > 0) {
                        base_name[base_len] = '\0';
                    } else {
                        // If we can't even fit part of the name, just use the timestamp
                        snprintf(display_name, sizeof(display_name), "[%s]", time_str);
                        goto skip_name;
                    }
                }

                // Use a temporary buffer to ensure we don't overflow
                char temp_buffer[MAX_PATH_LEN];
                snprintf(temp_buffer, sizeof(temp_buffer), "[%s] ", time_str);
                strncat(temp_buffer, base_name, MAX_PATH_LEN - strlen(temp_buffer) - 1);
                strncpy(display_name, temp_buffer, sizeof(display_name) - 1);
                display_name[sizeof(display_name) - 1] = '\0';
            } else {
                // Make sure we don't overflow the buffer
                int time_len = strlen(time_str);
                int file_len = strlen(filename);
                // Need space for: "[", time_str, "] ", filename, and null terminator
                if (time_len + file_len + 3 >= MAX_PATH_LEN) {
                    // Truncate the filename to fit
                    char truncated_name[MAX_PATH_LEN];
                    int trunc_len = MAX_PATH_LEN - time_len - 4;
                    if (trunc_len > 0) {
                        strncpy(truncated_name, filename, trunc_len);
                        truncated_name[trunc_len] = '\0';

                        // Use a temporary buffer to ensure we don't overflow
                        char temp_buffer[MAX_PATH_LEN];
                        snprintf(temp_buffer, sizeof(temp_buffer), "[%s] ", time_str);
                        strncat(temp_buffer, truncated_name, MAX_PATH_LEN - strlen(temp_buffer) - 1);
                        strncpy(display_name, temp_buffer, sizeof(display_name) - 1);
                        display_name[sizeof(display_name) - 1] = '\0';
                    } else {
                        // If we can't even fit part of the name, just use the timestamp
                        snprintf(display_name, sizeof(display_name), "[%s]", time_str);
                    }
                } else {
                    // Use a temporary buffer to ensure we don't overflow
                    char temp_buffer[MAX_PATH_LEN];
                    snprintf(temp_buffer, sizeof(temp_buffer), "[%s] ", time_str);
                    strncat(temp_buffer, filename, MAX_PATH_LEN - strlen(temp_buffer) - 1);
                    strncpy(display_name, temp_buffer, sizeof(display_name) - 1);
                    display_name[sizeof(display_name) - 1] = '\0';
                }
            }

            skip_name:

            content->files[count] = strdup(display_name);

            // Initialize rect position for rendering
            if (content->files[count]) {
                content->file_rects[count].x = 50;
                content->file_rects[count].y = 50 + (count * 40);
                count++;
                log_message(LOG_DEBUG, "Added history entry %d: %s", count-1, display_name);
            }
        } else {
            // Fallback if we can't parse the filename
            content->files[count] = strdup(path);
            if (content->files[count]) {
                // Initialize rect position
                content->file_rects[count].x = 50;
                content->file_rects[count].y = 50 + (count * 40);
                count++;
                log_message(LOG_DEBUG, "Added history entry %d (fallback): %s", count-1, path);
            }
        }
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

// Get the actual ROM path from a displayed history entry
const char* get_history_rom_path(int display_index) {
    if (display_index < 0 || display_index >= history_count || !sorted_history) {
        log_message(LOG_ERROR, "Invalid history index: %d (max: %d)", display_index, history_count-1);
        return NULL;
    }

    // Fix path format if needed (remove extra slash)
    char* path = sorted_history[display_index]->path;
    if (strncmp(path, "sdmc://", 7) == 0) {
        // Remove the extra slash
        static char fixed_path[MAX_PATH_LEN];
        snprintf(fixed_path, sizeof(fixed_path), "sdmc:/%s", path + 7);
        log_message(LOG_INFO, "Fixed history ROM path for index %d: %s -> %s",
                    display_index, path, fixed_path);
        return fixed_path;
    }

    log_message(LOG_INFO, "Getting history ROM path for index %d: %s",
                display_index, path);
    return path;
}

// Debug function to dump history entries
void dump_history_entries(void) {
    log_message(LOG_INFO, "=== DUMPING HISTORY ENTRIES ===");
    log_message(LOG_INFO, "Total history entries: %d", history_count);

    if (!sorted_history) {
        log_message(LOG_INFO, "No sorted history array");
        return;
    }

    for (int i = 0; i < history_count; i++) {
        if (sorted_history[i]) {
            log_message(LOG_INFO, "Entry %d: %s (timestamp: %lld)",
                        i, sorted_history[i]->path, (long long)sorted_history[i]->timestamp);
        } else {
            log_message(LOG_INFO, "Entry %d: NULL", i);
        }
    }
    log_message(LOG_INFO, "=== END HISTORY DUMP ===");
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
