#ifndef BROWSER_H
#define BROWSER_H

#include <SDL.h>
#include <SDL_ttf.h>

#define MAX_ENTRIES 1024
#define ENTRIES_PER_PAGE 15
#define MAX_PATH_LEN 512

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
                        TTF_Font *font, SDL_Color color, SDL_Rect *rect);
DirContent* list_files(const char* path);
void go_up_directory(DirContent* content, char* current_path, const char* rom_directory);
void change_directory(DirContent* content, int selected_index, char* current_path);
void set_selection(DirContent* content, SDL_Renderer *renderer, TTF_Font *font,
                  SDL_Color *colors, int selected_index, int current_page);

#endif // BROWSER_H
