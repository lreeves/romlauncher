#ifndef CONFIG_H
#define CONFIG_H

#include <SDL2/SDL.h>
#include "uthash.h"

// Color definitions
#define COLOR_BACKGROUND    (SDL_Color){200, 200, 200, 255}
#define COLOR_TEXT         (SDL_Color){0, 0, 0, 255}
#define COLOR_TEXT_HIGHLIGHT (SDL_Color){0, 0, 255, 255}
#define COLOR_TEXT_SELECTED (SDL_Color){0, 128, 0, 255}
#define COLOR_TEXT_ERROR    (SDL_Color){255, 0, 0, 255}
#define COLOR_STATUS_BAR (SDL_Color){220, 220, 220, 255}
#define COLOR_STATUS_TEXT (SDL_Color){20, 20, 20, 255}
#define MAX_PATH_LEN 1024

#define SCREEN_W 1280
#define SCREEN_H 720
#define STATUS_BAR_HEIGHT 30
#define LOAD_ARTWORK 0

// Any path constants should not have a trailing slash

#ifdef ROMLAUNCHER_BUILD_LINUX
    #define ROMLAUNCHER_DATA_DIRECTORY "/home/luke/.config/Ryujinx/sdcard/romlauncher"
    #define ROM_DIRECTORY "/home/luke/.config/Ryujinx/sdcard/roms"
    #define RETROARCH_DIRECTORY "/home/luke/.config/Ryujinx/sdcard/retroarch"
#else
    #define ROMLAUNCHER_DATA_DIRECTORY "sdmc:/romlauncher"
    #define ROM_DIRECTORY "sdmc:/roms"
    #define RETROARCH_DIRECTORY "sdmc:/retroarch"
#endif

#define ROMLAUNCHER_MEDIA_DIRECTORY ROMLAUNCHER_DATA_DIRECTORY "/media"

typedef struct {
    char key[256];           // key string
    char value[512];         // value string
    UT_hash_handle hh;       // makes this structure hashable
} config_entry;

extern config_entry *favorites;  // Global favorites hash table
extern config_entry *default_core_mappings;  // Global default core mappings
extern config_entry *system_names_mappings;  // Global system names mappings

// Function declarations
void init_default_core_mappings(void);
void free_default_core_mappings(void);
void init_system_names_mappings(void);
void free_system_names_mappings(void);
const char* get_system_full_name(const char *short_name);
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
