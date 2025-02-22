#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "browser.h"
#include "logging.h"
#include "config.h"

SDL_Texture* render_text(SDL_Renderer *renderer, const char* text,
                              TTF_Font *font, const SDL_Color color, SDL_Rect *rect) {
    SDL_Surface *surface;
    SDL_Texture *texture;

    // Check if this path is a favorite
    char full_path[MAX_PATH_LEN * 2];  // Double buffer size to ensure space for path + filename
    char display_text[MAX_PATH_LEN * 2 + 2];  // Extra space for "* " prefix
    snprintf(full_path, MAX_PATH_LEN * 2, "%s/%s", current_path, text);

    if (is_favorite(full_path)) {
        snprintf(display_text, sizeof(display_text), "* %s", text);
        surface = TTF_RenderText_Blended(font, display_text, color);
    } else {
        surface = TTF_RenderText_Blended(font, text, color);
    }
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    rect->w = surface->w;
    rect->h = surface->h;

    SDL_FreeSurface(surface);
    return texture;
}

static int compare_strings(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

DirContent* list_files(const char* path) {
    log_message(LOG_INFO, "Starting to list files");

    DIR *dir;
    struct dirent *entry;

    DirContent* content = malloc(sizeof(DirContent));
    if (!content) return NULL;

    content->dirs = calloc(MAX_ENTRIES, sizeof(char*));
    content->files = calloc(MAX_ENTRIES, sizeof(char*));
    content->dir_textures = calloc(MAX_ENTRIES, sizeof(SDL_Texture*));
    content->file_textures = calloc(MAX_ENTRIES, sizeof(SDL_Texture*));
    content->dir_rects = calloc(MAX_ENTRIES, sizeof(SDL_Rect));
    content->file_rects = calloc(MAX_ENTRIES, sizeof(SDL_Rect));

    if (!content->dirs || !content->files || !content->dir_textures ||
        !content->file_textures || !content->dir_rects || !content->file_rects) {
        free(content->dirs);
        free(content->files);
        free(content->dir_textures);
        free(content->file_textures);
        free(content->dir_rects);
        free(content->file_rects);
        free(content);
        return NULL;
    }

    content->dir_count = 0;
    content->file_count = 0;

    dir = opendir(path);
    if (dir == NULL) {
        log_message(LOG_INFO, "Failed to open directory: %s", path);
        free(content->dirs);
        free(content->files);
        free(content);
        return NULL;
    }

    log_message(LOG_INFO, "Listing contents of: %s", path);

    while ((entry = readdir(dir)) != NULL) {
        if (content->dir_count >= MAX_ENTRIES || content->file_count >= MAX_ENTRIES) {
            log_message(LOG_INFO, "Maximum number of entries reached");
            break;
        }

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char *name = strdup(entry->d_name);
        if (name == NULL) continue;

        if (entry->d_type == DT_DIR) {
            content->dirs[content->dir_count++] = name;
        } else {
            content->files[content->file_count++] = name;
        }
    }

    if (content->dir_count > 0) {
        qsort(content->dirs, content->dir_count, sizeof(char*), compare_strings);
        log_message(LOG_INFO, "Directories:");
        for (int i = 0; i < content->dir_count; i++) {
            log_message(LOG_DEBUG, "[DIR] %s", content->dirs[i]);
            content->dir_rects[i].x = 50;
            content->dir_rects[i].y = 50 + ((i % ENTRIES_PER_PAGE) * 40);
            content->dir_textures[i] = NULL;
        }
    }

    if (content->file_count > 0) {
        qsort(content->files, content->file_count, sizeof(char*), compare_strings);
        log_message(LOG_DEBUG, "Files:");
        for (int i = 0; i < content->file_count; i++) {
            log_message(LOG_DEBUG, "%s", content->files[i]);
            content->file_rects[i].x = 50;
            int virtual_index = content->dir_count + i;
            content->file_rects[i].y = 50 + ((virtual_index % ENTRIES_PER_PAGE) * 40);
            content->file_textures[i] = NULL;
        }
    }

    closedir(dir);
    return content;
}

void go_up_directory(DirContent* content, char* current_path, const char* rom_directory) {
    char *last_slash = strrchr(current_path, '/');
    if (last_slash && strcmp(current_path, rom_directory) != 0) {
        *last_slash = '\0';

        for (int i = 0; i < content->dir_count; i++) {
            free(content->dirs[i]);
            if (content->dir_textures[i]) SDL_DestroyTexture(content->dir_textures[i]);
        }
        for (int i = 0; i < content->file_count; i++) {
            free(content->files[i]);
            if (content->file_textures[i]) SDL_DestroyTexture(content->file_textures[i]);
        }

        DirContent* new_content = list_files(current_path);
        if (new_content) {
            content->dirs = new_content->dirs;
            content->files = new_content->files;
            content->dir_count = new_content->dir_count;
            content->file_count = new_content->file_count;
            content->dir_textures = new_content->dir_textures;
            content->file_textures = new_content->file_textures;
            content->dir_rects = new_content->dir_rects;
            content->file_rects = new_content->file_rects;
            free(new_content);
        }
    }
}

void change_directory(DirContent* content, int selected_index, char* current_path) {
    if (!content || selected_index >= content->dir_count) return;

    char new_path[MAX_PATH_LEN];
    snprintf(new_path, MAX_PATH_LEN, "%s/%s", current_path, content->dirs[selected_index]);
    strncpy(current_path, new_path, MAX_PATH_LEN - 1);
    current_path[MAX_PATH_LEN - 1] = '\0';

    for (int i = 0; i < content->dir_count; i++) {
        free(content->dirs[i]);
        if (content->dir_textures[i]) SDL_DestroyTexture(content->dir_textures[i]);
    }
    for (int i = 0; i < content->file_count; i++) {
        free(content->files[i]);
        if (content->file_textures[i]) SDL_DestroyTexture(content->file_textures[i]);
    }

    DirContent* new_content = list_files(current_path);
    if (new_content) {
        content->dirs = new_content->dirs;
        content->files = new_content->files;
        content->dir_count = new_content->dir_count;
        content->file_count = new_content->file_count;
        content->dir_textures = new_content->dir_textures;
        content->file_textures = new_content->file_textures;
        content->dir_rects = new_content->dir_rects;
        content->file_rects = new_content->file_rects;
        free(new_content);
    }
}

void free_dir_content(DirContent* content) {
    if (!content) return;

    for (int i = 0; i < content->dir_count; i++) {
        free(content->dirs[i]);
        if (content->dir_textures[i]) SDL_DestroyTexture(content->dir_textures[i]);
    }
    for (int i = 0; i < content->file_count; i++) {
        free(content->files[i]);
        if (content->file_textures[i]) SDL_DestroyTexture(content->file_textures[i]);
    }

    // Free favorite groups structure if this was a favorites view
    if (content->is_favorites_view && content->groups) {
        FavoriteGroup* group = content->groups;
        while (group) {
            FavoriteEntry* entry = group->entries;
            while (entry) {
                FavoriteEntry* next_entry = entry->next;
                free(entry->path);
                free(entry->display_name);
                free(entry);
                entry = next_entry;
            }
            FavoriteGroup* next_group = group->next;
            free(group->group_name);
            free(group);
            group = next_group;
        }
    }

    free(content->dirs);
    free(content->files);
    free(content->dir_textures);
    free(content->file_textures);
    free(content->dir_rects);
    free(content->file_rects);
    free(content);
}

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

void set_selection(DirContent* content, SDL_Renderer *renderer, TTF_Font *font,
                  int selected_index, int current_page) {
    if (!content) return;

    char log_buf[MAX_PATH_LEN];

    int start_index = current_page * ENTRIES_PER_PAGE;
    int end_index = start_index + ENTRIES_PER_PAGE;

    for (int i = 0; i < content->dir_count; i++) {
        if (i < start_index || i >= end_index) {
            if (content->dir_textures[i]) {
                SDL_DestroyTexture(content->dir_textures[i]);
                content->dir_textures[i] = NULL;
            }
            continue;
        }
        snprintf(log_buf, sizeof(log_buf), "[DIR] %s", content->dirs[i]);
        if (content->dir_textures[i]) {
            SDL_DestroyTexture(content->dir_textures[i]);
        }
        content->dir_textures[i] = render_text(renderer, log_buf, font,
            i == selected_index ? COLOR_TEXT_HIGHLIGHT : COLOR_TEXT, &content->dir_rects[i]);
    }

    for (int i = 0; i < content->file_count; i++) {
        int virtual_index = content->dir_count + i;
        if (virtual_index < start_index || virtual_index >= end_index) {
            if (content->file_textures[i]) {
                SDL_DestroyTexture(content->file_textures[i]);
                content->file_textures[i] = NULL;
            }
            continue;
        }
        snprintf(log_buf, sizeof(log_buf), "%s", content->files[i]);
        if (content->file_textures[i]) {
            SDL_DestroyTexture(content->file_textures[i]);
        }
        int entry_index = content->dir_count + i;
        content->file_textures[i] = render_text(renderer, log_buf, font,
            entry_index == selected_index ? COLOR_TEXT_HIGHLIGHT : COLOR_TEXT, &content->file_rects[i]);
    }
}
