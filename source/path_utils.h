#ifndef PATH_UTILS_H
#define PATH_UTILS_H

/**
 * Converts an absolute ROM path to a relative path by removing ROM_DIRECTORY prefix.
 * Caller is responsible for freeing the returned string.
 * 
 * @param path The absolute path to convert
 * @return A newly allocated string with the relative path, or NULL on error
 */
char* absolute_rom_path_to_relative(char* path);

/**
 * Converts a relative ROM path to an absolute path by adding ROM_DIRECTORY prefix.
 * Caller is responsible for freeing the returned string.
 * 
 * @param path The relative path to convert
 * @return A newly allocated string with the absolute path, or NULL on error
 */
char* relative_rom_path_to_absolute(char* path);

#endif // PATH_UTILS_H
