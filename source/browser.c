#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "browser.h"
#include "logging.h"
#include "config.h"
#include <SDL_image.h>
#include <SDL_thread.h>
#include <stdint.h>

typedef struct {
    char rom_path[MAX_PATH_LEN];
    char rom_name[MAX_PATH_LEN];
    int request_id;
} BoxArtRequest;

int current_boxart_request_id = 0;

// Derive the short system name from the ROM path or extension
// Used primarily for artwork
#if LOAD_ARTWORK
static const char* derive_system_name(const char* rom_path, const char* ext) {
    // First check file extension
    if (strcasecmp(ext, "nes") == 0) return "nes";
    if (strcasecmp(ext, "sfc") == 0) return "snes";
    if (strcasecmp(ext, "gba") == 0) return "gba";
    if (strcasecmp(ext, "gbc") == 0) return "gbc";
    if (strcasecmp(ext, "gb") == 0) return "gb";

    // Then try to find system name in the path
    if (strstr(rom_path, "/snes/")) return "snes";
    if (strstr(rom_path, "/tg16/")) return "tg16";
    if (strstr(rom_path, "/gba/")) return "gba";
    if (strstr(rom_path, "/gbc/")) return "gbc";
    if (strstr(rom_path, "/gb/")) return "gb";

    // Return the extension as fallback
    return ext;
}
#endif

SDL_Texture* render_text(SDL_Renderer *renderer, const char* text,
                              TTF_Font *font, const SDL_Color color, SDL_Rect *rect, int is_favorites_view, const char* current_path) {
    SDL_Surface *surface;
    SDL_Texture *texture;

    if(!renderer) {
        log_message(LOG_ERROR, "Renderer is null when trying to render text");
        exit(1);
    }

    // Check if this path is a favorite
    char full_path[MAX_PATH_LEN * 2];  // Double buffer size to ensure space for path + filename
    char display_text[MAX_PATH_LEN * 2 + 2];  // Extra space for "* " prefix
    snprintf(full_path, MAX_PATH_LEN * 2, "%s/%s", current_path, text);

    if (is_favorite(full_path) && !is_favorites_view) {
        snprintf(display_text, sizeof(display_text), "* %s", text);
        surface = TTF_RenderText_Blended(font, display_text, color);
    } else {
        surface = TTF_RenderText_Blended(font, text, color);
    }

    if (!surface) {
        log_message(LOG_ERROR, "TTF_RenderText_Blended failed: %s", TTF_GetError());
        exit(1);
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);

    if(!texture) {
        log_message(LOG_ERROR, "Couldn't create SDL texture: %s", SDL_GetError());
        SDL_FreeSurface(surface);
        exit(1);
    }

    rect->w = surface->w;
    rect->h = surface->h;

    SDL_FreeSurface(surface);
    return texture;
}

#if LOAD_ARTWORK
static int boxart_loader_thread(void *data) {
    BoxArtRequest *req = (BoxArtRequest *)data;
    const char *ext = strrchr(req->rom_name, '.');
    if (!ext) {
        free(req);
        return 0;
    }
    ext++; // Skip the dot
    const char* system_name = derive_system_name(req->rom_path, ext);

    int basename_length = (int)(ext - req->rom_name - 1);
    char rom_basename[MAX_PATH_LEN];
    strncpy(rom_basename, req->rom_name, basename_length);
    rom_basename[basename_length] = '\0';

    char box_art_path[MAX_PATH_LEN];
    int path_len = snprintf(box_art_path, sizeof(box_art_path),
             "%s/media/%s/2dboxart/",
             ROMLAUNCHER_DATA_DIRECTORY, system_name);
    if (path_len <= 0 || (size_t)path_len >= sizeof(box_art_path)) {
        free(req);
        return 0;
    }
    strncat(box_art_path, rom_basename, sizeof(box_art_path) - path_len - 5);
    strcat(box_art_path, ".png");

    FILE* test = fopen(box_art_path, "r");
    if (!test) {
         free(req);
         return 0;
    }
    fclose(test);

    SDL_Surface* surface = IMG_Load(box_art_path);
    if (!surface) {
         free(req);
         return 0;
    }

    if (req->request_id != current_boxart_request_id) {
         SDL_FreeSurface(surface);
         free(req);
         return 0;
    }

    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_USEREVENT;
    event.user.code = 1;
    event.user.data1 = surface;
    event.user.data2 = (void *)(intptr_t)(req->request_id);
    SDL_PushEvent(&event);

    free(req);
    return 0;
}
#endif

