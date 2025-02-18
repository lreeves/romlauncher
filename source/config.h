#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>

// Function declarations
void config_put(const char *key, const char *value);
const char* config_get(const char *key);
void load_config(void);
void free_config(void);

#endif // CONFIG_H
