#ifndef BROWSER_H
#define BROWSER_H

#include <SDL.h>
#include <SDL_ttf.h>
#include "config.h"

#define MAX_ENTRIES 1024
#define ENTRIES_PER_PAGE 15
#define MAX_PATH_LEN 512

// External declarations
extern char current_path[MAX_PATH_LEN];
extern int is_favorite(const char *path);
extern void toggle_favorite(const char *path);

typedef struct {
    char **dirs;
    char **files;
    int dir_count;
    int file_count;
    SDL_Texture **dir_textures;
    SDL_Texture **file_textures;
    SDL_Rect *dir_rects;
    SDL_Rect *file_rects;
} DirContent;

// Function declarations
SDL_Texture* render_text(SDL_Renderer *renderer, const char* text,
                        TTF_Font *font, const SDL_Color color, SDL_Rect *rect);
DirContent* list_files(const char* path);
void go_up_directory(DirContent* content, char* current_path, const char* rom_directory);
void change_directory(DirContent* content, int selected_index, char* current_path);
void set_selection(DirContent* content, SDL_Renderer *renderer, TTF_Font *font,
                  int selected_index, int current_page);

// Favorites management
void toggle_current_favorite(DirContent* content, int selected_index, const char* current_path);

// Favorites menu
DirContent* list_favorites(void);
void free_dir_content(DirContent* content);

#endif // BROWSER_H
