#ifndef BROWSER_H
#define BROWSER_H

#include <SDL.h>
#include <SDL_ttf.h>
#include "config.h"

#define MAX_ENTRIES 1024
#define ENTRIES_PER_PAGE 15
#define BOXART_MAX_WIDTH 350

// External declarations
extern int current_boxart_request_id;
extern int is_favorite(const char *path);
extern void toggle_favorite(const char *path);

typedef struct FavoriteEntry {
    char *path;
    char *display_name;
    struct FavoriteEntry *next;
} FavoriteEntry;

typedef struct FavoriteGroup {
    char *group_name;
    FavoriteEntry *entries;
    int entry_count;
    struct FavoriteGroup *next;
} FavoriteGroup;

typedef struct {
    char **dirs;
    char **files;
    int dir_count;
    int file_count;
    SDL_Texture **dir_textures;
    SDL_Texture **file_textures;
    SDL_Rect *dir_rects;
    SDL_Rect *file_rects;
    FavoriteGroup *groups;  // Used only for favorites view
    int is_favorites_view;
    int is_history_view;    // Flag for history view
    SDL_Texture *box_art_texture;
    SDL_Rect box_art_rect;
} DirContent;

// Function to load box art for a ROM
void load_box_art(DirContent* content, const char* rom_path, const char* rom_name);

// Function declarations
SDL_Texture* render_text(SDL_Renderer *renderer, const char* text,
                        TTF_Font *font, const SDL_Color color, SDL_Rect *rect, int is_favorites_view, const char* current_path);
DirContent* list_files(const char* path);
void go_up_directory(DirContent* content, char* current_path, const char* rom_directory);
void change_directory(DirContent* content, int selected_index, char* current_path);
void set_selection(DirContent* content, SDL_Renderer *renderer, TTF_Font *font,
                  int selected_index, int current_page, const char* current_path);
void free_dir_content(DirContent* content);

#endif // BROWSER_H
