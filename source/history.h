#ifndef HISTORY_H
#define HISTORY_H

#include <time.h>
#include "browser.h"

// Function declarations
void load_history(void);
void save_history(void);
void add_history_entry(const char *path, time_t timestamp);
void sort_history(void);
DirContent* list_history(void);
const char* get_history_entry_path(int index);
const char* get_history_rom_path(int display_index);
void free_history(void);
// Debug function to dump history entries
void dump_history_entries(void);

#endif // HISTORY_H
