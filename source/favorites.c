#include <string.h>
#include <stdlib.h>
#include "browser.h"
#include "config.h"
#include "logging.h"

static int compare_strings(const void* a, const void* b) {
    const char* str_a = (*(FavoriteGroup**)a)->group_name;
    const char* str_b = (*(FavoriteGroup**)b)->group_name;
    return strcmp(str_a, str_b);
}

static int compare_entries(const void* a, const void* b) {
    const char* str_a = (*(FavoriteEntry**)a)->display_name;
    const char* str_b = (*(FavoriteEntry**)b)->display_name;
    return strcmp(str_a, str_b);
}

static char* extract_group_name(const char* full_path, const char* rom_path) {
    log_message(LOG_DEBUG, "Extracting group from: %s (ROM path: %s)", full_path, rom_path);
    
    // Normalize paths for comparison (remove "sdmc:" prefix if present)
    char norm_full_path[MAX_PATH_LEN];
    char norm_rom_path[MAX_PATH_LEN];
    
    // Normalize full path
    if (strncmp(full_path, "sdmc:", 5) == 0) {
        strncpy(norm_full_path, full_path + 5, MAX_PATH_LEN - 1);
    } else {
        strncpy(norm_full_path, full_path, MAX_PATH_LEN - 1);
    }
    norm_full_path[MAX_PATH_LEN - 1] = '\0';
    
    // Normalize ROM path
    if (strncmp(rom_path, "sdmc:", 5) == 0) {
        strncpy(norm_rom_path, rom_path + 5, MAX_PATH_LEN - 1);
    } else {
        strncpy(norm_rom_path, rom_path, MAX_PATH_LEN - 1);
    }
    norm_rom_path[MAX_PATH_LEN - 1] = '\0';
    
    log_message(LOG_DEBUG, "Normalized paths: %s / %s", norm_full_path, norm_rom_path);
    
    // Find common ROM path prefix
    const char* relative_path = strstr(norm_full_path, norm_rom_path);
    if (!relative_path) {
        log_message(LOG_INFO, "ROM path not found in favorite path, using full path");
        // Extract directory path without filename
        char* last_slash = strrchr(norm_full_path, '/');
        if (!last_slash) return strdup("Uncategorized");
        
        return strndup(norm_full_path, last_slash - norm_full_path);
    }
    
    // Skip ROM path prefix
    relative_path += strlen(norm_rom_path);
    if (*relative_path == '/') relative_path++;
    
    // If there's no remaining path, use "Root"
    if (*relative_path == '\0') return strdup("Root");
    
    // Find last slash (this separates directory from filename)
    char* last_slash = strrchr(relative_path, '/');
    if (!last_slash) return strdup("Root");
    
    // Create group name from path without the filename
    char* group_name = strndup(relative_path, last_slash - relative_path);
    if (!group_name) return strdup("Uncategorized");
    
    // Remove leading slash if present
    if (group_name[0] == '/') {
        char* fixed_name = strdup(group_name + 1);
        free(group_name);
        group_name = fixed_name ? fixed_name : strdup("Uncategorized");
    }
    
    // Log the resulting group name
    log_message(LOG_DEBUG, "Extracted group name: %s", group_name);
    
    return group_name;
}

static char* get_display_name(const char* full_path) {
    const char* last_slash = strrchr(full_path, '/');
    return last_slash ? strdup(last_slash + 1) : strdup(full_path);
}

int is_group_header(const char* text) {
    return text && text[0] == '[' && text[strlen(text)-1] == ']';
}

int find_next_rom(DirContent* content, int current_index, int direction) {
    if (!content || !content->files || content->file_count == 0) {
        return current_index;
    }

    int index = current_index;
    do {
        index += direction;
        
        // Handle wrapping
        if (index >= content->file_count) {
            index = 0;
        }
        if (index < 0) {
            index = content->file_count - 1;
        }
        
        // Found a non-header entry
        if (!is_group_header(content->files[index])) {
            return index;
        }
        
        // If we've wrapped around to our starting point, just return the current index
        if (index == current_index) {
            break;
        }
        
    } while (1);
    
    return current_index;
}

