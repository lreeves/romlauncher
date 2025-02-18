#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>

// Log levels
#define LOG_DEBUG 0
#define LOG_INFO  1
#define LOG_ERROR 2

// Function declarations
void log_init(const char* filename);
void log_message(int level, const char* format, ...);
void log_close(void);

#endif // LOGGING_H
