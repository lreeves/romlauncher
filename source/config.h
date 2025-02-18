#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>

// Function declarations
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