static int compare_strings(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

static void truncate_text(TTF_Font* font, char* text, int max_width) {
    int w, h;
    TTF_SizeText(font, text, &w, &h);
    if (w <= max_width) return;
    const char *ellipsis = "...";
    int ellipsis_w;
    TTF_SizeText(font, ellipsis, &ellipsis_w, &h);
    int target_width = max_width - ellipsis_w;
    int len = strlen(text);
    while (len > 0) {
        text[len-1] = '\0';
        TTF_SizeText(font, text, &w, &h);
        if (w <= target_width) break;
        len = strlen(text);
    }
    strcat(text, ellipsis);
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
    content->box_art_texture = NULL;

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

        // Clear box art texture when changing directories
        if (content->box_art_texture) {
            SDL_DestroyTexture(content->box_art_texture);
            content->box_art_texture = NULL;
        }

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

    // Clear box art texture when changing directories
    if (content->box_art_texture) {
        SDL_DestroyTexture(content->box_art_texture);
        content->box_art_texture = NULL;
    }

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

void load_box_art(DirContent* content, const char* rom_path, const char* rom_name) {
    // Free any existing box art texture first to prevent memory leaks
    if (content->box_art_texture) {
        SDL_DestroyTexture(content->box_art_texture);
        content->box_art_texture = NULL;
    }

    if (!rom_name) return;
    // Cancel any previous box art loading request by incrementing the global request ID
    current_boxart_request_id++;
    BoxArtRequest* req = malloc(sizeof(BoxArtRequest));
    if (!req) return;
    strncpy(req->rom_path, rom_path, MAX_PATH_LEN - 1);
    req->rom_path[MAX_PATH_LEN - 1] = '\0';
    strncpy(req->rom_name, rom_name, MAX_PATH_LEN - 1);
    req->rom_name[MAX_PATH_LEN - 1] = '\0';
    req->request_id = current_boxart_request_id;

#if LOAD_ARTWORK
    // Spawn asynchronous thread to load box art
    SDL_CreateThread(boxart_loader_thread, "BoxArtLoader", req);
#endif
}

void free_dir_content(DirContent* content) {
    if (!content) return;

    if (content->box_art_texture) {
        SDL_DestroyTexture(content->box_art_texture);
        content->box_art_texture = NULL;
    }

    // Free directory entries
    if (content->dirs) {
        for (int i = 0; i < content->dir_count; i++) {
            if (content->dirs[i]) {
                free(content->dirs[i]);
                content->dirs[i] = NULL;
            }
            if (content->dir_textures && content->dir_textures[i]) {
                SDL_DestroyTexture(content->dir_textures[i]);
                content->dir_textures[i] = NULL;
            }
        }
    }

    // Free file entries
    if (content->files) {
        for (int i = 0; i < content->file_count; i++) {
            if (content->files[i]) {
                free(content->files[i]);
                content->files[i] = NULL;
            }
            if (content->file_textures && content->file_textures[i]) {
                SDL_DestroyTexture(content->file_textures[i]);
                content->file_textures[i] = NULL;
            }
        }
    }

    // Free favorite groups structure if this was a favorites view
    if (content->is_favorites_view && content->groups) {
        FavoriteGroup* group = content->groups;
        while (group) {
            FavoriteEntry* entry = group->entries;
            while (entry) {
                FavoriteEntry* next_entry = entry->next;
                if (entry->path) free(entry->path);
                if (entry->display_name) free(entry->display_name);
                free(entry);
                entry = next_entry;
            }
            FavoriteGroup* next_group = group->next;
            if (group->group_name) free(group->group_name);
            free(group);
            group = next_group;
        }
        content->groups = NULL;
    }

    // Free arrays
    if (content->dirs) free(content->dirs);
    if (content->files) free(content->files);
    if (content->dir_textures) free(content->dir_textures);
    if (content->file_textures) free(content->file_textures);
    if (content->dir_rects) free(content->dir_rects);
    if (content->file_rects) free(content->file_rects);
    
    free(content);
}


void set_selection(DirContent* content, SDL_Renderer *renderer, TTF_Font *font,
                  int selected_index, int current_page, const char* current_path) {
    if (!content) return;

    char log_buf[MAX_PATH_LEN];

    int start_index = current_page * ENTRIES_PER_PAGE;
    int end_index = start_index + ENTRIES_PER_PAGE;

    // Special handling for history view
    if (content->is_history_view) {
        log_message(LOG_DEBUG, "Setting selection for history view with %d entries", content->file_count);
        for (int i = 0; i < content->file_count; i++) {
            if (i < start_index || i >= end_index) {
                if (content->file_textures[i]) {
                    SDL_DestroyTexture(content->file_textures[i]);
                    content->file_textures[i] = NULL;
                }
                continue;
            }

            if (content->file_textures[i]) {
                SDL_DestroyTexture(content->file_textures[i]);
            }

            content->file_textures[i] = render_text(renderer, content->files[i], font,
                i == selected_index ? COLOR_TEXT_HIGHLIGHT : COLOR_TEXT,
                &content->file_rects[i], 0, current_path);

            log_message(LOG_DEBUG, "Rendered history entry %d: %s", i, content->files[i]);
        }
        return;
    }

    for (int i = 0; i < content->dir_count; i++) {
        if (i < start_index || i >= end_index) {
            if (content->dir_textures[i]) {
                SDL_DestroyTexture(content->dir_textures[i]);
                content->dir_textures[i] = NULL;
            }
            continue;
        }
        snprintf(log_buf, sizeof(log_buf), "[DIR] %s", content->dirs[i]);
        truncate_text(font, log_buf, 860);
        if (content->dir_textures[i]) {
            SDL_DestroyTexture(content->dir_textures[i]);
        }
        content->dir_textures[i] = render_text(renderer, log_buf, font,
            i == selected_index ? COLOR_TEXT_HIGHLIGHT : COLOR_TEXT, &content->dir_rects[i], content->is_favorites_view, current_path);
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
        // Get filename without extension
        char display_name[MAX_PATH_LEN];
        strncpy(display_name, content->files[i], MAX_PATH_LEN - 1);
        display_name[MAX_PATH_LEN - 1] = '\0';

        // Find and remove extension
        char *dot = strrchr(display_name, '.');
        if (dot) *dot = '\0';

        snprintf(log_buf, sizeof(log_buf), "%s", display_name);
        if (content->file_textures[i]) {
            SDL_DestroyTexture(content->file_textures[i]);
        }
        truncate_text(font, log_buf, 860);
        int entry_index = content->dir_count + i;

        // Special handling for the "no favorites" or "no history" message
        if ((content->is_favorites_view || content->is_history_view) && content->file_count == 1 && i == 0 &&
            (strstr(content->files[i], "No history yet") || strstr(content->files[i], "Use the X button"))) {
            // Render the text first to get its dimensions
            SDL_Texture* texture = render_text(renderer, log_buf, font, COLOR_TEXT, &content->file_rects[i], content->is_favorites_view, current_path);
            // Center the text on screen
            content->file_rects[i].x = (1280 - content->file_rects[i].w) / 2;  // Assuming 1280x720 screen
            content->file_rects[i].y = (720 - content->file_rects[i].h - STATUS_BAR_HEIGHT) / 2;  // Account for status bar
            content->file_textures[i] = texture;
            log_message(LOG_DEBUG, "Centered special message: %s at (%d, %d)",
                       content->files[i], content->file_rects[i].x, content->file_rects[i].y);
        } else {
            content->file_textures[i] = render_text(renderer, log_buf, font,
                entry_index == selected_index ? COLOR_TEXT_HIGHLIGHT : COLOR_TEXT, &content->file_rects[i], content->is_favorites_view, current_path);
        }
    }
    if (selected_index >= content->dir_count && selected_index < content->dir_count + content->file_count) {
        int file_index = selected_index - content->dir_count;
        load_box_art(content, current_path, content->files[file_index]);
    }
}
