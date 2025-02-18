#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "browser.h"
#include "logging.h"
#include "config.h"

SDL_Texture* render_text(SDL_Renderer *renderer, const char* text, 
                              TTF_Font *font, SDL_Color color, SDL_Rect *rect) {
    SDL_Surface *surface;
    SDL_Texture *texture;

    // Check if this path is a favorite
    char full_path[MAX_PATH_LEN * 2];  // Double buffer size to ensure space for path + filename
    char display_text[MAX_PATH_LEN * 2 + 2];  // Extra space for "* " prefix
    snprintf(full_path, MAX_PATH_LEN * 2, "%s/%s", current_path, text);
    
    if (is_favorite(full_path)) {
        snprintf(display_text, sizeof(display_text), "* %s", text);
        surface = TTF_RenderText_Solid(font, display_text, color);
    } else {
        surface = TTF_RenderText_Solid(font, text, color);
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
        log_message(LOG_INFO, "Files:");
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
    
    free(content->dirs);
    free(content->files);
    free(content->dir_textures);
    free(content->file_textures);
    free(content->dir_rects);
    free(content->file_rects);
    free(content);
}

DirContent* list_favorites(void) {
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
        free_dir_content(content);
        return NULL;
    }

    content->dir_count = 0;
    content->file_count = 0;

    config_entry *current, *tmp;
    HASH_ITER(hh, favorites, current, tmp) {
        if (content->file_count >= MAX_ENTRIES) break;
        
        content->files[content->file_count] = strdup(current->key);
        if (content->files[content->file_count]) {
            content->file_rects[content->file_count].x = 50;
            content->file_rects[content->file_count].y = 50 + (content->file_count * 40);
            content->file_textures[content->file_count] = NULL;
            content->file_count++;
        }
    }

    if (content->file_count > 0) {
        qsort(content->files, content->file_count, sizeof(char*), compare_strings);
        for (int i = 0; i < content->file_count; i++) {
            content->file_rects[i].y = 50 + ((i % ENTRIES_PER_PAGE) * 40);
        }
    }

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
                  SDL_Color *colors, int selected_index, int current_page) {
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
            colors[i == selected_index ? 8 : 1], &content->dir_rects[i]);
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
            colors[entry_index == selected_index ? 8 : 0], &content->file_rects[i]);
    }
}
