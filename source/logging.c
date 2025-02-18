#include <time.h>
#include <stdarg.h>
#include "logging.h"

static FILE* log_file = NULL;

void log_init(const char* filename) {
    log_file = fopen(filename, "w");
}

void log_message(int level, const char* format, ...) {
    if (log_file == NULL) return;

    time_t now;
    time(&now);
    char* timestamp = ctime(&now);
    timestamp[24] = '\0'; // Remove newline that ctime adds
    
    const char* level_str = "";
    switch(level) {
        case LOG_DEBUG: level_str = "DEBUG"; break;
        case LOG_INFO:  level_str = "INFO"; break;
        case LOG_ERROR: level_str = "ERROR"; break;
    }

    // Print the timestamp and level
    fprintf(log_file, "[%s] [%s] ", timestamp, level_str);

    // Handle the variable arguments
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    // Add newline and flush
    fprintf(log_file, "\n");
    fflush(log_file);
}

void log_close(void) {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}
