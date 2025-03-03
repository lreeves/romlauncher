#ifndef FAVORITES_H
#define FAVORITES_H

#include "browser.h"

// Function declarations
void load_favorites(void);
void save_favorites(void);
int is_favorite(const char *path);
void toggle_favorite(const char *path);
void free_favorites(void);

int is_group_header(const char* text);
int find_next_rom(DirContent* content, int current_index, int direction);
DirContent* list_favorites(void);
void toggle_current_favorite(DirContent* content, int selected_index, const char* current_path);
void dump_favorites(void);

#endif // FAVORITES_H
