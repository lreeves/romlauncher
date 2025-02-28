#include <stdio.h>
#include <stdarg.h>
#include "../source/logging.h"

// Mock implementation of log_message for tests
void log_message(int level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    printf("[TEST-LOG] ");
    vprintf(format, args);
    printf("\n");
    
    va_end(args);
}

// Mock implementations of other logging functions
void log_init(const char* filename) {
    printf("[TEST-LOG] Initialized logging with file: %s\n", filename);
}

void log_close(void) {
    printf("[TEST-LOG] Closed log\n");
}

void cleanup_old_logs(const char* directory) {
    printf("[TEST-LOG] Cleaned up logs in directory: %s\n", directory);
}
