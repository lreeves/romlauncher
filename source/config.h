#ifndef CONFIG_H
#define CONFIG_H

#define ROMLAUNCHER_DATA_DIRECTORY "sdmc:/romlauncher/"
#define ROM_PATH "sdmc:/roms/"

// Color definitions
#define COLOR_BACKGROUND    (SDL_Color){200, 200, 200, 255}
#define COLOR_TEXT         (SDL_Color){0, 0, 0, 255}
#define COLOR_TEXT_HIGHLIGHT (SDL_Color){0, 0, 255, 255}
#define COLOR_TEXT_SELECTED (SDL_Color){0, 128, 0, 255}
#define COLOR_TEXT_ERROR    (SDL_Color){255, 0, 0, 255}

#include <stdio.h>
#include <SDL2/SDL.h>
#include "uthash.h"

typedef struct {
    char key[256];           // key string
    char value[512];         // value string
    UT_hash_handle hh;       // makes this structure hashable
} config_entry;

extern config_entry *favorites;  // Global favorites hash table
extern config_entry *default_core_mappings;  // Global default core mappings

// Function declarations
void init_default_core_mappings(void);
void free_default_core_mappings(void);
void config_put(const char *key, const char *value);
const char* config_get(const char *key);
void load_config(void);
void free_config(void);

// Favorites management
void load_favorites(void);
void save_favorites(void);
int is_favorite(const char *path);
void toggle_favorite(const char *path);
void free_favorites(void);

#endif // CONFIG_H