DirContent* list_favorites(void) {
    DirContent* content = malloc(sizeof(DirContent));
    if (!content) return NULL;
    
    memset(content, 0, sizeof(DirContent));
    content->is_favorites_view = 1;
    
    // Get ROM path from config
    const char* rom_path = config_get("rom_path");
    if (!rom_path) rom_path = "sdmc:/roms";
    
    log_message(LOG_INFO, "Listing favorites with ROM path: %s", rom_path);
    
    // Count favorites to ensure we have some
    int favorite_count = 0;
    config_entry *current, *tmp;
    HASH_ITER(hh, favorites, current, tmp) {
        favorite_count++;
    }
    log_message(LOG_INFO, "Found %d favorites to list", favorite_count);
    
    // First pass: group favorites by directory
    FavoriteGroup* groups = NULL;
    config_entry *current, *tmp;
    
    // Process favorites
    HASH_ITER(hh, favorites, current, tmp) {
        log_message(LOG_DEBUG, "Processing favorite: %s", current->key);
        char* group_name = extract_group_name(current->key, rom_path);
        char* display_name = get_display_name(current->key);
        
        if (!group_name || !display_name) {
            log_message(LOG_ERROR, "Failed to get group name or display name for %s", current->key);
            if (group_name) free(group_name);
            if (display_name) free(display_name);
            continue;
        }
        
        log_message(LOG_DEBUG, "Grouped '%s' into '%s' with display name '%s'", 
                   current->key, group_name, display_name);
        
        // Find or create group
        FavoriteGroup* group = NULL;
        for (FavoriteGroup* g = groups; g != NULL; g = g->next) {
            if (strcmp(g->group_name, group_name) == 0) {
                group = g;
                break;
            }
        }
        
        if (!group) {
            group = malloc(sizeof(FavoriteGroup));
            if (!group) {
                free(group_name);
                free(display_name);
                continue;
            }
            group->group_name = group_name;
            group->entries = NULL;
            group->entry_count = 0;
            group->next = groups;
            groups = group;
        } else {
            free(group_name);
        }
        
        // Add entry to group
        FavoriteEntry* entry = malloc(sizeof(FavoriteEntry));
        if (!entry) {
            free(display_name);
            continue;
        }
        entry->path = strdup(current->key);
        entry->display_name = display_name;
        entry->next = group->entries;
        group->entries = entry;
        group->entry_count++;
    }
    
    // If no favorites exist or we couldn't process any, create a single entry with the help message
    if (!groups || group_idx == 0) {
        log_message(LOG_INFO, "No favorite groups created, showing help message");
        content->files = calloc(1, sizeof(char*));
        content->file_textures = calloc(1, sizeof(SDL_Texture*));
        content->file_rects = calloc(1, sizeof(SDL_Rect));
        
        if (!content->files || !content->file_textures || !content->file_rects) {
            free_dir_content(content);
            return NULL;
        }
        
        content->files[0] = strdup("Use the X button to add favorites!");
        content->file_count = 1;
        
        // Center the message on screen
        content->file_rects[0].x = 400; // Will be adjusted when rendered
        content->file_rects[0].y = 300; // Will be adjusted when rendered
        return content;
    }

    // Count total entries needed
    int total_entries = 0;
    for (FavoriteGroup* group = groups; group != NULL; group = group->next) {
        total_entries += 1 + group->entry_count; // Group name + entries
    }
    
    // Allocate arrays
    content->files = calloc(total_entries, sizeof(char*));
    content->file_textures = calloc(total_entries, sizeof(SDL_Texture*));
    content->file_rects = calloc(total_entries, sizeof(SDL_Rect));
    
    if (!content->files || !content->file_textures || !content->file_rects) {
        // Handle allocation failure
        free_dir_content(content);
        return NULL;
    }
    
    // Convert linked list to array for sorting
    FavoriteGroup** group_array = malloc(sizeof(FavoriteGroup*) * total_entries);
    if (!group_array) {
        free_dir_content(content);
        return NULL;
    }
    
    int group_idx = 0;
    for (FavoriteGroup* group = groups; group != NULL; group = group->next) {
        group_array[group_idx++] = group;
        
        // Convert linked list of entries to array and sort
        FavoriteEntry** entry_array = malloc(sizeof(FavoriteEntry*) * group->entry_count);
        if (entry_array) {
            int entry_idx = 0;
            for (FavoriteEntry* entry = group->entries; entry != NULL; entry = entry->next) {
                entry_array[entry_idx++] = entry;
            }
            
            // Sort entries by display name
            qsort(entry_array, group->entry_count, sizeof(FavoriteEntry*),
                  compare_entries);
            
            // Rebuild linked list in sorted order
            group->entries = NULL;
            for (int i = group->entry_count - 1; i >= 0; i--) {
                entry_array[i]->next = group->entries;
                group->entries = entry_array[i];
            }
            free(entry_array);
        }
    }
    
    // Sort groups by name
    qsort(group_array, group_idx, sizeof(FavoriteGroup*),
          compare_strings);
    
    // Fill arrays with sorted group names and entries
    int idx = 0;
    for (int i = 0; i < group_idx; i++) {
        FavoriteGroup* group = group_array[i];
        // Add group name
        char group_display[MAX_PATH_LEN];
        snprintf(group_display, sizeof(group_display), "[%s]", group->group_name);
        content->files[idx] = strdup(group_display);
        content->file_rects[idx].x = 30;
        content->file_rects[idx].y = 50 + ((idx % ENTRIES_PER_PAGE) * 40);
        idx++;
        
        // Add entries
        for (FavoriteEntry* entry = group->entries; entry != NULL; entry = entry->next) {
            content->files[idx] = strdup(entry->display_name);
            content->file_rects[idx].x = 50;
            content->file_rects[idx].y = 50 + ((idx % ENTRIES_PER_PAGE) * 40);
            idx++;
        }
    }
    
    content->file_count = idx;
    content->groups = groups; // Store for later cleanup
    
    free(group_array);
    return content;
}

void dump_favorites(void) {
    log_message(LOG_INFO, "===== DUMPING FAVORITES =====");
    int count = 0;
    config_entry *current, *tmp;
    
    HASH_ITER(hh, favorites, current, tmp) {
        count++;
        log_message(LOG_INFO, "Favorite %d: %s", count, current->key);
    }
    
    log_message(LOG_INFO, "Total favorites: %d", count);
    log_message(LOG_INFO, "===== END FAVORITES DUMP =====");
}

void toggle_current_favorite(DirContent* content, int selected_index, const char* current_path) {
    if (!content) return;

    // Only allow favoriting files, not directories
    if (selected_index < content->dir_count) return;

    int file_index = selected_index - content->dir_count;
    if (file_index >= content->file_count) return;

    char full_path[MAX_PATH_LEN];
    snprintf(full_path, MAX_PATH_LEN, "%s/%s", current_path, content->files[file_index]);

    toggle_favorite(full_path);
}
