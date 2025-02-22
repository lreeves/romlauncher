#include <string.h>
#include <stdlib.h>
#include "browser.h"
#include "config.h"
#include "logging.h"

static char* extract_group_name(const char* full_path, const char* rom_path) {
    // Skip the ROM path prefix
    const char* relative_path = strstr(full_path, rom_path);
    if (!relative_path) return strdup("");
    
    relative_path += strlen(rom_path);
    if (*relative_path == '/') relative_path++;
    
    // Find last slash
    char* last_slash = strrchr(relative_path, '/');
    if (!last_slash) return strdup("");
    
    // Create group name from path without the filename
    char* group_name = strndup(relative_path, last_slash - relative_path);
    return group_name ? group_name : strdup("");
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
    
    // First pass: group favorites by directory
    FavoriteGroup* groups = NULL;
    config_entry *current, *tmp;
    
    HASH_ITER(hh, favorites, current, tmp) {
        char* group_name = extract_group_name(current->key, rom_path);
        char* display_name = get_display_name(current->key);
        
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
    
    // If no favorites exist, create a single entry with the help message
    if (!groups) {
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
    
    // Fill arrays with group names and entries
    int idx = 0;
    for (FavoriteGroup* group = groups; group != NULL; group = group->next) {
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
    
    return content;
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
