#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include "logging.h"
#include "config.h"

static FILE* log_file = NULL;

typedef struct {
    char* name;
    time_t mtime;
} LogFile;

static int compare_logfiles(const void* a, const void* b) {
    const LogFile* fa = (const LogFile*)a;
    const LogFile* fb = (const LogFile*)b;
    // Sort in descending order (newest first)
    return fb->mtime - fa->mtime;
}

void cleanup_old_logs(const char* directory) {
    DIR* dir = opendir(directory);
    if (!dir) return;

    struct dirent* entry;
    LogFile* logfiles = NULL;
    int count = 0;
    int capacity = 10;
    logfiles = malloc(capacity * sizeof(LogFile));

    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".log") == NULL) continue;

        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", directory, entry->d_name);
        
        struct stat st;
        if (stat(fullpath, &st) == 0) {
            if (count >= capacity) {
                capacity *= 2;
                logfiles = realloc(logfiles, capacity * sizeof(LogFile));
            }
            logfiles[count].name = strdup(fullpath);
            logfiles[count].mtime = st.st_mtime;
            count++;
        }
    }
    closedir(dir);

    if (count > 5) {
        qsort(logfiles, count, sizeof(LogFile), compare_logfiles);
        // Keep the 5 newest files, delete the rest
        for (int i = 5; i < count; i++) {
            remove(logfiles[i].name);
        }
    }

    // Cleanup
    for (int i = 0; i < count; i++) {
        free(logfiles[i].name);
    }
    free(logfiles);
}

void log_init(const char* filename) {
    cleanup_old_logs(ROMLAUNCHER_DATA_DIRECTORY);
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
