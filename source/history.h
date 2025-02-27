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
void free_history(void);

#endif // HISTORY_H
